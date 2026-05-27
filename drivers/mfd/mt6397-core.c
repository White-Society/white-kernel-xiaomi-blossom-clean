// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Flora Fu, MediaTek
 *
 * Multi-Function Device (MFD) core driver for MediaTek PMICs:
 *   - MT6323
 *   - MT6357
 *   - MT6358
 *   - MT6397
 *
 * All chips share the same PMIC Wrapper (pwrap) bus and are handled by a
 * single unified driver; per-chip differences (register bases, IRQ layout,
 * sub-device list) are described via struct chip_data.
 */

#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>
#include <linux/mfd/mt6323/core.h>
#include <linux/mfd/mt6358/core.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6323/registers.h>
#include <linux/mfd/mt6358/registers.h>
#include <linux/mfd/mt6397/registers.h>
#include <linux/mfd/mt6357/core.h>
#include <linux/mfd/mt6357/registers.h>

/* ------------------------------------------------------------------ */
/* Register windows for sub-blocks (RTC, PWRC, etc.)                   */
/* ------------------------------------------------------------------ */

#define MT6323_RTC_BASE		0x8000
#define MT6323_RTC_SIZE		0x40

#define MT6358_RTC_BASE		0x0588
#define MT6358_RTC_SIZE		0x3c

#define MT6397_RTC_BASE		0xe000
#define MT6397_RTC_SIZE		0x3e

#define MT6323_PWRC_BASE	0x8000
#define MT6323_PWRC_SIZE	0x40

/* ------------------------------------------------------------------ */
/* Resources for sub-devices                                           */
/* ------------------------------------------------------------------ */

static const struct resource mt6323_rtc_resources[] = {
	DEFINE_RES_MEM(MT6323_RTC_BASE, MT6323_RTC_SIZE),
	DEFINE_RES_IRQ(MT6323_IRQ_STATUS_RTC),
};

static const struct resource mt6358_rtc_resources[] = {
	DEFINE_RES_MEM(MT6358_RTC_BASE, MT6358_RTC_SIZE),
	DEFINE_RES_IRQ(MT6358_IRQ_RTC),
};

static const struct resource mt6397_rtc_resources[] = {
	DEFINE_RES_MEM(MT6397_RTC_BASE, MT6397_RTC_SIZE),
	DEFINE_RES_IRQ(MT6397_IRQ_RTC),
};

static const struct resource mt6323_keys_resources[] = {
	DEFINE_RES_IRQ(MT6323_IRQ_STATUS_PWRKEY),
	DEFINE_RES_IRQ(MT6323_IRQ_STATUS_FCHRKEY),
};

static const struct resource mt6397_keys_resources[] = {
	DEFINE_RES_IRQ(MT6397_IRQ_PWRKEY),
	DEFINE_RES_IRQ(MT6397_IRQ_HOMEKEY),
};

/*
 * MT6357 keys (PWRKEY / PWRKEY_R) live in the PSC_TOP block.
 * Their Linux virq is resolved through the single irq_domain created
 * by mt6357_irq_init() below.
 *
 * NOTE: Bit 1 is intentionally skipped between PWRKEY (bit 0) and
 * PWRKEY_R (bit 2). According to MT6357 datasheet, bit 1 is reserved
 * or used for PWRKEY_F (falling edge), which is not exposed here.
 * Verify against <linux/mfd/mt6357/core.h> if adding more PSC IRQs.
 */
static const struct resource mt6357_keys_resources[] = {
	DEFINE_RES_IRQ(MT6357_IRQ_PWRKEY),
	DEFINE_RES_IRQ(MT6357_IRQ_PWRKEY_R),
};

static const struct resource mt6323_pwrc_resources[] = {
	DEFINE_RES_MEM(MT6323_PWRC_BASE, MT6323_PWRC_SIZE),
};

/* ------------------------------------------------------------------ */
/* MT6357 IRQ (PSC_TOP) — PWRKEY only                                  */
/*                                                                     */
/* IMPORTANT: Buck/LDO Over-Current IRQs are NOT exposed here through  */
/* the MFD layer. They live in separate HW blocks (BUCK_TOP / LDO_TOP) */
/* with their own status/mask registers. The regulator sub-driver      */
/* (mt6357-regulator.c) must register its own regmap_irq_chip instances */
/* for BUCK and LDO blocks if OC interrupt handling is required.       */
/*                                                                     */
/* Example for mt6357-regulator.c probe():                             */
/*   struct regmap *regmap = dev_get_regmap(pdev->dev.parent, NULL);   */
/*   int pmic_irq = platform_get_irq(to_platform_device(               */
/*                     pdev->dev.parent), 0);                          */
/*   ret = devm_regmap_add_irq_chip(&pdev->dev, regmap, pmic_irq,      */
/*                                  IRQF_ONESHOT, 0,                   */
/*                                  &mt6357_buck_irq_chip,             */
/*                                  &priv->buck_irq_data);             */
/*   int vproc_virq = regmap_irq_get_virq(priv->buck_irq_data,         */
/*                                        MT6357_IRQ_VPROC_OC);        */
/* ------------------------------------------------------------------ */

