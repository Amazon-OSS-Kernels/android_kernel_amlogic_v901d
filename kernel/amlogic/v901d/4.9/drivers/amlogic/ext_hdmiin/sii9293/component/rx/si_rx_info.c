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

#include "si_rx_info.h"
#include "si_drv_rx_isr.h"
#include "si_rx_audio.h"
#include "si_rx_video_mode_detection.h"
#include "si_drv_rx.h"
#ifdef SK_TX_EVITA
#include "si_drv_evita.h"
#endif

#define SPD_BUFFER_LENGTH IF_MIN_SPD_LENGTH

// -----------------------------------------------
struct list_of_packets_type {
	uint8_t AVI : 1;
	uint8_t SPD : 1;
	uint8_t Audio : 1;
	uint8_t VSIF : 1;
	uint8_t ISRC1 : 1;
	uint8_t ISRC2 : 1;
	uint8_t ACP : 1;
	uint8_t GBD : 1;
};

// -----------------------------------------------
struct rx_info_type {
	struct list_of_packets_type received_packets;
	struct list_of_packets_type failed_packets;
	uint32_t acp_type;

	uint8_t spd_buffer[SPD_BUFFER_LENGTH];
};

// -----------------------------------------------
static struct rx_info_type rx_info = {
	{0},
};
static uint8_t avi_data[AVI_LENGTH] = {0};
static uint8_t avi_version = {0};

// -----------------------------------------------
bool is_check_sum_correct(uint8_t *p_data, uint8_t length)
{
	uint8_t i;
	uint8_t check_sum = 0;

	for (i = 0; i < length; i++, p_data++)
		check_sum += *p_data;
	return check_sum == 0;
}

// -----------------------------------------------
void on_spd_receiving(uint8_t packet[IF_BUFFER_LENGTH])
{
	uint8_t data_length = packet[IF_LENGTH_INDEX];

	if ((data_length >= IF_MIN_SPD_LENGTH) &&
	    (data_length <= IF_MAX_SPD_LENGTH) &&
	    is_check_sum_correct(packet, data_length + IF_HEADER_LENGTH)) {
		// The packet looks valid.
		rx_info.received_packets.SPD = true;

		memcpy(rx_info.spd_buffer, packet, SPD_BUFFER_LENGTH);

		DEBUG_PRINT(MSG_STAT, ("Got SPD\n"));
	} else {
		// The packet is invalid.
		rx_info.failed_packets.SPD = true;
	}
}

// -----------------------------------------------
void on_vsif_receiving(uint8_t packet[IF_BUFFER_LENGTH])
{
	uint8_t data_length = packet[IF_LENGTH_INDEX];

	// DEBUG_PRINT(MSG_STAT, ("on_vsif_receiving\n"));

	// checksum and length verification
	if ((data_length >= IF_MIN_VSIF_LENGTH) &&
	    (data_length <= IF_MAX_VSIF_LENGTH) &&
	    is_check_sum_correct(packet, data_length + IF_HEADER_LENGTH)) {
		bool packet_new = false;

		// DEBUG_PRINT(MSG_STAT, ("valid vsif\n"));

		rx_info.received_packets.VSIF = true;
		switch (VMD_GetVsifPacketType(&packet[IF_HEADER_LENGTH],
					      packet[IF_LENGTH_INDEX])) {
		case NEW_3D:
		case NEW_EXTENDED_RESOLUTION:
			packet_new = true;
			DEBUG_PRINT(MSG_STAT,
				    ("Got NEW 3D / Extended Resolution\n"));
			VMD_VsifProcessing(&packet[IF_HEADER_LENGTH],
					   packet[IF_LENGTH_INDEX]);
			break;
		case BLANK_VSIF:
			RxInfo_NoVsiHandler();
			break;
		default:
			break;
		}
	}
}

// -----------------------------------------------
void RxInfo_NoAviHandler(void) { DEBUG_PRINT(MSG_STAT, ("RX: No AVI\n")); }

// -----------------------------------------------
void RxInfo_NoVsiHandler(void)
{
	DEBUG_PRINT(MSG_STAT, ("RX: No VSIF.\n"));
	VMD_OnHdmiVsifPacketDiscontinuation();
}

