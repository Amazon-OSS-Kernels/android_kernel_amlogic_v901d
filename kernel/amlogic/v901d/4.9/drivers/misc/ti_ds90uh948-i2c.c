/*
 * TI DS90UH948-Q1 2K FPD-Link III to OpenLDI Deserializer With HDCP (I2C bus)
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

#define REG_DEVICE_ID 0x00
#define REG_GPIO0_CONFIG 0x1D
#define REG_GPIO1_2_CONFIG 0x1E
#define REG_GPIO3_CONFIG 0x1F
#define REG_GPIO_REG5_CONFIG 0x20
#define REG_GPIO7_8_CONFIG 0x21
#define REG_ID0 0xF0 //'_'
#define REG_ID1 0xF1 //'U'
#define REG_ID2 0xF2 //'H'
#define REG_ID3 0xF3 //'9'
#define REG_ID4 0xF4 //'4'  , '2'
#define REG_ID5 0xF5 //'8'
#define REG_DUAL_RX_CTL 0x34
#define REG_HSCC_CONTROL 0x43
#define REG_928_GPIO_STATUS_1 0x6E
#define A8552_EN_DELAY_500 500
#define A8552_EN_DELAY_1000 1000

struct ds90uh94x {
	struct i2c_client *client;
	struct device *dev;
	unsigned int irq;
	char phys[32];
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
	dev_err(&client->dev, "%s: i2c transfer status (%d)\n", __func__, ret);
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

#if 1
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
#endif

static int ds90uh94x_parse_dt(struct device *dev, struct ds90uh94x *data)
{
	u32 test;

	device_property_read_u32(dev, "test", &test);
	dev_info(dev, "%s:test = %d\n", __func__, test);

	return 0;
}

static int aptiv_display_init(struct ds90uh94x *data)
{
	int error;
	u8 value = 0;

	//GPIO_REG_7 Aptiv a8552 enable pin
	value = 0x01; // GPIO_REG_7 and GPIO_REG_8, A8552_EN output low
	error = ds90uh94x_write_reg(data->client, REG_GPIO7_8_CONFIG,
				    value);
	dev_info(&data->client->dev,
		 "%s:write REG_GPIO7_8_CONFIG value = 0x%x err = %d\n",
		 __func__, value, error);
	if (error)
		return error;

	//Align MY21 VRM delay
	msleep(A8552_EN_DELAY_500);

	value = 0x09; // GPIO_REG_7 and GPIO_REG_8, A8552_EN output hi
	error = ds90uh94x_write_reg(data->client, REG_GPIO7_8_CONFIG,
				    value);
	dev_info(&data->client->dev,
		 "%s:write REG_GPIO7_8_CONFIG value = 0x%x err = %d\n",
		 __func__, value, error);
		if (error)
		return error;

	mdelay(A8552_EN_DELAY_1000);

	dev_info(&data->client->dev,
		"%s: aptiv dsipay init done, err = %d\n",
		__func__, error);
	return error;
}

static u8 read_a8552_fault(struct ds90uh94x *data)
{
	int error;
	u8 value = 0;
	// Check Fault pin status 0x6e bit 5 (low active)
		error = __ds90uh94x_read_reg(data->client,
				REG_928_GPIO_STATUS_1, 1, &value);
		dev_info(&data->client->dev,
				"%s:read REG_928_GPIO_STATUS_1(A8552 fault pin) value  = 0x%x err = %d\n",
				__func__, value, error);
		return value;
}

static int ds90uh94x_initialize(struct ds90uh94x *data)
{
	int error;
	int count = 0;
	u8 value = 0;

	error = __ds90uh94x_read_reg(data->client, REG_DEVICE_ID, 1, &value);
	dev_info(&data->client->dev, "%s:read REG_DEVICE_ID = 0x%x err = %d\n",
		 __func__, value, error);

	if (error)
		return error;

	dev_info(&data->client->dev,
		 "%s:REG_DEVICE_ID value/2 = %02x DTS address = %02x\n",
		 __func__, value / 2, data->client->addr);
	if ((value / 2) != data->client->addr)
		return -1;

	error = __ds90uh94x_read_reg(data->client, REG_ID4, 1, &value);
	dev_info(&data->client->dev, "%s:read REG_ID4 value = 0x%x err = %d\n",
		 __func__, value, error);

	if (error)
		return error;

	if (value == 0x32) { // 9 '2' 8
		dev_info(&data->client->dev, "%s: 928!!\n", __func__);

		// init Apitv display related GPIOs

		value = 0x21; // GPIO_REG_0 (APTIV LCD3v3 enable output low)
		error =
			ds90uh94x_write_reg(data->client,
				REG_GPIO0_CONFIG, value);
		dev_info(&data->client->dev,
			 "%s:write REG_GPIO0_CONFIG value = 0x%x err = %d\n",
			 __func__, value, error);

		if (error)
			return error;

		value = 0x01; // GPIO_REG_7 and GPIO_REG_8, A8552_EN output low
		error = ds90uh94x_write_reg(data->client, REG_GPIO7_8_CONFIG,
				    value);
		dev_info(&data->client->dev,
			 "%s:write REG_GPIO7_8_CONFIG value = 0x%x err = %d\n",
			 __func__, value, error);
		if (error)
			return error;

		msleep(A8552_EN_DELAY_500);

		value = 0x29; // GPIO_REG_0 (APTIV LCD3v3 enable output hi)
		error = ds90uh94x_write_reg(data->client,
				REG_GPIO0_CONFIG, value);
		dev_info(&data->client->dev,
			 "%s:write REG_GPIO0_CONFIG value = 0x%x err = %d\n",
			__func__, value, error);

		if (error)
			return error;

		aptiv_display_init(data);

		//msleep(A8552_EN_DELAY_500);
		// to set GPIO_REG5 as Input for A8552 fault pin (low active)
		value = 0x03;
		error = ds90uh94x_write_reg(data->client, REG_GPIO_REG5_CONFIG,
							 value);
		dev_info(&data->client->dev,
			 "%s:write REG_GPIO_REG5_CONFIG value = 0x%x err = %d\n",
			 __func__, value, error);

		if (error)
			return error;

		// Check Fault pin status 0x6e bit 5 (low active)
		error = __ds90uh94x_read_reg(data->client,
				REG_928_GPIO_STATUS_1, 1, &value);
		dev_info(&data->client->dev,
				"%s:read REG_928_GPIO_STATUS_1(A8552 fault pin) value  = 0x%x err = %d\n",
				__func__, value, error);

		if (error)
			return error;

		while (!(value & 0x20) && count < 3) {
			dev_info(&data->client->dev,
					"%s:A8552 Fault pin alert = 0x%x , %d times\n",
					__func__, value, count);
			aptiv_display_init(data);
			value = read_a8552_fault(data);
			count++;
		}


		value = 0xB; // GPIO_REG_3 Enable GPIO and Set OUTPUT high
		error =
		    ds90uh94x_write_reg(data->client, REG_GPIO3_CONFIG, value);
		dev_info(&data->client->dev,
			 "%s:write REG_GPIO3_CONFIG value = 0x%x err = %d\n",
			 __func__, value, error);

		if (error)
			return error;

		value = 0x30; // REG_GPIO1_2_CONFIG Enable GPIO and Set input
		error = ds90uh94x_write_reg(data->client, REG_GPIO1_2_CONFIG,
					    value);
		dev_info(&data->client->dev,
			 "%s:write REG_GPIO1_2_CONFIG value = 0x%x err = %d\n",
			 __func__, value, error);

		if (error)
			return error;
	}
	return 0;
}

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

	snprintf(data->phys, sizeof(data->phys), "i2c-%u-%04x/ds90uh948",
		 client->adapter->nr, client->addr);

	data->client = client;
	data->irq = client->irq;
	i2c_set_clientdata(client, data);

	error = ds90uh94x_initialize(data);
	if (error) {
		dev_err(&client->dev, "ds90uh948_i2c_probe fail!!!\n");
		return error;
	}
	dev_info(&client->dev, "ds90uh948_i2c_probe OK!!!\n");
	return 0;
}

static int ds90uh94x_i2c_remove(struct i2c_client *client)
{
	// struct ds90uh94x *data = i2c_get_clientdata(client);

	// ds90uh94x_remove(data);

	return 0;
}

static const struct i2c_device_id ds90uh94x_id[] = {{"ds90uh948", 0}, {} };
MODULE_DEVICE_TABLE(i2c, ds90uh94x_id);

#ifdef CONFIG_OF
static const struct of_device_id ds90uh94x_i2c_dt_ids[] = {
	{
	.compatible = "ti,ds90uh948",
	},
	{}
};
MODULE_DEVICE_TABLE(of, ds90uh94x_i2c_dt_ids);
#endif

static struct i2c_driver ds90uh94x_i2c_driver = {
	.driver = {
	    .name = "ds90uh948",
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
