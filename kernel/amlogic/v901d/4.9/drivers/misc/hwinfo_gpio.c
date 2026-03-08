/*
 * GPIO as HWINFO
 *
 * Copyright (C) 2019 Dennis Hsu, Jet-Opto Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/err.h>
#include "hwinfo_gpio.h"

#include <linux/amlogic/iomap.h>

#include <linux/pwm.h>

#define DRIVE_NAME "hwinfo_gpio"
#define MAX_NAME_LEN 20
#define GPIO_OFFSET 407

/* pwm_vs reg: vcbus */
#define VPU_VPU_PWM_V0                  0x2730
#define VPU_VPU_PWM_V1                  0x2731
#define VPU_VPU_PWM_V2                  0x2732
#define VPU_VPU_PWM_V3                  0x2733
#define VPU_VPU_PWM_H0			0x2734

#define ENCL_VIDEO_EN                   0x1ca0
#define ENCL_VIDEO_MAX_LNCNT            0x1cbb

#ifdef CONFIG_IDME
#define LEN_NOPANEL		7
extern const char *idme_get_model_name(void);
#endif

int g_hwinfo_gpio_val = -1;
//extern int g_audio_clk_mode;

struct pin_desc {
	struct gpio_desc *desc;
	const char *high_val;
	const char *low_val;
	struct list_head list;
};

struct evolove_pwm {
	unsigned int freq; /* pwm_vs: 1~4(vfreq),*/
	unsigned int cnt; /* internal used for pwm control */
	unsigned int pol;
	unsigned int duty; /* internal used for pwm control */
};
struct evolove_pwm fcs_pwm;


struct hwinfo_gpio {
	struct pwm_device *pwm;  // PWM out config
	int percentage;	 	 // PWM out percentage num
	int period;		 // PWM out period num
	int count;
	char sys_class_name[MAX_NAME_LEN];
	struct mutex hwinfo_lock;
	struct class hwinfo_class;
	struct list_head hwinfo_head;
};

struct hwinfo_gpio *hwinfo_global;

static bool check_headless(void)
{
#ifdef CONFIG_IDME
	const char *model_name;

	model_name = idme_get_model_name();

	if (!strncmp(model_name, "NOPANEL", LEN_NOPANEL))
		return true;
	else
		return false;
#endif
}

static void hwinfo_list_free(struct hwinfo_gpio *hwinfo)
{
	struct pin_desc *gpio;
	struct pin_desc *gpio_tmp;

	mutex_lock(&hwinfo->hwinfo_lock);
	list_for_each_entry_safe(gpio, gpio_tmp, &hwinfo->hwinfo_head, list) {
		list_del(&gpio->list);
		kfree(gpio);
	}
	mutex_unlock(&hwinfo->hwinfo_lock);
}

static ssize_t table_show(struct class *cls, struct class_attribute *attr,
			char *buf)
{
	struct hwinfo_gpio *hwinfo = container_of(cls,
					struct hwinfo_gpio, hwinfo_class);
	struct pin_desc *gpio;
	unsigned char num = 1;
	int len = 0;

	mutex_lock(&hwinfo->hwinfo_lock);
	list_for_each_entry(gpio, &hwinfo->hwinfo_head, list) {
		len += sprintf(buf+len,
			"[%d]: gpio=%-5d high_val=%-10s low_val=%-10s\n",
			num,
			(desc_to_gpio(gpio->desc) - GPIO_OFFSET),
			gpio->high_val,
			gpio->low_val);
		num++;
	}
	mutex_unlock(&hwinfo->hwinfo_lock);

	return len;
}

static ssize_t name_show(struct class *cls, struct class_attribute *attr,
			char *buf)
{
	struct hwinfo_gpio *hwinfo = container_of(cls,
					struct hwinfo_gpio, hwinfo_class);
	struct pin_desc *gpio;
	int gpio_value = 0;
	int len = 0;

	mutex_lock(&hwinfo->hwinfo_lock);
	list_for_each_entry(gpio, &hwinfo->hwinfo_head, list) {
		gpio_value = gpiod_get_value(gpio->desc);
		if (gpio_value == 1)
			len += sprintf(buf+len, "%s", gpio->high_val);
		else
			len += sprintf(buf+len, "%s", gpio->low_val);
	}
	mutex_unlock(&hwinfo->hwinfo_lock);

	return len;
}

