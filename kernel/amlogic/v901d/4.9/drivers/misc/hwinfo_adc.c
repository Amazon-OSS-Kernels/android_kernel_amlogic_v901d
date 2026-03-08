/*
 * ADC as HWINFO
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
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/iio/consumer.h>
#include <dt-bindings/iio/adc/amlogic-saradc.h>

#define DRIVE_NAME "hwinfo_adc"
#define MAX_NAME_LEN 20
#define POLL_INTERVAL_DEFAULT 25

struct adc {
	char name[MAX_NAME_LEN];
	unsigned int chan;
	int value; /* voltage/3.3v * 1023 */
	int tolerance;
	struct list_head list;
};

struct hwinfo_adc {
	int count;
	char sys_class_name[MAX_NAME_LEN];
	unsigned char chan[SARADC_CH_NUM];
	unsigned char chan_num;
	struct mutex hwinfo_lock;
	struct class hwinfo_class;
	struct list_head hwinfo_head;
	struct iio_channel *pchan[SARADC_CH_NUM];
};

static ssize_t value_show(struct class *cls, struct class_attribute *attr,
			char *buf)
{
	struct hwinfo_adc *hwinfo = container_of(cls,
					struct hwinfo_adc, hwinfo_class);
	struct adc *adc;
	int value, i;

	mutex_lock(&hwinfo->hwinfo_lock);
	for (i = 0; i < hwinfo->chan_num; i++) {
		if (iio_read_channel_processed(hwinfo->pchan[hwinfo->chan[i]],
				&value) >= 0) {
			pr_err("%s(#%d) value = %d\n", __func__,
					__LINE__,
					value);
			if (value < 0)
				continue;
			list_for_each_entry(adc, &hwinfo->hwinfo_head, list) {
				if ((adc->chan == hwinfo->chan[i])
				&& (value >= adc->value - adc->tolerance)
				&& (value <= adc->value + adc->tolerance)) {
					mutex_unlock(&hwinfo->hwinfo_lock);
					pr_err("%s(#%d) adc->name = %s\n",
						__func__,
						__LINE__,
						adc->name);
					return sprintf(buf, "%d", value);
				}
			}
		}
	}
	mutex_unlock(&hwinfo->hwinfo_lock);
	return 0;
}

static ssize_t name_show(struct class *cls, struct class_attribute *attr,
			char *buf)
{
	struct hwinfo_adc *hwinfo = container_of(cls,
					struct hwinfo_adc, hwinfo_class);
	struct adc *adc;
	int value, i;

	mutex_lock(&hwinfo->hwinfo_lock);
	for (i = 0; i < hwinfo->chan_num; i++) {
		if (iio_read_channel_processed(hwinfo->pchan[hwinfo->chan[i]],
				&value) >= 0) {
			pr_err("%s(#%d) value = %d\n",
			__func__,
			__LINE__,
			value);
			if (value < 0)
				continue;
			list_for_each_entry(adc, &hwinfo->hwinfo_head, list) {
				if ((adc->chan == hwinfo->chan[i])
				&& (value >= adc->value - adc->tolerance)
				&& (value <= adc->value + adc->tolerance)) {
					mutex_unlock(&hwinfo->hwinfo_lock);
					pr_err("%s(#%d) adc->name = %s\n",
					__func__,
					__LINE__,
					adc->name);
					return sprintf(buf, "%s", adc->name);
				}
			}
		}
	}
	mutex_unlock(&hwinfo->hwinfo_lock);
	return 0;
}

static ssize_t table_show(struct class *cls, struct class_attribute *attr,
			char *buf)
{
	struct hwinfo_adc *hwinfo = container_of(cls,
					struct hwinfo_adc, hwinfo_class);
	struct adc *adc;
	unsigned char adc_num = 1;
	int len = 0;

	mutex_lock(&hwinfo->hwinfo_lock);
	list_for_each_entry(adc, &hwinfo->hwinfo_head, list) {
		len += sprintf(buf+len,
			"[%d]: name=%-21s channel=%-3d value=%-5d tolerance=%-5d\n",
			adc_num,
			adc->name,
			adc->chan,
			adc->value,
			adc->tolerance);
		adc_num++;
	}
	mutex_unlock(&hwinfo->hwinfo_lock);

	return len;
}

