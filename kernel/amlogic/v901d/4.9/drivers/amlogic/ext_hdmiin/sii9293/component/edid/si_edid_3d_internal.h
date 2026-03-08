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

#ifndef __SI_EDID_3D_INTERNAL_H__
#define __SI_EDID_3D_INTERNAL_H__

#include "si_common.h"

#define HDMI_3D_SVD_STRUCTURE_LENGTH 16 // maximum by HDMI 1.4 spec

struct svd_t {
	uint8_t Vic : 7;
	uint8_t Native : 1;
};

union VIC3DFormat_t {
	uint8_t Data;
	struct {
		uint8_t FrameSequential : 1; // FS_SUPP
		uint8_t TopBottom : 1;       // TB_SUPP
		uint8_t LeftRight : 1;       // LR_SUPP
	} Fields;

};

struct Mandatory3dFmt_t {
	struct svd_t VicCode;
	union VIC3DFormat_t vic3dFmt;
};

#define Mandatory3dFmt_60 3
#define Mandatory3dFmt_50 3

// VIC in mandatory 3D formats

#define VIC_1080P_24 32
#define VIC_1080i_50 20
#define VIC_1080i_60 5
#define VIC_720P_50 19
#define VIC_720P_60 4

// MHL 3D formats

#define FRAME_SEQUENTIAL 0x01
#define TOP_BOTTOM 0x02
#define LEFT_RIGHT 0x04

#endif // __SI_EDID_3D_INTERNAL_H__
