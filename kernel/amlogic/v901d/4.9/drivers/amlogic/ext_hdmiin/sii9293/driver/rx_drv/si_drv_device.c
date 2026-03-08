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
/**
 * @file si_drv_sii5293.c
 *
 * @brief 5293 driver.
 * @brief Driver layer internal file
 *
 *****************************************************************************/

#include "si_platform.h" // Interface to board environment
#include "si_drv_device.h"
#include "si_drv_cbus.h"

#include "si_sii5293_registers.h"
#if defined(INC_CEC)
#include "si_cec_config.h"
#include "si_drv_cpi_internal.h"
#endif
#include "si_drv_cra_cfg.h"
#include "si_drv_rx.h"
#include "si_drv_rx_isr.h"
#include "si_drv_board.h"
#ifdef SK_TX_EVITA
#include "si_drv_evita.h"
#endif

#ifdef MHAWB_SUPPORT
#include "si_drv_mhawb.h"
#include "si_drv_hawb.h"
#endif

#define HW_RESET_PERIOD 10 // 10 ms.
#define HW_RESET_DELAY 100
uint32_t g_portType = SiiPortType_HDMI;

/*****************************************************************************/
/**
 *  @brief Reset the 5293 chip by toggle the reset pin at 5293
 *
 *  @param[in]      hwResetPeriod       The delay time during the low level.
 *  @param[in]      hwResetDelay        The delay time after reset hw.
 *
 *****************************************************************************/
void SiiHwReset(uint16_t hwResetPeriod, uint16_t hwResetDelay)
{
	// Toggle RX reset pin
	SiiGpioControl(GPIO_RST, false);
	HalTimerWait(hwResetPeriod);
	SiiGpioControl(GPIO_RST, true);

	// then wait per spec
	HalTimerWait(hwResetDelay);
}

/*****************************************************************************/
/**
 *  @brief  Set the port type of the specific Rx port.
 *
 *  @param[in]      portIndex       - SiiPORT_x     - Rx port (0-1).
 *                                  - SiiPORT_ALL   - SiiPORT_ALL       - All
 *ports are acted on simultaneously.
 *  @param[in]      portType        true - enable, false - disable
 *
 *  @return     void
 *
 * @note: The 'portIndex' parameter value 0xFF should not be used unless
 *        all ports are HDMI1.3/a (not MHL or CDC)
 *
 *****************************************************************************/
void SiiDrvSwitchPortType(uint32_t portType)
{
	static uint8_t peq_val_HDMI[8] = {0xC6, 0XF5, 0XF4, 0XE4,
					  0XD4, 0XA4, 0X94, 0X25};
	static uint8_t peq_val_MHL[8] = {0x26, 0X10, 0X24, 0X11,
					 0X12, 0X22, 0X1A, 0X25};
	if (portType == SiiPortType_HDMI)
		SiiRegWriteBlock(REG_PEQ_VAL0, peq_val_HDMI, 8);
	else
		SiiRegWriteBlock(REG_PEQ_VAL0, peq_val_MHL, 8);

	g_portType = portType;
}

/*****************************************************************************/
/**
 *  @brief This function disables the device or places the device in
 *         software reset if it does not power up in a disabled state.
 *         It may be used to initialize registers that require a value
 *         different from the power-up state and are common to one or
 *         more of the other component modules.
 *
 *  @return     State
 *  @retval     true if the configuration was successful
 *  @retval     false if some failure occurred
 *
 * @note: This function leaves the device in a disabled state until the
 *        SiiDrvDeviceRelease function is called.
 *
 *****************************************************************************/