// -----------------------------------------------
void RxInfo_ResetData(void)
{
	memset(&rx_info, 0, sizeof(rx_info));

	RxAVI_ResetData();
	rx_info.acp_type = acp_GeneralAudio;

	RxAudio_OnAcpPacketUpdate(rx_info.acp_type);

	VMD_ResetInfoFrameData();
}

// -----------------------------------------------
void RxInfo_ResetAudioInfoFrameData(void) { RxAudio_OnAudioInfoFrame(NULL, 0); }

void on_acp_receiving(uint8_t packet[IF_BUFFER_LENGTH])
{
	rx_info.received_packets.ACP = true;
	rx_info.acp_type = (uint32_t)packet[1];

	RxAudio_OnAcpPacketUpdate(rx_info.acp_type);
}

void on_avi_receiving(uint8_t packet[IF_BUFFER_LENGTH])
{
	uint8_t data_length = packet[IF_LENGTH_INDEX];

	if ((data_length >= IF_MIN_AVI_LENGTH) &&
	    (data_length <= IF_MAX_AVI_LENGTH) &&
	    is_check_sum_correct(packet, data_length + IF_HEADER_LENGTH)) {
		RxAVI_StoreData(packet);
		VMD_OnAviPacketReceiving(RxAVI_GetVic());
		SiiDrvRxVideoPathSet();

#ifdef SK_TX_EVITA
		SiiDrvEvitaAviIfUpdate();
#endif
		rx_info.received_packets.AVI = true;
		DEBUG_PRINT(MSG_STAT, ("New AVI\n"));
		// DEBUG_PRINT(MSG_STAT, ("%02X %02X %02X %02X %02X\n", (int)
		// d[0], (int) d[1], (int) d[2], (int) d[3], (int) d[4]));
	} else {
		rx_info.failed_packets.AVI = true;
	}
}

void on_aud_receiving(uint8_t packet[IF_BUFFER_LENGTH])
{
	uint8_t data_length = packet[IF_LENGTH_INDEX];

	if ((data_length >= IF_MIN_AUDIO_LENGTH) &&
	    (data_length <= IF_MAX_AUDIO_LENGTH) &&
	    is_check_sum_correct(packet, data_length + IF_HEADER_LENGTH)) {
		rx_info.received_packets.Audio = true;
#ifdef SK_TX_EVITA
		SiiDrvEvitaAudioIfUpdate(packet);
#endif
		RxAudio_OnAudioInfoFrame(&packet[IF_HEADER_LENGTH],
					 data_length);
	} else {
		rx_info.failed_packets.Audio = true;
	}
}

uint8_t RxAVI_GetVic(void) { return avi_data[3] & 0x7F; }

uint8_t RxAVI_GetReplication(void) { return avi_data[4] & 0x0F; }

uint32_t RxAVI_GetColorimetry(void)
{
	uint32_t c = (uint32_t)((avi_data[1] >> 6) & 0x03);

	if (Colorimetry_Extended == c) {
		uint8_t ec = (avi_data[2] >> 4) & 0x07;

		if (ec < 2)
			c = Colorimetry_xv601 + (uint32_t)ec;
		else
			c = Colorimetry_NoInfo;
	}
	return c;
}

uint32_t RxAVI_GetColorSpace(void)
{
	return (uint32_t)((avi_data[0] >> 5) & 0x03);
}

void RxAVI_StoreData(uint8_t *p_data)
{
	memcpy(avi_data, &p_data[IF_HEADER_LENGTH], AVI_LENGTH);

	avi_version = p_data[1];
	if (avi_version < 2) {
		avi_data[3] = 0; // VIC=0
		avi_data[4] = 0; // Repetition
	}
}

void RxAVI_ResetData(void)
{
	memset(&avi_data, 0, AVI_LENGTH);
	avi_version = 0;
}

uint32_t RxAVI_GetRangeQuantization(void)
{
	uint32_t type;

	if (ColorSpace_RGB == RxAVI_GetColorSpace()) {
		type = (avi_data[2] & 0x0C) >> 2;
		if (type > Range_Full)
			type = Range_Default;
	} else {
		type = ((avi_data[4] & 0xC0) >> 6) + 1;
		if (type > Range_Full)
			type = Range_Limited;
	}
	return type;
}
