// SPDX-License-Identifier: GPL-2.0
/* Minimal stubs for MediaTek charger framework to satisfy linking
 * (only intended to allow building this tree for development/testing).
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/srcu.h>
#include "mtk_charger_intf.h"

struct charger_device *charger_device_register(
    const char *name,
    struct device *parent, void *devdata, const struct charger_ops *ops,
    const struct charger_properties *props)
{
    struct charger_device *chg;

    chg = kzalloc(sizeof(*chg), GFP_KERNEL);
    if (!chg)
        return ERR_PTR(-ENOMEM);

    if (props)
        chg->props = *props;
    chg->ops = ops;
    mutex_init(&chg->ops_lock);
    charger_dev_set_drvdata(chg, devdata);
    /* keep device struct initialized enough for drvdata usage */
    device_initialize(&chg->dev);
    chg->dev.parent = parent;

    return chg;
}

void charger_device_unregister(struct charger_device *charger_dev)
{
    if (!charger_dev)
        return;
    /* avoid touching device core too much in stub */
    kfree(charger_dev);
}

struct charger_device *get_charger_by_name(const char *name)
{
    return NULL;
}

int charger_dev_notify(struct charger_device *charger_dev, int event)
{
    return 0;
}

/* simple no-op detection helpers used in drivers */
void Charger_Detect_Init(void)
{
}

void Charger_Detect_Release(void)
{
}


MODULE_LICENSE("GPL");
