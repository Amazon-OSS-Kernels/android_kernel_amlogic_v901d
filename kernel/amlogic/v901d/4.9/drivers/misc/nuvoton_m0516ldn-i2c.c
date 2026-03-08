/*
 * Nuvoton M0516LDN MCU
 *
 * Copyright (C) 2019 Dennis Hsu, Jet-Opto Inc.
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
#include <linux/nuvoton_m0516ldn-i2c.h>
#include <linux/timer.h>
#include <../drivers/staging/amazon/soc_state_pwm.h>

//Global Variable
int g_version;
int g_rd_version;
int g_project_id;
int g_customer_id;
int g_ldrom_version;
int g_ldrom_rd_version;
int force_update_rom;
struct m0516ldn *g_m0516ldn = NULL;
extern void *m0516ldn_audio_set_mute_ptr;
char current_time_mode = 1;
char current_step;
struct timer_list update_timer;

#define STEPWIDTH	17 /* brightness of 1 ~ 255 in 15 steps */
#define DAYTIME_MODE	1  /* day time 1, nighttime 0 */
#define MIN_STEP		1
#define MAX_STEP		15
#define MCU_UPDATE_TIMEOUT_TIME 			240 // Set timeout to be 240 seconds or 4 minutes
#define MCU_DISPLAY_UPDATE_START_PWM_FREQ	140

uint daytime_settings[15][2] = {{3050, 54},
				{5012, 54},
				{7012, 54},
				{9012, 54},
				{11012, 54},
				{13012, 54},
				{14511, 54},
				{17012, 54},
				{19012, 54},
				{21012, 54},
				{23012, 54},
				{25012, 54},
				{27012, 54},
				{29012, 54},
				{30988, 54}
};

uint nighttime_settings[15][2] = {{328, 5},
				{885, 6},
				{1526, 7},
				{2167, 7},
				{2808, 8},
				{3449, 9},
				{4090, 9},
				{4731, 10},
				{5372, 11},
				{6013, 12},
				{6867, 12},
				{7722, 13},
				{8577, 14},
				{9432, 14},
				{10298, 15}
};

static int __m0516ldn_read_reg(struct i2c_client *client,
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

static int __m0516ldn_write_reg(struct i2c_client *client, u8 reg, u16 len,
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

static int m0516ldn_parse_dt(struct device *dev, struct m0516ldn *data)
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

static int m0516ldn_ir_led_control(struct m0516ldn *data, int value)
{
	int error = 0;
	int ret = 0;

	error = __m0516ldn_write_reg(data->client,
						REG_UT60_RESET,
						1,
						&value);
	if (error)
		ret = -EINVAL;

	return ret;
}

static int m0516ldn_initialize(struct m0516ldn *data)
{
	int error;
	u8 value = 0;
	u16 value16 = 0;
	u32 value32 = 0;
	u8 fail_sts = 0;
	u8 count = 0;

	/*
	 *Read APP ROM Version
	 */
	for (count = 0 ; count < 20 ; count++) {
		error = __m0516ldn_read_reg(data->client, REG_VERSION, 2, &value16);
		dev_info(&data->client->dev, "%s:read REG_VERSION = %04x err = %d\n",
			__func__, value16, error);
		if (error) {
			printk(KERN_ERR "m0516 get fail, count = %d\n", count);
			if (count >= 20)
				return error;
			msleep(20);
		} else {
			printk(KERN_ERR "m0516 get success, count = %d\n", count);
			msleep(100);
			break;
		}
	}

	g_version = value16;

	value16 = 0;
	error = __m0516ldn_read_reg(data->client, REG_RD_VERSION, 2, &value16);
	dev_info(&data->client->dev, "%s:read REG_RD_VERSION = %04x err = %d\n",
			__func__,
			value16,
			error);

	if (error)
		return error;
	g_rd_version = value16;

	dev_info(&data->client->dev, "g_version = %08x g_rd_version =%08x\n",
			g_version,
			g_rd_version);

	/*
	 *Read LDROM Version
	 */
	value16 = 0;
	error = __m0516ldn_read_reg(data->client,
							REG_LDROM_VERSION,
							2,
							&value16);
	dev_info(&data->client->dev, "%s:read REG_LDROM_VERSION = %04x err = %d\n",
			__func__,
			value16,
			error);

	if (error)
		return error;
	g_ldrom_version = value16;

	value16 = 0;
	error = __m0516ldn_read_reg(data->client,
							REG_LDROM_RD_VERSION,
							2,
							&value16);
	dev_info(&data->client->dev, "%s:read REG_LDROM_RD_VERSION = %04x err = %d\n",
			__func__,
			value16,
			error);

	if (error)
		return error;
	g_ldrom_rd_version = value16;

	dev_info(&data->client->dev, "g_ldrom_version = %08x g_ldrom_rd_version =%08x\n",
			g_ldrom_version,
			g_ldrom_rd_version);

	/*
	 *Read Project id
	 */
	value16 = 0;
	error = __m0516ldn_read_reg(data->client, REG_PROJECT_ID, 2, &value16);
	dev_info(&data->client->dev, "%s:read REG_PROJECT_ID = %04x err = %d\n",
			__func__,
			value16,
			error);

	if (error)
		return error;
	g_project_id = value16;

	/*
	 *Read Customer id
	 */
	value16 = 0;
	error = __m0516ldn_read_reg(data->client, REG_CUSTOMER_ID, 2, &value16);
	dev_info(&data->client->dev, "%s:read REG_CUSTOMER_ID = %04x err = %d\n",
			__func__,
			value16,
			error);

	if (error)
		return error;
	g_customer_id = value16;

	/*
	 *Read Current Boot Index
	 */
	value = 0;
	error = __m0516ldn_read_reg(data->client,
					REG_MULTIBOOT_INDEX,
					1,
					&value);
	dev_info(&data->client->dev, "%s:read REG_MULTIBOOT_INDEX = %02x err = %d\n",
			__func__,
			value,
			error);

	if (error)
		return error;

	if (value == MULTIBOOT_AP1ROM) {
		dev_err(&data->client->dev,
		"MCU Boot up fail in MULTIBOOT_AP1ROM, please download AP2ROM again!!!\n");
		/*
		 * Get Update Fail Status
		 */
		force_update_rom = 0;
		value32 = 0;
		error = __m0516ldn_read_reg(data->client,
						REG_UPDATE_FAIL_STATUS,
						4,
						&value32);
		if (error)
			return error;

		dev_info(&data->client->dev, "%s:[1]read REG_UPDATE_FAIL_STATUS = 0x%08x err = %d\n",
				__func__,
				value32,
				error);

		fail_sts = (value32 >> 8 & 0xff);//fail status
		if ((fail_sts == 1) || (fail_sts == 2) || (fail_sts == 3))
			force_update_rom = (value32 & 0xff);//fail target
		else
			force_update_rom = 0;

		dev_info(&data->client->dev, "%s:fail target = %02x fail status = %02x abort reason = %02x uprgrade retry count = %02x\n",
		__func__,
		force_update_rom,//upgrade fail target
		fail_sts,//uprgrade fail status
		(value32 >> 16 & 0xff),//uprgrade abort reason
		(value32 >> 24 & 0xff));//uprgrade retry count
	} else if (value == MULTIBOOT_LDROM)
		return -1;

	return 0;
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

	error = __m0516ldn_write_reg(data->client,
						REG_BACKLIGHT_POWER,
						1,
						&on_off);
	if (error)
		ret = -EINVAL;

	msleep(20);
	return ret;
}

int bl_hotplug_set_level(struct bl_hotplug_i2cdev *data, u16 level)
{
	int error;
	int ret = 0;
	char buff[2] = { 0, 0};
	int step;
	int halo_level;

	if (data == NULL) {
		pr_err("%s data is null\n", __func__);
		return -1;
	}
#if 0
	level = level >> 1;	// half the level

	if (level == 0)
		level = 1;	//keep the minimal brightness 1

	data->brightness = level;//8bit level 1-127

	level = level << 8; //16bit  0000 ~ 7f00 the same 127 level

#else
	data->brightness = level;

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

#endif
	buff[0] = level & 0xff;
	buff[1] = (level >> 8) & 0xff;

	dev_err(&data->client->dev, "backlight = %04x swap = %02x%02x\n",
			level,
			buff[0],
			buff[1]);

	error = __m0516ldn_write_reg(data->client, REG_BACKLIGHT, 2, &buff);
	if (error)
		ret = -EINVAL;

	dev_err(&data->client->dev, "Halo LED brightness = %d\n", halo_level);
	error = __m0516ldn_write_reg(data->client, REG_LED_PERCENTAGE, 2,
		&halo_level);
	if (error)
		ret = -EINVAL;

	msleep(20);
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

static ssize_t version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u16 value16 = 0;

	error = __m0516ldn_read_reg(data->client, REG_VERSION, 2, &value16);
	if (error)
		value16 = -1;

	return scnprintf(buf, PAGE_SIZE, "%04x\n", value16);
}

static ssize_t rd_version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u16 value16 = 0;

	error = __m0516ldn_read_reg(data->client, REG_RD_VERSION, 2, &value16);
	if (error)
		value16 = -1;

	return scnprintf(buf, PAGE_SIZE, "%04x\n", value16);
}

static ssize_t project_id_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u16 value16 = 0;

	error = __m0516ldn_read_reg(data->client, REG_PROJECT_ID, 2, &value16);
	if (error)
		value16 = -1;

	return scnprintf(buf, PAGE_SIZE, "%04x\n", value16);
}

