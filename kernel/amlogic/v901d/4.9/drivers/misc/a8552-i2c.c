/*
 * A8852
 *
 * Copyright (C) 2020 Dennis Hsu, Jet-Opto Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/crc32.h>
#include <linux/a8552-i2c.h>

//Global Variable
#define LEN_FHD				3
#define LEN_APTIV			5

#ifdef CONFIG_IDME
extern const char *idme_get_model_name(void);
#endif

extern void set_backlight(char halo_value);

enum display_panel {
	FHD,
	APTIV,
};

u8 display_panel_type = FHD;

char current_time_mode = 1;
char current_step;
#define STEPWIDTH	17 /* brightness of 1 ~ 255 in 15 steps */
#define DAYTIME_MODE	1  /* day time 1, nighttime 0 */
#define MIN_STEP		1
#define MAX_STEP		15

uint daytime_settings[15][2] = {{3050, 70},
						{5012, 70},
						{7012, 70},
						{9012, 70},
						{11012, 70},
						{13012, 70},
						{14511, 70},
						{17012, 70},
						{19012, 70},
						{21012, 70},
						{23012, 70},
						{25012, 70},
						{27012, 70},
						{29012, 70},
						{30988, 70}
};

uint nighttime_settings[15][2] = {{328, 15},
						{885, 19},
						{1526, 23},
						{2167, 27},
						{2808, 31},
						{3449, 35},
						{4090, 39},
						{4713, 43},
						{5372, 47},
						{6013, 51},
						{6867, 55},
						{7722, 59},
						{8577, 63},
						{9432, 67},
						{10298, 70}
};


#if 1
static int __a8552_read_reg(struct i2c_client *client,
			      u8 reg, u16 len, void *val)
{
	struct i2c_msg xfer[2];
	//u8 buf[2];
	int ret;

	//buf[0] = reg & 0xff;
	//buf[1] = (reg >> 8) & 0xff;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 1;
	xfer[0].buf = &reg;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = val;

	ret = i2c_transfer(client->adapter, xfer, 2);
	dev_dbg(&client->dev, "%s: i2c transfer status (%d)\n",
			__func__, ret);
	if (ret == 2) {
		ret = 0;
	} else {
		if (ret >= 0)
			ret = -EIO;
		dev_err(&client->dev, "%s: i2c transfer failed (%d)\n",
			__func__, ret);
	}

	return ret;
}
#endif

static int __a8552_write_reg(struct i2c_client *client, u8 reg, u16 len,
			   const void *val)
{
	u8 *buf;
	size_t count;
	int ret;

	count = len + 1;
	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf[0] = reg & 0xff;
	//buf[1] = (reg >> 8) & 0xff;
	memcpy(&buf[1], val, len);
	ret = i2c_master_send(client, buf, count);
	if (ret == count) {
		ret = 0;
	} else {
		if (ret >= 0)
			ret = -EIO;
		dev_err(&client->dev, "%s: i2c send failed (%d)\n",
			__func__, ret);
	}

	kfree(buf);
	return ret;
}

static int a8552_parse_dt(struct device *dev, struct a8552 *data)
{
	int ret;
	const char *uname;

	ret = device_property_read_string(dev,
		 "test", &uname);
	if (ret < 0) {
		dev_err(dev, "invalid test\n");
		return -EINVAL;
	}

	return 0;
}

