/*
 * drivers/display/lcd/bl_extern/i2c_a8552.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <malloc.h>
#include <asm/arch/gpio.h>
#ifdef CONFIG_OF_LIBFDT
#include <libfdt.h>
#endif
#include <amlogic/aml_lcd.h>
#include <amlogic/aml_bl_extern.h>
#include "bl_extern.h"
#include "../aml_lcd_common.h"
#include "../aml_lcd_reg.h"


#define BL_EXTERN_NAME			"i2c_a8552"
#define BL_EXTERN_TYPE			BL_EXTERN_I2C
#define BL_EXTERN_I2C_ADDR		0x40 //7bit address
#define BL_EXTERN_I2C_ADDR_928	0x36 //7bit address


//928 register
#define REG_DES_928_GPIO0_CONFIG		0x1d
#define REG_DES_928_GPIO7_8_CONFIG		0x21

#define REG_LED_ENABLE_M						0x0
#define REG_LED_ENABLE_L						0x1
#define REG_LED_PWM_PERIOD_M					0x2
#define REG_LED_PWM_PERIOD_L					0x3
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

int i2c_a8552_power_on(void)
{
	int ret;
	u8 buf[2];

	BLEX("%s\n", __func__);

	buf[0] = REG_LED_ENABLE_M;
	buf[1] = 0x0;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LED_ENABLE_L;
	buf[1] = 0xDB;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	return ret;
}

int i2c_a8552_power_off(void)
{
	int ret = 0;
	u8 buf[2];

	BLEX("%s\n", __func__);

	buf[0] = REG_LED_ENABLE_M;
	buf[1] = 0x0;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LED_ENABLE_L;
	buf[1] = 0x00;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	return ret;
}

int i2c_a8552_set_level(unsigned int level)
{
	u8 buf[2];
	int ret = 0;
	u8 value = 0; //LED_3, LED_6 always 0
	u8 value_M = 0;
	u8 value_L = 0;

	//make sure the brightness is in range, force it to
	//100 if not in range
	if ((level < 0) || (level > 255))
		level = 100;

	level = level << 8; //16bit  0100 ~ ff00 the same 256 level
	value_L = level & 0xff;
	value_M = (level >> 8) & 0xff;

	buf[0] = REG_LED1_TON_M;
	buf[1] = value_M;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LED1_TON_L;
	buf[1] = value_L;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LED2_TON_M;
	buf[1] = value_M;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LED2_TON_L;
	buf[1] = value_L;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LED3_TON_M;
	buf[1] = value;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LED3_TON_L;
	buf[1] = value;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LED4_TON_M;
	buf[1] = value_M;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LED4_TON_L;
	buf[1] = value_L;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);
	buf[0] = REG_LED5_TON_M;
	buf[1] = value_M;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LED5_TON_L;
	buf[1] = value_L;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LED6_TON_M;
	buf[1] = value;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LED6_TON_L;
	buf[1] = value;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LED7_TON_M;
	buf[1] = value_M;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LED7_TON_L;
	buf[1] = value_L;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LED8_TON_M;
	buf[1] = value_M;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LED8_TON_L;
	buf[1] = value_L;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	buf[0] = REG_LOAD_PWM_TON_UPDATE;
	buf[1] = 0x1;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	return ret;
}

static int i2c_a8552_update(void)
{
	u8 buf[2];
	int ret = 0;
	struct aml_bl_extern_driver_s *bl_extern = aml_bl_extern_get_driver();

	if (bl_extern == NULL) {
		BLEXERR("%s driver is null\n", BL_EXTERN_NAME);
		return -1;
	}

	bl_extern->device_power_on = i2c_a8552_power_on;
	bl_extern->device_power_off = i2c_a8552_power_off;
	bl_extern->device_bri_update = i2c_a8552_set_level;

	//config i2c_slave 0x36 928 gpio0 output high
	buf[0] = REG_DES_928_GPIO0_CONFIG;
	buf[1] = 0x29;//GPIO_0
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR_928, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);
	BLEX("%s: 928 REG_DES_928_GPIO0_CONFIG = 0x%x\n", __func__, buf[1]);

	//config i2c_slave 0x36 928 gpio7 output high
	buf[0] = REG_DES_928_GPIO7_8_CONFIG;
	buf[1] = 0x9;//GPIO_7
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR_928, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);
	BLEX("%s: 928 REG_DES_928_GPIO7_8_CONFIG = 0x%x\n", __func__, buf[1]);

	return 0;
}

int i2c_a8552_probe(void)
{
	int ret = 0;
	ret = i2c_a8552_update();
	if (lcd_debug_print_flag)
		BLEX("%s: %d\n", __func__, ret);

	return ret;
}
