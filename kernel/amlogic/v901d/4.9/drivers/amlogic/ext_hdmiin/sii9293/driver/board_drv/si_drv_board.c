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

#include "si_drv_board.h"
#include "si_drv_cra_cfg.h"

#if (FPGA_BUILD_NEW == 1)
// # define
#else
#if !defined(__KERNEL__)
#include "si_starterkit.h"
#endif
#include "si_cs4384_registers.h"
#endif

bool bSpdif;
bool bTdm;
bool bExternalTxHdmi;

/*****************************************************************************/
/**
 *  @brief		Board Initialization
 *
 *  @return	Status
 *  @retval	true		Success
 *  @retval	false		Failure
 *
 *****************************************************************************/
bool SiiDrvBoardInit(void)
{
	bool success = true;

	// Starter Kit Board
	bool gpio_sw3_1 = false;
	bool gpio_sw3_2 = false;
	bool gpio_sw3_3 = false;
	bool gpio_sw3_4 = false;

	gpio_sw3_1 = !SiiGpioRead(GPIO_SW3_1);
	gpio_sw3_2 = !SiiGpioRead(GPIO_SW3_2);
	gpio_sw3_3 = !SiiGpioRead(GPIO_SW3_3);
	gpio_sw3_4 = !SiiGpioRead(GPIO_SW3_4);

	DEBUG_PRINT(MSG_ALWAYS, "GPIO SW3 BIT 1 (TDM-ON / I2S-OFF): %s\n",
		    gpio_sw3_1 ? "On" : "Off");
	DEBUG_PRINT(MSG_ALWAYS, "GPIO SW3 BIT 2 (S/PDIF): %s\n",
		    gpio_sw3_2 ? "On" : "Off");
	DEBUG_PRINT(MSG_ALWAYS, "GPIO SW3 BIT 3: %s\n",
		    gpio_sw3_3 ? "On" : "Off");
	DEBUG_PRINT(MSG_ALWAYS, "GPIO SW3 BIT 4: %s\n",
		    gpio_sw3_4 ? "On" : "Off");

	bSpdif = gpio_sw3_2;
	bTdm = gpio_sw3_1;

#if 0
	if (bSpdif) {
		// Pass through, no codec used, do nothing
	} else if (bTdm) {
		// Use codec for TDM 8 channels
		SiiRegWrite(CS_4384_CTRL1, 0x80);
		SiiRegWrite(CS_4384_CTRL2, 0xc3);
		SiiRegWrite(CS_4384_CTRL3, 0x00);
	} else {
		// Use codec for I2S 2 channels
		SiiRegWrite(CS_4384_CTRL1, 0x80);
		SiiRegWrite(CS_4384_CTRL2, 0x13);
		SiiRegWrite(CS_4384_CTRL3, 0x00);
	}
#endif

	return success;
}

#if defined(__KERNEL__)
// set On/Off for one GPIO in ioexpander.
static void ControlIOExpander(uint8_t IOIndex, bool bOn)
{
	uint16_t IOValue = 0;

	CraReadIOExpanderI2c(DEV_I2C_0, IO_EXPANDER_ADDR, (uint8_t *)&IOValue,
			     2);

	if (bOn)
		IOValue &= ~(1 << IOIndex);
	else
		IOValue |= (1 << IOIndex);

	CraWriteIOExpanderI2c(DEV_I2C_0, IO_EXPANDER_ADDR, (uint8_t *)&IOValue,
			      2);
}
#endif

/*****************************************************************************/
/**
 *  @brief		Board LED Control
 *  @param[in]      ledIndex	index to LED to be controlled
 *  @param[in]      bOn		true to turn on LED; false to turn off
 *
 *****************************************************************************/
void SiiLedControl(uint8_t ledIndex, bool bOn)
{
#if defined(__KERNEL__)
	ControlIOExpander(ledIndex, bOn);
#else
	SkPlatformStarterKitLedControl(ledIndex, bOn);
#endif
}

/*****************************************************************************/
/**
 *  @brief		Board GPIO Control
 *  @param[in]      gpioIndex		index to GPIO to be controlled
 *  @param[in]      bOn			true to turn on; false to turn off
 *
 *****************************************************************************/
void SiiGpioControl(uint8_t gpioIndex, bool bOn)
{
#if !defined(__KERNEL__)
	SkPlatformStarterKitGpioControl(gpioIndex, bOn);
#else
	HalGpioSetPin(gpioIndex, bOn);
#endif
}

/*****************************************************************************/
/**
 *  @brief		Board GPIO Read
 *  @param[in]      gpioIndex		index to GPIO to be read
 *
 *  @return	Status
 *  @retval	true		On
 *  @retval	false		Off
 *
 *****************************************************************************/
bool SiiGpioRead(uint8_t gpioIndex)
{
#if !defined(__KERNEL__)
	return SkPlatformStarterKitGpioRead(gpioIndex);
#else
	return true;
#endif
}

/*****************************************************************************/
/**
 *  @brief		Board SPDIF Switch Status
 *
 *  @return	Status
 *  @retval	true		On
 *  @retval	false		Off
 *
 *****************************************************************************/
bool SiiSpdifEnableGet(void) { return bSpdif; }

/*****************************************************************************/
/**
 *  @brief		Board TDM Switch Status
 *
 *  @return	Status
 *  @retval	true		On
 *  @retval	false		Off
 *
 *****************************************************************************/
bool SiiTdmEnableGet(void) { return bTdm; }