static ssize_t customer_id_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u16 value16 = 0;

	error = __m0516ldn_read_reg(data->client, REG_CUSTOMER_ID, 2, &value16);
	if (error)
		value16 = -1;

	return scnprintf(buf, PAGE_SIZE, "%04x\n", value16);
}

static ssize_t multiboot_index_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u8 value = 0;

	error = __m0516ldn_read_reg(data->client,
						REG_MULTIBOOT_INDEX,
						1,
						&value);
	if (error)
		value = -1;

	return scnprintf(buf, PAGE_SIZE, "%02x\n", value);
}

static ssize_t ldrom_version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u16 value16 = 0;

	error = __m0516ldn_read_reg(data->client,
							REG_LDROM_VERSION,
							2,
							&value16);
	if (error)
		value16 = -1;

	return scnprintf(buf, PAGE_SIZE, "%04x\n", value16);
}

static ssize_t ldrom_rd_version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u16 value16 = 0;

	error = __m0516ldn_read_reg(data->client,
							REG_LDROM_RD_VERSION,
							2,
							&value16);
	if (error)
		value16 = -1;

	return scnprintf(buf, PAGE_SIZE, "%04x\n", value16);
}

static ssize_t earphone_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u8 value = 0;

	error = __m0516ldn_read_reg(data->client,
					REG_EAR_PHONE_EXIT,
					1,
					&value);
	if (error)
		value = -1;

	return scnprintf(buf, PAGE_SIZE, "%02x\n", value);
}

static ssize_t usbchg_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u8 value = 0;

	error = __m0516ldn_read_reg(data->client, REG_USB_CHARGE, 1, &value);
	if (error)
		value = -1;

	return scnprintf(buf, PAGE_SIZE, "%02x\n", value);
}

static ssize_t backlight_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u16 value16 = 0;

	error = __m0516ldn_read_reg(data->client, REG_BACKLIGHT, 2, &value16);
	if (error)
		value16 = -1;

	return scnprintf(buf, PAGE_SIZE, "%04x\n", value16);
}

static ssize_t backlight_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int error = 0;
	ssize_t ret;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u16 value16;
	u8 buff[2];

	if (kstrtou16(buf, 0x10, &value16))
		ret = -EINVAL;

	buff[0] = value16 & 0xff;
	buff[1] = (value16 >> 8) & 0xff;

	dev_err(dev, "backlight = %04x count= %d swap = %02x%02x\n",
			value16,
			count,
			buff[0],
			buff[1]);

	error = __m0516ldn_write_reg(data->client, REG_BACKLIGHT, 2, &buff);
	if (error)
		ret = -EINVAL;

	ret = count;

	return ret;
}