static const struct regmap_irq mt6357_irqs[] = {
	REGMAP_IRQ_REG(MT6357_IRQ_PWRKEY,   0, BIT(0)),
	/*
	 * Bit 1 is reserved or PWRKEY_F (falling edge).
	 * Only rising edge (PWRKEY_R, bit 2) is exposed here.
	 */
	REGMAP_IRQ_REG(MT6357_IRQ_PWRKEY_R, 0, BIT(2)),
};

static const struct regmap_irq_chip mt6357_irq_chip = {
	.name		= "mt6357-irq",
	.irqs		= mt6357_irqs,
	.num_irqs	= ARRAY_SIZE(mt6357_irqs),
	.num_regs	= 1,
	.status_base	= MT6357_PSC_TOP_INT_STATUS0,
	.mask_base	= MT6357_PSC_TOP_INT_MASK_CON0,
};

/*
 * MT6357-specific IRQ data container.
 * Preserves pointers to regmap_irq_chip_data for potential future use
 * (debugging, additional sub-devices, etc.). In the current upstream
 * pattern, only irq_domain is needed by MFD layer.
 */
struct mt6357_irq_data {
	struct regmap_irq_chip_data *psc;
};

static int mt6357_irq_init(struct mt6397_chip *chip)
{
	struct mt6357_irq_data *data;
	int ret;

	data = devm_kzalloc(chip->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	ret = devm_regmap_add_irq_chip(chip->dev, chip->regmap, chip->irq,
				       IRQF_ONESHOT, 0,
				       &mt6357_irq_chip, &data->psc);
	if (ret) {
		dev_err(chip->dev, "Failed to add mt6357 IRQ chip: %d\n", ret);
		return ret;
	}

	chip->irq_domain = regmap_irq_get_domain(data->psc);

	/*
	 * Save irq_data pointer for potential future use.
	 * Accessible via dev_get_drvdata(chip->dev) if needed.
	 */
	dev_set_drvdata(chip->dev, data);

	return 0;
}

/* ------------------------------------------------------------------ */
/* Sub-device (MFD cell) definitions                                   */
/* ------------------------------------------------------------------ */

/*
 * MT6357 sub-devices:
 *   - mt6357-regulator: declared WITHOUT resources, exactly like
 *     MT6323/MT6358/MT6397. Buck/LDO OC IRQs, if needed, must be wired up
 *     inside the regulator sub-driver itself (see comment above mt6357_irqs).
 *   - mtk-pmic-keys: uses the single PSC irq_domain.
 */
static const struct mfd_cell mt6357_devs[] = {
	{
		.name		= "mt6357-regulator",
		.of_compatible	= "mediatek,mt6357-regulator",
	},
	{
		.name		= "mtk-pmic-keys",
		.num_resources	= ARRAY_SIZE(mt6357_keys_resources),
		.resources	= mt6357_keys_resources,
		.of_compatible	= "mediatek,mt6357-keys",
	},
};

static const struct mfd_cell mt6323_devs[] = {
	{
		.name		= "mt6323-rtc",
		.num_resources	= ARRAY_SIZE(mt6323_rtc_resources),
		.resources	= mt6323_rtc_resources,
		.of_compatible	= "mediatek,mt6323-rtc",
	}, {
		.name		= "mt6323-regulator",
		.of_compatible	= "mediatek,mt6323-regulator",
	}, {
		.name		= "mt6323-led",
		.of_compatible	= "mediatek,mt6323-led",
	}, {
		.name		= "mtk-pmic-keys",
		.num_resources	= ARRAY_SIZE(mt6323_keys_resources),
		.resources	= mt6323_keys_resources,
		.of_compatible	= "mediatek,mt6323-keys",
	}, {
		.name		= "mt6323-pwrc",
		.num_resources	= ARRAY_SIZE(mt6323_pwrc_resources),
		.resources	= mt6323_pwrc_resources,
		.of_compatible	= "mediatek,mt6323-pwrc",
	},
};

static const struct mfd_cell mt6358_devs[] = {
	{
		.name		= "mt6358-regulator",
		.of_compatible	= "mediatek,mt6358-regulator",
	}, {
		.name		= "mt6358-rtc",
		.num_resources	= ARRAY_SIZE(mt6358_rtc_resources),
		.resources	= mt6358_rtc_resources,
		.of_compatible	= "mediatek,mt6358-rtc",
	}, {
		.name		= "mt6358-sound",
		.of_compatible	= "mediatek,mt6358-sound",
	},
};

static const struct mfd_cell mt6397_devs[] = {
	{
		.name		= "mt6397-rtc",
		.num_resources	= ARRAY_SIZE(mt6397_rtc_resources),
		.resources	= mt6397_rtc_resources,
		.of_compatible	= "mediatek,mt6397-rtc",
	}, {
		.name		= "mt6397-regulator",
		.of_compatible	= "mediatek,mt6397-regulator",
	}, {
		.name		= "mt6397-codec",
		.of_compatible	= "mediatek,mt6397-codec",
	}, {
		.name		= "mt6397-clk",
		.of_compatible	= "mediatek,mt6397-clk",
	}, {
		.name		= "mt6397-pinctrl",
		.of_compatible	= "mediatek,mt6397-pinctrl",
	}, {
		.name		= "mtk-pmic-keys",
		.num_resources	= ARRAY_SIZE(mt6397_keys_resources),
		.resources	= mt6397_keys_resources,
		.of_compatible	= "mediatek,mt6397-keys",
	},
};

/* ------------------------------------------------------------------ */
/* Per-chip glue: CID register, cell list, IRQ init callback           */
/* ------------------------------------------------------------------ */

struct chip_data {
	u32 cid_addr;
	u32 cid_shift;
	const struct mfd_cell *cells;
	int cell_size;
	int (*irq_init)(struct mt6397_chip *chip);
};

static const struct chip_data mt6323_core = {
	.cid_addr	= MT6323_CID,
	.cid_shift	= 0,
	.cells		= mt6323_devs,
	.cell_size	= ARRAY_SIZE(mt6323_devs),
	.irq_init	= mt6397_irq_init,
};

static const struct chip_data mt6357_core = {
	.cid_addr	= MT6357_SWCID,
	.cid_shift	= 8,
	.cells		= mt6357_devs,
	.cell_size	= ARRAY_SIZE(mt6357_devs),
	.irq_init	= mt6357_irq_init,
};

static const struct chip_data mt6358_core = {
	.cid_addr	= MT6358_SWCID,
	.cid_shift	= 8,
	.cells		= mt6358_devs,
	.cell_size	= ARRAY_SIZE(mt6358_devs),
	.irq_init	= mt6358_irq_init,
};

static const struct chip_data mt6397_core = {
	.cid_addr	= MT6397_CID,
	.cid_shift	= 0,
	.cells		= mt6397_devs,
	.cell_size	= ARRAY_SIZE(mt6397_devs),
	.irq_init	= mt6397_irq_init,
};

/* ------------------------------------------------------------------ */
/* Probe                                                               */
/* ------------------------------------------------------------------ */

static int mt6397_probe(struct platform_device *pdev)
{
	int ret;
	unsigned int id = 0;
	struct mt6397_chip *pmic;
	const struct chip_data *pmic_core;

	pmic = devm_kzalloc(&pdev->dev, sizeof(*pmic), GFP_KERNEL);
	if (!pmic)
		return -ENOMEM;

	pmic->dev = &pdev->dev;

	/*
	 * mt6397 MFD is a child of the SoC PMIC wrapper (pwrap).
	 * The regmap is provided by the parent device.
	 */
	pmic->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!pmic->regmap)
		return -ENODEV;

	pmic_core = of_device_get_match_data(&pdev->dev);
	if (!pmic_core)
		return -ENODEV;

	ret = regmap_read(pmic->regmap, pmic_core->cid_addr, &id);
	if (ret) {
		dev_err(&pdev->dev, "Failed to read chip id: %d\n", ret);
		return ret;
	}

	pmic->chip_id = (id >> pmic_core->cid_shift) & 0xff;

	platform_set_drvdata(pdev, pmic);

	pmic->irq = platform_get_irq(pdev, 0);
	if (pmic->irq < 0)
		return pmic->irq;
	if (!pmic->irq)
		return -EINVAL;

	if (pmic_core->irq_init) {
		ret = pmic_core->irq_init(pmic);
		if (ret)
			return ret;
	} else {
		dev_warn(&pdev->dev,
			 "No IRQ init function specified for this chip\n");
	}

	ret = devm_mfd_add_devices(&pdev->dev, PLATFORM_DEVID_NONE,
				   pmic_core->cells, pmic_core->cell_size,
				   NULL, 0, pmic->irq_domain);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add child devices: %d\n", ret);
		return ret;
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/* Device tree / platform matching                                     */
/* ------------------------------------------------------------------ */

static const struct of_device_id mt6397_of_match[] = {
	{
		.compatible = "mediatek,mt6323",
		.data	    = &mt6323_core,
	}, {
		.compatible = "mediatek,mt6357",
		.data	    = &mt6357_core,
	}, {
		.compatible = "mediatek,mt6358",
		.data	    = &mt6358_core,
	}, {
		.compatible = "mediatek,mt6397",
		.data	    = &mt6397_core,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, mt6397_of_match);

static const struct platform_device_id mt6397_id[] = {
	{ "mt6397", 0 },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(platform, mt6397_id);

static struct platform_driver mt6397_driver = {
	.probe = mt6397_probe,
	.driver = {
		.name		= "mt6397",
		.of_match_table	= of_match_ptr(mt6397_of_match),
	},
	.id_table = mt6397_id,
};
module_platform_driver(mt6397_driver);

MODULE_AUTHOR("Flora Fu, MediaTek");
MODULE_DESCRIPTION("Driver for MediaTek MT6323/MT6357/MT6358/MT6397 PMICs");
MODULE_LICENSE("GPL");