static int a8552_led_set_level(struct a8552 *data, u16 level)
{
	int error;
	int ret = 0;
	u8 value = 0; //LED_3, LED_6 always 0
	u8 value_M = 0;
	u8 value_L = 0;

	if (data == NULL) {
		pr_err("%s data is null\n", __func__);
		return -1;
	}

	dev_info(&data->client->dev, "level = %d\n", level);

	value_L = level & 0xff;
	value_M = (level >> 8) & 0xff;

	dev_info(&data->client->dev, "value_M = 0x%x\n", value_M);
	dev_info(&data->client->dev, "value_L = 0x%x\n", value_L);

	error = __a8552_write_reg(data->client,
						REG_LED1_TON_M,
						1,
						&value_M);
	if (error)
		ret = -EINVAL;

	error = __a8552_write_reg(data->client,
						REG_LED1_TON_L,
						1,
						&value_L);
	if (error)
		ret = -EINVAL;

	error = __a8552_write_reg(data->client,
						REG_LED2_TON_M,
						1,
						&value_M);
	if (error)
		ret = -EINVAL;

	error = __a8552_write_reg(data->client,
						REG_LED2_TON_L,
						1,
						&value_L);
	if (error)
		ret = -EINVAL;

	error = __a8552_write_reg(data->client,
						REG_LED3_TON_M,
						1,
						&value);//LED_3, LED_6 always 0
	if (error)
		ret = -EINVAL;

	error = __a8552_write_reg(data->client,
						REG_LED3_TON_L,
						1,
						&value);//LED_3, LED_6 always 0
	if (error)
		ret = -EINVAL;

	error = __a8552_write_reg(data->client,
						REG_LED4_TON_M,
						1,
						&value_M);
	if (error)
		ret = -EINVAL;

	error = __a8552_write_reg(data->client,
						REG_LED4_TON_L,
						1,
						&value_L);
	if (error)
		ret = -EINVAL;

	error = __a8552_write_reg(data->client,
						REG_LED5_TON_M,
						1,
						&value_M);
	if (error)
		ret = -EINVAL;

	error = __a8552_write_reg(data->client,
						REG_LED5_TON_L,
						1,
						&value_L);
	if (error)
		ret = -EINVAL;

	error = __a8552_write_reg(data->client,
						REG_LED6_TON_M,
						1,
						&value);//LED_3, LED_6 always 0
	if (error)
		ret = -EINVAL;

	error = __a8552_write_reg(data->client,
						REG_LED6_TON_L,
						1,
						&value);//LED_3, LED_6 always 0
	if (error)
		ret = -EINVAL;

	error = __a8552_write_reg(data->client,
						REG_LED7_TON_M,
						1,
						&value_M);
	if (error)
		ret = -EINVAL;

	error = __a8552_write_reg(data->client,
						REG_LED7_TON_L,
						1,
						&value_L);
	if (error)
		ret = -EINVAL;

	error = __a8552_write_reg(data->client,
						REG_LED8_TON_M,
						1,
						&value_M);
	if (error)
		ret = -EINVAL;

	error = __a8552_write_reg(data->client,
						REG_LED8_TON_L,
						1,
						&value_L);
	if (error)
		ret = -EINVAL;

	value = 0x01;
	error = __a8552_write_reg(data->client,
						REG_LOAD_PWM_TON_UPDATE,
						1,
						&value);
	if (error)
		ret = -EINVAL;

	return ret;
}

static int a8552_initialize(struct a8552 *data)
{
	int error;
	int ret = 0;
	u8 value = 0;

	if (data == NULL) {
		pr_err("%s data is null\n", __func__);
		return -1;
	}

	value = 0x0;
	error = __a8552_write_reg(data->client,
						REG_LED_ENABLE_M,
						1,
						&value);
	if (error)
		ret = -EINVAL;

	value = 0xDB;
	error = __a8552_write_reg(data->client,
						REG_LED_ENABLE_L,
						1,
						&value);
	if (error)
		ret = -EINVAL;
//OVP setting
	value = 0x18;
	error = __a8552_write_reg(data->client,
						REG_OVP_TH,
						1,
						&value);
	if (error)
		ret = -EINVAL;
//Boost Dithering and Thermal Derating
	value = 0x02;
	error = __a8552_write_reg(data->client,
						REG_BOOST_THERMAL_CFG,
						1,
						&value);
	if (error)
		ret = -EINVAL;
//Fault Mode
	value = 0x0A;
	error = __a8552_write_reg(data->client,
						REG_FAULT_MODE_M,
						1,
						&value);
	if (error)
		ret = -EINVAL;

	value = 0xBE;
	error = __a8552_write_reg(data->client,
						REG_FAULT_MODE_L,
						1,
						&value);
	if (error)
		ret = -EINVAL;


//0x10 00 = 4096 steps  0x6 DB = 1666 steps
	value = 0x10;
	error = __a8552_write_reg(data->client,
						REG_LED_PWM_PERIOD_M,
						1,
						&value);
	if (error)
		ret = -EINVAL;

	value = 0x00;
	error = __a8552_write_reg(data->client,
						REG_LED_PWM_PERIOD_L,
						1,
						&value);
	if (error)
		ret = -EINVAL;

	//led set level
	error = a8552_led_set_level(data, 0x7fff);
	if (error)
		ret = -EINVAL;

	//Read Fault mode L value
	error = __a8552_read_reg(data->client,
							REG_FAULT_STAT_L,
							1,
							&value);
	if (error)
		ret = -EINVAL;
	dev_info(&data->client->dev, "FAULT_STATUS_L (0x30) = 0x%x\n", value);

	//Read Fault mode M value
	error = __a8552_read_reg(data->client,
							REG_FAULT_STAT_M,
							1,
							&value);
	if (error)
		ret = -EINVAL;

	//Write FAULTHST A to 0x04
	value = 0x04;
	error = __a8552_write_reg(data->client,
						REG_FAULTHST_A,
						1,
						&value);
	if (error)
		ret = -EINVAL;

	//Write FAULTHST B to 0x00
	value = 0x00;
	error = __a8552_write_reg(data->client,
						REG_FAULTHST_B,
						1,
						&value);
	if (error)
		ret = -EINVAL;

	dev_info(&data->client->dev, "FAULT_STATUS_M (0x31) = 0x%x\n", value);
	//Read FAULTHST A
	error = __a8552_read_reg(data->client,
							REG_FAULTHST_A,
							1,
							&value);
	if (error)
		ret = -EINVAL;
	dev_info(&data->client->dev, "REG_FAULTHST_A (0x38) = 0x%x\n", value);

	//Read FAULTHST B
	error = __a8552_read_reg(data->client,
							REG_FAULTHST_B,
							1,
							&value);
	if (error)
		ret = -EINVAL;
	dev_info(&data->client->dev, "REG_FAULTHST_B (0x39) = 0x%x\n", value);

	return ret;
}

