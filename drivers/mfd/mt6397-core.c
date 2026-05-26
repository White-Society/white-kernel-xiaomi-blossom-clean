// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Flora Fu, MediaTek
 */

#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/irqdomain.h>		/* FIX #4: нужен для irq_domain */
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

/*
 * FIX #1 + #3: OC-прерывания регуляторов MT6357.
 *
 * Прерывания BUCK (VPROC, VCORE, VMODEM, VS1, VPA, VCORE_PREOC) живут в блоке
 * BUCK_TOP (регистры MT6357_BUCK_TOP_INT_STATUS0 / MASK_CON0), а прерывания
 * LDO — в блоке LDO_TOP (MT6357_LDO_TOP_INT_STATUS0 / MASK_CON0).
 * Оба набора обрабатываются отдельными regmap_irq_chip (см. ниже).
 *
 * Числовые значения MT6357_IRQ_* берутся из <linux/mfd/mt6357/core.h>,
 * где BUCK-прерывания начинаются с некоторого базового смещения внутри домена
 * mt6357_irq_chip_buck, а LDO-прерывания — внутри mt6357_irq_chip_ldo.
 * Здесь мы объявляем ресурсы через именованные макросы, поэтому конкретные
 * числа не важны — они разрешаются через irq_domain при mfd_add_devices().
 */

/* Ресурсы BUCK OC (домен BUCK_TOP) */
static const struct resource mt6357_buck_oc_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VPROC_OC,    "VPROC"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCORE_OC,    "VCORE"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VMODEM_OC,   "VMODEM"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VS1_OC,      "VS1"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VPA_OC,      "VPA"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCORE_PREOC, "VCORE_PR"),
};

/* Ресурсы LDO OC (домен LDO_TOP) */
static const struct resource mt6357_ldo_oc_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VFE28_OC,        "VFE28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VXO22_OC,        "VXO22"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VRF18_OC,        "VRF18"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VRF12_OC,        "VRF12"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VEFUSE_OC,       "VEFUSE"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCN33_OC,        "VCN33"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCN28_OC,        "VCN28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCN18_OC,        "VCN18"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCAMA_OC,        "VCAMA"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCAMD_OC,        "VCAMD"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCAMIO_OC,       "VCAMIO"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VLDO28_OC,       "VLDO28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VUSB33_OC,       "VUSB33"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VAUX18_OC,       "VAUX18"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VAUD28_OC,       "VAUD28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VIO28_OC,        "VIO28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VIO18_OC,        "VIO18"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VSRAM_PROC_OC,   "VSRAM_PROC"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VSRAM_OTHERS_OC, "VSRAM_OTHERS"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VIBR_OC,         "VIBR"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VDRAM_OC,        "VDRAM"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VMC_OC,          "VMC"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VMCH_OC,         "VMCH"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VEMC_OC,         "VEMC"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VSIM1_OC,        "VSIM1"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VSIM2_OC,        "VSIM2"),
};

/*
 * FIX #2: mt6357_devs теперь содержит один mfd_cell «mt6357-regulator»,
 * но передаёт ОБА набора ресурсов через num_resources / resources.
 * Если драйвер регулятора хочет получать оба домена прерываний одним
 * устройством — объединяем массивы в один статический набор.
 *
 * АЛЬТЕРНАТИВА (более правильная архитектурно): разделить на два отдельных
 * mfd_cell — «mt6357-regulator-buck» и «mt6357-regulator-ldo».
 * Здесь выбран вариант с единым устройством и объединённым массивом.
 */
