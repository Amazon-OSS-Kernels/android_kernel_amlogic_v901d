/*
 * drivers/amlogic/watchdog/meson_wdt.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/watchdog.h>
#include <linux/of.h>
#include <uapi/linux/reboot.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/amlogic/iomap.h>
#include <linux/ftrace.h>

#undef pr_fmt
#define pr_fmt(fmt)    "meson-watchdog: %s: " fmt, __func__

#define	CTRL	0x0
#define	CTRL1	0x4
#define	TCNT	0x8
#define	RESET	0xc

struct aml_wdt_dev {
	unsigned int min_timeout, max_timeout;
	unsigned int default_timeout, reset_watchdog_time, shutdown_timeout;
	unsigned int firmware_timeout, suspend_timeout, timeout;
	unsigned int one_second;
	struct device *dev;
	struct mutex lock;
	unsigned int reset_watchdog_method;
	struct delayed_work boot_queue;
	void __iomem *reg_base;
	struct notifier_block pm_notifier;
	struct notifier_block reboot_notifier;
	bool is_running; /* 1:enable 0:disable */
};

#define     AO_RTI_STICKY_REG3                                 (0xff800000 + (0x4f << 2))
/*set val to record suspend flow*/
#define BL30_SUSPEND_START				0x16
#define BL30_SUSPEND_ENTER_SUSPEND			0x1
#define BL30_SUSPEND_SWITCH_BL301_0			0x2
#define BL30_SUSPEND_SET_WAKEUP_SRC			0x3
#define BL30_SUSPEND_CLK81_TO_24M			0x4
#define BL30_SUSPEND_STORE_SYS_PLL			0x5
#define BL30_SUSPEND_CPU_CLK_TO_24M			0x6
#define BL30_SUSPEND_CPU_OFF				0x7
#define BL30_SUSPEND_DDR_SELFRESH			0x8
#define BL30_SUSPEND_STORE_PLL				0x9
#define BL30_SUSPEND_SWITCH_BL301_1			0xa
#define BL30_SUSPEND_CLR_WAKEUP_SRC			0xb
#define BL30_SUSPEND_DDR_RESUME				0xc
#define BL30_SUSPEND_RESTORE_PLL			0xd
#define BL30_SUSPEND_CLK81_RESTORE_CLK			0xe
#define BL30_SUSPEND_SET_WAKEUP_REASON			0xf
#define BL30_SUSPEND_RESTART_ARM			0x10
#define BL30_SUSPEND_AP_POWER_INIT			0x11
#define BL30_SUSPEND_WATCHDOG_SET			0x12
#define BL30_SUSPEND_RESTORE_SYS_PLL			0x13
#define BL30_SUSPEND_RETURN_BL31			0x14

#define BL30_SUSPEND_LINUX_NOIRQ			0x17
#define BL30_RESUME_LINUX_NOIRQ			0x0

int aml_reg_read_write(unsigned reg, unsigned writeval,
				 unsigned *readval)
{
	void __iomem *vaddr;
	reg = round_down(reg, 0x4);
	vaddr = ioremap(reg, 0x4);

	if (readval)
		*readval = readl(vaddr);
	else
		writel(writeval, vaddr);
	iounmap(vaddr);
	return 0;
}

unsigned int aml_get_STR_sticky_reg(void)
{
	unsigned int rtn;
	aml_reg_read_write(AO_RTI_STICKY_REG3, 0, &rtn);
	pr_info("%s:0x%x\n", __FUNCTION__, rtn);
	return rtn;
}

static void aml_update_bits(void __iomem  *reg, unsigned int mask,
							unsigned int val)
{
	unsigned int tmp, orig;

	orig = readl(reg);
	tmp = orig & ~mask;
	tmp |= val & mask;
	writel(tmp, reg);
}

static void enable_watchdog(struct aml_wdt_dev *awdtv)
{
	aml_update_bits(awdtv->reg_base + CTRL, 0x1<<18, 0x1<<18);
}

static void disable_watchdog(struct aml_wdt_dev *awdtv)
{
	aml_update_bits(awdtv->reg_base + CTRL, 0x1<<18, 0<<18);
}

static void reset_watchdog(struct aml_wdt_dev *awdtv)
{
	writel(0x0, awdtv->reg_base + RESET);
}

static void set_watchdog_cnt(struct aml_wdt_dev *awdtv, unsigned int cnt)
{
	writel((cnt & 0xffff), awdtv->reg_base + TCNT);
}

static unsigned int read_watchdog_tcnt(struct aml_wdt_dev *awdtv)
{
	return (readl(awdtv->reg_base + TCNT) & 0xffff0000) >> 16;
}

