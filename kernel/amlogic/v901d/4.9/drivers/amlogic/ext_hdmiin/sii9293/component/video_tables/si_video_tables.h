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

#ifndef SI_VIDEO_TABLES_H
#define SI_VIDEO_TABLES_H

#include "si_common.h"

struct sizesPix_t {
	uint16_t H; // Number of horizontal pixels
	uint16_t V; // Number of vertical pixels

};

struct videoMode_t {
	uint8_t
	    Vic4x3; // CEA VIC for 4:3 picture aspect rate, 0 if not available
	uint8_t
	    Vic16x9; // CEA VIC for 16:9 picture aspect rate, 0 if not available
	uint8_t HdmiVic;   // HDMI VIC for 16:9 picture aspect rate, 0 if not
			   // available
	struct sizesPix_t Active; // Number of active pixels
	struct sizesPix_t Total;  // Total number of pixels
	struct sizesPix_t Blank;  // Number of blank pixels
	struct sizesPix_t SyncOffset;	  // Offset of sync pulse
	struct sizesPix_t SyncWidth;	  // Width of sync pulse
	uint16_t HFreq;		  // in kHz
	uint8_t VFreq;		  // in Hz
	uint16_t PixFreq;	  // in MHz
	uint16_t PixClk;	  // in 10kHz units
	uint8_t Interlaced : 1; // true for interlaced video
	uint8_t HPol : 1; // true on negative polarity for horizontal pulses
	uint8_t VPol : 1; // true on negative polarity for vertical pulses

	uint8_t
	    NtscPal : 2;    // 60/120/240Hz (false) or 50/100/200Hz (true) TVs
	uint8_t Repetition; // Allowed video pixel repetition
	uint8_t MaxAudioSR8Ch; // maximum allowed audio sample rate for 8
			       // channel audio in kHz

};

#define NMB_OF_CEA861_VIDEO_MODES 48
#define NMB_OF_HDMI_VIDEO_MODES 4
#define NMB_OF_VIDEO_MODES (NMB_OF_CEA861_VIDEO_MODES + NMB_OF_HDMI_VIDEO_MODES)

// repetition factor
#define RP1 0x01 // x1 (no repetition)
#define RP2 0x02 // x2 (doubled)
#define RP4 0x04 // x4
#define RP5 0x08 // x5
#define RP7 0x10 // x7
#define RP8 0x20 // x8
#define RP10 0x40 // x10

#define PROG 0 // progressive scan
#define INTL 1 // interlaced scan
#define POS 0 // positive pulse
#define NEG 1 // negative pulse

#define NTSC 1 // NTSC system (60Hz)
#define PAL 2 // PAL system (50Hz)

#define SI_VIDEO_MODE_NON_STD ((uint8_t)(-1)) //!< Non-standard video format ID
#define SI_VIDEO_MODE_PC_OTHER ((uint8_t)(-2))
#define SI_VIDEO_MODE_NOT_STABLE ((uint8_t)(-3))
#define SI_VIDEO_MODE_3D_RESOLUTION_MASK 0x80

extern ROM const struct videoMode_t VideoModeTable[NMB_OF_VIDEO_MODES + 1];

#define LAST_KNOWN_HDMI_VIC 4
extern ROM const uint8_t hdmiVicToVideoTableIndex[LAST_KNOWN_HDMI_VIC + 1];

#define LAST_KNOWN_CEA_VIC 64
extern ROM const uint8_t ceaVicToVideoTableIndex[LAST_KNOWN_CEA_VIC + 1];

// indexes in ceaVicToVideoTableIndex[]
enum {
	vm1_640x480p = 0,	    // 0
	vm2_3_720x480p,		    // 1
	vm4_1280x720p,		    // 2
	vm5_1920x1080i,		    // 3
	vm6_7_720_1440x480i,	    // 4
	vm8_9_720_1440x240p_1,	    // 5
	vm8_9_720_1440x240p_2,	    // 6
	vm10_11_2880x480i,	    // 7
	vm12_13_2880x240p_1,	    // 8
	vm12_13_2880x240p_2,	    // 9
	vm14_15_1440x480p,	    // 10
	vm16_1920x1080p,	    // 11
	vm17_18_720x576p,	    // 12
	vm19_1280x720p,		    // 13
	vm20_1920x1080i,	    // 14
	vm21_22_720_1440x576i,	    // 15
	vm23_24_720_1440x288p_1,    // 16
	vm23_24_720_1440x288p_2,    // 17
	vm23_24_720_1440x288p_3,    // 18
	vm25_26_2880x576i,	    // 19
	vm27_28_2880x288p_1,	    // 20
	vm27_28_2880x288p_2,	    // 21
	vm27_28_2880x288p_3,	    // 22
	vm29_30_1440x576p,	    // 23
	vm31_1920x1080p,	    // 24
	vm32_1920x1080p,	    // 25
	vm33_1920x1080p,	    // 26
	vm34_1920x1080p,	    // 27
	vm35_36_2880x480p,	    // 28
	vm37_38_2880x576p,	    // 29
	vm39_1920x1080i_1250_total, // 30
	vm40_1920x1080i,	    // 31
	vm41_1280x720p,		    // 32
	vm42_43_720x576p,	    // 33
	vm44_45_720_1440x576i,	    // 34
	vm46_1920x1080i,	    // 35
	vm47_1280x720p,		    // 36
	vm48_49_720x480p,	    // 37
	vm50_51_720_1440x480i,	    // 38
	vm52_53_720x576p,	    // 39
	vm54_55_720_1440x576i,	    // 40
	vm56_57_720x480p,	    // 41
	vm58_59_720_1440x480i,	    // 42

	vm60_12800x720p,  // 43
	vm61_12800x720p,  // 44
	vm62_12800x720p,  // 45
	vm63_19200x1080p, // 46
	vm64_19200x1080p, // 47

	vm_numVideoModes
};

#endif // SI_VIDEO_TABLES_H
