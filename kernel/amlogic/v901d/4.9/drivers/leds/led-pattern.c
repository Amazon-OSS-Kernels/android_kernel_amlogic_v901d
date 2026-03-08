/*
 * led pattern driver for Jane
 *
 * Copyright (C) 2017 Technologies inc.
 * Jian Hu <jian.hu@amlogic.com>
 * Copyright (C) 2017 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/leds.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/leds_pwm.h>
#include <linux/slab.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/workqueue.h>

unsigned int pattern_val;
unsigned int led_scpi_switch;

#define LED_ON_BRIGHTNESS 255
//static DEFINE_SPINLOCK(pattern_lock);

enum breathe_state {
	BREATHE_STATE_OFF = 0,
	BREATHE_STATE_UP,
	BREATHE_STATE_PAUSE,
	BREATHE_STATE_DOWN,
	BREATHE_STATE_ON,
};

struct trigger_breathe_data {
	enum breathe_state state;
	struct timer_list timer;
	bool			activated;
};

void led_breathe_function(unsigned long data)
{
	struct led_classdev *ldev = (struct led_classdev *) data;
	struct trigger_breathe_data *breathe_data = ldev->trigger_data;

	int brightness = LED_OFF;
	unsigned long delay = 0;

	/* @todo breathe led setting */
	switch (breathe_data->state) {
	case BREATHE_STATE_OFF:
		delay = msecs_to_jiffies(30);

		brightness = LED_OFF;
		breathe_data->state = BREATHE_STATE_UP;
		break;

	case BREATHE_STATE_UP:
		delay = msecs_to_jiffies(45);

		brightness = ldev->brightness + 5;
		if (brightness > ldev->max_brightness) {
			brightness =  ldev->max_brightness;
			breathe_data->state = BREATHE_STATE_PAUSE;
		}
		break;

	case BREATHE_STATE_PAUSE:
		delay = msecs_to_jiffies(20);

		brightness = ldev->max_brightness;
		breathe_data->state = BREATHE_STATE_DOWN;
		break;

	case BREATHE_STATE_DOWN:
		delay = msecs_to_jiffies(31);

		brightness = ldev->brightness - 5;
		if (brightness < 0) {
			brightness =  LED_OFF;
			breathe_data->state = BREATHE_STATE_OFF;
		}
		break;

	default:
		delay = 0;
		brightness = LED_OFF;
		break;

	}

	led_set_brightness(ldev, brightness);
	mod_timer(&breathe_data->timer, jiffies + delay);
}

void led_breathe(struct led_classdev *led_cdev)
{
	struct trigger_breathe_data *breathe_data;

	breathe_data = led_cdev->trigger_data;
	setup_timer(&breathe_data->timer,
		led_breathe_function, (unsigned long)led_cdev);

	/* @todo init parameter */
	breathe_data->state = BREATHE_STATE_OFF;

	led_breathe_function(breathe_data->timer.data);
	breathe_data->activated = true;
}

/*pattern = 1, led on*/
int red_solid(struct led_classdev *led_cdev)
{
	led_set_brightness(led_cdev, LED_ON_BRIGHTNESS);

	return 0;
}

/*pattern = 2, led off*/
int red_off(struct led_classdev *led_cdev)
{
	led_cdev->blink_delay_on = 0;
	led_cdev->blink_delay_off = 0;
	led_set_brightness(led_cdev, LED_OFF);

	return 0;
}

/*pattern = 3, led breathe*/
int red_breathe(struct led_classdev *led_cdev)
{
	led_breathe(led_cdev);

	return 0;
}

/*pattern = 4, led blink*/
int red_blink(struct led_classdev *led_cdev)
{
	unsigned long delay_on = 500;
	unsigned long delay_off = 500;

	led_cdev->brightness = LED_ON_BRIGHTNESS;
	led_blink_set(led_cdev, &delay_on, &delay_off);

	return 0;
}

/*pattern = 5, led blink once*/
int red_single_blink_on(struct led_classdev *led_cdev)
{
	unsigned long delay_on = 500;
	unsigned long delay_off = 500;

	led_cdev->brightness = 0;
	led_blink_set_oneshot(led_cdev, &delay_on, &delay_off, 0);

	return 0;
}