static ssize_t sii5293_backlight_store(struct class *cls,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	struct hwinfo_gpio *hwinfo = container_of(cls,
					struct hwinfo_gpio, hwinfo_class);
	u32 value = 0;
	int ret = 0;
	int duty_cycle = 0;

	if (kstrtou32(buf, 10, &value))	//str to int num
		ret = -EINVAL;

//	pr_info("PWM value = %d\n", value);
	if (value >= 99)	//Limit value
		value = 99;

	if (value >= 0 && value <= 99) {	//range num 0 ~ 99 %
		duty_cycle = (value * hwinfo->period) / 100;
//		pr_info("duty_cycle = %d\n", duty_cycle);
		pwm_config(hwinfo->pwm, duty_cycle, hwinfo->period);
		pwm_enable(hwinfo->pwm);
		hwinfo->percentage = value;
	}

	return count;
}

void set_backlight(char halo_value)
{
	int duty_cycle = 0;

	pr_info("PWM value = %d\n", halo_value);
	if (halo_value >= 100)	//Limit value
		halo_value = 99;
	else if (halo_value <= 0)	//Limit value
		halo_value = 0;

	if (halo_value >= 0 && halo_value <= 99) {	//range num 0 ~ 99 %
		duty_cycle = (halo_value * hwinfo_global->period) / 100;
		pr_info("duty_cycle = %d\n", duty_cycle);
		pwm_config(hwinfo_global->pwm, duty_cycle, hwinfo_global->period);
		pwm_enable(hwinfo_global->pwm);
		hwinfo_global->percentage = halo_value;
	}

	return;
}
EXPORT_SYMBOL(set_backlight);


static ssize_t sii5293_backlight_show(struct class *cls,
	struct class_attribute *attr, char *buf)
{
	struct hwinfo_gpio *hwinfo = container_of(cls,
					struct hwinfo_gpio, hwinfo_class);
	int len = 0;

	pr_info("percentage = %d\n", hwinfo->percentage);
	len = sprintf(buf, "%d\n", hwinfo->percentage);
	return len;
}

static ssize_t master_show(struct class *class,	struct class_attribute *attr,
	char *buf)
{
	if (g_hwinfo_gpio_val == 1)
		return sprintf(buf, "%s\n", "cpu1 master");
	else
		return sprintf(buf, "%d\n", g_audio_clk_mode);
}

static ssize_t master_store(struct class *class, struct class_attribute *attr,
	const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int audio_change = 0;

	ret = kstrtouint(buf, 10, &audio_change);
	if (ret != 0) {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	if (g_hwinfo_gpio_val == 1)
		pr_info("%s master\n", __func__);
	else {
		if (audio_change != 0)
			g_audio_clk_mode = 1;
		else
			g_audio_clk_mode = 0;
		pr_info("%s salve\n", __func__);
	}

	return count;
}

static void evolve_pwm_vs(void)
{
	unsigned int pwm_hi, n, sw;
	unsigned int vs[8], ve[8];
	int i;
	const char *model_name;
#ifdef CONFIG_IDME
	model_name = idme_get_model_name();
	if (!strncmp(model_name, "1080P", 4)) {
		pwm_hi = fcs_pwm.duty;
		n = fcs_pwm.freq;
		sw = (fcs_pwm.cnt * 10 / n + 5) / 10;
		//pwm_hi = (fcs_pwm.duty * sw) / 100;
		pwm_hi = (pwm_hi * 10 / n + 5) / 10;
		pwm_hi = (pwm_hi > 1) ? pwm_hi : 1;

		pr_info("pwm_vs: n=%d, sw=%d, pwm_high=%d, duty=%d\n",
			n, sw, pwm_hi, fcs_pwm.duty);

		for (i = 0; i < n; i++) {
			vs[i] = 1 + (sw * i);
			ve[i] = vs[i] + pwm_hi - 1;
		}
		for (i = n; i < 8; i++) {
			vs[i] = 0x1fff;
			ve[i] = 0x1fff;
		}
		for (i = 0; i < 8; i++) {
			pr_info("pwm_vs: vs[%d]=%d, ve[%d]=%d\n",
				i, vs[i], i, ve[i]);
			}

		aml_write_vcbus(VPU_VPU_PWM_V0, (fcs_pwm.pol << 31) |
			(2 << 14) | /* vsync latch */
			(ve[0] << 16) | (vs[0]));
		aml_write_vcbus(VPU_VPU_PWM_V1, (ve[1] << 16) | (vs[1]));
		aml_write_vcbus(VPU_VPU_PWM_V2, (ve[2] << 16) | (vs[2]));
		aml_write_vcbus(VPU_VPU_PWM_V3, (ve[3] << 16) | (vs[3]));

		pr_info("PWM_V0=0x%08x\n", aml_read_vcbus(VPU_VPU_PWM_V0));
		pr_info("PWM_V1=0x%08x\n", aml_read_vcbus(VPU_VPU_PWM_V1));
		pr_info("PWM_V2=0x%08x\n", aml_read_vcbus(VPU_VPU_PWM_V2));
		pr_info("PWM_V3=0x%08x\n", aml_read_vcbus(VPU_VPU_PWM_V3));

		aml_vcbus_update_bits(VPU_VPU_PWM_H0, 0x80000000, 0x80000000);
		aml_vcbus_update_bits(VPU_VPU_PWM_V0, 0x1fff, vs[4]);
		aml_vcbus_update_bits(VPU_VPU_PWM_V0, 0x1fff0000,
			(ve[4] << 16));
		aml_write_vcbus(VPU_VPU_PWM_V1, (ve[5] << 16) | (vs[5]));
		aml_write_vcbus(VPU_VPU_PWM_V2, (ve[6] << 16) | (vs[6]));
		aml_write_vcbus(VPU_VPU_PWM_V3, (ve[7] << 16) | (vs[7]));
		pr_info("VPU_VPU_PWM_V4=0x%08x\n",
			aml_read_vcbus(VPU_VPU_PWM_V0));
		pr_info("VPU_VPU_PWM_V5=0x%08x\n",
			aml_read_vcbus(VPU_VPU_PWM_V1));
		pr_info("VPU_VPU_PWM_V6=0x%08x\n",
			aml_read_vcbus(VPU_VPU_PWM_V2));
		pr_info("VPU_VPU_PWM_V7=0x%08x\n",
			aml_read_vcbus(VPU_VPU_PWM_V3));
		aml_vcbus_update_bits(VPU_VPU_PWM_H0, 0x80000000, 0);
	}
#endif
}

static ssize_t fcs_pwm_freq_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
		return sprintf(buf, "%d\n", fcs_pwm.freq);
}

