/*
 * drivers/display/lcd/bl_extern/i2c_m0516ldn.c
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


#define BL_EXTERN_NAME			"i2c_m0516ldn"
#define BL_EXTERN_TYPE			BL_EXTERN_I2C
#define BL_EXTERN_I2C_ADDR		0x66 //7bit address

#define REG_BACKLIGHT				0x20
#define REG_BACKLIGHT_POWER			0x25

int i2c_m0516ldn_power_on(void)
{
	int ret;
	u8 buf[2];

	BLEX("%s\n", __func__);

	buf[0] = REG_BACKLIGHT_POWER;
	buf[1] = 0x1;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);

	return ret;
}

int i2c_m0516ldn_power_off(void)
{
	int ret = 0;
	u8 buf[2];

	BLEX("%s\n", __func__);

	buf[0] = REG_BACKLIGHT_POWER;
	buf[1] = 0x0;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);

	return ret;
}

int i2c_m0516ldn_set_level(unsigned int level)
{
	u8 buf[3];
	int ret = 0;

	//make sure the brightness level is in range, force it to 100
	//if it's not in range
	if ((level< 0 ) || (level > 255))
		level =100;
	buf[0] = REG_BACKLIGHT;
	level = level << 8; //16bit  0100 ~ ff00 the same 256 level
	buf[1] = level & 0xff;
	buf[2] = (level >> 8) & 0xff;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 3);

	return ret;
}

static int i2c_m0516ldn_update(void)
{
	struct aml_bl_extern_driver_s *bl_extern = aml_bl_extern_get_driver();

	if (bl_extern == NULL) {
		BLEXERR("%s driver is null\n", BL_EXTERN_NAME);
		return -1;
	}

	bl_extern->device_power_on = i2c_m0516ldn_power_on;
	bl_extern->device_power_off = i2c_m0516ldn_power_off;
	bl_extern->device_bri_update = i2c_m0516ldn_set_level;

	return 0;
}

int i2c_m0516ldn_probe(void)
{
	int ret = 0;
	ret = i2c_m0516ldn_update();
	if (lcd_debug_print_flag)
		BLEX("%s: %d\n", __func__, ret);

	return ret;
}

