/*
 * TI DS90UH940-Q1 1080p FPD-Link III to CSI-2 Deserializer with HDCP (I2C bus)
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
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/extcon.h>

#define REG_DEVICE_ID			0x00
#define REG_I2C_CONTROL			0x05
#define REG_SESDES_ID			0x07
#define REG_SLAVE_ID_0			0x08
#define REG_SLAVE_ID_1			0x09
#define REG_SLAVEALIAS_ID_0		0x10
#define REG_SLAVEALIAS_ID_1		0x11
#define REG_STATUS_ID			0x1C
#define REG_GPIO0_CONFIG		0x1D
#define REG_GPIO1_CONFIG		0x1E
#define REG_PORT_SELECT			0x34
#define REG_GEN_STATUS			0x1C
#define DV_DEVICE				0x30

#define SERDES_DELAYED_WORK
#ifdef SERDES_DELAYED_WORK
#define SERDES_REPORT_DELAY_1		(10*HZ)
#define SERDES_REPORT_DELAY_2		(3*HZ)
#define SERDES_LOCK_STATUS_BITMASK	0x1 //bit 0
#define SERDES_LOCK_STATUS_UNLOCKED	0x0
u8 lock_status;

#define SERDES_CABLE_LINK_BITMASK	0x1 //bit 0
#define SERDES_CABLE_LINK_NOT_DETECTED	0x0 //Cable link not detected
#define SERDES_CABLE_LINK_DETECTED		0x1 //Cable link detected
u8 serdes1_status;
#endif

#ifdef CONFIG_IDME
extern unsigned int idme_get_board_rev(void);

#define LEN_FHD		3
#define LEN_APTIV		5
extern const char *idme_get_model_name(void);

#endif

enum display_panel {
	FHD,
	APTIV,
};

u8 display_panel_type = FHD;

struct ds90uh94x {
	struct i2c_client *client;
	struct device		*dev;
	unsigned int		irq;
	char			phys[32];
#ifdef SERDES_DELAYED_WORK
	struct delayed_work delayed_work_lock_status;
	struct delayed_work delayed_work_serdes1_status;
	struct extcon_dev *edev;
#endif
};

#ifdef SERDES_DELAYED_WORK
static const unsigned int hub_cable[] = {
	EXTCON_DISP_HUB,
	EXTCON_NONE,
};
#endif

static int __ds90uh94x_read_reg(struct i2c_client *client,
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

static int ds90uh94x_D_GPIO_config(struct ds90uh94x *data, u8 reg, u8 value)
{
	int error;

	//PORT_SEL Bit 1 = PORT1_SEL D_GPIO 1234
	error = ds90uh94x_write_reg(data->client,
				REG_PORT_SELECT,
				0x02);
	dev_info(&data->client->dev, "%s:write REG_PORT_SELECT value = 0x02 err = %d\n",
			__func__,
			error);

	if (error)
		return error;

	error = ds90uh94x_write_reg(data->client, reg, value);
	dev_info(&data->client->dev, "%s:write reg=%d value = %02x error = %d\n",
				__func__,
				reg,
				value,
				error);

	if (error)
		return error;

	//PORT_SEL Bit 0 = PORT0_SEL , Switch Back to PORT0
	error = ds90uh94x_write_reg(data->client, REG_PORT_SELECT, 0x02);
	dev_info(&data->client->dev, "%s:write REG_PORT_SELECT value = 0x01 err = %d\n",
			__func__,
			error);

	if (error)
		return error;
	return 0;
}

static int ds90uh94x_GPIO_config(struct ds90uh94x *data, u8 reg, u8 value)
{
	int error;

	// PORT_SEL Bit 0 = PORT0_SEL , Switch Back to PORT0
	error = ds90uh94x_write_reg(data->client, REG_PORT_SELECT, 0x01);
	dev_info(&data->client->dev,
		 "%s:write REG_PORT_SELECT value = 0x01 err = %d\n", __func__,
		 error);

	if (error)
		return error;

	error = ds90uh94x_write_reg(data->client, reg, value);
	dev_info(&data->client->dev, "%s:write reg=0x%x value = 0x%x error = %d\n",
				__func__,
				reg,
				value,
				error);

	if (error)
		return error;

	return 0;
}

static int ds90uh94x_initialize(struct ds90uh94x *data)
{
	int error;
	u8 value = 0;
	int board_id = DV_DEVICE;

	error = __ds90uh94x_read_reg(data->client, REG_DEVICE_ID, 1, &value);
	dev_info(&data->client->dev, "%s:read REG_DEVICE_ID = 0x%x err = %d\n",
			__func__,
			value,
			error);

	if (error)
		return error;

	dev_info(&data->client->dev, "%s:REG_DEVICE_ID value/2 = %02x DTS address = %02x\n",
				__func__,
				value/2,
				data->client->addr);
	if ((value/2) != data->client->addr)
		return -1;

	//I2C Pass All Bit 7 =  1: Enable Forward Control Channel pass-through
	value = 0x9e;
	error = ds90uh94x_write_reg(data->client, REG_I2C_CONTROL, value);
	dev_info(&data->client->dev, "%s:write REG_I2C_CONTROL value = 0x%x err = %d\n",
			__func__,
			value,
			error);
	if (error)
		return error;

	error = __ds90uh94x_read_reg(data->client, REG_SESDES_ID, 1, &value);
	dev_info(&data->client->dev, "%s:read REG_SESDES_ID = 0x%x err = %d\n",
			__func__,
			value,
			error);

	if (error)
		return error;

	if (value == 0x24) {//949 i2c address
		dev_info(&data->client->dev, "%s: DES ID is 949!!\n", __func__);
		value = 0x5; // D_GPIO0 0x5 = GPIO mode output
		error = ds90uh94x_D_GPIO_config(data, REG_GPIO0_CONFIG, value);
		if (error)
			return error;

		value = 0x3; // D_GPIO1 0x3 = GPIO mode input
		error = ds90uh94x_D_GPIO_config(data, REG_GPIO1_CONFIG, value);
		if (error)
			return error;

		error = __ds90uh94x_read_reg(data->client, REG_PORT_SELECT,
			 1, &value);
		dev_info(&data->client->dev,
			"%s:read REG_PORT_SELECT =x%x err = %d\n",
			 __func__, value, error);

		if (error)
			return error;

		value = value | 0x40; // RX_LOCK asserted even if activ video is
		//not being sent
		error = ds90uh94x_write_reg(data->client,
					REG_PORT_SELECT, value);
		dev_info(&data->client->dev,
			 "%s:write DUAL_RX_CTR value = 0x%x err = %d\n",
			 __func__, value, error);

		if (error)
			return error;

	} else if ((value == 0x18) || (value == 0x00)) {//925 i2c address or need to send
	//PWM signal first then check SERDES ID again
		if (value == 0x18)
			dev_info(&data->client->dev, "%s: DES ID is 925!!\n", __func__);
		else
			dev_info(&data->client->dev, "%s: DES ID is 0x00!!\n", __func__);
#ifdef CONFIG_IDME
		board_id = idme_get_board_rev();
#endif //CONFIG_IDME

		value = 0x5;//GPIO1 Sil5293 interrupt 0x5 = Remote GPIO Control
		error = ds90uh94x_GPIO_config(data, REG_GPIO1_CONFIG, value);
		if (error)
			return error;

		value = 0x3; // GPIO0 0x3 = GPIO mode input
		error = ds90uh94x_GPIO_config(data, REG_GPIO0_CONFIG, value);
		if (error)
			return error;
			//Select indirect CSI-2 register 0x16
		value = 0x16;
		error = ds90uh94x_D_GPIO_config(data, 0x6c, value);
		if (error)
			return error;
			//Clear PASS indicator from GPIO0
		value = 0x00;
		error = ds90uh94x_D_GPIO_config(data, 0x6d, value);
		if (error)
			return error;

		if (display_panel_type == APTIV)
			value = 0x1; // D_GPIO0 pull hi for RU media hub 5Vdet always enable
		else
			value = 0x5; // D_GPIO0 output, remote control mode for FHD panel
		error = ds90uh94x_D_GPIO_config(data, REG_GPIO0_CONFIG, value);
		if (error)
			return error;

		//re check serializer id
		error = __ds90uh94x_read_reg(data->client, REG_SESDES_ID, 1, &value);
		dev_info(&data->client->dev, "%s:Read check REG_SESDES_ID = 0x%x err = %d\n",
				__func__,
				value,
				error);
		if (error)
			return error;
		if (value == 0x18)
			dev_info(&data->client->dev, "%s: DES ID is 925!!\n", __func__);
		}

	return 0;
}

#ifdef SERDES_DELAYED_WORK
void lock_report(struct work_struct *work)
{
	int error;
	u8 value = 0;
	struct delayed_work *dwork = to_delayed_work(work);
	struct ds90uh94x *data = container_of(dwork,
			struct ds90uh94x, delayed_work_lock_status);

	dev_dbg(&data->client->dev, "%s: enter data = %p\n", __func__, data);

	error = __ds90uh94x_read_reg(data->client, REG_GEN_STATUS, 1, &value);
	if (lock_status != (value & SERDES_LOCK_STATUS_BITMASK)) {
		lock_status = (value & SERDES_LOCK_STATUS_BITMASK);
		dev_info(&data->client->dev,
				"%s: lock_status changed = %02x\n",
				__func__,
				lock_status);
		/*
		 *	notify lock status changed
		 *	/sys/bus/i2c/drivers/ds90uh940/1-002c/lock_status
		 */
		sysfs_notify(&(data->client->dev.kobj), NULL, "lock_status");
	}

	schedule_delayed_work(&data->delayed_work_lock_status,
				SERDES_REPORT_DELAY_2);
}
#endif