static int aml_wdt_start(struct watchdog_device *wdog)
{
	struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);

	mutex_lock(&wdev->lock);
	if (wdog->timeout == 0xffffffff)
		set_watchdog_cnt(wdev, wdev->default_timeout *
							wdev->one_second);
	else
		set_watchdog_cnt(wdev, wdog->timeout * wdev->one_second);
	enable_watchdog(wdev);
	mutex_unlock(&wdev->lock);
#if 0
	if (wdev->boot_queue)
		cancel_delayed_work(&wdev->boot_queue);
#endif
	wdev->is_running = true;
	dev_info(wdev->dev, "start watchdog\n");

	return 0;
}

static int aml_wdt_stop(struct watchdog_device *wdog)
{
	struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);

	mutex_lock(&wdev->lock);
	disable_watchdog(wdev);
	mutex_unlock(&wdev->lock);
	wdev->is_running = false;
	dev_info(wdev->dev, "stop watchdog\n");

	return 0;
}

static int aml_wdt_ping(struct watchdog_device *wdog)
{
	struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);

	mutex_lock(&wdev->lock);
	reset_watchdog(wdev);
	mutex_unlock(&wdev->lock);

	return 0;
}

static int aml_wdt_set_timeout(struct watchdog_device *wdog,
				unsigned int timeout)
{
	struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);

	mutex_lock(&wdev->lock);
	set_watchdog_cnt(wdev, timeout * wdev->one_second);
	wdog->timeout = timeout;
	wdev->timeout = timeout;
	mutex_unlock(&wdev->lock);

	dev_info(wdev->dev, "set timeout\n");
	return 0;
}

unsigned int aml_wdt_get_timeleft(struct watchdog_device *wdog)
{
	struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);
	unsigned long timeleft, tick;

	mutex_lock(&wdev->lock);
	timeleft = read_watchdog_tcnt(wdev);
	tick = readl(wdev->reg_base + TCNT) & 0xffff;
	mutex_unlock(&wdev->lock);

	return (tick - timeleft)/wdev->one_second;
}

static void boot_moniter_work(struct work_struct *work)
{
	struct aml_wdt_dev *wdev = container_of(work, struct aml_wdt_dev,
							boot_queue.work);
	reset_watchdog(wdev);
	mod_delayed_work(system_freezable_wq, &wdev->boot_queue,
	round_jiffies(msecs_to_jiffies(wdev->reset_watchdog_time*1000)));
}

static const struct watchdog_info aml_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
	.identity = "aml Watchdog",
};

static const struct watchdog_ops aml_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= aml_wdt_start,
	.stop		= aml_wdt_stop,
	.ping		= aml_wdt_ping,
	.set_timeout	= aml_wdt_set_timeout,
	.get_timeleft   = aml_wdt_get_timeleft,
};

void aml_init_pdata(struct aml_wdt_dev *wdev)
{
	int ret;
	struct clk *clk;

	ret = of_property_read_u32(wdev->dev->of_node, "default_timeout",
						&wdev->default_timeout);
	if (ret) {
		dev_err(wdev->dev,
		"dt probe default_timeout failed: %d\n", ret);
		wdev->default_timeout = 5;
	}
	ret = of_property_read_u32(wdev->dev->of_node, "reset_watchdog_method",
					&wdev->reset_watchdog_method);
	if (ret) {
		dev_err(wdev->dev,
			"dt probe reset_watchdog_method failed: %d\n", ret);
		wdev->reset_watchdog_method = 1;
	}
	ret = of_property_read_u32(wdev->dev->of_node, "reset_watchdog_time",
						&wdev->reset_watchdog_time);
	if (ret) {
		dev_err(wdev->dev,
			"dt probe reset_watchdog_time failed: %d\n", ret);
		wdev->reset_watchdog_time = 2;
	}

	ret = of_property_read_u32(wdev->dev->of_node, "shutdown_timeout",
						&wdev->shutdown_timeout);
	if (ret) {
		dev_err(wdev->dev,
			"dt probe shutdown_timeout failed: %d\n", ret);
		wdev->shutdown_timeout = 10;
	}

	ret = of_property_read_u32(wdev->dev->of_node, "firmware_timeout",
						&wdev->firmware_timeout);
	if (ret) {
		dev_err(wdev->dev,
				"dt probe firmware_timeout failed: %d\n", ret);
		wdev->firmware_timeout = 6;
	}

	ret = of_property_read_u32(wdev->dev->of_node, "suspend_timeout",
						&wdev->suspend_timeout);
	if (ret) {
		dev_err(wdev->dev,
				"dt probe suspend_timeout failed: %d\n", ret);
		wdev->suspend_timeout = 6;
	}


	wdev->reg_base = of_iomap(wdev->dev->of_node, 0);
	clk = clk_get(wdev->dev, NULL);
	aml_update_bits(wdev->reg_base + CTRL, 0x3<<21, 0x3<<21);
	aml_update_bits(wdev->reg_base + CTRL, 0x3<<24, 0x3<<24);
	aml_update_bits(wdev->reg_base + CTRL, 0x3ffff, 24000);

	wdev->one_second = 1000;
	wdev->max_timeout = 0xffff/1000;
	wdev->min_timeout = 1;
}

