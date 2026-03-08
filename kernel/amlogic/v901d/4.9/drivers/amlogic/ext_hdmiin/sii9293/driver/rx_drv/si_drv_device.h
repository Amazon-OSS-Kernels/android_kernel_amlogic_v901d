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
#ifndef __SI_DRV_DEVICE_H__
#define __SI_DRV_DEVICE_H__
#include "si_common.h"

enum {
	SiiTERM_HDMI = 0xC8,	// Enable for HDMI mode
	SiiTERM_MHL = 0x49,	// Enable for MHL mode
	SiiTERM_DISABLE = 0x4B, // Disable
};

enum {
	SiiPortType_HDMI = 0, // Port is in HDMI mode
	SiiPortType_MHL,      // Port is in MHL mode
};

//------------------------------------------------------------------------------
// Function:    SiiDrvDeviceInitialize
// Description: This function disables the device or places the device in
//              software reset if it does not power up in a disabled state.
//              It may be used to initialize registers that require a value
//              different from the power-up state and are common to one or
//              more of the other component modules.
// Parameters:  none
// Returns:     It returns true if the initialization is successful, or false
//              if some failure occurred.
//
// Note:        The function leaves the device in disabled state until the
//              SiiDeviceRelease function is called.
//------------------------------------------------------------------------------

bool SiiDrvDeviceInitialize(void);

void SiiDrvDeviceRelease(void);

//------------------------------------------------------------------------------
// Function:    SiiDrvDeviceManageInterrupts
// Description: Monitors 5293 interrupts and calls an interrupt process
//              function in the applicable driver.
// NOTE:    This function is not designed to be called directly from a physical
//          interrupt handler unless provisions have been made to avoid conflict
//          with normal level I2C accesses.
//          It is intended to be called from normal level by monitoring a flag
//          set by the physical handler.
//------------------------------------------------------------------------------

void SiiDrvDeviceManageInterrupts(void);

//------------------------------------------------------------------------------
// Function:    SiiDrvSwitchDeviceRXTermControl
// Description: Enable or disable RX termination for the selected port(s)
// Parameters:  portIndex   - 0-4:  Switch port to control
//                          - 0xFF: Apply to all ports.
//              enableVal   - The bit pattern to be used to enable or disable
//                            termination
//                          0x00 - Enable for HDMI mode
//                          0x55 - Enable for MHL mode
//                          0xFF - Disable
//
// Note:        The 'enableVal' parameter for this function is NOT boolean as
//              it is for the companion si_DeviceXXXcontrol functions.
// Note:        The 'portIndex' parameter value 0xFF should not be used unless
//              all ports are HDMI1.3/a (not MHL or CDC)
//------------------------------------------------------------------------------
void SiiDrvSwitchDeviceRXTermControl(uint8_t enableVal);
void SiiDrvSwitchDeviceHpdControl(bool enableHPD, uint8_t mode);
extern int sil5293_source_det;
#endif // __SI_DRV_DEVICE_H__