bool SiiDrvDeviceInitialize(void)
{
	bool success = false;
	uint16_t deviceID;
	uint8_t revison;

#if defined(__KERNEL__)
	gDriverContext.chip_revision = 0xFF;
#endif

	SiiHwReset(HW_RESET_PERIOD, HW_RESET_DELAY);
	for (;;) {
		SiiDrvSoftwareReset(RX_M__SRST__SRST);

		deviceID = SiiRegRead(REG_DEV_IDH_RX);
		deviceID = (deviceID << 8) | SiiRegRead(REG_DEV_IDL_RX);

		revison = SiiRegRead(REG_DEV_REV);
#if defined(__KERNEL__)
		gDriverContext.chip_revision = revison;
#endif
		if (deviceID == 0x5293) {
			DEBUG_PRINT(MSG_ALWAYS, "Device ID: %04X\n",
				    (int)deviceID);
			DEBUG_PRINT(MSG_ALWAYS, "Device Revision: %02X\n",
				    (int)revison);
		} else if (deviceID == 0x9293) {
			DEBUG_PRINT(MSG_ALWAYS, "Device ID: %04X\n",
				    (int)deviceID);
			DEBUG_PRINT(MSG_ALWAYS, "Device Revision: %02X\n",
				    (int)revison);
		} else {
			DEBUG_PRINT(MSG_ALWAYS, "\n Device Id check failed!\n");
			break;
		}

#ifdef SK_TX_EVITA
		if (!SiiDrvEvitaInit()) {
			DEBUG_PRINT(MSG_ALWAYS,
				    "\n SiiDrvEvitaInitialize failed!\n");
			break;
		}
#endif
		if (!SiiMhlRxInitialize()) {
			DEBUG_PRINT(MSG_ALWAYS,
				    "\n SkAppDeviceInitCbus failed!\n");
			break;
		}

		if (!SiiDrvRxInitialize()) {
			DEBUG_PRINT(MSG_ALWAYS,
				    "\n SkAppDeviceInitRx failed!\n");
			break;
		}

#ifdef MHAWB_SUPPORT
		mhawb_init();
#endif
		SiiDrvSwitchPortType(SiiPortType_HDMI);
		success = true;
		break;
	}

	return success;
}

void SiiDrvDeviceRelease(void)
{
	DEBUG_PRINT(MSG_ALWAYS,
	"SiI5293 may continue to output video. Driver features and APIs will not work.\n");
#ifdef MHAWB_SUPPORT
	mhawb_destroy();
#endif
}

/*****************************************************************************/
/**
 *  @brief  Enable or disable RX termination for the selected port(s)
 *
 *  @param[in]      portIndex       - 0-1:  Switch port to control
 *                                  - 0xFF: Apply to all ports.
 *  @param[in]      enableVal       The bit pattern to be used to enable
 *                                  or disable termination
 *                                      0x00 - Enable for HDMI mode
 *                                      0x55 - Enable for MHL mode
 *                                      0xFF - Disable
 *
 *  @return     void
 *
 * @note: The 'enableVal' parameter for this function is NOT boolean as
 *        it is for the companion si_DeviceXXXcontrol functions.
 *        The 'portIndex' parameter value 0xFF should not be used unless
 *        all ports are HDMI1.3/a (not MHL or CDC)
 *
 *****************************************************************************/
void SiiDrvSwitchDeviceRXTermControl(uint8_t enableVal)
{
	SiiRegModify(REG_RX_CTRL5, MSK_TERM, enableVal);
}

uint32_t SiiDrvGetPortType(void) { return g_portType; }
#ifdef ENABLE_HDCP
/*****************************************************************************/
/**
 *  @brief  Enable or disable HDCP access for the selected port(s)
 *
 *  @param[in]      portIndex           - 0:  Switch port to control.
 *                                      - 0xFF: Apply to all ports.
 *  @param[in]      enableHDCP          - true: to enable
 *                                      - false: to disable
 *
 *  @return     void
 *
 * @note: The 'portIndex' parameter value 0xFF should not be used unless
 *        all ports are HDMI1.3/a (not MHL or CDC)
 *
 *****************************************************************************/
