// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Flora Fu, MediaTek
 */

#include <linux/interrupt.h>
#include <linux/ioport.h>
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

#define MT6323_RTC_BASE		0x8000
#define MT6323_RTC_SIZE		0x40

#define MT6358_RTC_BASE		0x0588
#define MT6358_RTC_SIZE		0x3c

#define MT6397_RTC_BASE		0xe000
#define MT6397_RTC_SIZE		0x3e

#define MT6323_PWRC_BASE	0x8000
#define MT6323_PWRC_SIZE	0x40

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

static const struct resource mt6323_pwrc_resources[] = {
	DEFINE_RES_MEM(MT6323_PWRC_BASE, MT6323_PWRC_SIZE),
};

/* 1. Оставляем ТОЛЬКО прерывания оверклокинга/перегрузки регуляторов питания MT6357 */
static const struct resource mt6357_regulators_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VPROC_OC, "VPROC"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCORE_OC, "VCORE"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VMODEM_OC, "VMODEM"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VS1_OC, "VS1"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VPA_OC, "VPA"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCORE_PREOC, "VCORE_PR"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VFE28_OC, "VFE28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VXO22_OC, "VXO22"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VRF18_OC, "VRF18"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VRF12_OC, "VRF12"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VEFUSE_OC, "VEFUSE"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCN33_OC, "VCN33"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCN28_OC, "VCN28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCN18_OC, "VCN18"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCAMA_OC, "VCAMA"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCAMD_OC, "VCAMD"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCAMIO_OC, "VCAMIO"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VLDO28_OC, "VLDO28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VUSB33_OC, "VUSB33"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VAUX18_OC, "VAUX18"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VAUD28_OC, "VAUD28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VIO28_OC, "VIO28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VIO18_OC, "VIO18"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VSRAM_PROC_OC, "VSRAM_PROC"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VSRAM_OTHERS_OC, "VSRAM_OTHERS"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VIBR_OC, "VIBR"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VDRAM_OC, "VDRAM"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VMC_OC, "VMC"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VMCH_OC, "VMCH"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VEMC_OC, "VEMC"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VSIM1_OC, "VSIM1"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VSIM2_OC, "VSIM2"),
};

/* 2. Из 20 девайсов оставляем ОДИН-ЕДИНСТВЕННЫЙ, который нам реально нужен */
static const struct mfd_cell mt6357_devs[] = {
    {
        .name = "mt6357-regulator",
        .of_compatible = "mediatek,mt6357-regulator",
        .num_resources = ARRAY_SIZE(mt6357_regulators_resources),
        .resources = mt6357_regulators_resources,
    },
};

static const struct mfd_cell mt6323_devs[] = {
	{
		.name = "mt6323-rtc",
		.num_resources = ARRAY_SIZE(mt6323_rtc_resources),
		.resources = mt6323_rtc_resources,
		.of_compatible = "mediatek,mt6323-rtc",
	}, {
		.name = "mt6323-regulator",
		.of_compatible = "mediatek,mt6323-regulator"
	}, {
		.name = "mt6323-led",
		.of_compatible = "mediatek,mt6323-led"
	}, {
		.name = "mtk-pmic-keys",
		.num_resources = ARRAY_SIZE(mt6323_keys_resources),
		.resources = mt6323_keys_resources,
		.of_compatible = "mediatek,mt6323-keys"
	}, {
		.name = "mt6323-pwrc",
		.num_resources = ARRAY_SIZE(mt6323_pwrc_resources),
		.resources = mt6323_pwrc_resources,
		.of_compatible = "mediatek,mt6323-pwrc"
	},
};

static const struct mfd_cell mt6358_devs[] = {
	{
		.name = "mt6358-regulator",
		.of_compatible = "mediatek,mt6358-regulator"
	}, {
		.name = "mt6358-rtc",
		.num_resources = ARRAY_SIZE(mt6358_rtc_resources),
		.resources = mt6358_rtc_resources,
		.of_compatible = "mediatek,mt6358-rtc",
	}, {
		.name = "mt6358-sound",
		.of_compatible = "mediatek,mt6358-sound"
	},
};