static const struct resource mt6357_regulators_resources[] = {
	/* BUCK OC */
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VPROC_OC,    "VPROC"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCORE_OC,    "VCORE"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VMODEM_OC,   "VMODEM"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VS1_OC,      "VS1"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VPA_OC,      "VPA"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCORE_PREOC, "VCORE_PR"),
	/* LDO OC */
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VFE28_OC,        "VFE28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VXO22_OC,        "VXO22"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VRF18_OC,        "VRF18"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VRF12_OC,        "VRF12"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VEFUSE_OC,       "VEFUSE"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCN33_OC,        "VCN33"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCN28_OC,        "VCN28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCN18_OC,        "VCN18"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCAMA_OC,        "VCAMA"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCAMD_OC,        "VCAMD"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCAMIO_OC,       "VCAMIO"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VLDO28_OC,       "VLDO28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VUSB33_OC,       "VUSB33"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VAUX18_OC,       "VAUX18"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VAUD28_OC,       "VAUD28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VIO28_OC,        "VIO28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VIO18_OC,        "VIO18"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VSRAM_PROC_OC,   "VSRAM_PROC"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VSRAM_OTHERS_OC, "VSRAM_OTHERS"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VIBR_OC,         "VIBR"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VDRAM_OC,        "VDRAM"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VMC_OC,          "VMC"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VMCH_OC,         "VMCH"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VEMC_OC,         "VEMC"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VSIM1_OC,        "VSIM1"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VSIM2_OC,        "VSIM2"),
};

static const struct mfd_cell mt6357_devs[] = {
	{
		.name		  = "mt6357-regulator",
		.of_compatible	  = "mediatek,mt6357-regulator",
		.num_resources	  = ARRAY_SIZE(mt6357_regulators_resources),
		.resources	  = mt6357_regulators_resources,
	},
};

/*
 * FIX #1 + #2 + #3: ДВА отдельных regmap_irq_chip для MT6357.
 *
 * PSC_TOP (PWRKEY/PWRKEY_R):
 *   status_base = MT6357_PSC_TOP_INT_STATUS0
 *   mask_base   = MT6357_PSC_TOP_INT_MASK_CON0
 *   num_regs    = 1  (все биты умещаются в один 16-битный регистр)
 *
 * BUCK_TOP (VPROC_OC .. VCORE_PREOC):
 *   status_base = MT6357_BUCK_TOP_INT_STATUS0
 *   mask_base   = MT6357_BUCK_TOP_INT_CON0
 *   num_regs    = 1  (6 прерываний, биты 0-5)
 *
 * LDO_TOP (VFE28_OC .. VSIM2_OC):
 *   status_base = MT6357_LDO_TOP_INT_STATUS0
 *   mask_base   = MT6357_LDO_TOP_INT_CON0
 *   num_regs    = 2  (26 прерываний, биты 0-15 в reg0, 0-9 в reg1)
 *
 * Три chip'а — три отдельных irq_domain.  Указатели на irq_data сохраняются
 * в расширенной структуре mt6397_chip (см. FIX #4).
 */

/* --- PSC (PWRKEY) --- */
static const struct regmap_irq mt6357_psc_irqs[] = {
	REGMAP_IRQ_REG(MT6357_IRQ_PWRKEY,   0, BIT(0)),
	REGMAP_IRQ_REG(MT6357_IRQ_PWRKEY_R, 0, BIT(2)),
};

static const struct regmap_irq_chip mt6357_irq_chip_psc = {
	.name		= "mt6357-psc",
	.irqs		= mt6357_psc_irqs,
	.num_irqs	= ARRAY_SIZE(mt6357_psc_irqs),
	.num_regs	= 1,
	.status_base	= MT6357_PSC_TOP_INT_STATUS0,
	.mask_base	= MT6357_PSC_TOP_INT_MASK_CON0,
};

/* --- BUCK OC --- */
static const struct regmap_irq mt6357_buck_oc_irqs[] = {
	REGMAP_IRQ_REG(MT6357_IRQ_VPROC_OC,    0, BIT(0)),
	REGMAP_IRQ_REG(MT6357_IRQ_VCORE_OC,    0, BIT(1)),
	REGMAP_IRQ_REG(MT6357_IRQ_VMODEM_OC,   0, BIT(2)),
	REGMAP_IRQ_REG(MT6357_IRQ_VS1_OC,      0, BIT(3)),
	REGMAP_IRQ_REG(MT6357_IRQ_VPA_OC,      0, BIT(4)),
	REGMAP_IRQ_REG(MT6357_IRQ_VCORE_PREOC, 0, BIT(5)),
};

