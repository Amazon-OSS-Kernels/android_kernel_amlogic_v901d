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

#ifndef SII_VIDEOMODEDETECTION_H
#define SII_VIDEOMODEDETECTION_H

#include "si_common.h"
// VMD_GetVsifPacketType() return type
enum {
	NOT_HDMI_VSIF,		 // VSIF packet is not HDMI VSIF
	NEW_EXTENDED_RESOLUTION, // VSIF packet carries Extended Resolution
				 // info: first detection
	OLD_EXTENDED_RESOLUTION, // VSIF packet carries Extended Resolution
				 // info: no change
	NEW_3D,			 // VSIF packet with 3D info: first detection
	OLD_3D,	   // VSIF packet with 3D info: no change from last time
	BLANK_VSIF // VSIF packet with blank data, mean no hdmi vsif
};

struct sync_info_type {
	uint16_t ClocksPerLine;	  // number of pixel clocks per line
	uint16_t TotalLines;	  // number of lines
	uint16_t PixelFreq;	  // pixel frequency in 10kHz units
	uint8_t Interlaced : 1; // true for interlaced video
	uint8_t HPol : 1; // true on negative polarity for horizontal pulses
	uint8_t VPol : 1; // true on negative polarity for vertical pulses
};

#if defined(__KERNEL__)
#define VIDEO_STABLE_TIME 200
#else
#define VIDEO_STABLE_TIME 500
#endif

void VMD_ResetTimingData(void);

void VMD_ResetInfoFrameData(void);

uint8_t VMD_GetVideoIndex(void);

// returns true if video format is detected, false otherwise
bool VMD_DetectVideoResolution(void);

bool VMD_WasResolutionChanged(void);

void VMD_OnAviPacketReceiving(uint8_t cea861vic);

void VMD_VsifProcessing(uint8_t *p_packet, uint8_t length);

void VMD_HdmiVsifProcessing(uint8_t *p_packet, uint8_t length);

void VMD_MhlVsifProcessing(uint8_t *p_packet, uint8_t length);

uint32_t VMD_GetVsifPacketType(uint8_t *p_packet, uint8_t length);

uint32_t VMD_GetHdmiVsifPacketType(uint8_t *p_packet,
					      uint8_t length);

uint32_t VMD_GetMhlVsifPacketType(uint8_t *p_packet, uint8_t length);

void VMD_OnHdmiVsifPacketDiscontinuation(void);

void VMD_Init(void);

uint16_t VMD_GetPixFreq10kHz(void);

void SiiRxSetVideoStableTimer(void);

#if !defined(__KERNEL__)
void SiiRxFormatDetect(void);
#endif

void RX_ConfigureGpioAs3dFrameIndicator(void);

#if defined(__KERNEL__)
char *SiiRx3DTypeGet(void);
#endif

#endif // SII_VIDEOMODEDETECTION_H