static int aml_wtd_reboot_notify(struct notifier_block *nb,
				 unsigned long event,
				void *dummy)
{
	struct aml_wdt_dev *wdev;

	wdev = container_of(nb, struct aml_wdt_dev, reboot_notifier);
	if (event == SYS_DOWN || event == SYS_HALT) {
		disable_watchdog(wdev);
		dev_info(wdev->dev,
			"reboot_notify: disable watchdog (event = %lu)\n",
			event);
	}
	if (wdev->reset_watchdog_method == 1)
		cancel_delayed_work(&wdev->boot_queue);
	return NOTIFY_OK;
}

static struct notifier_block aml_wdt_reboot_notifier = {
	.notifier_call = aml_wtd_reboot_notify,
};

#ifdef CONFIG_AMLOGIC_DEBUG_LOCKUP
/* HARDLOCKUP safe window: watchdog_thresh * 2 * /5 *3 *2 = 24 second*/
#define HARDLOCKUP_WIN	 60
struct aml_wdt_dev *g_awdt;
void aml_wdt_disable_dbg(void)
{
	static int flg;
	int cnt;

	if (!g_awdt || flg)
		return;
	cnt = readl(g_awdt->reg_base + TCNT) & 0xffff;
	if (cnt < HARDLOCKUP_WIN * g_awdt->one_second)
		cnt = HARDLOCKUP_WIN * g_awdt->one_second;
	set_watchdog_cnt(g_awdt, cnt);
}
#endif

int wdt_debug;

void wdt_debug_hang_bus(void)
{
	static void __iomem *reg_base = NULL;

	reg_base = ioremap(0xff63c20c , 0x4);
	if (!reg_base) {
		printk("failed to ioremap register.\n");
		return;
	}
	*(volatile unsigned int*)reg_base = 0x1300000 ;

	printk("[%s:%d] make system hang:%p\n", __func__, wdt_debug, reg_base);

	pr_info("VPU[0x%04x]=0x%08x\n", 0x1700, aml_read_vcbus(0x1700));
}

static ssize_t wdt_debug_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	int len;

	len = sprintf(buf, "test case: %d\n", wdt_debug);

	return len;
}

static ssize_t wdt_debug_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	unsigned long tmp;
	int rc;

	rc = kstrtoul(buf, 0, &tmp);
	if (rc)
		return rc;

	wdt_debug = tmp;

	return count;
}

static DEVICE_ATTR_RW(wdt_debug);

void aml_wdt_stop_panic(void *data)
{
	struct aml_wdt_dev *wdt = data;

	if (!wdt)
		return;

	pr_info("aml_wdt_stop_panic()\n");
	disable_watchdog(wdt);
	wdt->is_running = false;
}

static int aml_wdt_probe(struct platform_device *pdev)
{
	struct watchdog_device *aml_wdt;
	struct aml_wdt_dev *wdev;
	int ret;

	aml_wdt = devm_kzalloc(&pdev->dev, sizeof(*aml_wdt), GFP_KERNEL);
	if (!aml_wdt)
		return -ENOMEM;

	wdev = devm_kzalloc(&pdev->dev, sizeof(*wdev), GFP_KERNEL);
	if (!wdev)
		return -ENOMEM;
	wdev->dev		= &pdev->dev;
	mutex_init(&wdev->lock);
	aml_init_pdata(wdev);

	aml_wdt->info	      = &aml_wdt_info;
	aml_wdt->ops	      = &aml_wdt_ops;
	aml_wdt->min_timeout = wdev->min_timeout;
	aml_wdt->max_timeout = wdev->max_timeout;
	aml_wdt->timeout = 0xffffffff;
	wdev->timeout = 0xffffffff;

	watchdog_set_drvdata(aml_wdt, wdev);
	platform_set_drvdata(pdev, aml_wdt);
	wdev->is_running = false;
	if (wdev->reset_watchdog_method == 1) {

		INIT_DELAYED_WORK(&wdev->boot_queue, boot_moniter_work);
		mod_delayed_work(system_freezable_wq, &wdev->boot_queue,
	 round_jiffies(msecs_to_jiffies(wdev->reset_watchdog_time*1000)));
		aml_wdt_start(aml_wdt);
		dev_info(wdev->dev, "creat work queue for watch dog\n");
	}

	if (sysfs_create_file(&pdev->dev.kobj, &dev_attr_wdt_debug.attr)) {
		dev_err(wdev->dev, "create sysfs file failed!\n");
		return -EINVAL;
	}

	ret = watchdog_register_device(aml_wdt);
	if (ret)
		return ret;

	wdev->reboot_notifier = aml_wdt_reboot_notifier;
	register_reboot_notifier(&wdev->reboot_notifier);
	dev_info(wdev->dev, "AML Watchdog Timer probed done\n");
#ifdef CONFIG_AMLOGIC_DEBUG_LOCKUP
	g_awdt = wdev;
#endif
	set_aml_wdt_stop_func(aml_wdt_stop_panic, wdev);
	return 0;
}