struct class_attribute hwinfo_adc_attrs[] = {
	__ATTR_RO(table),
	__ATTR_RO(name),
	__ATTR_RO(value),
	__ATTR_NULL
};

static void adc_list_free(struct hwinfo_adc *hwinfo)
{
	struct adc *adc;
	struct adc *adc_tmp;

	mutex_lock(&hwinfo->hwinfo_lock);
	list_for_each_entry_safe(adc, adc_tmp, &hwinfo->hwinfo_head, list) {
		list_del(&adc->list);
		kfree(adc);
	}
	mutex_unlock(&hwinfo->hwinfo_lock);
}

static void adc_get_valid_chan(struct hwinfo_adc *hwinfo)
{
	unsigned char incr;
	struct adc *adc;

	mutex_lock(&hwinfo->hwinfo_lock);
	hwinfo->chan_num = 0; /*recalculate*/
	list_for_each_entry(adc, &hwinfo->hwinfo_head, list) {
		if (hwinfo->chan_num == 0) {
			hwinfo->chan[hwinfo->chan_num++] = adc->chan;
		} else {
			for (incr = 0; incr < hwinfo->chan_num; incr++) {
				if (adc->chan == hwinfo->chan[incr])
					break;
				if (incr == (hwinfo->chan_num - 1))
					hwinfo->chan[hwinfo->chan_num++] =
						adc->chan;
			}
		}
	}
	mutex_unlock(&hwinfo->hwinfo_lock);
}

static int hwinfo_adc_parse_dt(struct platform_device *pdev,
			struct hwinfo_adc *hwinfo)
{
	int ret;
	int count;
	int state = 0;
	unsigned char cnt;
	const char *uname;
	struct adc *adc;
	struct of_phandle_args chanspec;

	pr_err("%s(#%d) hwinfo_adc_parse_dt\n", __func__, __LINE__);
	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "failed to get device node\n");
		return -EINVAL;
	}

	count = of_property_count_strings(pdev->dev.of_node,
		"io-channel-names");
	if (count < 0) {
		dev_err(&pdev->dev, "failed to get io-channel-names");
		return -ENODATA;
	}

	for (cnt = 0; cnt < count; cnt++) {
		ret = of_parse_phandle_with_args(pdev->dev.of_node,
			"io-channels", "#io-channel-cells", cnt, &chanspec);
		if (ret)
			return ret;

		if (!chanspec.args_count)
			return -EINVAL;

		if (chanspec.args[0] >= SARADC_CH_NUM) {
			dev_err(&pdev->dev, "invalid channel index[%u]\n",
					chanspec.args[0]);
			return -EINVAL;
		}

		ret = of_property_read_string_index(pdev->dev.of_node,
				"io-channel-names", cnt, &uname);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid channel name index[%d]\n",
					cnt);
			return -EINVAL;
		}

		hwinfo->pchan[chanspec.args[0]] =
		devm_iio_channel_get(&pdev->dev, uname);
		if (IS_ERR(hwinfo->pchan[chanspec.args[0]]))
			return PTR_ERR(hwinfo->pchan[chanspec.args[0]]);
	}
#if 0
	ret = of_property_read_u32(pdev->dev.of_node, "poll-interval", &value);
	if (ret)
		hwinfo->poll_period = POLL_INTERVAL_DEFAULT;
	else
		hwinfo->poll_period = value;
