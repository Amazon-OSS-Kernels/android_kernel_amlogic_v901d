/*
 * drivers/amlogic/usb_power/usb_power.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of.h>

#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include "../../gpio/gpiolib.h"


#define OWNER_NAME  "usb_power"


#define USB_POWER_ERR(fmt, args...)	\
	pr_err("usb power: " fmt, ##args)

#define USB_POWER_DBG(fmt, args...)	\
	pr_debug("usb power: " fmt, ##args)

static unsigned int usb_power_state;
static int   gpio_power_pin;


//usb_power=on --------- pull up
//usb_power=off --------- pull down
static int __init usb_power_setup(char *str)
{
	if (str == NULL)
		return -EINVAL;

	usb_power_state = 0;
	USB_POWER_ERR("cmdline usb_power: %s\n", str);
	usb_power_state = (strcmp(str, "on") == 0);
	return 1;
}

__setup("usb_power=", usb_power_setup);




static int amlogic_usb_power_probe(struct platform_device *pdev)
{
	int ret   = -1;
	const char *value = NULL;
	struct gpio_desc *gpio_desc = NULL;

	// Check command line
	if (!usb_power_state) {
		USB_POWER_ERR("don't control USB POWER.\n");
		USB_POWER_ERR("nothing to do\n");
		return 0;
	}
	gpio_power_pin = 0;
	ret = of_property_read_string(pdev->dev.of_node, "power_pin", &value);
	if ((ret) || (value == NULL)) {
		USB_POWER_ERR("no power_pin\n");
		return 0;
	}

	gpio_desc = of_get_named_gpiod_flags(pdev->dev.of_node,
						"power_pin", 0, NULL);
	if (gpio_desc == NULL) {
		USB_POWER_ERR("can't get gpio desc\n");
		return 0;
	}
	gpio_power_pin = desc_to_gpio(gpio_desc);

	USB_POWER_ERR("gpio_power_pin = %d\n", gpio_power_pin);
	if (!gpio_power_pin) {
		USB_POWER_ERR("gpio power pin invaild\n");
		return 0;
	}
	ret = gpio_request(gpio_power_pin, OWNER_NAME);
	if (ret) {
		USB_POWER_ERR("gpio power pin request failed(%d)\n", ret);
		return 0;
	}
	USB_POWER_ERR("gpio power pin pull up\n");
	ret = gpio_direction_output(gpio_power_pin, 1);
	USB_POWER_ERR("set usb power %s.\n", (ret) ? "failed" : "ok");

	return ret;
}

static int amlogic_usb_power_remove(struct platform_device *pdev)
{
	if (!usb_power_state) {
		USB_POWER_ERR("don't control USB POWER.\n");
		USB_POWER_ERR("nothing to do\n");
		return 0;
	}

	if (gpio_power_pin)
		gpio_free(gpio_power_pin);
	return 0;
}

static const struct of_device_id amlogic_usb_power_match[] = {
	{ .compatible = "amlogic, tm2-usb_power", },
	{}
};

static struct platform_driver amlogic_usb_power_driver = {
	.driver = {
		.name = "amlogic, tm2-usb_power",
		.of_match_table = amlogic_usb_power_match,
	},
	.probe  = amlogic_usb_power_probe,
	.remove = amlogic_usb_power_remove,
};

static int __init amlogic_usb_power_init(void)
{
	return platform_driver_register(&amlogic_usb_power_driver);
}

arch_initcall_sync(amlogic_usb_power_init);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("AMLOGIC_USB_POWER");
MODULE_LICENSE("GPL");
