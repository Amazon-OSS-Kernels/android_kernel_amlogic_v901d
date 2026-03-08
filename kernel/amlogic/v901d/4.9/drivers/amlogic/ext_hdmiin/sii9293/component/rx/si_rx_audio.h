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

#ifndef SIIRXAUDIO_H
#define SIIRXAUDIO_H

#include "si_rx_info.h"

enum {
	RX_CFG_128FS = 0,
	RX_CFG_256FS = 1,
	RX_CFG_384FS = 2,
	RX_CFG_512FS = 3,
};

#define SI_AUDIO_ST_CH_LEN 5
struct SiiRxAudioFormat_t {

	uint8_t audioLayout : 1; //!< incoming HDMI audio layout (0 or 1)

	uint8_t
	    audioEncoded : 1; //!< true for encoded audio, false for PCM and DSD
	uint8_t
	    audioChannelAllocation; //!< audio Channel Allocation (See CEA-861D)
	uint8_t
	    audioStatusChannel[SI_AUDIO_ST_CH_LEN]; //!< first 5 bytes of Audio
						    //!< Status Channel
};

void RxAudio_OnAudioInfoFrame(uint8_t *p_data, uint8_t length);
void RxAudio_OnChannelStatusChange(void);
void RxAudio_Init(void);
void RxAudio_Stop(void);
void RxAudio_ReStart(void);
void RxAudio_OnAcpPacketUpdate(uint32_t acp_type);

#endif // SIIRXAUDIO_H
