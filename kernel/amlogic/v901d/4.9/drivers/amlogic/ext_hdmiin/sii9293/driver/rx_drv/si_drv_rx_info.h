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

#ifndef __SI_DRV_RX_INFO_H__
#define __SI_DRV_RX_INFO_H__

#include "si_common.h"

// muting during HDCP authentication
//#define RX_OUT_AV_MUTE_M__HDCP_RX				BIT0

// Propagation of incoming AV Mute packet from upstream to downstream
#define RX_OUT_AV_MUTE_M__INP_AV_MUTE_CAME BIT1

// mute due to RX chip is not ready
#define RX_OUT_AV_MUTE_M__RX_IS_NOT_READY BIT2

// mute because of no AVI packet coming and therefore input color space is
// unknown
#define RX_OUT_AV_MUTE_M__NO_AVI BIT4

// mute due to an HDCP error
#define RX_OUT_AV_MUTE_M__RX_HDCP_ERROR BIT7

enum {
	INFO_AVI = 0x00,
	INFO_SPD,
	INFO_AUD,
	INFO_MPEG,
	INFO_UNREC,
	INFO_ACP,
	INFO_VSI,
};

void RxInfo_InterruptHandler(uint8_t info_type);

#endif // __SI_DRV_RX_INFO_H__
