/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#ifndef SII_DRV_BOARD_H
#define SII_DRV_BOARD_H

#include "si_common.h"

#if !defined(__KERNEL__)
#define GPIO_RST GPIO_ID_P0_1
#endif
#define GPIO_AUD_RST GPIO_ID_P0_2
#define GPIO_SW3_5 GPIO_ID_P0_4
#define GPIO_SW3_6 GPIO_ID_P0_5
#define GPIO_SW3_7 GPIO_ID_P0_6
#define GPIO_SW3_8 GPIO_ID_P0_7

#define GPIO_SW3_1 GPIO_SW3_8
#define GPIO_SW3_2 GPIO_SW3_7
#define GPIO_SW3_3 GPIO_SW3_6
#define GPIO_SW3_4 GPIO_SW3_5

#if defined(__KERNEL__)
#define LED_ID_2 (10) // MHL cable indicator
#define LED_ID_3 (11) // HDMI cable indicator
#else
enum {
	LED_ID_GREEN = 0x00,
	LED_ID_AMBER = 0x01,
	LED_ID_2 = 0x02,
	LED_ID_3 = 0x03,
};
#endif

enum {
	GPIO_ID_P0_0 = 0x00,
	GPIO_ID_P0_1,
	GPIO_ID_P0_2,
	GPIO_ID_P0_3,
	GPIO_ID_P0_4,
	GPIO_ID_P0_5,
	GPIO_ID_P0_6,
	GPIO_ID_P0_7,

	GPIO_ID_P2_0,
	GPIO_ID_P2_1,
	GPIO_ID_P2_2,
	GPIO_ID_P2_3,
	GPIO_ID_P2_4,
	GPIO_ID_P2_5,
	GPIO_ID_P2_6,
	GPIO_ID_P2_7,
};

#if defined(__KERNEL__)
#define IO_EXPANDER_ADDR (0x40)
#endif

bool SiiDrvBoardInit(void);

void SiiLedControl(uint8_t ledIndex, bool bOn);

void SiiGpioControl(uint8_t gpioIndex, bool bOn);

bool SiiGpioRead(uint8_t gpioIndex);

bool SiiSpdifEnableGet(void);

bool SiiTdmEnableGet(void);

#endif // SII_DRV_BOARD_H