static ssize_t fcs_pwm_freq_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int temp = 0;

	ret = kstrtouint(buf, 10, &temp);

	if ((ret == 0) && (temp >= 1 && temp <= 4)) {
		fcs_pwm.freq = temp;
		evolve_pwm_vs();
		pr_info("fcs_pwm_freq = %d\n", fcs_pwm.freq);
	} else
		pr_info("%s: invalid args\n", __func__);

	return count;
}

static ssize_t fcs_pwm_pol_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
		return sprintf(buf, "%d\n", fcs_pwm.pol);
}

static ssize_t fcs_pwm_pol_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int temp = 0;

	ret = kstrtouint(buf, 10, &temp);

	if ((ret == 0) && (temp <= 1)) {
		fcs_pwm.pol = temp;
		evolve_pwm_vs();
		pr_info("fcs_pwm.pol = %d\n", fcs_pwm.pol);
	} else
		pr_info("%s: invalid args\n", __func__);

	return count;
}

static ssize_t fcs_pwm_duty_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
		return sprintf(buf, "%d\n", fcs_pwm.duty);
}

static ssize_t fcs_pwm_duty_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int temp = 0;

	ret = kstrtouint(buf, 10, &temp);

	if ((ret == 0) && (temp <= 100)) {
		fcs_pwm.duty = temp;
		evolve_pwm_vs();
		pr_info("fcs_pwm_duty = %d\n", fcs_pwm.duty);
	} else
		pr_info("%s: invalid args\n", __func__);

	return count;
}

struct class_attribute hwinfo_gpio_attrs[] = {
	__ATTR(change_master, 0644, master_show, master_store),
	__ATTR(fcs_pwm_freq, 0644, fcs_pwm_freq_show, fcs_pwm_freq_store),
	__ATTR(fcs_pwm_pol, 0644, fcs_pwm_pol_show, fcs_pwm_pol_store),
	__ATTR(fcs_pwm_duty, 0644, fcs_pwm_duty_show, fcs_pwm_duty_store),
	__ATTR_RO(table),
	__ATTR_RO(name),
	__ATTR_RW(sii5293_backlight),
	__ATTR_NULL
};

static int hwinfo_gpio_parse_dt(struct platform_device *pdev,
			struct hwinfo_gpio *hwinfo)
{
	int ret = 0, i;
	int state = 0;
	const char *uname;
	struct pin_desc *gpio_desc;

