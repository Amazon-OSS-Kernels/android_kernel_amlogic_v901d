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

#if !defined(SII_HAL_H)
#define SII_HAL_H

#include <linux/kernel.h>
#include "osal/include/osal.h"
#include "si_osdebug.h"

#ifdef __cplusplus
extern "C" {
#endif /* _defined (__cplusplus) */

#ifndef FALSE
#define FALSE false
#endif

#ifndef TRUE
#define TRUE true
#endif

#define ENABLE (1)
#define DISABLE (0)

#define ON true
#define OFF false

#define SET_BITS 0xFF
#define CLEAR_BITS 0x00

#ifndef BIT_0
#define BIT_0 (1 << 0)
#define BIT_1 (1 << 1)
#define BIT_2 (1 << 2)
#define BIT_3 (1 << 3)
#define BIT_4 (1 << 4)
#define BIT_5 (1 << 5)
#define BIT_6 (1 << 6)
#define BIT_7 (1 << 7)
#endif

#ifndef BIT0
#define BIT0 BIT_0
#define BIT1 BIT_1
#define BIT2 BIT_2
#define BIT3 BIT_3
#define BIT4 BIT_4
#define BIT5 BIT_5
#define BIT6 BIT_6
#define BIT7 BIT_7
#endif


/***** public type definitions ***********************************************/

typedef void (*fwIrqHandler_t)(void);
typedef uint8_t (*fnCheckDevice)(uint8_t dev);

#if defined(ROM)
#undef ROM
#endif

#define ROM /*nothing*/


/** Status return value type for HAL module functions.*/
enum {
	HAL_RET_SUCCESS, //The operation was successfully completed
	HAL_RET_FAILURE, //Generic error
	HAL_RET_PARAMETER_ERROR, //Invalid parameter passed to a HAL function
	HAL_RET_NO_DEVICE, //specified hardware device was not found
	HAL_RET_DEVICE_NOT_OPEN, //he specified device is not open for use
	HAL_RET_NOT_INITIALIZED, //HAL module has not been initialized
	HAL_RET_OUT_OF_RESOURCES, //(memory/hardware) were not available
	HAL_RET_TIMEOUT, //The requested operation timed out
	HAL_RET_ALREADY_INITIALIZED, //HalInit called more than once
};

/*
 * @brief GPIO pin state definitions taken from the firmware's GPIO header file
 */
#define settingMode3X 0
#define settingMode1X 1
#define settingMode9290 0
#define settingMode938x 1
#define GPIO_PIN_SW5_P4_ENABLED 0
#define GPIO_PIN_SW5_P4_DISABLED 1

#define INT_ACTIVE_HIGH 1
#define INT_ACTIVE_LOW 0
#define INT_ACTIVE_DEFAULT INT_ACTIVE_LOW
#define INT_IS_ASSERTED INT_ACTIVE_DEFAULT
#define GPIO_OFFSET 407
#define GPIO_INT_PIN (GPIODV_9 + GPIO_OFFSET)
#define GPIO_INT_IRQ_FLAG (IRQF_TRIGGER_HIGH | IRQF_ONESHOT)

enum { GPIO_136 = 0x00, GPIO_140, GPIO_INT, GPIO_RST, GPIO_INVALID = 0xFF };

uint32_t HalInit(void);

uint32_t HalTerm(void);

uint32_t HalOpenI2cDevice(char const *DeviceName, char const *DriverName);

uint32_t HalCloseI2cDevice(void);

uint32_t HalSmbusReadByteData(uint8_t command, uint8_t *pRetByteRead);

uint32_t HalSmbusWriteByteData(uint8_t command, uint8_t writeByte);

uint32_t HalSmbusReadWordData(uint8_t command, uint16_t *pRetWordRead);

uint32_t HalSmbusWriteWordData(uint8_t command, uint16_t wordData);

uint32_t HalSmbusReadBlock(uint8_t command, uint8_t *buffer,
			   uint8_t *bufferLen);

uint32_t HalSmbusWriteBlock(uint8_t command, uint8_t const *blockData,
			    uint8_t length);

uint32_t HalI2cMasterWrite(uint8_t i2cAddr, uint8_t length, uint8_t *buffer);

uint32_t HalI2cMasterRead(uint8_t i2cAddr, uint8_t length, uint8_t *buffer);

uint8_t I2C_ReadByte(uint8_t deviceID, uint8_t offset);

uint8_t I2C_WriteByte(uint8_t deviceID, uint8_t offset, uint8_t value);

uint8_t I2C_ReadBlock(uint8_t deviceID, uint8_t offset, uint8_t *buf,
		      uint8_t len);

uint32_t HalInstallIrqHandler(fwIrqHandler_t irqHandler);

void HalEnableIrq(uint8_t bEnable);

uint32_t HalRemoveIrqHandler(void);

bool is_interrupt_asserted(void);

uint32_t HalInstallCheckDeviceCB(fnCheckDevice fn);

void HalTimerInit(void);

uint32_t HalTimerSysTicks(void);

void HalTimerSet(uint8_t index, uint16_t m_sec);

void HalTimerWait(uint16_t m_sec);

uint8_t HalTimerExpired(uint8_t timerIndex);

uint16_t HalTimerElapsed(uint8_t elapsedTimerIndex);

uint32_t HalGpioSetPin(uint32_t gpio, int value);

uint32_t HalGpioGetPin(uint32_t gpio, int *value);

uint32_t HalGetGpioIrqNumber(uint32_t gpio, unsigned int *irqNumber);

uint32_t HalEnableI2C(int bEnable);

uint32_t HalAcquireIsrLock(void);

uint32_t HalReleaseIsrLock(void);

/**
 * @}
 * end sii_hal_api group
 */

#ifdef __cplusplus
}
#endif /* _defined (__cplusplus) */

#endif /* _defined (SII_HAL_H) */