static void led_set_twenty_percent(struct work_struct *work)
{
	struct led_pwm_data *ldata = container_of(to_delayed_work(work),
			struct led_pwm_data, led_work);
	struct led_classdev *led_cdev = &ldata->cdev;

	led_set_brightness(led_cdev, 51);/* 20 percent brightness */
}

/*pattern = 6, led blink once and set 20 percent brightness*/
int red_blink_once_20_percent(struct led_classdev *led_cdev)
{
	unsigned long delay_on = 500;
	unsigned long delay_off = 500;
	struct led_pwm_data *ldata = container_of(led_cdev,
			struct led_pwm_data, cdev);

	led_cdev->brightness = 0;
	led_blink_set_oneshot(led_cdev, &delay_on, &delay_off, 0);
	schedule_delayed_work(&ldata->led_work, msecs_to_jiffies(1100));

	return 0;
}

static void red_breathe_off(struct led_classdev *led_cdev)
{
	struct trigger_breathe_data *breathe_data = led_cdev->trigger_data;

	if (breathe_data->activated) {
		del_timer_sync(&breathe_data->timer);
		breathe_data->activated = false;
	}
}

static ssize_t led_pattern_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", pattern_val);
}

static ssize_t led_pattern_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	int val, res;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	//unsigned long flags;
	struct led_pwm_data *ldata = container_of(led_cdev,
			struct led_pwm_data, cdev);

	 res = sscanf(buf, "%d", &val);
	if (res != 1) {
		dev_err(dev, "Can't parse parameter\n");
		return -EINVAL;
	}

	//spin_lock_irqsave(&pattern_lock, flags);

	red_breathe_off(led_cdev);
	cancel_delayed_work_sync(&ldata->led_work);
	switch (val) {
	case 1:
		red_solid(led_cdev);
		pattern_val = 1;
		break;
	case 2:
		red_off(led_cdev);
		pattern_val = 2;
		break;
	case 3:
		red_breathe(led_cdev);
		pattern_val = 3;
		break;
	case 4:
		red_blink(led_cdev);
		pattern_val = 4;
		break;
	case 5:
		red_single_blink_on(led_cdev);
		pattern_val = 5;
		break;
	case 6:
		red_blink_once_20_percent(led_cdev);
		pattern_val = 6;
		break;
	default:
		dev_err(dev, "unknown led pattern,only support 1-5 patterns\n");
		break;
	}
	//spin_unlock_irqrestore(&pattern_lock, flags);

	return size;
}

static DEVICE_ATTR(led_pattern, 0644,
				 led_pattern_show,
				 led_pattern_store);

static ssize_t led_scpi_switch_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", led_scpi_switch);
}

static ssize_t led_scpi_switch_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	 int val, res;

	 res = sscanf(buf, "%d", &val);
	if (res != 1) {
		dev_err(dev, "Can't parse parameter\n");
		return -EINVAL;
	}

	scpi_set_led_pattern(val);

	return size;
}
static DEVICE_ATTR(led_scpi, 0644,
				 led_scpi_switch_show,
				 led_scpi_switch_store);

void led_pattern_init(struct led_classdev *led_cdev)
{
	int rc;
	struct trigger_breathe_data *breathe_data;
	struct led_pwm_data *ldata = container_of(led_cdev,
			struct led_pwm_data, cdev);
	breathe_data = kzalloc(sizeof(*breathe_data), GFP_KERNEL);
	if (!breathe_data)
		return;
	led_cdev->trigger_data = breathe_data;

	rc = device_create_file(led_cdev->dev, &dev_attr_led_pattern);
	rc = device_create_file(led_cdev->dev, &dev_attr_led_scpi);
	if (rc)
		goto err_out_led_pattern;

	INIT_DELAYED_WORK(&ldata->led_work, led_set_twenty_percent);
	pattern_val = 0;

	return;
err_out_led_pattern:
	device_remove_file(led_cdev->dev, &dev_attr_led_pattern);
}

void led_pattern_exit(struct led_classdev *led_cdev)
{
	struct led_pwm_data *ldata = container_of(led_cdev,
			struct led_pwm_data, cdev);

	cancel_delayed_work_sync(&ldata->led_work);
	device_remove_file(led_cdev->dev, &dev_attr_led_pattern);
	/* Stop blinking */
	led_set_brightness(led_cdev, LED_OFF);

	kfree(led_cdev->trigger_data);
}
