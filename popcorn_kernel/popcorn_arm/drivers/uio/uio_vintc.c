// SPDX-License-Identifier: GPL-2.0-only
/*
 * drivers/uio/uio_vintc.c
 *
 * Userspace I/O platform driver with generic IRQ handling code.
 *
 * Copyright (C) 2008 Magnus Damm
 *
 * Based on uio_pdrv.c by Uwe Kleine-Koenig,
 * Copyright (C) 2008 by Digi International Inc.
 * All rights reserved.
 */

#include <linux/platform_device.h>
#include <linux/uio_driver.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/stringify.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#define DRIVER_NAME "uio_vintc"
extern int (*popcorn_ipi_handler_ptr)(void);
struct uio_vintc_platdata {
	struct uio_info *uioinfo;
	spinlock_t lock;
	unsigned long flags;
	struct platform_device *pdev;
	void __iomem* reg_base;
};

/* Bits in uio_vintc_platdata.flags */
enum {
	UIO_IRQ_DISABLED = 0,
};

static int uio_vintc_open(struct uio_info *info, struct inode *inode)
{
	struct uio_vintc_platdata *priv = info->priv;

	/* Wait until the Runtime PM code has woken up the device */
	return 0;
}

static int uio_vintc_release(struct uio_info *info, struct inode *inode)
{
	struct uio_vintc_platdata *priv = info->priv;

	/* Tell the Runtime PM code that the device has become idle */
	return 0;
}

void interrupt_to_core(uint16_t coreid)
{
    void __iomem* target_addr = ioremap(0x0f10000c, 0x4);
    writel(coreid<<16, target_addr);
    iounmap(target_addr);
}
EXPORT_SYMBOL(interrupt_to_core);

static irqreturn_t uio_vintc_handler(int irq, struct uio_info *dev_info)
{
	if(popcorn_ipi_handler_ptr)	popcorn_ipi_handler_ptr();
	else {
		printk("error, popcorn_ipi_handler_ptr is null\n");
		return IRQ_NONE;
		}

	struct uio_vintc_platdata *priv = dev_info->priv;
	//printk("interrupt handled by uio driver\n");
	readl(priv->reg_base + 0x8);
	//printk("interrupt vm id is %#x\n", readl(priv->reg_base + 0x8));
	return IRQ_HANDLED;
}

static int uio_vintc_irqcontrol(struct uio_info *dev_info, s32 irq_on)
{
	struct uio_vintc_platdata *priv = dev_info->priv;
	unsigned long flags;

	/* Allow user space to enable and disable the interrupt
	 * in the interrupt controller, but keep track of the
	 * state to prevent per-irq depth damage.
	 *
	 * Serialize this operation to support multiple tasks and concurrency
	 * with irq handler on SMP systems.
	 */

	spin_lock_irqsave(&priv->lock, flags);
	if (irq_on) {
		if (__test_and_clear_bit(UIO_IRQ_DISABLED, &priv->flags))
			enable_irq(dev_info->irq);
	} else {
		if (!__test_and_set_bit(UIO_IRQ_DISABLED, &priv->flags))
			disable_irq_nosync(dev_info->irq);
	}
	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static int uio_vintc_probe(struct platform_device *pdev)
{
	printk("NOTICE: virtual interrupt controller registering\n");
	struct uio_info *uioinfo = dev_get_platdata(&pdev->dev);
	struct device_node *node = pdev->dev.of_node;
	struct uio_vintc_platdata *priv;
	struct uio_mem *uiomem;
	int ret = -EINVAL;
	int i;

	if (node) {
		const char *name;

		/* alloc uioinfo for one device */
		uioinfo = devm_kzalloc(&pdev->dev, sizeof(*uioinfo),
				       GFP_KERNEL);
		if (!uioinfo) {
			dev_err(&pdev->dev, "unable to kmalloc\n");
			return -ENOMEM;
		}

		if (!of_property_read_string(node, "linux,uio-name", &name))
			uioinfo->name = devm_kstrdup(&pdev->dev, name, GFP_KERNEL);
		else
			uioinfo->name = devm_kasprintf(&pdev->dev, GFP_KERNEL,
						       "%pOFn", node);

		uioinfo->version = "devicetree";
		/* Multiple IRQs are not supported */
	}

	if (!uioinfo || !uioinfo->name || !uioinfo->version) {
		dev_err(&pdev->dev, "missing platform_data\n");
		return ret;
	}

	if (uioinfo->handler || uioinfo->irqcontrol ||
	    uioinfo->irq_flags & IRQF_SHARED) {
		dev_err(&pdev->dev, "interrupt configuration error\n");
		return ret;
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "unable to kmalloc\n");
		return -ENOMEM;
	}

	priv->uioinfo = uioinfo;
	spin_lock_init(&priv->lock);
	priv->flags = 0; /* interrupt is enabled to begin with */
	priv->pdev = pdev;
	priv->reg_base = ioremap(0x0f100000, 0x100);

	if (!uioinfo->irq) {
		ret = platform_get_irq(pdev, 0);
		uioinfo->irq = ret;
		if (ret == -ENXIO)
			uioinfo->irq = UIO_IRQ_NONE;
		else if (ret < 0) {
			dev_err(&pdev->dev, "failed to get IRQ\n");
			return ret;
		}
	}

	uiomem = &uioinfo->mem[0];

	for (i = 0; i < pdev->num_resources; ++i) {
		struct resource *r = &pdev->resource[i];

		if (r->flags != IORESOURCE_MEM)
			continue;

		if (uiomem >= &uioinfo->mem[MAX_UIO_MAPS]) {
			dev_warn(&pdev->dev, "device has more than "
					__stringify(MAX_UIO_MAPS)
					" I/O memory resources.\n");
			break;
		}

		uiomem->memtype = UIO_MEM_PHYS;
		uiomem->addr = r->start;
		uiomem->size = resource_size(r);
		uiomem->name = r->name;
		++uiomem;
	}

	while (uiomem < &uioinfo->mem[MAX_UIO_MAPS]) {
		uiomem->size = 0;
		++uiomem;
	}


	uioinfo->handler = uio_vintc_handler;
	uioinfo->irqcontrol = uio_vintc_irqcontrol;
	uioinfo->open = uio_vintc_open;
	uioinfo->release = uio_vintc_release;
	uioinfo->priv = priv;

	ret = uio_register_device(&pdev->dev, priv->uioinfo);
	if (ret) {
		dev_err(&pdev->dev, "unable to register uio device\n");
		return ret;
	}

	platform_set_drvdata(pdev, priv);
	return 0;
}

static int uio_vintc_remove(struct platform_device *pdev)
{
	struct uio_vintc_platdata *priv = platform_get_drvdata(pdev);

	uio_unregister_device(priv->uioinfo);

	priv->uioinfo->handler = NULL;
	priv->uioinfo->irqcontrol = NULL;

	return 0;
}

static struct of_device_id uio_of_genirq_match[] = {
	{ .compatible = "arm,vintc", },
	{ /* Sentinel */ },
};
MODULE_DEVICE_TABLE(of, uio_of_genirq_match);

static struct platform_driver uio_vintc = {
	.probe = uio_vintc_probe,
	.remove = uio_vintc_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(uio_of_genirq_match),
	},
};

module_platform_driver(uio_vintc);

MODULE_AUTHOR("Magnus Damm");
MODULE_DESCRIPTION("Userspace I/O platform driver with generic IRQ handling");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRIVER_NAME);
