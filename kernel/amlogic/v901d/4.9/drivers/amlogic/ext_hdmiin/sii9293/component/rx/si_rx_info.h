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

#ifndef __SI_RX_INFO_H__
#define __SI_RX_INFO_H__

#include "si_drv_rx_info.h"

#define IF_BUFFER_LENGTH 31

#define IF_HEADER_LENGTH 4
#define IF_TITLE_INDEX 0
#define IF_VERSION_INDEX 1
#define IF_LENGTH_INDEX 2
#define IF_CHECKSUM_INDEX 3
#define IF_DATA_INDEX IF_HEADER_LENGTH

#define IF_MIN_AVI_LENGTH 13   // the 861C standard defines the length as 13
#define IF_MAX_AVI_LENGTH 15   // maximum allowed by the chip
#define IF_MIN_AUDIO_LENGTH 10 // the 861C standard defines the length as 10
#define IF_MAX_AUDIO_LENGTH 10 // maximum allowed by the chip
#define IF_MIN_MPEG_LENGTH 10  // the 861C standard defines the length as 10
#define IF_MAX_MPEG_LENGTH 27  // maximum allowed by the chip
#define IF_MIN_SPD_LENGTH 25   // the 861C standard defines the length as 25
#define IF_MAX_SPD_LENGTH 27   // maximum allowed by the chip
#define IF_MIN_VSIF_LENGTH 4   // minimum length by HDMI 1.4
#define IF_MAX_VSIF_LENGTH 27  // maximum allowed by the chip

/**
 * @brief InfoFrame/InfoPacket codes
 */
enum {
	SII_IF_AVI = 0x82,   //!< AVI InfoFrame
	SII_IF_SPD = 0x83,   //!< SPD InfoFrame
	SII_IF_AUDIO = 0x84, //!< Audio InfoFrame
	SII_IF_MPEG = 0x85,  //!< MPEG InfoFrame
	SII_IF_ISRC1 = 0x05, //!< ISRC1 InfoPacket
	SII_IF_ISRC2 = 0x06, //!< ISRC2 InfoPacket
	SII_IF_ACP = 0x04,   //!< ACP InfoPacket
	SII_IF_GC = 0x03,    //!< General Control InfoPacket
	SII_IF_GBD = 0x0A,   //!< GBD InfoPacket
	SII_IF_VSIF = 0x81,  //!< VSIF InfoFrame
};

enum {
	acp_GeneralAudio = 0,
	acp_IEC60958 = 1,
	acp_DvdAudio = 2,
	acp_SuperAudioCD = 3
};

#define AVI_LENGTH 5

enum {
	ColorSpace_RGB = 0,
	ColorSpace_YCbCr422,
	ColorSpace_YCbCr444
};

enum {
	Colorimetry_NoInfo,
	Colorimetry_ITU601,
	Colorimetry_ITU709,
	Colorimetry_Extended, // if extended, but unknown
	Colorimetry_xv601 = 10,
	Colorimetry_xv709,
};

enum {
	Range_Default = 0, // For RGB, default means full range for IT formats,
			   // and limited range for CE formats
	Range_Limited,
	Range_Full,
};

void RxInfo_NoAviHandler(void);

void RxInfo_NoVsiHandler(void);

void RxInfo_ResetData(void);

void RxInfo_ResetAudioInfoFrameData(void);

bool is_check_sum_correct(uint8_t *p_data, uint8_t length);

void on_spd_receiving(uint8_t packet[IF_BUFFER_LENGTH]);

void on_vsif_receiving(uint8_t packet[IF_BUFFER_LENGTH]);

void on_acp_receiving(uint8_t packet[IF_BUFFER_LENGTH]);

void on_avi_receiving(uint8_t packet[IF_BUFFER_LENGTH]);

void on_aud_receiving(uint8_t packet[IF_BUFFER_LENGTH]);

uint8_t RxAVI_GetVic(void);
uint8_t RxAVI_GetReplication(void);
uint32_t RxAVI_GetColorimetry(void);
uint32_t RxAVI_GetColorSpace(void);

void RxAVI_StoreData(uint8_t *p_data);
void RxAVI_ResetData(void);

uint32_t RxAVI_GetRangeQuantization(void);
#endif // __SI_RX_INFO_H__
