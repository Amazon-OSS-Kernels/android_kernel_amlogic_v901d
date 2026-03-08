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

#ifndef __SI_CRA_H__
#define __SI_CRA_H__

#include "si_common.h"
#include "si_drv_cra_cfg.h"

typedef uint16_t SiiReg_t;

// Standard result codes are in the range of 0 - 4095
enum {
	SII_SUCCESS = 0,	   // Success.
	SII_ERR_FAIL,		   // General failure.
	SII_ERR_INVALID_PARAMETER, //
	SII_ERR_IN_USE,		   // Module already initialized.
	SII_ERR_NOT_AVAIL,	   // Allocation of resources failed.
};

#if defined(__KERNEL__)
uint32_t CraReadBlockI2c(uint32_t busIndex, uint8_t deviceId,
				 uint8_t regAddr, uint8_t *pBuffer,
				 uint16_t count);
uint32_t CraWriteBlockI2c(uint32_t busIndex, uint8_t deviceId,
				  uint8_t regAddr, const uint8_t *pBuffer,
				  uint16_t count);
uint32_t CraReadIOExpanderI2c(uint32_t busIndex,
				      uint8_t deviceId, const uint8_t *pBuffer,
				      uint16_t count);
uint32_t CraWriteIOExpanderI2c(uint32_t busIndex,
				       uint8_t deviceId, const uint8_t *pBuffer,
				       uint16_t count);
#endif
bool SiiCraInitialize(void);
uint32_t SiiCraGetLastResult(void);
bool SiiRegInstanceSet(SiiReg_t virtualAddress, uint8_t newInstance);

void SiiRegReadBlock(SiiReg_t virtualAddr, uint8_t *pBuffer, uint16_t count);
uint8_t SiiRegRead(SiiReg_t virtualAddr);
void SiiRegWriteBlock(SiiReg_t virtualAddr, const uint8_t *pBuffer,
		      uint16_t count);
void SiiRegWrite(SiiReg_t virtualAddr, uint8_t value);
uint16_t SiiRegReadWord(SiiReg_t reg_addr);
void SiiRegWriteWord(SiiReg_t reg_addr, uint16_t value);
void SiiRegModify(SiiReg_t virtualAddr, uint8_t mask, uint8_t value);
void SiiRegBitsSet(SiiReg_t virtualAddr, uint8_t bitMask, bool setBits);
void SiiRegBitsSetNew(SiiReg_t virtualAddr, uint8_t bitMask, bool setBits);

// Special purpose
void SiiRegEdidReadBlock(SiiReg_t segmentAddr, SiiReg_t virtualAddr,
			 uint8_t *pBuffer, uint16_t count);
extern struct pageConfig_t g_addrDescriptor[SII_CRA_MAX_DEVICE_INSTANCES]
				    [SII_CRA_DEVICE_PAGE_COUNT];
extern SiiReg_t g_siiRegPageBaseReassign[];
extern SiiReg_t g_siiRegPageBaseRegs[SII_CRA_DEVICE_PAGE_COUNT];
#endif // __SI_CRA_H__
