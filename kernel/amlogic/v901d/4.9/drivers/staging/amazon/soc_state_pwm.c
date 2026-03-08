/*
 * soc_state_pwm.c
 *
 * Device SoC state and PWM information driver
 *
 * Copyright 2020 Amazon Technologies, Inc. All Rights Reserved.
 *
 * * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/sign_of_life.h>
#include "soc_state_pwm.h"

#define PWM_SCALE_FACTOR                    1e9
#define PWM_FREQ_MIN                        1
#define PWM_FREQ_MAX                        100

// Index from the lcr_data table in sign_of_life_platform.h
enum smode_index {
	INDEX_UNAVAILABLE = 0,
	INDEX_CB_POWER_KEY = 12,
	INDEX_CB_USB = 13,
	INDEX_CB_POWER_SUPPLY = 14,
	INDEX_WB_SW = 15,
	INDEX_WB_KERNEL_PANIC = 16,
	INDEX_WB_KERNEL_WD = 17,
	INDEX_WB_HW_WD = 18,
	INDEX_FACTORY_RESET = 20,
	INDEX_OTA = 21,
	INDEX_POSTRECOVERY = 22,
};

// Data structure for the platform data of "soc_state_pwm device"
struct soc_state_pwm_data {
	struct pwm_device *pwm_dev;
	unsigned int normal_boot_freq;
	unsigned int recovery_boot_freq;
	unsigned int postrecovery_boot_freq;
	bool enabled;
	enum boot_mode_type current_boot_mode;
};

static struct soc_state_pwm_data ssp_data;

static unsigned long convert_to_cycle(unsigned int frequency)
{
	unsigned long cycle;
	if (frequency == 0) {
		pr_err("soc_state_pwm [%s] zero frequency\n", __func__);
		return 0;
	}
	cycle = (1000000000 / frequency);
	return cycle;
}

/*
 * send_pwm
 * Description: this function will set the life cycle reason
 * and send the corresponding pwm frequency for the mode
 * @Return 0 on success, -1 on failure
 */
static int send_pwm(void)
{
	unsigned long cycle = 0;
	enum boot_mode_type mode = ssp_data.current_boot_mode;

	if (mode == MODE_NORMAL) {
		cycle = convert_to_cycle(ssp_data.normal_boot_freq);
		pwm_config(ssp_data.pwm_dev, (cycle / 2), cycle);
	} else if (mode == MODE_RECOVERY) {
		cycle = convert_to_cycle(ssp_data.recovery_boot_freq);
		pwm_config(ssp_data.pwm_dev, (cycle / 2), cycle);
	} else if (mode == MODE_POSTRECOVERY) {
		cycle = convert_to_cycle(ssp_data.postrecovery_boot_freq);
		pwm_config(ssp_data.pwm_dev, (cycle / 2), cycle);
	} else { //INDEX_UNAVAILABLE
		pr_info("soc_state_pwm [%s] mode unavailable, no config was set\n", __func__);
		return -1;
	}
	if (ssp_data.enabled) {
		pwm_enable(ssp_data.pwm_dev);
		pr_info("soc_state_pwm [%s] successfully enabled PWM\n", __func__);
	} else {
		pr_info("soc_state_pwm [%s] pwm is currently disabled, please enable\n", __func__);
	}
	return 0;
}

enum boot_mode_type get_boot_mode_type(void)
{
	return ssp_data.current_boot_mode;
}
EXPORT_SYMBOL(get_boot_mode_type);

int send_pwm_signal(unsigned int pwm_freq)
{
	int ret = -1;
	unsigned long cycle = convert_to_cycle(pwm_freq);
	pwm_config(ssp_data.pwm_dev, (cycle / 2), cycle);

	if (ssp_data.enabled) {
		pwm_enable(ssp_data.pwm_dev);
		pr_info("soc_state_pwm [%s] successfully sent pwm freq: %d\n", __func__, pwm_freq);
		ret = 0;
	} else {
		pr_info("soc_state_pwm [%s] pwm is currently disabled, please enable\n", __func__);
	}

	return ret;
}
EXPORT_SYMBOL(send_pwm_signal);

/*
 * read_lcr
 * Description: this function will set the life cycle reason
 * and send the corresponding pwm frequency for the mode
 * @Return 0 on success, -1 on failure
 */