#ifdef CONFIG_AMLOGIC_BL_HOTPLUG
int backlight_power_control(struct bl_hotplug_i2cdev *data, u8 on_off)
{
	int error = 0;
	int ret = 0;

	if (data == NULL) {
		pr_err("%s data is null\n", __func__);
		return -1;
	}

	dev_info(&data->client->dev, "on_off = %d\n", on_off);
	if (on_off == 1) {
		on_off = 0x0;
		error = __a8552_write_reg(data->client,
							REG_LED_ENABLE_M,
							1,
							&on_off);
		if (error)
			ret = -EINVAL;

		on_off = 0xDB;
		error = __a8552_write_reg(data->client,
							REG_LED_ENABLE_L,
							1,
							&on_off);
		if (error)
			ret = -EINVAL;
	} else {
		on_off = 0x0;
		error = __a8552_write_reg(data->client,
							REG_LED_ENABLE_M,
							1,
							&on_off);
		if (error)
			ret = -EINVAL;

		on_off = 0x0;
		error = __a8552_write_reg(data->client,
							REG_LED_ENABLE_L,
							1,
							&on_off);
		if (error)
			ret = -EINVAL;
	}

	msleep(20);
	return ret;
}

int bl_hotplug_set_level(struct bl_hotplug_i2cdev *data, u16 level)
{
	int error;
	int ret = 0;
	int step;
	int halo_level;

	if (data == NULL) {
		pr_err("%s data is null\n", __func__);
		return -1;
	}

#if 0
	data->brightness = level;//8bit level 0-255
	level = level << 8; //16bit  0000 ~ ff00 the same 256 level
#else
	step = level/STEPWIDTH;

	/* make sure the step is in the right range 1 ~ 15*/
	if (step < MIN_STEP)
		step = MIN_STEP;

	if (step > MAX_STEP)
		step = MAX_STEP;

	current_step = step;

	if (current_time_mode == DAYTIME_MODE) {
		level = daytime_settings[step-1][0];
		halo_level = daytime_settings[step-1][1];
	} else {
		level = nighttime_settings[step-1][0];
		halo_level = nighttime_settings[step-1][1];
	}
	data->brightness = level;

#endif

	error = a8552_led_set_level((struct a8552 *)data, level);
	if (error)
		ret = -EINVAL;

	msleep(20);

	set_backlight(halo_level);

	return ret;
}

int bl_hotplug_power_on(struct bl_hotplug_i2cdev *data)
{
	int ret = 0;
	u8 on_off = 1;

	if (data == NULL) {
		pr_err("%s data is null\n", __func__);
		return -1;
	}

	ret = backlight_power_control(data, on_off);
	/* restore bl level */
	bl_hotplug_set_level(data, data->brightness);
	return ret;
}

int bl_hotplug_power_off(struct bl_hotplug_i2cdev *data)
{
	int ret = 0;
	u8 on_off = 0;

	if (data == NULL) {
		pr_err("%s data is null\n", __func__);
		return -1;
	}

	ret = backlight_power_control(data, on_off);
	return ret;
}
#endif

static void check_panel_model_name(void)
{
#ifdef CONFIG_IDME
	const char *model_name;

	model_name = idme_get_model_name();

	if (!strncmp(model_name, "FHD", LEN_FHD))
		display_panel_type = FHD;

	if (!strncmp(model_name, "APTIV", LEN_APTIV))
		display_panel_type = APTIV;

	printk(KERN_ERR "display_panel_name = %d\n", display_panel_type);
#endif
}