/**
 *	audio_get_mute
 *  0x00 : Audio Unmute.
 *  0x01 : Audio Mute.
 */
int m0516ldn_audio_get_mute(u8 *value)
{
	if  (g_m0516ldn != NULL)
		return  __m0516ldn_read_reg(g_m0516ldn->client, REG_AUDIO_MUTE, 1, value);
	else
		return -1;
}

/**
 *	audio_set_mute
 *  0x00 : Audio Unmute.
 *  0x01 : Audio Mute.
 */
int m0516ldn_audio_set_mute(u8 value)
{
	pr_info("m0516ldn_audio_set_mute value %x\n", value);
	if  (g_m0516ldn != NULL)
		return  __m0516ldn_write_reg(g_m0516ldn->client, REG_AUDIO_MUTE, 1, &value);
	else
		return -1;
}

static ssize_t audio_mute_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u8 value = 0;

	error = __m0516ldn_read_reg(data->client, REG_AUDIO_MUTE, 1, &value);
	if (error)
		value = -1;

	return scnprintf(buf, PAGE_SIZE, "%02x\n", value);
}

static ssize_t audio_mute_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int error = 0;
	ssize_t ret;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u8 value;

	if (kstrtou8(buf, 10, &value))
		ret = -EINVAL;

	dev_err(dev, "audio mute = %d count= %d\n", value, count);

	error = __m0516ldn_write_reg(data->client, REG_AUDIO_MUTE, 1, &value);
	if (error)
		ret = -EINVAL;

	ret = count;

	return ret;
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
	struct m0516ldn *data = dev_get_drvdata(dev);
	u8 value = 0;
	char buff[2] = { 0, 0};
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

		buff[0] = brightness & 0xff;
		buff[1] = (brightness >> 8) & 0xff;

		dev_err(&data->client->dev, "backlight = %04x swap = %02x%02x\n",
				brightness,
				buff[0],
				buff[1]);

		error = __m0516ldn_write_reg(data->client, REG_BACKLIGHT, 2, &buff);
		if (error)
			ret = -EINVAL;

		dev_err(&data->client->dev, "Halo LED brightness = %d\n", halo_brightness);
		error = __m0516ldn_write_reg(data->client, REG_LED_PERCENTAGE, 2,
			&halo_brightness);
		if (error)
			ret = -EINVAL;

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

static ssize_t led_percentage_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u16 value16 = 0;

	error = __m0516ldn_read_reg(data->client,
				REG_LED_PERCENTAGE,
				2, &value16);
	if (error)
		value16 = -1;

	return scnprintf(buf, PAGE_SIZE, "%04x\n", value16);
}

static ssize_t led_percentage_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int error = 0;
	ssize_t ret;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u16 value16;
	u8 buff[2];

	if (kstrtou16(buf, 0x10, &value16))
		ret = -EINVAL;

	buff[0] = value16 & 0xff;
	buff[1] = (value16 >> 8) & 0xff;

	dev_err(dev, "led_percentage = %04x count= %d swap = %02x%02x\n",
			value16,
			count,
			buff[0],
			buff[1]);

	error = __m0516ldn_write_reg(data->client,
				REG_LED_PERCENTAGE,
				2, &value16);
	if (error)
		ret = -EINVAL;

	ret = count;

	return ret;
}

static ssize_t pcb_temperature_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u16 value16 = 0;
	u8 temp[2];

	error = __m0516ldn_read_reg(data->client,
							REG_PCB_TEMPERATURE,
							2,
							&value16);
	if (error)
		value16 = -1;

	temp[0] = value16 & 0xff;
	temp[1] = (value16 >> 8) & 0xff;
	value16 = value16 - 0x1000;

	dev_err(dev, "panel temperature = %04d, temp[0] = %02x, temp[1] = %02x\n",
				value16, temp[0], temp[1]);

	return scnprintf(buf, PAGE_SIZE, "%04d\n", value16);
}

static ssize_t panel_temperature_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u16 value16 = 0;
	u8 temp[2];

	error = __m0516ldn_read_reg(data->client,
							REG_PANEL_TEMPERATURE,
							2,
							&value16);
	if (error)
		value16 = -1;

	temp[0] = value16 & 0xff;
	temp[1] = (value16 >> 8) & 0xff;
	value16 = value16 - 0x1000;

	dev_err(dev, "panel temperature = %04d, temp[0] = %02x, temp[1] = %02x\n",
				value16, temp[0], temp[1]);

	return scnprintf(buf, PAGE_SIZE, "%04d\n", value16);
}

static ssize_t ds90uh949_powerdown_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u8 value = 0;

	error = __m0516ldn_read_reg(data->client,
					REG_DS90UH949_POWER_DOWN,
					1,
					&value);
	if (error)
		value = -1;

	return scnprintf(buf, PAGE_SIZE, "%02x\n", value);
}

static ssize_t ds90uh949_powerdown_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int error = 0;
	ssize_t ret;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u8 value = 0;

	if (kstrtou8(buf, 10, &value))
		ret = -EINVAL;

	dev_err(dev, "949_powerdown = %d count= %d\n", value, count);

	error = __m0516ldn_write_reg(data->client,
						REG_DS90UH949_POWER_DOWN,
						1,
						&value);
	if (error)
		ret = -EINVAL;

	ret = count;

	return ret;
}

static ssize_t ds90uh948_powerdown_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u8 value = 0;

	error = __m0516ldn_read_reg(data->client,
					REG_DS90UH948_POWER_DOWN,
					1,
					&value);
	if (error)
		value = -1;

	return scnprintf(buf, PAGE_SIZE, "%02x\n", value);
}

