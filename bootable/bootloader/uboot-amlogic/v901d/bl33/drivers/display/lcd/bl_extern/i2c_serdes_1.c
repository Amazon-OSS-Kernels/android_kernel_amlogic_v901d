/*
 * drivers/display/lcd/bl_extern/i2c_serdes.c
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


#define BL_EXTERN_NAME			"i2c_serdes_1"
#define BL_EXTERN_TYPE			BL_EXTERN_I2C
#define BL_EXTERN_I2C_ADDR		0x1A //7bit address

#define REG_I2C_CONTROL		0x17
#define REG_DES_CAP			0x20
#define REG_BRIDGE_CTL		0x4F

#define DES_948			948
#define DES_928			928

int i2c_serdes_1_probe(int* des_id)
{
	int ret = 0;
	u8 buf[2];
	u8 value = 0;

	BLEX("%s\n", __func__);
	//mdelay(2000);//workaroubd , wait 2000ms mcu initial successfully

	buf[0] = REG_DES_CAP;
	ret = aml_bl_extern_i2c_read(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);
	value = buf[1];
	BLEX("%s: REG_DES_CAP = 0x%x\n", __func__, value);
	if (value == 0xb)//dual link
	{
		*des_id = DES_948;
		buf[0] = REG_BRIDGE_CTL;
		buf[1] = 0x00;//dual pixel
		ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
		if (ret)
			BLEX("%s: %d\n", __func__, ret);

	}
	else if (value == 0x3)//single link
	{
		*des_id = DES_928;
		buf[0] = REG_BRIDGE_CTL;
		buf[1] = 0x40;//single pixel
		ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
		if (ret)
			BLEX("%s: %d\n", __func__, ret);
	}
	else
		*des_id = 0;

	buf[0] = REG_I2C_CONTROL;
	buf[1] = 0x9e;
	ret = aml_bl_extern_i2c_write(LCD_EXT_I2C_BUS_1, BL_EXTERN_I2C_ADDR, buf, 2);
	if (ret)
		BLEX("%s: %d\n", __func__, ret);

	return ret;
}
