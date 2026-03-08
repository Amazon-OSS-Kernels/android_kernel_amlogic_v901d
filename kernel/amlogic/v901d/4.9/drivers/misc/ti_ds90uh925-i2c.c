/*
 * TI DS90UH925-Q1 1080p FPD-Link III to CSI-2 Deserializer with HDCP (I2C bus)
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
#include <linux/delay.h>
//#include "board_version.h"

#define REG_DEVICE_ID 0x00
#define REG_CONFIG_0 0x03
#define REG_STATUS_ID 0x0C
#define REG_GPIO0_CONFIG 0x0D
#define REG_GPIO12_CONFIG 0x0E
#define REG_GPIO34_CONFIG 0x0F
#define REG_GPIO56_CONFIG 0x10
#define REG_DATA_PATH_CONTROL 0x12

#define WATCHDOG_DELAYED_WORK
#define WATCHDOG_REPORT_DELAY_1 (10 * HZ)
#define WATCHDOG_REPORT_DELAY_2 (13 * HZ / 1000) // 1000 ms
#define WATCHDOG_REPORT_DELAY_3 (5 * HZ / 1000) // 5 ms
#define SERDES_CABLE_LINK_BITMASK 0x1		  // bit 0
#define SERDES_CABLE_LINK_NOT_DETECTED 0x0	  // Cable link not detected
#define SERDES_CABLE_LINK_DETECTED 0x1		  // Cable link detected
u8 watchdog_level;

struct ds90uh94x {
	struct i2c_client *client;
	struct device *dev;
	unsigned int irq;
	char phys[32];
#ifdef WATCHDOG_DELAYED_WORK
	struct delayed_work delayed_work_watchdog_status;
#endif
};

static int __ds90uh94x_read_reg(struct i2c_client *client, u8 reg, u16 len,
				void *val)
{
	struct i2c_msg xfer[2];
	// u8 buf[2];
	int ret;

	// buf[0] = reg & 0xff;
	// buf[1] = (reg >> 8) & 0xff;

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
	dev_dbg(&client->dev, "%s: i2c transfer status (%d)\n", __func__, ret);
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

static int __ds90uh94x_write_reg(struct i2c_client *client, u8 reg, u16 len,
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
	// buf[1] = (reg >> 8) & 0xff;
	memcpy(&buf[1], val, len);

	ret = i2c_master_send(client, buf, count);
	if (ret == count) {
		ret = 0;
	} else {
		if (ret >= 0)
			ret = -EIO;
		dev_err(&client->dev, "%s: i2c send failed (%d)\n", __func__,
			ret);
	}

	kfree(buf);
	return ret;
}

static int ds90uh94x_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	return __ds90uh94x_write_reg(client, reg, 1, &val);
}

static int ds90uh94x_parse_dt(struct device *dev, struct ds90uh94x *data)
{
	u32 test;

	device_property_read_u32(dev, "test", &test);
	dev_info(dev, "%s:test = %d\n", __func__, test);

	return 0;
}

static int ds90uh94x_initialize(struct ds90uh94x *data)
{
	int error;
	u8 value = 0;

	error = __ds90uh94x_read_reg(data->client, REG_DEVICE_ID, 1, &value);
	dev_info(&data->client->dev, "%s:read REG_DEVICE_ID = 0x%x err = %d\n",
		 __func__, value, error);

	if (error)
		return error;

	dev_info(&data->client->dev,
		 "%s:REG_DEVICE_ID value = %02x DTS address = %02x\n", __func__,
		 value / 2, data->client->addr);
	if ((value / 2) != data->client->addr)
		return -1;

	value = 0x93; // 925 Headphone Detect GPIO3 input ; Maunal Reset GPIO4
		      // output high
	error = ds90uh94x_write_reg(data->client, REG_GPIO34_CONFIG, value);
	dev_info(&data->client->dev,
		 "%s:write REG_GPIO34_CONFIG value = %02x error = %d\n",
		 __func__, value, error);

	if (error)
		return error;

	msleep(1000);

	value = 0xda; // I2C Pass Through from 925
	error = ds90uh94x_write_reg(data->client, REG_CONFIG_0, value);
	dev_info(&data->client->dev,
		 "%s:write REG_CONFIG_0 value = %02x error = %d\n", __func__,
		 value, error);

	if (error)
		return error;

	value = 0x9; // 925 backlight REG_GPIO56_CONFIG output high
	error = ds90uh94x_write_reg(data->client, REG_GPIO56_CONFIG, value);
	dev_info(&data->client->dev,
		 "%s:write REG_GPIO56_CONFIG value = %02x error = %d\n",
		 __func__, value, error);

	if (error)
		return error;

	value = 0x53; // 925 Sil5293 interrupt GPIO1 input ; Power Latch Request
		      // GPIO2 output low
	error = ds90uh94x_write_reg(data->client, REG_GPIO12_CONFIG, value);
	dev_info(&data->client->dev,
		 "%s:write REG_GPIO12_CONFIG value = %02x error = %d\n",
		 __func__, value, error);

	if (error)
		return error;

	value = 0x13; // 925 Headphone Detect GPIO3 input ; Maunal Reset GPIO4
		      // output low
	error = ds90uh94x_write_reg(data->client, REG_GPIO34_CONFIG, value);
	dev_info(&data->client->dev,
		 "%s:write REG_GPIO34_CONFIG value = %02x error = %d\n",
		 __func__, value, error);

	if (error)
		return error;

	value = 0x93; // 925 Sil5293 interrupt GPIO1 input ; Power Latch Request
		      // GPIO2 output high
	error = ds90uh94x_write_reg(data->client, REG_GPIO12_CONFIG, value);
	dev_info(&data->client->dev,
		 "%s:write REG_GPIO12_CONFIG value = %02x error = %d\n",
		 __func__, value, error);

	if (error)
		return error;

	value = 0x4; // 925 Must set 18bit Video Select; bit2 = 1:18bit  0:24bit
	error = ds90uh94x_write_reg(data->client, REG_DATA_PATH_CONTROL, value);
	dev_info(&data->client->dev,
		 "%s:write REG_DATA_PATH_CONTROL value = %02x error = %d\n",
		 __func__, value, error);

	if (error)
		return error;

	value = 0x5; // 925 GPIO0 , PWM + WATCHDOG
			  // GPIO0 Remote Enable
	error = ds90uh94x_write_reg(data->client, REG_GPIO0_CONFIG, value);
	dev_info(&data->client->dev,
		 "%s:write REG_GPIO0_CONFIG value = %02x error = %d\n",
		 __func__, value, error);

	if (error)
		return error;

	return 0;

}

#ifdef WATCHDOG_DELAYED_WORK
void watchdog_report(struct work_struct *work)
{
	int error = 0;
	u8 value = 0;
	int retry = 20;
	struct delayed_work *dwork = to_delayed_work(work);
	struct ds90uh94x *data =
	    container_of(dwork, struct ds90uh94x, delayed_work_watchdog_status);

	dev_dbg(&data->client->dev, "%s: enter data = %p\n", __func__, data);

	if (watchdog_level == 1) {
		while (retry > 0) {
			value = 0x1; // 925 WatchDog GPIO0 output low
			error = ds90uh94x_write_reg(data->client,
						    REG_GPIO0_CONFIG, value);

//			dev_info(&data->client->dev,
//	"%s:kick watchdog write REG_GPIO0_CONFIG value = %02x\n",
//				 __func__, value);

			if (error) {
				msleep(20);
				retry--;
				continue;
			} else
				break;
		}
		watchdog_level = 0;

		schedule_delayed_work(&data->delayed_work_watchdog_status,
			      WATCHDOG_REPORT_DELAY_3);
	} else {
		while (retry > 0) {
			value = 0x9; // 925 WatchDog GPIO0 output high
			error = ds90uh94x_write_reg(data->client,
						    REG_GPIO0_CONFIG, value);

//			dev_info(&data->client->dev,
//	"%s:kick watchdog write REG_GPIO0_CONFIG = %02x\n",
//				 __func__, value);

			if (error) {
				msleep(20);
				retry--;
				continue;
			} else
				break;
		}
		watchdog_level = 1;

		schedule_delayed_work(&data->delayed_work_watchdog_status,
			      WATCHDOG_REPORT_DELAY_2);
	}
}
#endif

static int ds90uh94x_i2c_probe(struct i2c_client *client,
			       const struct i2c_device_id *id)
{
	struct ds90uh94x *data;
	int error;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C check functionality failed.\n");
		return -EIO;
	}

	data = devm_kzalloc(&client->dev, sizeof(struct ds90uh94x), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	ds90uh94x_parse_dt(&client->dev, data);

	snprintf(data->phys, sizeof(data->phys), "i2c-%u-%04x/ds90uh925",
		 client->adapter->nr, client->addr);

	data->client = client;
	data->irq = client->irq;
	i2c_set_clientdata(client, data);

	error = ds90uh94x_initialize(data);
	if (error) {
		dev_err(&client->dev, "ds90uh925_i2c_probe fail!!!\n");
		return error;
	}

	dev_info(&data->client->dev, "ds90uh925_i2c_probe OK!!!\n");
	return 0;
}

static int ds90uh94x_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id ds90uh94x_id[] = {{"ds90uh925", 0}, {} };
MODULE_DEVICE_TABLE(i2c, ds90uh94x_id);

#ifdef CONFIG_OF
static const struct of_device_id ds90uh94x_i2c_dt_ids[] = {
	{
	.compatible = "ti,ds90uh925",
	},
	{}
};
MODULE_DEVICE_TABLE(of, ds90uh94x_i2c_dt_ids);
#endif

static struct i2c_driver ds90uh94x_i2c_driver = {
	.driver = {
		.name = "ds90uh925",
		//.pm	= &ds90uh94x_pm_ops,
		.of_match_table = of_match_ptr(ds90uh94x_i2c_dt_ids),
	},
	.probe = ds90uh94x_i2c_probe,
	.remove = ds90uh94x_i2c_remove,
	.id_table = ds90uh94x_id,
};

module_i2c_driver(ds90uh94x_i2c_driver);

MODULE_AUTHOR("Dennis Hsu <dennis.hsu@jet-opto.com.tw>");
MODULE_DESCRIPTION(
	"TI DS90UH94x 1080p OpenLDI to FPD-Link III serializer driver");
MODULE_LICENSE("GPL v2");