static ssize_t ds90uh948_powerdown_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int error = 0;
	ssize_t ret;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u8 value = 0;

	if (kstrtou8(buf, 10, &value))
		ret = -EINVAL;

	dev_err(dev, "948_powerdown = %d count= %d\n", value, count);

	error = __m0516ldn_write_reg(data->client,
						REG_DS90UH948_POWER_DOWN,
						1,
						&value);
	if (error)
		ret = -EINVAL;

	ret = count;

	return ret;
}

static ssize_t ut60_reset_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u8 value = 0;

	error = __m0516ldn_read_reg(data->client,
					REG_UT60_RESET,
					1,
					&value);
	if (error)
		value = -1;

	return scnprintf(buf, PAGE_SIZE, "%x\n", value);
}

static ssize_t ut60_reset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int error = 0;
	ssize_t ret;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u8 value = 0;

	if (kstrtou8(buf, 10, &value))
		ret = -EINVAL;

	dev_err(dev, "ut60_reset = %d count= %d\n", value, count);

	error = __m0516ldn_write_reg(data->client,
						REG_UT60_RESET,
						1,
						&value);
	if (error)
		ret = -EINVAL;

	ret = count;

	return ret;
}

static ssize_t backlight_power_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u8 value = 0;

	error = __m0516ldn_read_reg(data->client,
					REG_BACKLIGHT_POWER,
					1,
					&value);
	if (error)
		value = -1;

	return scnprintf(buf, PAGE_SIZE, "%02x\n", value);
}

static ssize_t backlight_power_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int error = 0;
	ssize_t ret;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u8 value = 0;

	if (kstrtou8(buf, 10, &value))
		ret = -EINVAL;

	dev_err(dev, "backlight_power = %d count= %d\n", value, count);

	error = __m0516ldn_write_reg(data->client,
						REG_BACKLIGHT_POWER,
						1,
						&value);
	if (error)
		ret = -EINVAL;

	ret = count;

	return ret;
}

static ssize_t factory_mode_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u8 value = 0;

	error = __m0516ldn_read_reg(data->client,
					REG_FACTORY_MODE,
					1,
					&value);
	if (error)
		value = -1;

	return scnprintf(buf, PAGE_SIZE, "%02x\n", value);
}

static ssize_t factory_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int error = 0;
	ssize_t ret;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u8 value = 0;

	if (kstrtou8(buf, 10, &value))
		ret = -EINVAL;

	dev_err(dev, "factory_mode = %d count= %d\n", value, count);

	error = __m0516ldn_write_reg(data->client,
						REG_FACTORY_MODE,
						1,
						&value);
	if (error)
		ret = -EINVAL;

	ret = count;

	return ret;
}

static ssize_t force_update_rom_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%02x\n", force_update_rom);
}

static ssize_t force_update_rom_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	ssize_t ret;
	u8 value = 0;

	if (kstrtou8(buf, 10, &value))
		ret = -EINVAL;

	force_update_rom = value;
	dev_err(dev, "force_update_rom = %d value= %d\n",
		force_update_rom,
		value);

	ret = count;

	return ret;
}

static ssize_t diag_status_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int error;
	struct m0516ldn *data = dev_get_drvdata(dev);
	u32 value32 = 0;
	u8 temp[4];
	u8 value;

	value = 0x01;//start detecting
	error = __m0516ldn_write_reg(data->client,
					REG_DIAGNOSTIC_DETECT,
					1,
					&value);
	if (error)
		value = -1;

	msleep(1000);
	error = __m0516ldn_read_reg(data->client,
					REG_DIAGNOSTIC_DETECT,
					1,
					&value);
	if (error)
		value = -1;

	if (value == 0x2) { //diag detect is finished
		error = __m0516ldn_read_reg(data->client,
				REG_DIAGNOSTIC_STATUS,
				4,
				&value32);
		if (error)
			value32 = -1;

		temp[0] = value32 & 0xff;
		temp[1] = (value32 >> 8) & 0xff;
		temp[2] = (value32 >> 16) & 0xff;
		temp[3] = (value32 >> 24) & 0xff;

		dev_err(dev, "diag status = %08x, temp[0] = %02x, temp[1] = %02x\n",
			value32, temp[0], temp[1]);

		dev_err(dev, "diag status = %08x, temp[2] = %02x, temp[3] = %02x\n",
			value32, temp[2], temp[3]);
	}

	return scnprintf(buf,
			PAGE_SIZE,
			"0x%08x\n%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
			value32,
			((value32 & STATUS_SYSTEM_ERR) ==
			STATUS_SYSTEM_ERR)?"SYSTEM_ERR\n":"",
			((value32 & STATUS_BACKLIGHT_ERR) ==
			STATUS_BACKLIGHT_ERR)?"BACKLIGHT_ERR\n":"",
			((value32 & STATUS_DS90UH949_ERR) ==
			STATUS_DS90UH949_ERR)?"DS90UH949_ERR\n":"",
			((value32 & STATUS_IR_TRANSMIT_ERR) ==
			STATUS_IR_TRANSMIT_ERR)?"IR_TRANSMIT_ERR\n":"",
			((value32 & STATUS_TFT_LCD_ERR) ==
			STATUS_TFT_LCD_ERR)?"TFT_LCD_ERR\n":"",
			((value32 & STATUS_I2C_SLAVE_ERR) ==
			STATUS_I2C_SLAVE_ERR)?"I2C_SLAVE_ERR\n":"",
			((value32 & STATUS_LED_LIGHTING_ERR) ==
			STATUS_LED_LIGHTING_ERR)?"LED_LIGHTING_ERR\n":"",
			((value32 & STATUS_ADC_PANEL_ERR) ==
			STATUS_ADC_PANEL_ERR)?"ADC_PANEL_ERR\n":"",
			((value32 & STATUS_ADC_PCB_ERR) ==
			STATUS_ADC_PCB_ERR)?"ADC_PCB_ERR\n":"",
			((value32 & STATUS_USB_CHARGE_ERR) ==
			STATUS_SYSTEM_ERR)?"USB_CHARGE_ERR\n":"",
			((value32 & STATUS_RESERVERD1_ERR) ==
			STATUS_RESERVERD1_ERR)?"RESERVERD1_ERR\n":"",
			((value32 & STATUS_RESERVERD2_ERR) ==
			STATUS_RESERVERD2_ERR)?"RESERVERD2_ERR\n":"",
			((value32 & STATUS_RESERVERD3_ERR) ==
			STATUS_RESERVERD3_ERR)?"RESERVERD3_ERR\n":"",
			((value32 & STATUS_RESERVERD4_ERR) ==
			STATUS_RESERVERD4_ERR)?"RESERVERD4_ERR\n":"",
			((value32 & STATUS_RESERVERD5_ERR) ==
			STATUS_RESERVERD5_ERR)?"RESERVERD5_ERR\n":"",
			((value32 & STATUS_RESERVERD6_ERR) ==
			STATUS_RESERVERD6_ERR)?"RESERVERD6_ERR\n":"");
}