static ssize_t time_mode_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", current_time_mode);
}

static ssize_t time_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	ssize_t ret;
	struct a8552 *data = dev_get_drvdata(dev);
	u8 value = 0;
	int brightness;
	int halo_brightness;
	int error = 0;

	if (kstrtou8(buf, 10, &value))
		ret = -EINVAL;

	if (current_time_mode != value) {
		current_time_mode = value;
		if (current_time_mode == DAYTIME_MODE) {
			brightness = daytime_settings[current_step-1][0];
			halo_brightness = daytime_settings[current_step-1][1];
		} else {
			brightness = nighttime_settings[current_step-1][0];
			halo_brightness = nighttime_settings[current_step-1][1];
		}

		error = a8552_led_set_level((struct a8552 *)data, brightness);
		if (error)
			ret = -EINVAL;

		/* Put handle Halo LED brightness here */
		msleep(20);

	} else {
		current_time_mode = value;
	}

	dev_err(dev, "current_time_mode = %d value= %d\n",
		current_time_mode,
		value);

	ret = count;

	return ret;
}

static DEVICE_ATTR(time_mode, 0644, time_mode_show, time_mode_store);

static struct attribute *a8852_attrs[] = {
	&dev_attr_time_mode.attr,
	NULL
};

static const struct attribute_group a8852_attr_group = {
	.attrs = a8852_attrs,
};

static int a8852_sysfs_init(struct a8552 *data)
{
	struct i2c_client *client = data->client;
	int error = 0;

	error = sysfs_create_group(&client->dev.kobj, &a8852_attr_group);
	if (error) {
		dev_err(&client->dev, "Failure %d creating sysfs group\n",
			error);
		return error;
	}

	return error;
}

static int a8552_i2c_probe(struct i2c_client *client,
				      const struct i2c_device_id *id)
{
	struct a8552 *data;
	int error;

	check_panel_model_name();

	if (display_panel_type == FHD) {
		printk(KERN_ERR "display panel is FHD, force probe failed\n");
		return -ENOMEM;
	}

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C check functionality failed.\n");
		return -EIO;
	}

	data = devm_kzalloc(&client->dev, sizeof(struct a8552), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	a8552_parse_dt(&client->dev, data);

	snprintf(data->phys, sizeof(data->phys), "i2c-%u-%04x/a8552",
		 client->adapter->nr, client->addr);

	data->client = client;
	data->irq = client->irq;
	i2c_set_clientdata(client, data);

	error = a8552_initialize(data);
	if (error) {
		dev_info(&client->dev, "a8552_i2c_probe fail!!!\n");
		return error;
	}

	error = a8852_sysfs_init(data);
	if (error) {
		dev_err(&client->dev, "a8852_sysfs_init failed!!\n");
		return error;
		}
#ifdef CONFIG_AMLOGIC_BL_HOTPLUG
	data->power_on = bl_hotplug_power_on;
	data->power_off = bl_hotplug_power_off;
	data->set_level = bl_hotplug_set_level;
	aml_bl_hotplug_set_a8552((struct bl_hotplug_i2cdev *)data);
#endif
	dev_info(&client->dev, "a8552_i2c_probe OK!!!\n");
	return 0;
}

static int a8552_i2c_remove(struct i2c_client *client)
{
	//struct a8552 *data = i2c_get_clientdata(client);
#ifdef CONFIG_AMLOGIC_BL_HOTPLUG
	aml_bl_hotplug_set_a8552(NULL);
#endif

	sysfs_remove_group(&client->dev.kobj, &a8852_attr_group);

	return 0;
}

static const struct i2c_device_id a8552_id[] = {
	{ "a8552", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, a8552_id);

#ifdef CONFIG_OF
static const struct of_device_id a8552_i2c_dt_ids[] = {
	{ .compatible = "allegro,a8552", },
	{ }
};
MODULE_DEVICE_TABLE(of, a8552_i2c_dt_ids);
#endif

static struct i2c_driver a8552_i2c_driver = {
	.driver = {
		.name	= "a8552",
		//.pm	= &a8552_pm_ops,
		.of_match_table = of_match_ptr(a8552_i2c_dt_ids),
	},
	.probe		= a8552_i2c_probe,
	.remove		= a8552_i2c_remove,
	.id_table	= a8552_id,
};

module_i2c_driver(a8552_i2c_driver);

MODULE_AUTHOR("Dennis Hsu <dennis.hsu@jet-opto.com.tw>");
MODULE_DESCRIPTION("A8552 driver");
MODULE_LICENSE("GPL v2");