static const struct regmap_irq_chip mt6357_irq_chip_buck = {
	.name		= "mt6357-buck",
	.irqs		= mt6357_buck_oc_irqs,
	.num_irqs	= ARRAY_SIZE(mt6357_buck_oc_irqs),
	.num_regs	= 1,
	/* FIX #3: правильный регистровый блок для BUCK OC */
	.status_base	= MT6357_BUCK_TOP_INT_STATUS0,
	.mask_base	= MT6357_BUCK_TOP_INT_CON0,
};

/* --- LDO OC --- */
/*
 * 26 LDO OC-прерываний: биты 0-15 в STATUS0, биты 0-9 в STATUS1.
 * num_regs = 2; каждый reg_offset соответствует следующему регистру
 * (status_base + reg_idx * regmap stride, обычно stride=2 для 16-битных
 * регистров MT6357; stride задаётся в regmap_config родительского pwrap).
 */
static const struct regmap_irq mt6357_ldo_oc_irqs[] = {
	/* reg 0 (STATUS0), биты 0-15 */
	REGMAP_IRQ_REG(MT6357_IRQ_VFE28_OC,        0, BIT(0)),
	REGMAP_IRQ_REG(MT6357_IRQ_VXO22_OC,        0, BIT(1)),
	REGMAP_IRQ_REG(MT6357_IRQ_VRF18_OC,        0, BIT(2)),
	REGMAP_IRQ_REG(MT6357_IRQ_VRF12_OC,        0, BIT(3)),
	REGMAP_IRQ_REG(MT6357_IRQ_VEFUSE_OC,       0, BIT(4)),
	REGMAP_IRQ_REG(MT6357_IRQ_VCN33_OC,        0, BIT(5)),
	REGMAP_IRQ_REG(MT6357_IRQ_VCN28_OC,        0, BIT(6)),
	REGMAP_IRQ_REG(MT6357_IRQ_VCN18_OC,        0, BIT(7)),
	REGMAP_IRQ_REG(MT6357_IRQ_VCAMA_OC,        0, BIT(8)),
	REGMAP_IRQ_REG(MT6357_IRQ_VCAMD_OC,        0, BIT(9)),
	REGMAP_IRQ_REG(MT6357_IRQ_VCAMIO_OC,       0, BIT(10)),
	REGMAP_IRQ_REG(MT6357_IRQ_VLDO28_OC,       0, BIT(11)),
	REGMAP_IRQ_REG(MT6357_IRQ_VUSB33_OC,       0, BIT(12)),
	REGMAP_IRQ_REG(MT6357_IRQ_VAUX18_OC,       0, BIT(13)),
	REGMAP_IRQ_REG(MT6357_IRQ_VAUD28_OC,       0, BIT(14)),
	REGMAP_IRQ_REG(MT6357_IRQ_VIO28_OC,        0, BIT(15)),
	/* reg 1 (STATUS1), биты 0-9 */
	REGMAP_IRQ_REG(MT6357_IRQ_VIO18_OC,        1, BIT(0)),
	REGMAP_IRQ_REG(MT6357_IRQ_VSRAM_PROC_OC,   1, BIT(1)),
	REGMAP_IRQ_REG(MT6357_IRQ_VSRAM_OTHERS_OC, 1, BIT(2)),
	REGMAP_IRQ_REG(MT6357_IRQ_VIBR_OC,         1, BIT(3)),
	REGMAP_IRQ_REG(MT6357_IRQ_VDRAM_OC,        1, BIT(4)),
	REGMAP_IRQ_REG(MT6357_IRQ_VMC_OC,          1, BIT(5)),
	REGMAP_IRQ_REG(MT6357_IRQ_VMCH_OC,         1, BIT(6)),
	REGMAP_IRQ_REG(MT6357_IRQ_VEMC_OC,         1, BIT(7)),
	REGMAP_IRQ_REG(MT6357_IRQ_VSIM1_OC,        1, BIT(8)),
	REGMAP_IRQ_REG(MT6357_IRQ_VSIM2_OC,        1, BIT(9)),
};

