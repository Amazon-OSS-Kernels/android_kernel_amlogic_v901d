/*
 * Nuvoton M0516LDN MCU
 *
 * Copyright (C) 2019 Dennis Hsu, Jet-Opto Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef LINUX_I2C_NUVOTON_M0516LDN_H
#define LINUX_I2C_NUVOTON_M0516LDN_H

#include <linux/amlogic/media/vout/lcd/aml_bl.h>

#define M0516LDN_LDROM			"FCAMonitorLDROM.img"
#define M0516LDN_APPROM			"FCAMonitorAPPROM.img"
#define MAX_NAME_LEN				64
#define PACKAGE_WRITE_SIZE			64
#define PACKAGE_WRITE_HEADER		6
#define PACKAGE_BUFF_SIZE	(PACKAGE_WRITE_SIZE + PACKAGE_WRITE_HEADER)
#define REG_VERSION					0x01
#define REG_RD_VERSION				0x02
#define REG_PROJECT_ID				0x03
#define REG_CUSTOMER_ID				0x04
#define REG_MULTIBOOT_INDEX			0x05
#define REG_LDROM_VERSION			0x06
#define REG_LDROM_RD_VERSION		0x07
#define REG_UPDATE_STATUS			0x10
#define REG_UPDATE_INITIAL			0x11
#define REG_UPDATE_TARGET			0x12
#define REG_UPDATE_PACKAGE			0x13
#define REG_UPDATE_FINISH			0x14
#define REG_UPDATE_FAIL_STATUS		0x15
#define REG_BACKLIGHT				0x20
#define REG_AUDIO_MUTE				0x21
#define REG_EAR_PHONE_EXIT			0x22
#define REG_USB_CHARGE				0x23
#define REG_DS90UH949_POWER_DOWN	0x24
#define REG_BACKLIGHT_POWER			0x25
#define REG_DS90UH948_POWER_DOWN	0x26
#define REG_LED_PERCENTAGE			0x27
#define REG_PCB_TEMPERATURE			0x28
#define REG_PANEL_TEMPERATURE		0x29
#define REG_UT60_RESET				0x2A
#define REG_FACTORY_MODE			0x50
#define REG_DIAGNOSTIC_STATUS		0x70
#define REG_DIAGNOSTIC_DETECT		0x71

struct imgheader {
	int version;
	int rd_version;
	int project_id;
	int customer_id;
	int rom;
	unsigned int bin_size;
	unsigned int bin_crc32;
	unsigned int reserved;
};

enum update_status {
	STATUS_IDLE = 0x00,
	STATUS_WAIT_PACKAGE = 0x01,
	STATUS_WRITE_PACKAGE = 0x02,
	STATUS_SEND_LAST_PACKAGE_AGAIN = 0x03,
	STATUS_SEND_NEXT_PACKAGE = 0x04,
	STATUS_UPDATE_FINISH = 0x05,
	STATUS_UNKNOWN_FAIL = 0x06,
	STATUS_PACKAGE_INDEX_FAIL = 0x07,
	STATUS_PACKAGE_QTY_FAIL = 0x08,
	STATUS_PACKAGE_CHECKSUM_FAIL = 0x09,
	STATUS_WRITE_DATA_FAIL = 0x0A,	//write data to flash fail
	STATUS_WRITE_SIZE_FAIL = 0x0B,	//write size over the range
	STATUS_FAIL_TARGET = 0x0C,		//update target index fail
	STATUS_FAIL_DATA_VERIFY = 0x0D,	//update target data verify fail
	STATUS_MAX = 0xFF,
};

enum multiboot_rom {
	MULTIBOOT_LDROM = 0x00,
	MULTIBOOT_AP1ROM = 0x01,
	MULTIBOOT_AP2ROM = 0x02,
};

enum update_rom {
	UPDATE_LDROM = 0x01,
	UPDATE_APPROM = 0x02,
};

enum diag_err_status {
	STATUS_SYSTEM_ERR			= 0x00000001,//bit0,1
	STATUS_BACKLIGHT_ERR		= 0x00000004,//bit2,3
	STATUS_DS90UH949_ERR		= 0x00000010,//bit4,5
	STATUS_IR_TRANSMIT_ERR		= 0x00000040,//bit6,7
	STATUS_TFT_LCD_ERR			= 0x00000100,//bit8,9
	STATUS_I2C_SLAVE_ERR		= 0x00000400,//bit10,11
	STATUS_LED_LIGHTING_ERR		= 0x00001000,//bit12,13
	STATUS_ADC_PANEL_ERR		= 0x00004000,//bit14,15
	STATUS_ADC_PCB_ERR			= 0x00010000,//bit16,17
	STATUS_USB_CHARGE_ERR		= 0x00040000,//bit18,19
	STATUS_RESERVERD1_ERR		= 0x00100000,//bit20,21
	STATUS_RESERVERD2_ERR		= 0x00400000,//bit22,23
	STATUS_RESERVERD3_ERR		= 0x01000000,//bit24,25
	STATUS_RESERVERD4_ERR		= 0x04000000,//bit26,27
	STATUS_RESERVERD5_ERR		= 0x10000000,//bit28,29
	STATUS_RESERVERD6_ERR		= 0x40000000,//bit30,31
	STATUS_DIAG_MAX				= 0xFFFFFFFF,
};

struct m0516ldn {
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
extern void aml_bl_hotplug_set_m0516ldn(struct bl_hotplug_i2cdev *data);
#endif

#endif /* LINUX_I2C_NUVOTON_M0516LDN_H */
