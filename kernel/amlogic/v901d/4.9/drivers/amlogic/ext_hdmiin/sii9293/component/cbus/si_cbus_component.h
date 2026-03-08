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

#ifndef __SI_CBUS_COMPONENT_H__
#define __SI_CBUS_COMPONENT_H__

#include "si_common.h"
#include "si_cbus_enums.h"

//------------------------------------------------------------------------------
//  Manifest Constants
//------------------------------------------------------------------------------

#define MHL_DEVCAP_SIZE 16
#define MHL_INTERRUPT_SIZE 4
#define MHL_STATUS_SIZE 4
#define MHL_SCRATCHPAD_SIZE 16
#define MHL_MAX_BUFFER_SIZE                                                    \
	MHL_SCRATCHPAD_SIZE // manually define highest number

#define CBUS_MAX_COMMAND_QUEUE 6
#define CBUS_MAX_BURST_QUEUE 6

#define CBUS_BURST_WAIT_TIMER                                                  \
	1200 // This timer will be useless after enable HAWB
#define CBUS_ABORT_TIMER 2000
#define CBUS_DCAP_READY_TIMER 1000
//------------------------------------------------------------------------------
//  CBUS Component typedefs
//------------------------------------------------------------------------------

//
// structure to hold command details from upper layer to CBUS module
//
struct cbus_req_t {
	uint8_t retry;	    // retry times
	uint8_t reqStatus;  // CBUS_IDLE, CBUS_PENDING
	uint8_t command;    // VS_CMD or RCP opcode
	uint8_t offsetData; // Offset of register on CBUS or RCP data
	uint8_t length; // Only applicable to write burst. ignored otherwise.
	uint8_t msgData[MHL_MAX_BUFFER_SIZE]; // Pointer to message data area.
	uint8_t retData[2];  // Pointer to read back message data area.
	uint8_t *pdatabytes; // pointer for write burst or read many bytes
};

struct cbus_rev_t {
	bool arrived;	    // CBUS message is arrived
	uint8_t command;    // VS_CMD or RCP opcode
	uint8_t offsetData; // Offset of register on CBUS or RCP data
};

struct cbus_burst_t {
	uint8_t retry; // retry times
	uint8_t burstStatus;
	uint8_t offset;
	uint8_t length;
	uint8_t burstData[MHL_MAX_BUFFER_SIZE];
};

struct cbusChannelState_t {
	bool connected;		      // True if a connected MHL port
	bool dcap_ready;		      // device capability ready
	bool dcap_ongoing;		      // True if read dcap is on going
	uint8_t remote_dcap[MHL_DEVCAP_SIZE]; // cached remote dcap registers
	uint8_t state;	     // State of command execution for this channel
	uint8_t activeIndex; // Active queue entry for req.
	uint8_t activeBurst;
#if defined(__KERNEL__)
	struct _SiiOsTimerInfo_t *abortTimer;
	struct _SiiOsTimerInfo_t *burstTimer;
	struct _SiiOsTimerInfo_t *dcapTimer;
#else
	uint32_t abortTimer;
	uint32_t burstTimer;
	uint32_t dcapTimer;
#endif
	bool abortState;
	bool burstWaitState;
	struct cbus_burst_t burst[CBUS_MAX_BURST_QUEUE];
	struct cbus_req_t request[CBUS_MAX_COMMAND_QUEUE];
	struct cbus_rev_t receive;
};

struct CbusInstanceData_t {
	struct cbusChannelState_t chState;
};

//------------------------------------------------------------------------------
//  Standard component functions
//-------------------------------------------------------------
bool SiiMhlRxInitialize(void);

//------------------------------------------------------------------------------
//  Component Specific functions
//------------------------------------------------------------------------------

uint8_t SiiMhlRxIntrHandler(void);

uint8_t SiiCbusRequestStatus(void);

void SiiCbusRequestSetIdle(uint8_t newState);

bool SiiMhlRxIsQueueFull(void);

bool SiiMhlRxIsQueueEmpty(void);

uint8_t SiiCbusChannelStatus(void);

bool SiiMhlRxCbusConnected(void);

void SiiCbusRequestDataGet(struct cbus_req_t *pCmdRequest);

bool SiiMhlRxSendRAPCmd(uint8_t actCode);

bool SiiMhlRxSendRCPCmd(uint8_t keyCode);

bool SiiMhlRxSendUCPCmd(uint8_t keyCode);

bool SiiCbusSendMscMsgCmd(uint8_t subCmd, uint8_t mscData);

bool SiiMhlRxSendRcpk(uint8_t keyCode);

bool SiiMhlRxSendRcpe(uint8_t cmdStatus);