static int read_lcr(void)
{
	int current_mode_index;
	int ret;
	current_mode_index = life_cycle_get_mode_index();

	if ((current_mode_index >= INDEX_CB_POWER_KEY) && (current_mode_index <= INDEX_WB_HW_WD)) {
		ssp_data.current_boot_mode = MODE_NORMAL;
		pr_info("soc_state_pwm [%s] set normal mode\n", __func__);
		ssp_data.enabled = true;
		ret = send_pwm();
		if (ret == -1) {
			pr_err("soc_state_pwm [%s] could not send normal pwm\n", __func__);
			return -1;
		}
	} else if (current_mode_index == INDEX_FACTORY_RESET ||
			   current_mode_index == INDEX_OTA) {
		ssp_data.current_boot_mode = MODE_RECOVERY;
		pr_info("soc_state_pwm [%s] set recovery mode\n", __func__);
		ssp_data.enabled = true;
		ret = send_pwm();
		if (ret == -1) {
			pr_err("soc_state_pwm [%s] could not send recovery pwm\n", __func__);
			return -1;
		}
	} else if (current_mode_index == INDEX_POSTRECOVERY) {
		ssp_data.current_boot_mode = MODE_POSTRECOVERY;
		pr_info("soc_state_pwm [%s] set postrecovery mode\n", __func__);
		ssp_data.enabled = true;
		ret = send_pwm();
		if (ret == -1) {
			pr_err("soc_state_pwm [%s] could not send postrecovery pwm\n", __func__);
			return -1;
		}
	} else { //MODE_UNAVAILABLE
		pr_info("soc_state_pwm [%s] set mode unavailable\n", __func__);
		return -1;
	}
	return 0;
}

static ssize_t pwm_freq_normal_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf,
							  size_t len)
{
	ssize_t ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (ret) {
		pr_err("soc_state_pwm [%s] PWM: Invalid input for pwm frequency\n", __func__);
		ret = -EINVAL;
		goto fail;
	}

	if (val > PWM_FREQ_MAX || val < PWM_FREQ_MIN) {
		pr_err("soc_state_pwm [%s] PWM: Frequency out of range\n", __func__);
		ret = -EINVAL;
		goto fail;
	}
	pr_info("soc_state_pwm [%s] storing val as %d\n", __func__, val);
	ssp_data.normal_boot_freq = val;
	ret = len;

fail:
	return ret;
}

static ssize_t pwm_freq_normal_show(struct device *dev,
							 struct device_attribute *attr,
							 char *buf)
{
	ssize_t ret;
	ret = sprintf(buf, "soc_state_pwm [%s] normal boot frequency %d\n", __func__, ssp_data.normal_boot_freq);
	return ret;
}
static DEVICE_ATTR_RW(pwm_freq_normal);

static ssize_t pwm_freq_recovery_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf,
							  size_t len)
{
	ssize_t ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (ret) {
		pr_err("soc_state_pwm [%s] PWM: Invalid input for pwm frequency\n", __func__);
		ret = -EINVAL;
		goto fail;
	}

	if (val > PWM_FREQ_MAX || val < PWM_FREQ_MIN) {
		pr_err("soc_state_pwm [%s] PWM: Frequency out of range\n", __func__);
		ret = -EINVAL;
		goto fail;
	}
	pr_info("soc_state_pwm [%s] storing val as %d\n", __func__, val);
	ssp_data.recovery_boot_freq = val;
	ret = len;

fail:
	return ret;
}

static ssize_t pwm_freq_recovery_show(struct device *dev,
							 struct device_attribute *attr,
							 char *buf)
{
	ssize_t ret;
	ret = sprintf(buf, "soc_state_pwm [%s] recovery boot frequency %d\n", __func__, ssp_data.recovery_boot_freq);
	return ret;
}
static DEVICE_ATTR_RW(pwm_freq_recovery);

static ssize_t pwm_freq_postrecovery_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf,
							  size_t len)
{
	ssize_t ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (ret) {
		pr_err("soc_state_pwm [%s] PWM: Invalid input for pwm frequency\n", __func__);
		ret = -EINVAL;
		goto fail;
	}

	if (val > PWM_FREQ_MAX || val < PWM_FREQ_MIN) {
		pr_err("soc_state_pwm [%s] PWM: Frequency out of range\n", __func__);
		ret = -EINVAL;
		goto fail;
	}
	pr_info("soc_state_pwm [%s] storing val as %d\n", __func__, val);
	ssp_data.postrecovery_boot_freq = val;
	ret = len;

fail:
	return ret;
}

static ssize_t pwm_freq_postrecovery_show(struct device *dev,
							 struct device_attribute *attr,
							 char *buf)
{
	ssize_t ret;
	ret = sprintf(buf, "soc_state_pwm [%s] postrecovery boot frequency %d\n", __func__, ssp_data.postrecovery_boot_freq);
	return ret;
}
static DEVICE_ATTR_RW(pwm_freq_postrecovery);