static ssize_t lock_status_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	u32 value;
	ssize_t ret;

	if (kstrtou32(buf, 10, &value))
		ret = -EINVAL;

	ret = count;
	return ret;
}

static ssize_t lock_status_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	//sysfs_notify(&dev->kobj, NULL, "lock_status");
	return scnprintf(buf, PAGE_SIZE, "%d\n", lock_status);
}

static DEVICE_ATTR(lock_status, 0644, lock_status_show, lock_status_store);

#ifdef SERDES_DELAYED_WORK
void serdes1_report(struct work_struct *work)
{
	int error;
	u8 value = 0;
	struct delayed_work *dwork = to_delayed_work(work);
	struct ds90uh94x *data =
	    container_of(dwork, struct ds90uh94x, delayed_work_serdes1_status);

	dev_dbg(&data->client->dev, "%s: enter data = %p\n", __func__, data);

	error = __ds90uh94x_read_reg(data->client, REG_STATUS_ID, 1, &value);
	if (!serdes1_status)
		dev_info(&data->client->dev,
			 "%s: 940 serdes1_status = %d reg_value = 0x%x err = %d\n",
			 __func__, serdes1_status, value, error);
	if (serdes1_status != (value & SERDES_CABLE_LINK_BITMASK)) {
		serdes1_status = (value & SERDES_CABLE_LINK_BITMASK);

		if (serdes1_status == SERDES_CABLE_LINK_NOT_DETECTED) {
			dev_info(
			    &data->client->dev,
			    "%s: SERDES_CABLE_LINK_NOT_DETECTED TRI-STATE\n",
			    __func__);
		}
		dev_info(&data->client->dev,
			 "%s: serdes1_status changed = %02x\n", __func__,
			 serdes1_status);
		/*
		 *	for epoll wait, notify data changed
		 *	/sys/bus/i2c/drivers/ds90uh940/1-002c/cable_link
		 *  /sys/bus/i2c/drivers/ds90uh940/1-0034/cable_link
		 */
		sysfs_notify(&(data->client->dev.kobj), NULL, "cable_link");
		/*
		 *	for android extcon ExtconUEventObserver
		 *	/sys/class/extcon/extcon1/state
		 */
		extcon_set_state_sync(data->edev, EXTCON_DISP_HUB,
				      serdes1_status);
	}

	schedule_delayed_work(&data->delayed_work_serdes1_status,
			      SERDES_REPORT_DELAY_2);
}


