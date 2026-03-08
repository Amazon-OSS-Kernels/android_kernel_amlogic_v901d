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

#define SII_HAL_LINUX_INIT_C

/***** #include statements ***************************************************/
#include <linux/i2c.h>
#include "sii_hal.h"
#include "sii_hal_priv.h"

/***** local macro definitions ***********************************************/

/***** local type definitions ************************************************/

/***** local variable declarations *******************************************/

/***** local function prototypes *********************************************/

/***** global variable declarations *******************************************/

bool gHalInitedFlag;

/* @brief table used to hold device names of supported MHL devices */
struct i2c_device_id gMhlI2cIdTable[2];

/* @brief Semaphore used to prevent driver access from user mode from
 * colliding with the threaded interrupt handler
 */
// DECLARE_MUTEX(gIsrLock);
DEFINE_SEMAPHORE(gIsrLock); // 2.6.39 use this style ,tiger qin
struct mhlDeviceContext_t gMhlDevice;

/***** local functions *******************************************************/

/***** public functions ******************************************************/

/*****************************************************************************/
/**
 *  @brief Check if Hal has been properly initialized.
 *
 *****************************************************************************/
uint32_t HalInitCheck(void)
{
	if (!(gHalInitedFlag)) {
		SII_DEBUG_PRINT(MSG_ERR,
				"Error: Hal layer not currently initialize!\n");
		return HAL_RET_NOT_INITIALIZED;
	}

	return HAL_RET_SUCCESS;
}

/*****************************************************************************/
/**
 * @brief Initialize the HAL layer module.
 *
 *****************************************************************************/
uint32_t HalInit(void)
{
	uint32_t status;

	if (gHalInitedFlag) {
		SII_DEBUG_PRINT(MSG_ERR, "Error: Hal layer already inited!\n");
		return HAL_RET_ALREADY_INITIALIZED;
	}

	gMhlDevice.driver.driver.name = NULL;
	gMhlDevice.driver.id_table = NULL;
	gMhlDevice.driver.probe = NULL;
	gMhlDevice.driver.remove = NULL;

	gMhlDevice.pI2cClient = NULL;

	gMhlDevice.irqHandler = NULL;
	//	init_MUTEX(&gIsrLock);

	//	HalTimerInit();

	status = HalGpioInit();
	if (status != HAL_RET_SUCCESS)
		return status;

	gHalInitedFlag = true;

	return HAL_RET_SUCCESS;
}

/*****************************************************************************/
/**
 * @brief Terminate access to the hardware abstraction layer.
 *
 *****************************************************************************/
uint32_t HalTerm(void)
{
	uint32_t retStatus;

	retStatus = HalInitCheck();
	if (retStatus != HAL_RET_SUCCESS)
		return retStatus;

	//	HalTimerTerm();
	HalGpioTerm();

	gHalInitedFlag = false;

	return retStatus;
}

/*****************************************************************************/
/**
 * @brief Acquire the lock that prevents races with the interrupt handler.
 *
 *****************************************************************************/
uint32_t HalAcquireIsrLock(void)
{
	uint32_t retStatus;
	int status;

	retStatus = HalInitCheck();
	if (retStatus != HAL_RET_SUCCESS)
		return retStatus;

	status = down_interruptible(&gIsrLock);
	if (status != 0) {
		SII_DEBUG_PRINT(MSG_ERR,
				"HalAcquireIsrLock failed to acquire lock\n");
		return HAL_RET_FAILURE;
	}

	return HAL_RET_SUCCESS;
}

/*****************************************************************************/
/**
 * @brief Release the lock that prevents races with the interrupt handler.
 *
 *****************************************************************************/
uint32_t HalReleaseIsrLock(void)
{
	uint32_t retStatus;

	retStatus = HalInitCheck();
	if (retStatus != HAL_RET_SUCCESS)
		return retStatus;

	up(&gIsrLock);

	return retStatus;
}