void SiiDrvSwitchDeviceHdcpDdcControl(bool enableHDCP)
{
	uint8_t enableVal, enableMask;

	enableVal = enableHDCP ? SET_BITS : CLEAR_BITS;

	// only one port in 5293, no need to check portIndex
	enableMask = RX_M__SYS_SWTCH__RX0_EN | RX_M__SYS_SWTCH__DDC0_EN |
		     RX_M__SYS_SWTCH__DDC_DEL_EN;

	SiiRegModify(RX_A__SYS_SWTCH, enableMask, enableVal);
}
#endif
/*****************************************************************************/
/**
 *  @brief Enable or disable HPD for the selected port(s)
 *
 *  @param[in]      portIndex       - 0-4:  Switch port to control
 *                                  - 0xFF: Apply to all ports.
 *  @param[in]      enableHDCP      - true: to enable
 *                                  - false: to disable
 *  @param[in]      mode            - true:  to tristate the HPD
 *                                  - false: to clear hpd
 *
 *  @return     void
 *
 * @note: The 'portIndex' parameter value 0xFF should not be used unless
 *        all ports are HDMI1.3/a (not MHL or CDC)
 *
 *****************************************************************************/
void SiiDrvSwitchDeviceHpdControl(bool enableHPD, uint8_t mode)
{
	uint8_t enableVal;

	if (mode)
		enableVal = enableHPD ? VAL_HP_PORT_MHL : CLEAR_BITS;
	else
		enableVal = enableHPD ? VAL_HP_PORT_ALL_HI : CLEAR_BITS;
	SiiRegModify(REG_HP_CTRL, VAL_HP_PORT0_MASK, enableVal);
}

/*****************************************************************************/
/**
 *  @brief        Check connectivity of HDMI/MHL calbe.
 *            Update internal status of the cable.
 *             Update register settings for HDMI/MHL connectivity.
 *
 *****************************************************************************/
void SiiSwitchConnectionCheck(void)
{
	bool cableStatus = false;
	uint8_t mhlStatus = 0;

	// Check MHL cable in/out
	if (SiiDrvCbusMhlStatusGet(&mhlStatus)) {
		// MHL cable out
		if (mhlStatus == 0) {
#if defined(__KERNEL__)
			gDriverContext.mhl_cable_state = false;
#endif
			SiiDrvSwitchPortType(SiiPortType_HDMI);
			SiiLedControl(LED_ID_2, false);
			SiiLedControl(LED_ID_3, false);
			DEBUG_PRINT(MSG_ALWAYS,
				    "Cable connection change: MHL cable out\n");
		} else {
			// MHL cable in
#if defined(__KERNEL__)
			gDriverContext.mhl_cable_state = true;
#endif
			SiiDrvSwitchPortType(SiiPortType_MHL);
			SiiDrvSwitchDeviceHpdControl(true,
						     true); // HPD tri-state
			SiiDrvSwitchDeviceRXTermControl(SiiTERM_DISABLE);
			SiiLedControl(LED_ID_2, true);
			SiiLedControl(LED_ID_3, false);
			DEBUG_PRINT(MSG_ALWAYS,
				    "Cable connection change: MHL cable in\n");
		}
	}

	if (SiiDrvCableStatusGet(&cableStatus)) {
		if (cableStatus) {
#if defined(__KERNEL__)
			gDriverContext.pwr5v_state = true;
#endif
#ifdef ENABLE_HDCP
			SiiDrvSwitchDeviceHdcpDdcControl(true);
#endif
			if (SiiDrvGetPortType() == SiiPortType_HDMI) {
				SiiDrvSwitchDeviceRXTermControl(SiiTERM_HDMI);
				DEBUG_PRINT(MSG_ALWAYS, "HPD: ON\n");
				SiiDrvSwitchDeviceHpdControl(true,
							     false); //  HPD on
				SiiLedControl(LED_ID_2, false);
				SiiLedControl(LED_ID_3, true);
			}
			DEBUG_PRINT(MSG_ALWAYS,
				    "Cable connection change: cable in\n");
			sil5293_source_det = 1;
		} else {
#if defined(__KERNEL__)
			gDriverContext.pwr5v_state = false;
			gDriverContext.mhl_cable_state = false;
			SiiConnectionStateNotify(false);
#endif
			SiiDrvSwitchPortType(SiiPortType_HDMI);
			DEBUG_PRINT(MSG_ALWAYS, "HPD: OFF\n");
			SiiDrvSwitchDeviceHpdControl(false, false); // HPD low
			SiiDrvSwitchDeviceRXTermControl(SiiTERM_DISABLE);
#ifdef ENABLE_HDCP
			SiiDrvSwitchDeviceHdcpDdcControl(false);
#endif
			SiiLedControl(LED_ID_2, false);
			SiiLedControl(LED_ID_3, false);
			DEBUG_PRINT(MSG_ALWAYS,
				    "Cable connection change: cable out\n");
			sil5293_source_det = 0;
		}
	}
}