static const struct mfd_cell mt6397_devs[] = {
	{
		.name = "mt6397-rtc",
		.num_resources = ARRAY_SIZE(mt6397_rtc_resources),
		.resources = mt6397_rtc_resources,
		.of_compatible = "mediatek,mt6397-rtc",
	}, {
		.name = "mt6397-regulator",
		.of_compatible = "mediatek,mt6397-regulator",
	}, {
		.name = "mt6397-codec",
		.of_compatible = "mediatek,mt6397-codec",
	}, {
		.name = "mt6397-clk",
		.of_compatible = "mediatek,mt6397-clk",
	}, {
		.name = "mt6397-pinctrl",
		.of_compatible = "mediatek,mt6397-pinctrl",
	}, {
		.name = "mtk-pmic-keys",
		.num_resources = ARRAY_SIZE(mt6397_keys_resources),
		.resources = mt6397_keys_resources,
		.of_compatible = "mediatek,mt6397-keys"
	}
};

struct chip_data {
	u32 cid_addr;
	u32 cid_shift;
	const struct mfd_cell *cells;
	int cell_size;
	int (*irq_init)(struct mt6397_chip *chip);
};

static const struct chip_data mt6323_core = {
	.cid_addr = MT6323_CID,
	.cid_shift = 0,
	.cells = mt6323_devs,
	.cell_size = ARRAY_SIZE(mt6323_devs),
	.irq_init = mt6397_irq_init,
};

static const struct chip_data mt6358_core = {
	.cid_addr = MT6358_SWCID,
	.cid_shift = 8,
	.cells = mt6358_devs,
	.cell_size = ARRAY_SIZE(mt6358_devs),
	.irq_init = mt6358_irq_init,
};
static const struct chip_data mt6357_core = {
    .cid_addr = MT6357_SWCID,
    .cid_shift = 8,                /* Проверь по старому ядру, 8 или 0, но обычно 8 для сдвига ID */
    .cells = mt6357_devs,          /* Передаем наш урезанный массив с регулятором */
    .cell_size = ARRAY_SIZE(mt6357_devs),
    /* Строку .irq_init пока НЕ пишем, так как у нас нет функции mt6357_irq_init */
};

static const struct chip_data mt6397_core = {
	.cid_addr = MT6397_CID,
	.cid_shift = 0,
	.cells = mt6397_devs,
	.cell_size = ARRAY_SIZE(mt6397_devs),
	.irq_init = mt6397_irq_init,
};

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
     * mt6397 MFD is child device of soc pmic wrapper.
     * Regmap is set from its parent.
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
    if (pmic->irq <= 0)
        return pmic->irq;

    /* 1. Разбираемся с прерываниями, если функция указана */
    if (pmic_core->irq_init) {
        ret = pmic_core->irq_init(pmic);
        if (ret)
            return ret;
    } else {
        dev_warn(&pdev->dev, "No IRQ init function specified for this chip\n");
    }

    /* 2. Регистрируем наши регуляторы питания */
    ret = devm_mfd_add_devices(&pdev->dev, PLATFORM_DEVID_NONE,
                   pmic_core->cells, pmic_core->cell_size,
                   NULL, 0, pmic->irq_domain);
    
    /* 3. Проверяем, завелись ли суб-девайсы */
    if (ret) {
        if (pmic->irq_domain)
            irq_domain_remove(pmic->irq_domain);
        dev_err(&pdev->dev, "failed to add child devices: %d\n", ret);
    }

    return ret;
}

static const struct of_device_id mt6397_of_match[] = {
	{
		.compatible = "mediatek,mt6323",
		.data = &mt6323_core,
	}, {
		.compatible = "mediatek,mt6358",
		.data = &mt6358_core,
	}, {
		.compatible = "mediatek,mt6397",
		.data = &mt6397_core,
	}, {
		.compatible = "mediatek,mt6357", 
		.data = &mt6357_core,
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, mt6397_of_match);

static const struct platform_device_id mt6397_id[] = {
	{ "mt6397", 0 },
	{ },
};
MODULE_DEVICE_TABLE(platform, mt6397_id);

static struct platform_driver mt6397_driver = {
	.probe = mt6397_probe,
	.driver = {
		.name = "mt6397",
		.of_match_table = of_match_ptr(mt6397_of_match),
	},
	.id_table = mt6397_id,
};

module_platform_driver(mt6397_driver);

MODULE_AUTHOR("Flora Fu, MediaTek");
MODULE_DESCRIPTION("Driver for MediaTek MT6397 PMIC");
MODULE_LICENSE("GPL");