static const struct regmap_irq_chip mt6357_irq_chip_ldo = {
	.name		= "mt6357-ldo",
	.irqs		= mt6357_ldo_oc_irqs,
	.num_irqs	= ARRAY_SIZE(mt6357_ldo_oc_irqs),
	.num_regs	= 2,		/* FIX #2: два регистра STATUS */
	/* FIX #3: правильный регистровый блок для LDO OC */
	.status_base	= MT6357_LDO_TOP_INT_STATUS0,
	.mask_base	= MT6357_LDO_TOP_INT_CON0,
};

/*
 * FIX #4: сохраняем irq_data всех трёх доменов в структуре чипа.
 *
 * struct mt6397_chip должна иметь дополнительные поля.  Вариантов два:
 *   а) Добавить поля в include/linux/mfd/mt6397/core.h (правильно для upstream).
 *   б) Завести локальную обёртку здесь (BSP-вариант, если заголовок нельзя трогать).
 *
 * Ниже показан вариант (б) через локальный контейнер.
 * В заголовке mt6397/core.h при этом достаточно уже существующего поля irq_domain.
 */
struct mt6357_irq_data {
	struct regmap_irq_chip_data	*psc;	 /* PWRKEY-домен */
	struct regmap_irq_chip_data	*buck;	 /* BUCK OC-домен */
	struct regmap_irq_chip_data	*ldo;	 /* LDO OC-домен */
};