static u8 get_update_status(struct m0516ldn *data)
{
	int error;
	u8 value = 0;

	error = __m0516ldn_read_reg(data->client,
						REG_UPDATE_STATUS,
						1,
						&value);
	if (error)
		value = -1;

	return value;
}

static int set_update_initial(struct m0516ldn *data)
{
	u8 value = 0;

	return __m0516ldn_write_reg(data->client,
					REG_UPDATE_INITIAL,
					1,
					&value);
}

static int set_update_target(struct m0516ldn *data, enum update_rom rom)
{
	return __m0516ldn_write_reg(data->client,
					REG_UPDATE_TARGET,
					1,
					&rom);
}

static int set_update_finish(struct m0516ldn *data)
{
	u8 value = 0;

	return __m0516ldn_write_reg(data->client,
					REG_UPDATE_FINISH,
					1,
					&value);
}

static int get_rom_version(struct m0516ldn *data,
			enum update_rom rom,
			struct imgheader *header)
{
	int error;
	u8 value = 0;
	u16 value16 = 0;
	int retry = 20;

	while (retry > 0) {
		error = __m0516ldn_read_reg(data->client,
						REG_MULTIBOOT_INDEX,
						1,
						&value);
		dev_info(&data->client->dev, "%s:read [%d]REG_MULTIBOOT_INDEX = %02x err = %d\n",
				__func__,
				retry,
				value,
				error);

		if (error) {
			msleep(20);
			retry--;
			continue;
		}

		if ((value != MULTIBOOT_AP2ROM) && (retry == 0)) {
			dev_err(&data->client->dev, "MCU Boot up fail, please download APPROM again!!!\n");
			return -1;
		} else if (value == MULTIBOOT_AP2ROM) {
			msleep(100);
			break;
		}

		msleep(20);
		retry--;
	}

	if (rom == UPDATE_APPROM) {
		retry = 20;
		while (retry > 0) {
			value16 = 0;
			error = __m0516ldn_read_reg(data->client,
						REG_VERSION,
						2,
						&value16);
			dev_info(&data->client->dev, "%s:read [%d]REG_VERSION = %04x err = %d\n",
					__func__,
					retry,
					value16,
					error);

			if (error) {
				msleep(20);
				retry--;
				continue;
			}
			g_version = value16;

			value16 = 0;
			error = __m0516ldn_read_reg(data->client,
						REG_RD_VERSION,
						2,
						&value16);
			dev_info(&data->client->dev, "%s:read [%d]REG_RD_VERSION = %04x err = %d\n",
					__func__,
					retry,
					value16,
					error);

			if (error) {
				msleep(20);
				retry--;
				continue;
			}
			g_rd_version = value16;

			if ((g_version == 0) || (g_rd_version == 0)) {
				dev_info(&data->client->dev,
						"%s:MCU not ready\n",
						__func__);
				msleep(20);
				retry--;
				continue;
			} else if ((g_version == header->version) &&
				(g_rd_version == header->rd_version)) {
				dev_info(&data->client->dev,
					"%s:MCU update successfully, versions are matched\n",
					__func__);
				msleep(100);
				break;
			}

			msleep(20);
			retry--;
		}
	} else if (rom == UPDATE_LDROM) {
		retry = 20;
		while (retry > 0) {
			value16 = 0;
			error = __m0516ldn_read_reg(data->client,
						REG_LDROM_VERSION,
						2,
						&value16);
			dev_info(&data->client->dev, "%s:read [%d]REG_LDROM_VERSION = %04x err = %d\n",
					__func__,
					retry,
					value16,
					error);

			if (error) {
				msleep(20);
				retry--;
				continue;
			}
			g_ldrom_version = value16;

			value16 = 0;
			error = __m0516ldn_read_reg(data->client,
						REG_LDROM_RD_VERSION,
						2,
						&value16);
			dev_info(&data->client->dev, "%s:read [%d]REG_LDROM_RD_VERSION = %04x err = %d\n",
					__func__,
					retry,
					value16,
					error);

			if (error) {
				msleep(20);
				retry--;
				continue;
			}
			g_ldrom_rd_version = value16;

			if ((g_ldrom_version == 0) ||
				(g_ldrom_rd_version == 0)) {
				dev_info(&data->client->dev,
						"%s:MCU not ready\n",
						__func__);
				msleep(20);
				retry--;
				continue;
			} else if ((g_ldrom_version == header->version) &&
				(g_ldrom_rd_version == header->rd_version)) {
				dev_info(&data->client->dev,
					"%s:MCU update successfully, versions are matched\n",
					__func__);
				msleep(100);
				break;
			}

			msleep(20);
			retry--;
		}
	}

	return 0;
}

static u8 util_checksum(const u8 *buf, u8 size)
{
	int i = 0;
	u32 checksum = 0;

	for (i = 0; i < size; i++)
		checksum = (checksum + buf[i]) & 0xFF;

	return (checksum & 0xFF);
}

