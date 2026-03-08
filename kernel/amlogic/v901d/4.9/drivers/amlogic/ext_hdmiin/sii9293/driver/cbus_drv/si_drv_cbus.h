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

#ifndef __SI_DRV_CBUS_H__
#define __SI_DRV_CBUS_H__

#include "si_common.h"
#include "si_cbus_component.h"

//------------------------------------------------------------------------------
// Driver enums
//------------------------------------------------------------------------------

enum {
	SiiCBUS_INT = 0x0001,			// A CBUS interrupt has occurred
	SiiCBUS_NACK_RECEIVED_FM_PEER = 0x0002, // Peer sent a NACK
	SiiCBUS_DCAP_RDY_RECEIVED_FM_PEER = 0x0004, // DCAP_RDY received
	SiiCBUS_PATH_EN_RECEIVED_FM_PEER = 0x0008,  // PATH_EN received
	SiiCBUS_DCAP_CHG_RECEIVED_FM_PEER = 0x0010, // DCAP_CHG received
	SiiCBUS_SCRATCHPAD_WRITTEN_BY_PEER =
	    0x0020, // DSCR_CHG received.peer writes into scrAtchpad
	SiiCBUS_REQ_WRT_RECEIVED_FM_PEER = 0x0040, // REQ_WRT received
	SiiCBUS_GRT_WRT_RECEIVED_FM_PEER = 0x0080, // GRT_WRT received

	SiiCBUS_3D_REQ_RECEIVED_FM_PEER = 0x0100, // 3D_REQ received
	SiiCBUS_DDC_ABORT = 0x0200,		  // DDC_ABORT
	SiiCBUS_MSC_ABORT_RES = 0x0400,		  // DDC_ABORT
	SiiCBUS_MSC_ABORT = 0x0800,		  // DDC_ABORT

	SiiCBUS_MSC_MSG_RCVD = 0x1000,	      // MSC_MSG received
	SiiCBUS_MSC_CMD_DONE = 0x2000,	      // MSC_MSG received
	SiiCBUS_CBUS_CONNECTION_CHG = 0x4000, // MSC_MSG received
	SiiCBUS_MHL_CONNECTION_CHG = 0x8000,  // MHL cable connected

};

//------------------------------------------------------------------------------
// CBUS Driver Manifest Constants
//------------------------------------------------------------------------------

// Version that this chip supports
#define MHL_VER_MAJOR (0x01 << 4) // bits 4..7
#define MHL_VER_MINOR 0x00 // bits 0..3
#define MHL_VERSION (MHL_VER_MAJOR | MHL_VER_MINOR)

#define CBUS_VER_MAJOR (0x01 << 4) // bits 4..7
#define CBUS_VER_MINOR 0x00 // bits 0..3
#define MHL_CBUS_VERSION (CBUS_VER_MAJOR | CBUS_VER_MINOR)

#define MHL_DEV_CAT_POW_DF 0x11

#define MHL_DEV_CAT_SOURCE 0x00
#define MHL_DEV_CAT_SINGLE_INPUT_SINK 0x01
#define MHL_DEV_CAT_MULTIPLE_INPUT_SINK 0x02
#define MHL_DEV_CAT_UNPOWERED_DONGLE 0x03
#define MHL_DEV_CAT_SELF_POWERED_DONGLE 0x04
#define MHL_DEV_CAT_HDCP_REPEATER 0x05
#define MHL_DEV_CAT_OTHER 0x06

#define MHL_POWER_SUPPLY_CAPACITY 16 // 160 mA current
#define MHL_POWER_SUPPLY_PROVIDED 16 // 160mA 0r 0 for Wolverine.
#define MHL_VIDEO_LINK_MODE_SUPPORT 1 // Bit 0 = Supports RGB 4:4:4
#define MHL_AUDIO_LINK_MODE_SUPPORT 1 // Bit 0 = 2-Channel
#define MHL_HDCP_STATUS 0 // Bits set dynamically

// initialize MHL registers with the correct values
#define MHL_DEV_STATE 0x00
#define MHL_MHL_VERSION 0x20
#define MHL_DEV_CAT 0x31 // changed from 0x11 for sink 900ma output support
#define MHL_ADOPTER_ID_H 0x01
#define MHL_ADOPTER_ID_L 0x42
#define MHL_VID_LINK_MODE 0x3F
#define MHL_AUD_LINK_MODE 0x03
#define MHL_VIDEO_TYPE 0x8F
#define MHL_LOG_DEV_MAP 0x41
#define MHL_BANDWIDTH 0x0F
#define MHL_FEATURE_FLAG 0x1F // changed from 0x07 to support UCP
#define MHL_FEATURE_SP_SUPPORT 0x04
#define MHL_DEVICE_ID_H 0x52
#define MHL_DEVICE_ID_L 0x93
#define MHL_SCRATCH_PAD_SIZE 0x10
#define MHL_INT_STAT_SIZE 0x33
#define MHL_RESERVED 0x00