static ssize_t cable_link_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	u32 value;
	ssize_t ret;

	if (kstrtou32(buf, 10, &value))
		ret = -EINVAL;

	ret = count;
	return ret;
}

static ssize_t cable_link_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	// sysfs_notify(&dev->kobj, NULL, "cable_link");
	return scnprintf(buf, PAGE_SIZE, "%d\n", serdes1_status);
}

static DEVICE_ATTR(cable_link, 0644, cable_link_show, cable_link_store);

static struct attribute *ds90uh94x_attrs[] = {
	&dev_attr_lock_status.attr,
	&dev_attr_cable_link.attr,
	NULL
};

static const struct attribute_group ds90uh94x_attr_group = {
	.attrs = ds90uh94x_attrs,
};

static int ds90uh94x_sysfs_init(struct ds90uh94x *data)
{
	struct i2c_client *client = data->client;
	int error = 0;

	error = sysfs_create_group(&client->dev.kobj, &ds90uh94x_attr_group);
	if (error) {
		dev_err(&client->dev, "Failure %d creating sysfs group\n",
			error);
		return error;
	}

	return error;
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
#endif
}

static int ds90uh94x_i2c_probe(struct i2c_client *client,
				      const struct i2c_device_id *id)
{
	struct ds90uh94x *data;
	int error;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C check functionality failed.\n");
		return -EIO;
	}

	data = devm_kzalloc(&client->dev, sizeof(struct ds90uh94x), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	ds90uh94x_parse_dt(&client->dev, data);

	snprintf(data->phys, sizeof(data->phys), "i2c-%u-%04x/ds90uh940",
		 client->adapter->nr, client->addr);

	check_panel_model_name();

	data->client = client;
	data->irq = client->irq;
	i2c_set_clientdata(client, data);

	error = ds90uh94x_initialize(data);
	if (error) {
		dev_err(&client->dev, "ds90uh940_i2c_probe fail!!!\n");
		return error;
	}

#ifdef SERDES_DELAYED_WORK
	error = ds90uh94x_sysfs_init(data);
	if (error)
		return error;

	/* Allocate extcon device */
	data->edev = devm_extcon_dev_allocate(&client->dev, hub_cable);
	if (IS_ERR(data->edev)) {
		dev_err(&client->dev, "failed to allocate memory for extcon\n");
		return -ENOMEM;
	}

	data->edev->name = "HUB";
	data->edev->dev.parent = &client->dev;
	data->edev->supported_cable = hub_cable;
	/* Register extcon device */
	error = devm_extcon_dev_register(&client->dev, data->edev);
	if (error) {
		dev_err(&client->dev, "failed to register extcon device\n");
		return error;
	}

	INIT_DELAYED_WORK(&data->delayed_work_serdes1_status, serdes1_report);
	schedule_delayed_work(&data->delayed_work_serdes1_status,
			      SERDES_REPORT_DELAY_1);

	INIT_DELAYED_WORK(&data->delayed_work_lock_status,
				lock_report);
	schedule_delayed_work(&data->delayed_work_lock_status,
				SERDES_REPORT_DELAY_1);
#endif

	dev_info(&client->dev, "ds90uh940_i2c_probe OK!!!\n");

	return 0;
}