#if 0
static u32 computeFileCRC(const u8 *buf, int size)
{
	int read_size = 0;
	u32 crc = 0;

	crc = crc32(0L ^ 0xffffffff, NULL, 0) ^ 0xffffffff;
	while (size > 0) {
		if (size >= PACKAGE_WRITE_SIZE)
			read_size = PACKAGE_WRITE_SIZE;
		else
			read_size = size;

		crc = crc32(crc ^ 0xffffffff, buf, read_size) ^ 0xffffffff;

		size -= read_size;
		buf += read_size;
	}
	return crc;
}
#endif

static int send_mcu_update_complete_pwm(enum boot_mode_type boot_mode)
{
	unsigned int update_complete_pwm_freq;

	if (boot_mode == MODE_NORMAL)
		update_complete_pwm_freq = NORMAL_BOOT_PWM_FREQ_DEFAULT;
	else if (boot_mode == MODE_RECOVERY)
		update_complete_pwm_freq = RECOVERY_BOOT_PWM_FREQ_DEFAULT;
	else if (boot_mode == MODE_POSTRECOVERY)
		update_complete_pwm_freq = POSTRECOVERY_BOOT_PWM_FREQ_DEFAULT;
	else // Default send Normal Boot
		update_complete_pwm_freq = NORMAL_BOOT_PWM_FREQ_DEFAULT;
	return send_pwm_signal(update_complete_pwm_freq);
}

static void send_update_timeout_pwm(unsigned long data)
{
	enum boot_mode_type *boot_mode = (enum boot_mode_type *) data;

	pr_info("Update timer expired\n");
	send_mcu_update_complete_pwm(*boot_mode);
}

static void start_update_timer(struct timer_list *update_timer, enum boot_mode_type *boot_mode)
{
	if (timer_pending(update_timer))
		del_timer(update_timer);
	setup_timer(update_timer, send_update_timeout_pwm, (unsigned long) boot_mode);
	update_timer->expires = (unsigned long) (jiffies + (HZ * MCU_UPDATE_TIMEOUT_TIME));
	add_timer(update_timer);
	pr_info("Update timer started\n");
}

static ssize_t update_bin_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	const struct firmware *cfg;
	int ret, size, write_size, qty, i = 0;
	enum update_status status = 0;
	const u8 *p = NULL;
	u8 chksum = 0, rom = 0;
	struct m0516ldn *data = dev_get_drvdata(dev);
	char firmware[MAX_NAME_LEN];
	char pkg_buff[PACKAGE_BUFF_SIZE];
	struct imgheader *header;
	unsigned int crc32 = 0;
	int error = 0;
	int is_update_start_pwm_sent = 0;
	int mcu_update_pwm_ret = 0;
	enum boot_mode_type current_boot_mode = get_boot_mode_type();

	if (kstrtou8(buf, 10, &rom))
		ret = -EINVAL;

	dev_info(dev, "update_bin parameter = %d count = %d\n", rom, count);
	if (force_update_rom)
		if (rom != force_update_rom) {
			dev_info(dev, "Need to recovery rom = %d\n",
				force_update_rom);
			ret = -ENOENT;
			goto out;
		}

	if (rom == UPDATE_LDROM)
		snprintf(firmware, MAX_NAME_LEN, "%s", M0516LDN_LDROM);
	else if (rom == UPDATE_APPROM)
		snprintf(firmware, MAX_NAME_LEN, "%s", M0516LDN_APPROM);
	else {
		ret = -ENOENT;
		goto out;
	}

	ret = request_firmware(&cfg, firmware, dev);
	if (ret < 0) {
		dev_err(dev, "Failure to request config file %s\n",
			firmware);
		ret = -ENOENT;
		goto out;
	} else {
		dev_info(dev, "Found configuration file: %s filesize: %d\n",
			firmware,
			cfg->size);
		ret = count;
	}

	//Parse img header
	header = (struct imgheader *)cfg->data;
	p = cfg->data + sizeof(struct imgheader);
#if 0
	crc32 = computeFileCRC(p, header->bin_size);