bool SiiMhlRxSendRapk(uint8_t cmdStatus);

bool SiiMhlRxSendUcpk(uint8_t cmdStatus);

bool SiiMhlRxSendUcpe(uint8_t cmdStatus);

bool SiiMhlRxSendMsge(uint8_t opcode);

bool SiiCbusWriteCommand(struct cbus_req_t *pReq);

bool SiiCbusWriteStatus(uint8_t regOffset, uint8_t value);

bool SiiCbusSetInt(uint8_t regOffset, uint8_t regBit);

bool SiiCbusWriteBurst(void);

//------------------------------------------------------------------------------
// Function:    SiMhlRxSendEdidChange
//------------------------------------------------------------------------------
bool SiMhlRxSendEdidChange(void);

//------------------------------------------------------------------------------
// Function:    SiiGrtWrt
//------------------------------------------------------------------------------
bool SiiCbusGrtWrt(void);

//------------------------------------------------------------------------------
// Function:    SiiReqWrt
//------------------------------------------------------------------------------
bool SiiCbusReqWrt(void);

//------------------------------------------------------------------------------
// Function:    SiiMhlRxSendDcapChange
//------------------------------------------------------------------------------
bool SiiMhlRxSendDcapChange(void);

//------------------------------------------------------------------------------
// Function:    SiiDscrChange
//------------------------------------------------------------------------------
bool SiiCbusSendDscrChange(void);

//------------------------------------------------------------------------------
// Function:    SiiSendDcapRdy
//------------------------------------------------------------------------------
bool SiiCbusSendDcapRdy(void);

//------------------------------------------------------------------------------
// Function:    SiiMhlRxPathEnable
// Description: Check if the channel is an active channel
//------------------------------------------------------------------------------
bool SiiMhlRxPathEnable(bool enable);

//------------------------------------------------------------------------------
// Function:    SiiCbusSendMscCommand
// Description: sends general MSC commands
//------------------------------------------------------------------------------
bool SiiCbusSendMscCommand(uint8_t cmd);

//------------------------------------------------------------------------------
// Function:    SiiMhlRxWrtPeersScratchpad
// Description: sends MHL write burst cmd
//------------------------------------------------------------------------------
bool SiiCbusWritePeersScratchpad(uint8_t startOffset, uint8_t length,
				   uint8_t *pMsgData);

//------------------------------------------------------------------------------
// Function:    SiiReadDevCapReg
// Description: Read device capability register
//------------------------------------------------------------------------------
bool SiiMhlRxReadDevCapReg(uint8_t regOffset);

//------------------------------------------------------------------------------
// Function:    SiMhlRxHpdSet
// Description: Send MHL_SET_HPD to source
// parameters:	setHpd - true/false
//------------------------------------------------------------------------------
bool SiMhlRxHpdSet(bool setHpd);

//------------------------------------------------------------------------------
// Function:    SiMhlRxMscCmdRetDataNtfy
// Description: Response data received from peer in response to an MSC command
//------------------------------------------------------------------------------
void SiMhlRxMscCmdRetDataNtfy(uint8_t mscData);

//------------------------------------------------------------------------------
// Function:    SiiMhlRxConnNtfy
// Description: This is a notification API for Cbus connection change, prototype
//				is defined in si_cbus_component.h
//------------------------------------------------------------------------------
void SiiMhlRxConnNtfy(bool connected);

//------------------------------------------------------------------------------
// Function:    SiiMhlRxScratchpadWrittenNtfy
// Description: This is a notification API for scratchpad bein written by peer
//------------------------------------------------------------------------------
void SiiMhlRxScratchpadWrittenNtfy(void);

//------------------------------------------------------------------------------
// Function:    SiiMhlRxRcpRapRcvdNtfy
// Description: process RCP/RAP msg
//------------------------------------------------------------------------------
void SiiMhlRxRcpRapRcvdNtfy(uint8_t cmd, uint8_t rcvdCode);

#if !defined(__KERNEL__)
void SiiCbuschkTimers(void);
#else
//------------------------------------------------------------------------------
// Function:    SiiCbusAbortTimerStart
//------------------------------------------------------------------------------
void SiiCbusAbortTimerStart(void);
#endif

//------------------------------------------------------------------------------
// Function:    SiiCbusAbortStateSet
//------------------------------------------------------------------------------
void SiiCbusAbortStateSet(bool value);

//------------------------------------------------------------------------------
// Function:    SiiCbusAbortStateGet
//------------------------------------------------------------------------------
bool SiiCbusAbortStateGet(void);

uint8_t SiiCbusRemoteDcapGet(uint8_t offset);

#endif // __SI_CBUS_COMPONENT_H__