	pr_err("%s(#%d) hwinfo_gpio_parse_dt\n", __func__, __LINE__);
	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "failed to get device node\n");
		return -EINVAL;
	}

	ret = of_property_read_string(pdev->dev.of_node,
		 "hwinfo_sys_class_node", &uname);
	if (ret < 0) {
		dev_err(&pdev->dev, "invalid sys_class_name\n");
		return -EINVAL;
	}
	snprintf(hwinfo->sys_class_name, MAX_NAME_LEN, "%s", uname);

	ret = of_property_read_u32(pdev->dev.of_node,
			"hwinfo_num",
			&hwinfo->count);
	if (ret) {
		dev_err(&pdev->dev, "failed to get hwinfo_num!\n");
		return -EINVAL;
	}

	pr_err("%s(#%d) count = %d\n", __func__, __LINE__, hwinfo->count);

	for (i = 0; i < hwinfo->count; i++) {
		pr_err("%s(#%d) ========== %d =================\n",
				__func__,
				__LINE__,
				i);

		gpio_desc = kzalloc(sizeof(struct pin_desc), GFP_KERNEL);

		if (!gpio_desc) {
			dev_err(&pdev->dev, "alloc mem failed!\n");
			return -ENOMEM;
		}

		pr_err("%s(#%d)[%d] get all gpio desc\n",
				__func__,
				__LINE__,
				i);


		gpio_desc->desc = devm_gpiod_get_index(&pdev->dev,
			"IR_CHA", i, GPIOD_OUT_HIGH);

		if (!gpio_desc->desc) {
			pr_info("error IR_CHA pin\n");
		} else {
			gpiod_direction_output(gpio_desc->desc,
			GPIOF_OUT_INIT_HIGH);
		}

		gpio_desc->desc = devm_gpiod_get_index(&pdev->dev,
			"IRAUD_EN", i, GPIOD_OUT_HIGH);

		if (!gpio_desc->desc) {
			pr_info("error IRAUD_EN pin\n");
		} else {
			gpiod_direction_output(gpio_desc->desc,
			GPIOF_OUT_INIT_HIGH);
		}

		gpio_desc->desc = devm_gpiod_get_index(&pdev->dev,
			"PWR_AMP", i, GPIOD_OUT_HIGH);

		if (!gpio_desc->desc) {
			pr_info("error PWR_AMP pin\n");
		} else {
			gpiod_direction_output(gpio_desc->desc,
			GPIOF_OUT_INIT_HIGH);
		}

		//get all gpio desc.
		gpio_desc->desc = devm_gpiod_get_index(&pdev->dev,
							"hwinfo",
							i,
							GPIOD_IN);
		if (!gpio_desc->desc) {
			state = -EINVAL;
			goto err;
		}
		pr_err("%s(#%d) gpio[%d] = %d\n", __func__,
				__LINE__,
				i,
				desc_to_gpio(gpio_desc->desc));
		gpiod_direction_input(gpio_desc->desc);
		gpiod_set_pull(gpio_desc->desc, GPIOD_PULL_UP);
		g_hwinfo_gpio_val = gpiod_get_value(gpio_desc->desc);
		if (check_headless()) {
			g_hwinfo_gpio_val = 1;
			dev_err(&pdev->dev,
				"headless device, set the g_hwinfo_gpio_val to %d\n",
				g_hwinfo_gpio_val);
		}
		ret = of_property_read_string_index(pdev->dev.of_node,
			"hwinfo_high_val", i, &gpio_desc->high_val);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"find key_name=%d finished\n", i);
			state = -EINVAL;
			goto err;
		}
		pr_err("%s(#%d) high_val[%d] = %s\n", __func__,
				__LINE__,
				i,
				gpio_desc->high_val);

		ret = of_property_read_string_index(pdev->dev.of_node,
			"hwinfo_low_val", i, &gpio_desc->low_val);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid key value index[%d]\n",
				i);
			state = -EINVAL;
			goto err;
		}
		pr_err("%s(#%d) low_val[%d] = %s\n", __func__,
				__LINE__,
				i,
				gpio_desc->low_val);

		list_add_tail(&gpio_desc->list, &hwinfo->hwinfo_head);
	}

	return 0;
err:
	kfree(gpio_desc);
	return state;
}