#endif
	crc32 = crc32(0L ^ 0xffffffff, p, header->bin_size) ^ 0xffffffff;

	dev_info(dev, "header info: version = %08x rd version = %08x prj_id = %08x cust_id =%08x\n",
		header->version,
		header->rd_version,
		header->project_id,
		header->customer_id);
	dev_info(dev, "header info: rom = %d bin_size = %d bin_crc32 = %08x computed crc32 = %08x\n",
		header->rom,
		header->bin_size,
		header->bin_crc32,
		crc32);

	if (header->rom != rom) {
		dev_info(dev, "update wrong rom\n");
		ret = -ENOENT;
		goto out;
	}

	if (header->bin_crc32 != crc32) {
		dev_info(dev, "the image is corrupted\n");
		ret = -ENOENT;
		goto out;
	}

	if (rom == UPDATE_APPROM) {
		dev_info(dev, "header info: version = %08x rd version = %08x g_version = %08x g_rd_version =%08x\n",
		header->version,
		header->rd_version,
		g_version,
		g_rd_version);

		if ((header->version == 0x02) && (header->rd_version == 0x13)) {
			dev_info(dev, "the firmware 0213 has problem and blocked for updating\n");
			ret = -ENOENT;
			goto out;
		}

		if ((g_version == 0x02) && (g_rd_version == 0x13)) {
			dev_info(dev, "Panel with 0213 firmware version will not be updated\n");
			ret = -ENOENT;
			goto out;
		}

		if ((((header->version == g_version))
			&& (header->rd_version <= g_rd_version))
			&& (!force_update_rom)) {
			dev_info(dev, "no need update\n");
			ret = -ENOENT;
			goto out;
		} else if (header->version < g_version) {
			dev_info(dev, "no rollback\n");
			ret = -ENOENT;
			goto out;
		}
	} else if (rom == UPDATE_LDROM) {
		dev_info(dev, "header info: version = %08x rd version = %08x g_ldrom_version = %08x g_ldrom_rd_version =%08x\n",
		header->version,
		header->rd_version,
		g_ldrom_version,
		g_ldrom_rd_version);

		if (((header->version == g_ldrom_version)
			&& (header->rd_version == g_ldrom_rd_version))
			&& (!force_update_rom)) {
			dev_info(dev, "no need update\n");
			ret = -ENOENT;
			goto out;
		} else if (((g_ldrom_version == 0xff00)
			&& (g_ldrom_rd_version == 0xff00))) {
			dev_info(dev, "wrong ldrom version\n");
			ret = -ENOENT;
			goto out;
		//remove first, due to some old ldrom is 0x4d
		//} else if (header->version < g_ldrom_version) {
		//	dev_info(dev, "no rollback\n");
		//	ret = -ENOENT;
		//	goto out;
		}
	}

	//Init Update
	start_update_timer(&update_timer, &current_boot_mode);
	if (send_pwm_signal(MCU_DISPLAY_UPDATE_START_PWM_FREQ) == -1) {
		ret = -ENOENT;
		dev_info(dev, "unable to send update start pwm signal\n");
		goto out;
	}
	is_update_start_pwm_sent = 1;

	set_update_initial(data);
	msleep(20);
	//first time, will be switch to LDROM/APPROM
	set_update_target(data, rom);
	msleep(20);
	while ((status = get_update_status(data)) != STATUS_WAIT_PACKAGE) {
		dev_info(dev, "get_update_status %d\n", status);
		set_update_target(data, rom);
		msleep(20);
		dev_info(dev, "send set_update_target rom =%d count = %d\n",
				rom,
				i);
		i++;
	}

	//Start Send Package
	size = cfg->size - sizeof(struct imgheader);
	p = cfg->data + sizeof(struct imgheader);
	qty = (size / PACKAGE_WRITE_SIZE) +
			(((size % PACKAGE_WRITE_SIZE) == 0) ? 0 : 1);
	i = 0;
	while (size > 0) {
		if (size > PACKAGE_WRITE_SIZE)
			write_size = PACKAGE_WRITE_SIZE;
		else
			write_size = size;

		memset(pkg_buff, 0x00, PACKAGE_BUFF_SIZE);
		//add index
		pkg_buff[0] = i & 0xff;
		pkg_buff[1] = (i >> 8) & 0xff;
		//add qty
		pkg_buff[2] = qty & 0xff;
		pkg_buff[3] = (qty >> 8) & 0xff;

		chksum = util_checksum(p, write_size);
		dev_info(dev, "[%d]chksum = %02x write_size = %d qty = %d\n",
				i,
				chksum,
				write_size,
				qty);
		//add checksum
		pkg_buff[4] = chksum & 0xff;
		//add data size
		pkg_buff[5] = PACKAGE_WRITE_SIZE & 0xff;
		//add data
		memcpy(&pkg_buff[6], p, write_size);

		__m0516ldn_write_reg(data->client,
						REG_UPDATE_PACKAGE,
						PACKAGE_BUFF_SIZE,
						&pkg_buff);

		msleep(20);
		while ((status = get_update_status(data)) !=
						STATUS_SEND_NEXT_PACKAGE) {
			dev_info(dev, "writre package get_update_status %d\n",
					status);
			if (status == STATUS_SEND_LAST_PACKAGE_AGAIN)
				__m0516ldn_write_reg(data->client,
						REG_UPDATE_PACKAGE,
						PACKAGE_BUFF_SIZE,
						&pkg_buff);
			else if ((status >= STATUS_UNKNOWN_FAIL) &&
					(status <= STATUS_FAIL_DATA_VERIFY)) {
				ret = -EAGAIN;
				goto out;
			}
			msleep(20);
		}

		size -= write_size;
		p += write_size;
		i++;
		msleep(20);
	}

	//make sure last package will be written successfully
	if (get_update_status(data) == STATUS_SEND_NEXT_PACKAGE) {
		dev_info(dev, "set_update_finish  loop:%d\n", i);
		msleep(20);
		set_update_finish(data);
	}
	msleep(100);
	error = get_rom_version(data, rom, header);
	if (error) {
		dev_err(dev, "update fail...\n");
		ret = -EAGAIN;
	} else {
		dev_err(dev, "update success...\n");
		force_update_rom = 0; //clear
		ret = count;
	}

//release:
	release_firmware(cfg);
out:
	if (is_update_start_pwm_sent)
		mcu_update_pwm_ret = send_mcu_update_complete_pwm(current_boot_mode);

	if (timer_pending(&update_timer) && mcu_update_pwm_ret == 0) {
		if (del_timer_sync(&update_timer) >= 0)
			dev_info(dev, "disabling Update timer\n");
		else
			dev_info(dev, "failed to disable Update timer\n");
	}

	return ret;
}

static DEVICE_ATTR(version, 0444, version_show, NULL);
static DEVICE_ATTR(rd_version, 0444, rd_version_show, NULL);
static DEVICE_ATTR(project_id, 0444, project_id_show, NULL);
static DEVICE_ATTR(customer_id, 0444, customer_id_show, NULL);
static DEVICE_ATTR(multiboot_index, 0444, multiboot_index_show, NULL);
static DEVICE_ATTR(ldrom_version, 0444, ldrom_version_show, NULL);
static DEVICE_ATTR(ldrom_rd_version, 0444, ldrom_rd_version_show, NULL);
static DEVICE_ATTR(earphone, 0444, earphone_show, NULL);
static DEVICE_ATTR(usbchg, 0444, usbchg_show, NULL);
static DEVICE_ATTR(backlight, 0644, backlight_show, backlight_store);
static DEVICE_ATTR(audio_mute, 0644, audio_mute_show, audio_mute_store);
static DEVICE_ATTR(time_mode, 0644, time_mode_show, time_mode_store);
static DEVICE_ATTR(led_percentage,
					0644,
					led_percentage_show,
					led_percentage_store);
static DEVICE_ATTR(pcb_temperature, 0444, pcb_temperature_show, NULL);
static DEVICE_ATTR(panel_temperature, 0444, panel_temperature_show, NULL);
static DEVICE_ATTR(ds90uh949_powerdown,
					0644,
					ds90uh949_powerdown_show,
					ds90uh949_powerdown_store);