/*****************************************************************************/
/**
 *  @brief Monitors 5293 interrupts and calls an interrupt processor
 *         function in the applicable driver if an interrupt is encountered.
 *
 *  @return     State
 *  @retval     true if device interrupts occurs
 *  @retval     false if no interrupts
 *
 * @note: This function is not designed to be called directly from a physical
 *        interrupt handler unless provisions have been made to avoid conflict
 *        with normal level I2C accesses.
 *        It is intended to be called from normal level by monitoring a flag
 *        set by the physical handler.
 *
 *****************************************************************************/
void SiiDrvDeviceManageInterrupts(void)
{
	uint8_t intStatus2;
#if !defined(__KERNEL__)
	// DEBUG_PRINT(MSG_STAT, ("-------------------SiiMhlTxDeviceIsr start
	// -----------------\n"));
	while (is_interrupt_asserted()) // check interrupt assert bit
#else
	if (SiiRegRead(RX_A__INTR_STATE) & RX_M__INTR_STATE)
#endif
	{
#ifdef MHAWB_SUPPORT
		if (g_mhawb.state >= MHAWB_STATE_INIT) {
			if (mhawb_do_isr_work() == MHAWB_EVENT_HANDLER_SUCCESS)
				return;
		}
#endif
		// Determine the pending interrupts and service them with driver
		// calls Each function will call its appropriate callback
		// function if a status change is detected that needs upper
		// level attention.

		// Get the register interrupt info
		intStatus2 = SiiRegRead(REG_INTR_STATE_2);

		if (intStatus2 & BIT_RX_INTR) {
			DEBUG_PRINT(MSG_DBG, ("RX Interrupt\n"));
			SiiRxInterruptHandler();
#ifdef ENABLE_HDCP
			SiiRxSetVideoStableTimer();
#endif
		}
		if (intStatus2 & BIT_CBUS_INTR) {
			DEBUG_PRINT(MSG_DBG, ("CBUS Interrupt\n"));
#ifdef MHAWB_SUPPORT
			if (g_mhawb.state == MHAWB_STATE_TAKEOVER ||
			    g_mhawb.state == MHAWB_STATE_DISABLED) {
				SiiDrvHawbProcessInterrupts();
			}
#endif
			SiiDrvCbusProcessInterrupts();
			SiiMhlRxIntrHandler();
		}
#if defined(INC_CEC)
		if (intStatus2 & BIT_CEC_INTR) {
			DrvCpiProcessInterrupts();
			DEBUG_PRINT(MSG_DBG, ("CEC Interrupt\n"));
		}
#endif

		SiiSwitchConnectionCheck();
	}
#if defined(__KERNEL__)
	// DEBUG_PRINT(MSG_STAT, ("-------------------SiiMhlTxDeviceIsr end
	// -------------------\n"));
#endif
}