static void aml_wdt_shutdown(struct platform_device *pdev)
{
	struct watchdog_device *wdog = platform_get_drvdata(pdev);
	struct aml_wdt_dev *wdev = watchdog_get_drvdata(wdog);

	if (wdev->reset_watchdog_method == 1)
		cancel_delayed_work(&wdev->boot_queue);
	disable_watchdog(wdev);
}

static int aml_wdt_remove(struct platform_device *pdev)
{
	struct watchdog_device *wdog = platform_get_drvdata(pdev);

	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_wdt_debug.attr);
	aml_wdt_stop(wdog);
	return 0;
}

#ifdef	CONFIG_PM
static int aml_wdt_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int aml_wdt_resume(struct platform_device *pdev)
{
	return 0;
}

static unsigned int wdt_cnt;

static int aml_wdt_suspend_late(struct device *dev)
{
	struct watchdog_device *aml_wdt = dev_get_drvdata(dev);
	struct aml_wdt_dev *awdev = watchdog_get_drvdata(aml_wdt);

	unsigned int en = readl(awdev->reg_base + CTRL) & (1<<18);

	wdt_cnt = readl(awdev->reg_base + TCNT) & 0xffff;
	pr_info("%s:%d en:0x%x, wdt not disabled\n", __func__, wdt_cnt, en>>18);
	aml_reg_read_write(AO_RTI_STICKY_REG3, BL30_SUSPEND_LINUX_NOIRQ, NULL);
	/*disable_watchdog(awdev);*/

	return 0;
}

static int aml_wdt_resume_early(struct device *dev)
{
	struct watchdog_device *aml_wdt = dev_get_drvdata(dev);
	struct aml_wdt_dev *awdev = watchdog_get_drvdata(aml_wdt);

	unsigned int en =  readl(awdev->reg_base + CTRL) & (1 << 18);

	pr_info("%s:%d en:0x%x\n", __func__, wdt_cnt, en >> 18);
	set_watchdog_cnt(awdev, (unsigned int)wdt_cnt);
	enable_watchdog(awdev);
	pr_info("%s:%d en:0x%x\n", __func__, wdt_cnt, en >> 18);
	aml_reg_read_write(AO_RTI_STICKY_REG3, BL30_RESUME_LINUX_NOIRQ, NULL);

	return 0;
}
#else
#define	aml_wdt_suspend	NULL
#define	aml_wdt_resume		NULL
#endif

static const struct of_device_id aml_wdt_of_match[] = {
	{ .compatible = "amlogic, meson-wdt", },
	{},
};
MODULE_DEVICE_TABLE(of, aml_wdt_of_match);

#ifdef CONFIG_PM
static const struct dev_pm_ops aml_wdt_pm_ops = {
	.suspend_noirq = aml_wdt_suspend_late,
	.resume_noirq = aml_wdt_resume_early,
};
#endif
static struct platform_driver aml_wdt_driver = {
	.probe		= aml_wdt_probe,
	.remove		= aml_wdt_remove,
	.shutdown	= aml_wdt_shutdown,
	.suspend	= aml_wdt_suspend,
	.resume		= aml_wdt_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "meson_wdt",
		.of_match_table = aml_wdt_of_match,
#ifdef CONFIG_PM
		.pm = &aml_wdt_pm_ops,
#endif
	},
};

static int __init aml_wdt_driver_init(void)
{
	return platform_driver_register(&(aml_wdt_driver));
}

module_init(aml_wdt_driver_init);

static void __exit aml_wdt_driver_exit(void)
{
	platform_driver_unregister(&(aml_wdt_driver));
}

module_exit(aml_wdt_driver_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:aml_wdt");