static DEVICE_ATTR(ds90uh948_powerdown,
					0644,
					ds90uh948_powerdown_show,
					ds90uh948_powerdown_store);
static DEVICE_ATTR(ut60_reset,
					0644,
					ut60_reset_show,
					ut60_reset_store);
static DEVICE_ATTR(update_bin, 0200, NULL, update_bin_store);
static DEVICE_ATTR(backlight_power,
					0644,
					backlight_power_show,
					backlight_power_store);
static DEVICE_ATTR(factory_mode,
					0644,
					factory_mode_show,
					factory_mode_store);
static DEVICE_ATTR(force_update_rom,
					0644,
					force_update_rom_show,
					force_update_rom_store);
static DEVICE_ATTR(diag_status, 0444, diag_status_show, NULL);

static struct attribute *m0516ldn_attrs[] = {
	&dev_attr_version.attr,
	&dev_attr_rd_version.attr,
	&dev_attr_project_id.attr,
	&dev_attr_customer_id.attr,
	&dev_attr_multiboot_index.attr,
	&dev_attr_ldrom_version.attr,
	&dev_attr_ldrom_rd_version.attr,
	&dev_attr_earphone.attr,
	&dev_attr_usbchg.attr,
	&dev_attr_backlight.attr,
	&dev_attr_audio_mute.attr,
	&dev_attr_time_mode.attr,
	&dev_attr_led_percentage.attr,
	&dev_attr_pcb_temperature.attr,
	&dev_attr_panel_temperature.attr,
	&dev_attr_ds90uh949_powerdown.attr,
	&dev_attr_ds90uh948_powerdown.attr,
	&dev_attr_ut60_reset.attr,
	&dev_attr_update_bin.attr,
	&dev_attr_backlight_power.attr,
	&dev_attr_factory_mode.attr,
	&dev_attr_diag_status.attr,
	&dev_attr_force_update_rom.attr,
	NULL
};

static const struct attribute_group m0516ldn_attr_group = {
	.attrs = m0516ldn_attrs,
};

static int m0516ldn_sysfs_init(struct m0516ldn *data)
{
	struct i2c_client *client = data->client;
	int error = 0;

	error = sysfs_create_group(&client->dev.kobj, &m0516ldn_attr_group);
	if (error) {
		dev_err(&client->dev, "Failure %d creating sysfs group\n",
			error);
		return error;
	}

	return error;
}

static int m0516ldn_i2c_probe(struct i2c_client *client,
				      const struct i2c_device_id *id)
{
	struct m0516ldn *data;
	int error;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C check functionality failed.\n");
		return -EIO;
	}

	data = devm_kzalloc(&client->dev, sizeof(struct m0516ldn), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	m0516ldn_parse_dt(&client->dev, data);

	snprintf(data->phys, sizeof(data->phys), "i2c-%u-%04x/m0516ldn",
		 client->adapter->nr, client->addr);

	data->client = client;
	data->irq = client->irq;
	i2c_set_clientdata(client, data);

	error = m0516ldn_initialize(data);
	if (error) {
		dev_info(&client->dev, "nuvoton_m0516ldn_i2c_probe fail!!!\n");
		return error;
	}

	error = m0516ldn_sysfs_init(data);
	if (error)
		return error;

	g_m0516ldn = data;
#ifdef CONFIG_AMLOGIC_BL_HOTPLUG
	data->power_on = bl_hotplug_power_on;
	data->power_off = bl_hotplug_power_off;
	data->set_level = bl_hotplug_set_level;
	aml_bl_hotplug_set_m0516ldn((struct bl_hotplug_i2cdev *)data);
#endif
	pr_info("assign value to m0516ldn audio mute function ptr\n");
	m0516ldn_audio_set_mute_ptr = m0516ldn_audio_set_mute;
	if (m0516ldn_audio_set_mute_ptr == NULL)
		pr_info("m0516ldn_audio_set_mute_ptr is null\n");
	else
		pr_info("m0516ldn_audio_set_mute_ptr %p\n", m0516ldn_audio_set_mute_ptr);

	m0516ldn_ir_led_control(data, 0); /* turn off IR LEDs */

	dev_info(&client->dev, "nuvoton_m0516ldn_i2c_probe OK!!!\n");

	return 0;
}

static int m0516ldn_i2c_remove(struct i2c_client *client)
{
	//struct m0516ldn *data = i2c_get_clientdata(client);

#ifdef CONFIG_AMLOGIC_BL_HOTPLUG
	aml_bl_hotplug_set_m0516ldn(NULL);
#endif
	pr_info("remove value to m0516ldn audio mute function ptr\n");
	m0516ldn_audio_set_mute_ptr = NULL;
	sysfs_remove_group(&client->dev.kobj, &m0516ldn_attr_group);
	return 0;
}

static const struct i2c_device_id m0516ldn_id[] = {
	{ "m0516ldn", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, m0516ldn_id);

#ifdef CONFIG_OF
static const struct of_device_id m0516ldn_i2c_dt_ids[] = {
	{ .compatible = "nuvoton,m0516ldn", },
	{ }
};
MODULE_DEVICE_TABLE(of, m0516ldn_i2c_dt_ids);
#endif

static struct i2c_driver m0516ldn_i2c_driver = {
	.driver = {
		.name	= "m0516ldn",
		//.pm	= &m0516ldn_pm_ops,
		.of_match_table = of_match_ptr(m0516ldn_i2c_dt_ids),
	},
	.probe		= m0516ldn_i2c_probe,
	.remove		= m0516ldn_i2c_remove,
	.id_table	= m0516ldn_id,
};

module_i2c_driver(m0516ldn_i2c_driver);

MODULE_AUTHOR("Dennis Hsu <dennis.hsu@jet-opto.com.tw>");
MODULE_DESCRIPTION("Nuvoton M0516LDN driver");
MODULE_LICENSE("GPL v2");
