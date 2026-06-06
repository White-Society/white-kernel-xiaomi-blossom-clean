// SPDX-License-Identifier: GPL-2.0
/*
 * Charger type detection helpers for MediaTek platforms.
 *
 * Provides Charger_Detect_Init / Charger_Detect_Release used by
 * charger drivers (e.g. sgm41513) to manage BC1.2 detection flow.
 *
 * In the original 4.19 kernel, these were implemented in the USB PHY
 * driver (drivers/misc/mediatek/usb20/mt6765/usb20_phy.c) which is
 * not present in this 5.10 kernel tree.
 *
 * The SGM41513 IC performs its own BC1.2 DPDM detection independently
 * via I2C registers (REG07 FORCE_DPDM -> REG08 result), so the USB PHY
 * detection is complementary. These stubs allow the charger driver to
 * function correctly without the full USB20 subsystem.
 */

#include <linux/kernel.h>
#include <linux/module.h>

void Charger_Detect_Init(void)
{
    pr_debug("[charger_detect] Charger_Detect_Init\n");
}
EXPORT_SYMBOL(Charger_Detect_Init);

void Charger_Detect_Release(void)
{
    pr_debug("[charger_detect] Charger_Detect_Release\n");
}
EXPORT_SYMBOL(Charger_Detect_Release);

MODULE_LICENSE("GPL");