static int ds90uh94x_i2c_remove(struct i2c_client *client)
{
#ifdef SERDES_DELAYED_WORK
	struct ds90uh94x *data = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&data->delayed_work_serdes1_status);
	devm_extcon_dev_unregister(&client->dev, data->edev);

	cancel_delayed_work_sync(&data->delayed_work_lock_status);
#endif

	return 0;
}

static void ds90uh94x_shutdown(struct i2c_client *client)
{
#ifdef SERDES_DELAYED_WORK
	struct ds90uh94x *data = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&data->delayed_work_serdes1_status);
	devm_extcon_dev_unregister(&client->dev, data->edev);

	cancel_delayed_work_sync(&data->delayed_work_lock_status);
#endif
}

static const struct i2c_device_id ds90uh94x_id[] = {
	{ "ds90uh940", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ds90uh94x_id);

#ifdef CONFIG_OF
static const struct of_device_id ds90uh94x_i2c_dt_ids[] = {
	{ .compatible = "ti,ds90uh940", },
	{ }
};
MODULE_DEVICE_TABLE(of, ds90uh94x_i2c_dt_ids);
#endif

static struct i2c_driver ds90uh94x_i2c_driver = {
	.driver = {
		.name	= "ds90uh940",
		//.pm	= &ds90uh94x_pm_ops,
		.of_match_table = of_match_ptr(ds90uh94x_i2c_dt_ids),
	},
	.probe		= ds90uh94x_i2c_probe,
	.remove	= ds90uh94x_i2c_remove,
	.shutdown 	= ds90uh94x_shutdown,
	.id_table	= ds90uh94x_id,
};

module_i2c_driver(ds90uh94x_i2c_driver);

MODULE_AUTHOR("Dennis Hsu <dennis.hsu@jet-opto.com.tw>");
MODULE_DESCRIPTION("TI DS90UH94x 1080p OpenLDI to FPD-Link III serializer driver");
MODULE_LICENSE("GPL v2");