static ssize_t pwm_enable_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf,
							  size_t len)
{
	ssize_t ret;
	int err;
	u8 enable;

	ret = kstrtou8(buf, 10, &enable);
	if (ret) {
		pr_err("soc_state_pwm [%s] PWM: Invalid input for the enable attribute\n", __func__);
		ret = -EINVAL;
		goto fail;
	}

	if (enable > 1 || enable < 0) {
		pr_err("soc_state_pwm [%s] PWM: enable is either 0 or 1\n", __func__);
		ret = -EINVAL;
		goto fail;
	}
	pr_info("soc_state_pwm [%s] storing val as %d\n", __func__, enable);
	ssp_data.enabled = (bool)enable;
	err = send_pwm(); // final enabling using the pwm api to send out
	if (err == -1) {
		pr_err("soc_state_pwm [%s] could not send pwm through pwm_enable\n", __func__);
		ret = -EINVAL;
		goto fail;
	}
	ret = len;

fail:
	return ret;
}

static ssize_t pwm_enable_show(struct device *dev,
							 struct device_attribute *attr,
							 char *buf)
{
	ssize_t ret;
	ret = sprintf(buf, "soc_state_pwm [%s] enable %d\n", __func__, (int)ssp_data.enabled);
	return ret;
}
static DEVICE_ATTR_RW(pwm_enable);

static int soc_pwm_probe(struct platform_device *pdev)
{
	int ret;
	unsigned long cycle;
	struct soc_state_pwm_data *pdata;

	// Create driver data
	pdata = devm_kzalloc(&pdev->dev, sizeof(struct soc_state_pwm_data),
						GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		pr_err("[%s] fail to allocate memory for soc pwm driver data\n", __func__);
		goto fail;
	}

	pdata->normal_boot_freq = NORMAL_BOOT_PWM_FREQ_DEFAULT;
	pdata->recovery_boot_freq = RECOVERY_BOOT_PWM_FREQ_DEFAULT;
	pdata->postrecovery_boot_freq = POSTRECOVERY_BOOT_PWM_FREQ_DEFAULT;
	pdata->enabled = false;
	pdata->current_boot_mode = MODE_UNAVAILABLE;

	// PWM API: Get device data
	pdata->pwm_dev = devm_pwm_get(&pdev->dev, NULL);
	if (IS_ERR(pdata->pwm_dev)) {
		dev_err(&pdev->dev, "soc_state_pwm [%s] unable to request PWM\n", __func__);
		ret = PTR_ERR(pdata->pwm_dev);
		goto fail;
	} else {
		cycle = convert_to_cycle(NORMAL_BOOT_PWM_FREQ_DEFAULT);
		pwm_config(pdata->pwm_dev, (cycle / 2), cycle);
	}

	// Create sysfs entry
	ret = device_create_file(&pdev->dev, &dev_attr_pwm_freq_normal);
	if (ret) {
		pr_err("soc_state_pwm [%s] could not create freq_nb sysfs entry\n", __func__);
		goto fail;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_pwm_freq_recovery);
	if (ret) {
		pr_err("soc_state_pwm [%s] could not create freq_recovery sysfs entry\n", __func__);
		goto err1;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_pwm_freq_postrecovery);
	if (ret) {
		pr_err("soc_state_pwm [%s] could not create freq_postrecovery sysfs entry\n", __func__);
		goto err2;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_pwm_enable);
	if (ret) {
		pr_err("soc_state_pwm [%s] could not create enable sysfs entry\n", __func__);
		goto err3;
	}

	ssp_data = *pdata;
	ret = read_lcr();
	if (ret == -1) {
		pr_err("soc_state_pwm [%s] could not read lcr\n", __func__);
		ret = 0; //Expected for lcr that are not related to soc_state_pwm
	}

	goto exit;

err3:
	device_remove_file(&pdev->dev, &dev_attr_pwm_freq_postrecovery);

err2:
	device_remove_file(&pdev->dev, &dev_attr_pwm_freq_recovery);

err1:
	device_remove_file(&pdev->dev, &dev_attr_pwm_freq_normal);

fail:
	kfree(pdata);

exit:
	return ret;
}

static int soc_pwm_remove(struct platform_device *pdev)
{
	pwm_disable(ssp_data.pwm_dev);
	return 0;
}

static const struct of_device_id soc_pwm_driver_of_match[] = {
	{
		.compatible	= "amazon,soc_state_pwm",
	},
	{},
};
MODULE_DEVICE_TABLE(of, soc_pwm_driver_of_match);

static struct platform_driver soc_pwm_driver = {
	.probe = soc_pwm_probe,
	.remove = soc_pwm_remove,
	.driver = {
		.name = "soc_state_pwm",
		.owner = THIS_MODULE,
		.of_match_table = soc_pwm_driver_of_match,
	},
};

static int __init soc_pwm_init(void)
{
	return platform_driver_register(&soc_pwm_driver);
}

static void __exit soc_pwm_exit(void)
{
	platform_driver_unregister(&soc_pwm_driver);
}

rootfs_initcall(soc_pwm_init);
module_exit(soc_pwm_exit);

MODULE_AUTHOR("Amazon.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PWM device driver for sending SoC state over GPIO");
