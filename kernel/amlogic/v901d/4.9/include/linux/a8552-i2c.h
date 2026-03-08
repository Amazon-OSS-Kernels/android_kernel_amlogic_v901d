/*
 * A8852
 *
 * Copyright (C) 2020 Dennis Hsu, Jet-Opto Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef LINUX_I2C_A8552_H
#define LINUX_I2C_A8552_H

#include <linux/amlogic/media/vout/lcd/aml_bl.h>

#define REG_LED_ENABLE_M						0x0
#define REG_LED_ENABLE_L						0x1
#define REG_LED_PWM_PERIOD_M					0x2
#define REG_LED_PWM_PERIOD_L					0x3
#define REG_OVP_TH						0x4
#define REG_BOOST_THERMAL_CFG					0x5
#define REG_FAULT_MODE_M						0x6
#define REG_FAULT_MODE_L						0x7
#define REG_LED1_TON_M							0x10
#define REG_LED1_TON_L							0x11
#define REG_LED2_TON_M							0x12
#define REG_LED2_TON_L							0x13
#define REG_LED3_TON_M							0x14
#define REG_LED3_TON_L							0x15
#define REG_LED4_TON_M							0x16
#define REG_LED4_TON_L							0x17
#define REG_LED5_TON_M							0x18
#define REG_LED5_TON_L							0x19
#define REG_LED6_TON_M							0x1a
#define REG_LED6_TON_L							0x1b
#define REG_LED7_TON_M							0x1c
#define REG_LED7_TON_L							0x1d
#define REG_LED8_TON_M							0x1e
#define REG_LED8_TON_L							0x1f
#define REG_LOAD_PWM_TON_UPDATE					0x24
#define REG_FAULT_STAT_L						0x30
#define REG_FAULT_STAT_M						0x31
#define REG_FAULTHST_A							0x38
#define REG_FAULTHST_B							0x39

struct a8552 {
	struct i2c_client *client;
	struct device		*dev;
	unsigned int		irq;
	char			phys[32];
#ifdef CONFIG_AMLOGIC_BL_HOTPLUG
	u16	brightness;
	int (*power_on)(struct bl_hotplug_i2cdev *data);
	int (*power_off)(struct bl_hotplug_i2cdev *data);
	int (*set_level)(struct bl_hotplug_i2cdev *data, u16 level);
#endif
};

#ifdef CONFIG_AMLOGIC_BL_HOTPLUG
extern void aml_bl_hotplug_set_a8552(struct bl_hotplug_i2cdev *data);
#endif

#endif /* LINUX_I2C_A8552_H */