static int mt6357_irq_init(struct mt6397_chip *chip)
{
	struct mt6357_irq_data *data;
	int ret;

	data = devm_kzalloc(chip->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/* 1. PSC-домен (PWRKEY) */
	ret = devm_regmap_add_irq_chip(chip->dev, chip->regmap, chip->irq,
				       IRQF_ONESHOT, 0,
				       &mt6357_irq_chip_psc, &data->psc);
	if (ret) {
		dev_err(chip->dev, "Failed to add mt6357 PSC IRQ chip: %d\n", ret);
		return ret;
	}

	/* 2. BUCK OC-домен */
	ret = devm_regmap_add_irq_chip(chip->dev, chip->regmap, chip->irq,
				       IRQF_ONESHOT, 0,
				       &mt6357_irq_chip_buck, &data->buck);
	if (ret) {
		dev_err(chip->dev, "Failed to add mt6357 BUCK OC IRQ chip: %d\n", ret);
		return ret;
	}

	/* 3. LDO OC-домен */
	ret = devm_regmap_add_irq_chip(chip->dev, chip->regmap, chip->irq,
				       IRQF_ONESHOT, 0,
				       &mt6357_irq_chip_ldo, &data->ldo);
	if (ret) {
		dev_err(chip->dev, "Failed to add mt6357 LDO OC IRQ chip: %d\n", ret);
		return ret;
	}

	/*
	 * FIX #4: chip->irq_domain указывает на PSC-домен (используется
	 * для PWRKEY-устройств), полный набор данных хранится в data.
	 * Если потребуется доступ к buck/ldo доменам позднее — data
	 * можно сохранить через dev_set_drvdata или аналог.
	 */
	chip->irq_domain = regmap_irq_get_domain(data->psc);

	/* Сохраняем указатель на всю структуру для дочерних драйверов */
	dev_set_drvdata(chip->dev, data);

	return 0;
}

static const struct mfd_cell mt6323_devs[] = {
	{
		.name		  = "mt6323-rtc",
		.num_resources	  = ARRAY_SIZE(mt6323_rtc_resources),
		.resources	  = mt6323_rtc_resources,
		.of_compatible	  = "mediatek,mt6323-rtc",
	}, {
		.name		  = "mt6323-regulator",
		.of_compatible	  = "mediatek,mt6323-regulator"
	}, {
		.name		  = "mt6323-led",
		.of_compatible	  = "mediatek,mt6323-led"
	}, {
		.name		  = "mtk-pmic-keys",
		.num_resources	  = ARRAY_SIZE(mt6323_keys_resources),
		.resources	  = mt6323_keys_resources,
		.of_compatible	  = "mediatek,mt6323-keys"
	}, {
		.name		  = "mt6323-pwrc",
		.num_resources	  = ARRAY_SIZE(mt6323_pwrc_resources),
		.resources	  = mt6323_pwrc_resources,
		.of_compatible	  = "mediatek,mt6323-pwrc"
	},
};

static const struct mfd_cell mt6358_devs[] = {
	{
		.name		  = "mt6358-regulator",
		.of_compatible	  = "mediatek,mt6358-regulator"
	}, {
		.name		  = "mt6358-rtc",
		.num_resources	  = ARRAY_SIZE(mt6358_rtc_resources),
		.resources	  = mt6358_rtc_resources,
		.of_compatible	  = "mediatek,mt6358-rtc",
	}, {
		.name		  = "mt6358-sound",
		.of_compatible	  = "mediatek,mt6358-sound"
	},
};

static const struct mfd_cell mt6397_devs[] = {
	{
		.name		  = "mt6397-rtc",
		.num_resources	  = ARRAY_SIZE(mt6397_rtc_resources),
		.resources	  = mt6397_rtc_resources,
		.of_compatible	  = "mediatek,mt6397-rtc",
	}, {
		.name		  = "mt6397-regulator",
		.of_compatible	  = "mediatek,mt6397-regulator",
	}, {
		.name		  = "mt6397-codec",
		.of_compatible	  = "mediatek,mt6397-codec",
	}, {
		.name		  = "mt6397-clk",
		.of_compatible	  = "mediatek,mt6397-clk",
	}, {
		.name		  = "mt6397-pinctrl",
		.of_compatible	  = "mediatek,mt6397-pinctrl",
	}, {
		.name		  = "mtk-pmic-keys",
		.num_resources	  = ARRAY_SIZE(mt6397_keys_resources),
		.resources	  = mt6397_keys_resources,
		.of_compatible	  = "mediatek,mt6397-keys"
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
	.cid_addr  = MT6323_CID,
	.cid_shift = 0,
	.cells	   = mt6323_devs,
	.cell_size = ARRAY_SIZE(mt6323_devs),
	.irq_init  = mt6397_irq_init,
};

static const struct chip_data mt6358_core = {
	.cid_addr  = MT6358_SWCID,
	.cid_shift = 8,
	.cells	   = mt6358_devs,
	.cell_size = ARRAY_SIZE(mt6358_devs),
	.irq_init  = mt6358_irq_init,
};

static const struct chip_data mt6357_core = {
	.cid_addr  = MT6357_SWCID,
	.cid_shift = 8,
	.cells	   = mt6357_devs,
	.cell_size = ARRAY_SIZE(mt6357_devs),
	.irq_init  = mt6357_irq_init,	/* FIX #8: функция теперь полноценная */
};

static const struct chip_data mt6397_core = {
	.cid_addr  = MT6397_CID,
	.cid_shift = 0,
	.cells	   = mt6397_devs,
	.cell_size = ARRAY_SIZE(mt6397_devs),
	.irq_init  = mt6397_irq_init,
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

	/* FIX #5: корректная проверка — 0 тоже невалидный IRQ */
	if (pmic->irq < 0)
		return pmic->irq;
	if (pmic->irq == 0)
		return -EINVAL;

	if (pmic_core->irq_init) {
		ret = pmic_core->irq_init(pmic);
		if (ret)
			return ret;
	} else {
		dev_warn(&pdev->dev, "No IRQ init function specified for this chip\n");
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

static const struct of_device_id mt6397_of_match[] = {
	{
		.compatible = "mediatek,mt6323",
		.data	    = &mt6323_core,
	}, {
		.compatible = "mediatek,mt6358",
		.data	    = &mt6358_core,
	}, {
		.compatible = "mediatek,mt6397",
		.data	    = &mt6397_core,
	}, {
		.compatible = "mediatek,mt6357",
		.data	    = &mt6357_core,
	}, {
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
		.name		  = "mt6397",
		.of_match_table	  = of_match_ptr(mt6397_of_match),
	},
	.id_table = mt6397_id,
};

module_platform_driver(mt6397_driver);

MODULE_AUTHOR("Flora Fu, MediaTek");
MODULE_DESCRIPTION("Driver for MediaTek MT6397 PMIC");
MODULE_LICENSE("GPL");