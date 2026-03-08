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
#include "si_cra.h"

#include "si_sii5293_registers.h"

/*****************************************************************************/
/**
 *  @brief		Process One InfoFrame Packet
 *
 *  @param[in]		packet_address		packet address
 *
 *****************************************************************************/
static void process_one_packet(uint16_t packet_address)
{
	uint8_t d[IF_BUFFER_LENGTH];

	uint8_t packet_title = SiiRegRead(packet_address);

	switch (packet_title) {
	case SII_IF_AVI:
		SiiRegReadBlock(packet_address, d,
				IF_MAX_AVI_LENGTH + IF_HEADER_LENGTH);
		on_avi_receiving(d);
		break;
	case SII_IF_AUDIO:
		SiiRegReadBlock(packet_address, d,
				IF_MAX_AUDIO_LENGTH + IF_HEADER_LENGTH);
		on_aud_receiving(d);
		break;
	case SII_IF_SPD:
		SiiRegReadBlock(packet_address, d,
				IF_MAX_SPD_LENGTH + IF_HEADER_LENGTH);
		on_spd_receiving(d);
		break;
	case SII_IF_ACP:
		SiiRegReadBlock(packet_address, d, IF_BUFFER_LENGTH);
		on_acp_receiving(d);
		break;
	case SII_IF_VSIF:
		SiiRegReadBlock(packet_address, d, IF_BUFFER_LENGTH);
		//		DEBUG_PRINT(MSG_ALWAYS, ("Got VSIF in
		//process_one_packet\n"));
		on_vsif_receiving(d);
		break;
	}
}

/*****************************************************************************/
/**
 *  @brief		Handler for InfoFrame Interrupts
 *
 *  @param[in]		info_frame_interrupts		InfoFrame Interrupts
 *
 *****************************************************************************/
void RxInfo_InterruptHandler(uint8_t info_type)
{
	switch (info_type) {
	case INFO_AVI:
		process_one_packet(RX_A__AVI_TYPE);
		break;
	case INFO_AUD:
		process_one_packet(RX_A__AUD_TYPE);
		break;
	case INFO_ACP:
		process_one_packet(RX_A__ACP_BYTE1);
		break;
	case INFO_VSI:
		process_one_packet(RX_A__VSI_TYPE);
		break;
	default:
		break;
	}
}