// MHL_AUD_LINK_MODE : 0x06
#define MHL_AUD_LINK_MODE_2CH 0x01
#define MHL_AUD_LINK_MODE_8CH 0x02

#define MHL_DEV_SUPPORTS_DISPLAY_OUTPUT (0x01 << 0)
#define MHL_DEV_SUPPORTS_VIDEO_OUTPUT (0x01 << 1)
#define MHL_DEV_SUPPORTS_AUDIO_OUTPUT (0x01 << 2)
#define MHL_DEV_SUPPORTS_MEDIA_HANDLING (0x01 << 3)
#define MHL_DEV_SUPPORTS_TUNER (0x01 << 4)
#define MHL_DEV_SUPPORTS_RECORDING (0x01 << 5)
#define MHL_DEV_SUPPORTS_SPEAKERS (0x01 << 6)
#define MHL_DEV_SUPPORTS_GUI (0x01 << 7)

#define MHL_BANDWIDTH_LIMIT 22 // 225 MHz

enum { burstId_3D_VIC = 0x0010, burstId_3D_DTD = 0x0011 };
struct Mhl2HighLow_t {
	uint8_t high;
	uint8_t low;
};
#define BURST_ID(bid)                                                          \
	((((uint16_t)bid.high) << 8) | ((uint16_t)bid.low))

// see MHL2.0 spec section 5.9.1.2
struct Mhl2VideoDescriptor_t {
	uint8_t reservedHigh;
	unsigned char FrameSequential : 1; // FS_SUPP
	unsigned char TopBottom : 1;	   // TB_SUPP
	unsigned char LeftRight : 1;	   // LR_SUPP
	unsigned char reservedLow : 5;
};

struct Mhl2VideoFormatData_t {
	struct Mhl2HighLow_t burstId;
	uint8_t checkSum;
	uint8_t totalEntries;
	uint8_t sequenceIndex;
	uint8_t numEntriesThisBurst;
	struct Mhl2VideoDescriptor_t videoDescriptors[5];
};

#define MaxVideoEntry 5

struct CbusDrvInstanceData_t {
	uint16_t statusFlags;

	// CBUS transfer values read at last interrupt for each specific channel
	uint8_t busConnected;
	uint8_t mhlConnected;
	uint8_t vsCmd;
	uint8_t vsData;
	uint8_t msgData0;
	uint8_t msgData1;
	uint8_t cecAbortReason;
	uint8_t ddcAbortReason;
	uint8_t mscAbortResReason;
	uint8_t mscAbortReason;

	struct Mhl2VideoFormatData_t videoFormatData[MaxVideoEntry];
};

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusIntrFlagGet
// Description: Returns interrupt flag
//------------------------------------------------------------------------------
bool SiiDrvCbusIntrFlagGet(void);

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusIntrFlagSet
// Description: reset interrupt flags
//------------------------------------------------------------------------------
void SiiDrvCbusIntrFlagSet(void);

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusNackFromPeerGet
// Description: checks if peer sent a NACK
// Parameters:  channel - CBus channel
// Returns:     true if peer sent a NACK, false if not.
//------------------------------------------------------------------------------
bool SiiDrvCbusNackFromPeerGet(void);

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusDevCapChangedGet
// Description: Returns if the peer's device capability values are changed
// Parameters:  pData - pointer to return data buffer (1 byte).
// Returns:     true/false
//------------------------------------------------------------------------------
bool SiiDrvCbusDevCapChangedGet(void);

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusScratchpadWrtnGet
// Description: Returns if the peer has written the scratchpad
// Returns:     true/false
//------------------------------------------------------------------------------
bool SiiDrvCbusScratchpadWrtnGet(void);

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusReqWrtGet
// Description: Returns if the peer is requesting for scratchpad write
// permission Returns:     true/false
//------------------------------------------------------------------------------
bool SiiDrvCbusReqWrtGet(void);

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusGrtWrtGet
// Description: Returns if the peer is requesting for scratchpad write
// permission Returns:     true/false
//------------------------------------------------------------------------------
bool SiiDrvCbusGrtWrtGet(void);
bool SiiDrvCbus3DReqGet(void);
void SiiDrvCbusBuild3DData(void);
bool SiiDrv3DWriteBurst(void);

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusVsDataGet
// Description: Returns the last VS cmd and data bytes retrieved by the CBUS
// ISR. Parameters:  pData - pointer to return data buffer (2 bytes). Returns:
// true if a new VS data was available, false if not.
//              pData[0] - VS_CMD value
//              pData[1] - VS_DATA value
//------------------------------------------------------------------------------
bool SiiDrvCbusVsDataGet(uint8_t *pData);

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusDevCapReadyGet
// Description: Returns if the peer's device capability values are ready
// Parameters:  pData - pointer to return data buffer (1 byte).
// Returns:     true/false
//------------------------------------------------------------------------------
bool SiiDrvCbusDevCapReadyGet(void);