static int hwinfo_gpio_probe(struct platform_device *pdev)
{
	struct hwinfo_gpio *hwinfo;
	int ret = 0;
	struct pwm_state pstate;	// add pwm func
	unsigned int duty_value;	// add pwm func

	pr_err("%s(#%d) hwinfo_gpio_probe\n", __func__, __LINE__);

	hwinfo = kzalloc(sizeof(struct hwinfo_gpio), GFP_KERNEL);
	if (!hwinfo)
		return -ENOMEM;

	platform_set_drvdata(pdev, hwinfo);

	mutex_init(&hwinfo->hwinfo_lock);
	INIT_LIST_HEAD(&hwinfo->hwinfo_head);

	pr_err("%s(#%d) pwm_out init\n", __func__, __LINE__);
	// PWM API: Get device data
	hwinfo->pwm = devm_pwm_get(&pdev->dev, NULL);
	if (IS_ERR(hwinfo->pwm)) {
		dev_err(&pdev->dev, "soc_state_pwm [%s] unable to request PWM\n",
			__func__);
		ret = PTR_ERR(hwinfo->pwm);
	} else {
		// get pwm duty_cycle property
		ret = of_property_read_u32(pdev->dev.of_node,
			 "duty_cycle", &duty_value);
		if (ret) {
			pr_err("not config pwm duty_cycle");
			goto err;
		}
		pwm_init_state(hwinfo->pwm, &pstate);
		pr_info("period= %d\n", pstate.period);
		hwinfo->period = pstate.period;
		pr_info("duty_cycle= %d\n", duty_value);
		hwinfo->percentage = ((duty_value * 100) / pstate.period);
		pr_info("percentage = %d\n", hwinfo->percentage);
		pwm_config(hwinfo->pwm, duty_value, pstate.period);
		pwm_enable(hwinfo->pwm);
	}
	pr_err("%s(#%d) pwm_out exit\n", __func__, __LINE__);

	ret = hwinfo_gpio_parse_dt(pdev, hwinfo);
	if (ret)
		goto err;

	pr_err("%s(#%d)init class\n", __func__, __LINE__);
	/*init class*/
	hwinfo->hwinfo_class.name = hwinfo->sys_class_name;
	hwinfo->hwinfo_class.owner = THIS_MODULE;
	hwinfo->hwinfo_class.class_attrs = hwinfo_gpio_attrs;
	ret = class_register(&hwinfo->hwinfo_class);
	if (ret) {
		dev_err(&pdev->dev, "fail to create gpio hwinfo class.\n");
		goto err;
	}

	pr_err("%s(#%d) hwinfo_gpio_probe OK!!!\n", __func__, __LINE__);

	if (g_hwinfo_gpio_val == 0)
		pr_info("cpu2 slave\n");
	else
		pr_info("cpu1 master\n");

	fcs_pwm.pol = 1;
	fcs_pwm.freq = 1;
	fcs_pwm.cnt = aml_read_vcbus(ENCL_VIDEO_MAX_LNCNT) + 1;
	fcs_pwm.duty = 50;
	evolve_pwm_vs();

	hwinfo_global = hwinfo;

	return ret;

err:
	hwinfo_list_free(hwinfo);
	kfree(hwinfo);
	return ret;
}

static int hwinfo_gpio_remove(struct platform_device *pdev)
{
	struct hwinfo_gpio *hwinfo;

	pr_err("%s(#%d) hwinfo_gpio_remove\n", __func__, __LINE__);
	hwinfo = platform_get_drvdata(pdev);
	class_unregister(&hwinfo->hwinfo_class);
	hwinfo_list_free(hwinfo);
	kfree(hwinfo);
	return 0;
}

static const struct of_device_id hwinfo_gpio_dt_ids[] = {
	{ .compatible = "jet-opto, hwinfo_gpio", },
	{ }
};
MODULE_DEVICE_TABLE(of, hwinfo_gpio_dt_ids);

static struct platform_driver hwinfo_gpio_driver = {
	.driver = {
		.name	= DRIVE_NAME,
		//.pm	= &hwinfo_gpio_ops,
		.of_match_table = of_match_ptr(hwinfo_gpio_dt_ids),
	},
	.probe		= hwinfo_gpio_probe,
	.remove		= hwinfo_gpio_remove,
};

module_platform_driver(hwinfo_gpio_driver);

MODULE_AUTHOR("Dennis Hsu <dennis.hsu@jet-opto.com.tw>");
MODULE_DESCRIPTION("GPIO as HWINFO");
MODULE_LICENSE("GPL");