#endif

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

	for (cnt = 0; cnt < hwinfo->count; cnt++) {
		adc = kzalloc(sizeof(struct adc), GFP_KERNEL);
		if (!adc) {
			dev_err(&pdev->dev, "alloc mem failed!\n");
			return -ENOMEM;
		}

		ret = of_property_read_string_index(pdev->dev.of_node,
			 "hwinfo_name", cnt, &uname);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid hwinfo_name index[%d]\n",
				cnt);
			state = -EINVAL;
			goto err;
		}
		snprintf(adc->name, MAX_NAME_LEN, "%s", uname);

		ret = of_property_read_u32_index(pdev->dev.of_node,
			"hwinfo_chan", cnt, &adc->chan);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid hwinfo_chan index[%d]\n",
				cnt);
			state = -EINVAL;
			goto err;
		}

		if (!hwinfo->pchan[adc->chan]) {
			dev_err(&pdev->dev, "invalid channel[%u], please enable it first by DTS\n",
					adc->chan);
			state = -EINVAL;
			goto err;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node,
			"hwinfo_val", cnt, &adc->value);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid hwinfo_val index[%d]\n",
				cnt);
			state = -EINVAL;
			goto err;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node,
			"hwinfo_tolerance", cnt, &adc->tolerance);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid hwinfo_tolerance index[%d]\n",
				cnt);
			state = -EINVAL;
			goto err;
		}
		list_add_tail(&adc->list, &hwinfo->hwinfo_head);
	}
	adc_get_valid_chan(hwinfo);
	return 0;
err:
	kfree(adc);
	return state;
}

static int hwinfo_adc_probe(struct platform_device *pdev)
{
	struct hwinfo_adc *hwinfo;
	int ret = 0;
	int length;
	char buf[4] = "";
	char *buf_ptr;

	pr_err("%s(#%d) hwinfo_adc_probe\n", __func__, __LINE__);

	hwinfo = kzalloc(sizeof(struct hwinfo_adc), GFP_KERNEL);
	if (!hwinfo)
		return -ENOMEM;

	platform_set_drvdata(pdev, hwinfo);

	mutex_init(&hwinfo->hwinfo_lock);
	INIT_LIST_HEAD(&hwinfo->hwinfo_head);

	ret = hwinfo_adc_parse_dt(pdev, hwinfo);
	if (ret)
		goto err;

	pr_err("%s(#%d)init class\n", __func__, __LINE__);
	/*init class*/
	hwinfo->hwinfo_class.name = hwinfo->sys_class_name;
	hwinfo->hwinfo_class.owner = THIS_MODULE;
	hwinfo->hwinfo_class.class_attrs = hwinfo_adc_attrs;
	ret = class_register(&hwinfo->hwinfo_class);
	if (ret) {
		dev_err(&pdev->dev, "fail to create adc hwinfo class.\n");
		goto err;
	}

	pr_err("%s(#%d) hwinfo_adc_probe OK!!!\n", __func__, __LINE__);

	buf_ptr = buf;
	length = name_show(&hwinfo->hwinfo_class, NULL, buf_ptr);

	return ret;

err:
	adc_list_free(hwinfo);
	kfree(hwinfo);
	return ret;
}

static int hwinfo_adc_remove(struct platform_device *pdev)
{
	struct hwinfo_adc *hwinfo;

	pr_err("%s(#%d) hwinfo_adc_remove\n", __func__, __LINE__);
	hwinfo = platform_get_drvdata(pdev);
	class_unregister(&hwinfo->hwinfo_class);
	adc_list_free(hwinfo);
	kfree(hwinfo);
	return 0;
}

static const struct of_device_id hwinfo_adc_dt_ids[] = {
	{ .compatible = "jet-opto, hwinfo_adc", },
	{ }
};
MODULE_DEVICE_TABLE(of, hwinfo_adc_dt_ids);

static struct platform_driver hwinfo_adc_driver = {
	.driver = {
		.name	= DRIVE_NAME,
		//.pm	= &hwinfo_adc_ops,
		.of_match_table = of_match_ptr(hwinfo_adc_dt_ids),
	},
	.probe		= hwinfo_adc_probe,
	.remove		= hwinfo_adc_remove,
};

module_platform_driver(hwinfo_adc_driver);

MODULE_AUTHOR("Dennis Hsu <dennis.hsu@jet-opto.com.tw>");
MODULE_DESCRIPTION("ADC as HWINFO");
MODULE_LICENSE("GPL");