//------------------------------------------------------------------------------
// Function:    SiiDrvPathEnableGet
// Description: Returns if the peer has sent PATH_EN
// Returns:     true/false
//------------------------------------------------------------------------------
bool SiiDrvPathEnableGet(void);

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusCmdRetDataGet
// Description: Clears the register to receive fresh response data back
// Parameters:  pData - pointer to return data buffer (2 bytes).
//------------------------------------------------------------------------------
bool SiiDrvCbusCmdRetDataGet(uint8_t *pData);

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusBusStatusGet
// Description: Returns the last Bus Status data retrieved by the CBUS ISR.
// Parameters:  pData - pointer to return data buffer (1 byte).
// Returns:     true if new bus status data is available, false if not.
//              pData - Destination for bus status data.
//------------------------------------------------------------------------------
bool SiiDrvCbusBusStatusGet(uint8_t *pData);

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusMhlStatusGet
// Description: Returns the last Bus Status data retrieved by the CBUS ISR.
// Parameters:  pData - pointer to return data buffer (1 byte).
// Returns:     true/false
//------------------------------------------------------------------------------
bool SiiDrvCbusMhlStatusGet(uint8_t *pData);

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusDdcAbortReasonGet
// Description: Returns the last DDC Abort reason received by the CBUS ISR.
// Parameters:  pData - pointer to return data buffer (1 byte).
// Returns:     true if a new DDC Abort reason data was available, false if not.
//              pData - Destination for DDC Abort reason data.
//------------------------------------------------------------------------------
bool SiiDrvCbusDdcAbortReasonGet(uint8_t *pData);

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusMscAbortReasonGet
// Description: Returns the last MSC Abort reason received by the CBUS ISR.
// Parameters:  pData - pointer to return data buffer (1 byte).
// Returns:     true if a new MSC Abort reason data was available, false if not.
//              pData - Destination for MSC Abort reason data.
//------------------------------------------------------------------------------
bool SiiDrvCbusMscAbortReasonGet(uint8_t *pData);

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusMscAbortFmPeerReasonGet
// Description: Returns the last MSC Abort reason received by the CBUS ISR.
// Parameters:  pData - pointer to return data buffer (1 byte).
// Returns:     pData - Destination for MSC Abort reason data.
//------------------------------------------------------------------------------
bool SiiDrvCbusMscAbortResReasonGet(uint8_t *pData);

//------------------------------------------------------------------------------
// Function:    SiiDrvInternalCBusWriteCommand
// Description: Write the specified Sideband Channel command to the CBUS.
//              Command can be a MSC_MSG command (RCP/MCW/RAP), or another
//              command such as READ_DEVCAP, GET_VENDOR_ID, SET_HPD, CLR_HPD,
//              etc.
//
// Parameters:  channel - CBUS channel to write
//              pReq    - Pointer to a cbus_req_t structure containing the
//                        command to write
// Returns:     TRUE    - successful write
//              FALSE   - write failed
//------------------------------------------------------------------------------

bool SiiDrvInternalCBusWriteCommand(struct cbus_req_t *pReq);

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusInitialize
// Description: Attempts to initialize the CBUS. If register reads return 0xFF,
//              it declares error in initialization.
//              Initializes discovery enabling registers and anything needed in
//              config register, interrupt masks.
// Returns:     TRUE if no problem
//------------------------------------------------------------------------------

bool SiiDrvCbusInitialize(void);

void SiiDrvCbusTermCtrl(bool terminate);

//------------------------------------------------------------------------------
// Function:    SiiDrvCbusDevCapsRegisterGet
// Description: Returns the CBus register value
// Parameters:  regAddr - register address
// Returns:     register value
//------------------------------------------------------------------------------
uint8_t SiiDrvCbusDevCapsRegisterGet(uint16_t regAddr);

bool SiiDrvCbusProcessInterrupts(void);
#if defined(__KERNEL__)
extern int input_dev_rap;
extern int input_dev_rcp;
extern int input_dev_ucp;
#endif
#endif // __SI_DRV_CBUS_H__
