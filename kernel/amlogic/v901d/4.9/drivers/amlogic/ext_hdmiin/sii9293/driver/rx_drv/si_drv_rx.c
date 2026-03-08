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

#include "si_cra.h"
#include "si_sii5293_registers.h"

#include "si_drv_rx.h"
#include "si_drv_rx_info.h"
#include "si_rx_audio.h"
#include "si_rx_info.h"
#include "si_drv_rx_isr.h"
#include "si_drv_device.h"

#include "si_video_tables.h"

#define RX_CTS_THRESHOLD 0x50	  // bits 3...6
#define SII_SCALED_XCLK 4096000UL // (2000 * 2048)

const uint8_t default_sval_regs[4] = {0x0F, 0x20, 0x00, 0x00};
static struct SiiRxTiming_t rxTimingInfo;
static struct SiiRxTiming_t *pRxTimingInfo = &rxTimingInfo;

void SiiDrvRxTimingInfoSet(struct SiiRxTiming_t *pNewRxTimingInfo)
{
	memcpy(pRxTimingInfo, pNewRxTimingInfo, sizeof(struct SiiRxTiming_t));
}

struct SiiRxTiming_t *SiiDrvRxTimingInfoGet(void) { return pRxTimingInfo; }

/*****************************************************************************/
/**
 *  @brief		Return RX Pixel Replicate
 *
 *  @return	RX RX Pixel Replicate
 *
 *****************************************************************************/
uint8_t SiiDrvRxGetPixelReplicate(void)
{
	uint8_t pix_repl =
	    (SiiRegRead(RX_A__SYS_CTRL1) & RX_M__SYS_CTRL1__ICLK) >> 4;
	return pix_repl;
}

/*****************************************************************************/
/**
 *  @brief		Return RX Video Status
 *
 *  @return	RX RX Video Status
 *
 *****************************************************************************/
uint8_t SiiDrvRxGetVideoStatus(void)
{
	uint8_t vid_stat_reg = SiiRegRead(RX_A__VID_STAT);
	return vid_stat_reg;
}

/*****************************************************************************/
/**
 *  @brief		Return RX Pixel Frequency in 10kHz units
 *
 *  @return	RX RX Pixel Frequency
 *
 *****************************************************************************/
uint16_t SiiDrvRxGetPixelFreq(void)
{
	uint16_t xcnt = 0;
	uint32_t pixel_freq = SII_SCALED_XCLK;

	xcnt = SiiRegReadWord(RX_A__VID_XPCNT0);
	xcnt &= 0x0FFF;

	if (xcnt == 0)
		pixel_freq = 65535;
	else
		pixel_freq /= xcnt;

	if (pixel_freq > 65535)
		pixel_freq = 65535;

	return (uint16_t)pixel_freq;
}

/*****************************************************************************/
/**
 *  @brief		Return RX Sync Information
 *
 *  @param[in]		p_sync_info		pointer to return data buffer
 *for sync information
 *
 *****************************************************************************/
void SiiDrvRxGetSyncInfo(struct sync_info_type *p_sync_info)
{
	uint8_t d[4];

	uint8_t pix_repl = SiiDrvRxGetPixelReplicate();
	// it is more reliable to read it from HW then from a shadow register
	// because the register could be sometimes reset to default value.

	uint8_t vid_stat_reg = SiiDrvRxGetVideoStatus();

	SiiRegReadBlock(RX_A__H_RESL, d, 4);

	p_sync_info->ClocksPerLine = d[0] | (d[1] << 8);
	p_sync_info->TotalLines = d[2] | (d[3] << 8);
	p_sync_info->ClocksPerLine *= pix_repl + 1;
	p_sync_info->PixelFreq = SiiDrvRxGetPixelFreq();
	p_sync_info->Interlaced =
	    (vid_stat_reg & RX_M__VID_STAT__INTERLACE) ? INTL : PROG;
	p_sync_info->HPol =
	    (vid_stat_reg & RX_M__VID_STAT__HSYNC_POL) ? POS : NEG;
	p_sync_info->VPol =
	    (vid_stat_reg & RX_M__VID_STAT__VSYNC_POL) ? POS : NEG;
}

/*****************************************************************************/
/**
 *  @brief		Return Status of Sync Detection
 *
 *  @return	Status
 *  @retval	true		Sync is detected
 *  @retval	false		Sync is not detected
 *
 *****************************************************************************/
bool SiiDrvRxIsSyncDetected(void)
{
	return 0 != (SiiRegRead(RX_A__STATE) & RX_M__STATE__SCDT);
}

/*****************************************************************************/
/**
 *  @brief		Return Status of HDMI Mode
 *
 *  @return	Status
 *  @retval	true		HDMI is detected
 *  @retval	false		HDMI is not detected
 *
 *****************************************************************************/
bool SiiDrvRxHdmiModeGet(void)
{
	// NOTE: this function checks HDMI_MODE_ENABLED but not
	// HDMI_MODE_DETECTED bit
	return 0 != (SiiRegRead(RX_A__AUDP_STAT) &
		     RX_M__AUDP_STAT__HDMI_MODE_ENABLED);
}

/*****************************************************************************/
/**
 *  @brief		Switch Video Mute
 *
 *  @param[in]		switch_on		true to switch on; false to
 *switch off
 *
 *****************************************************************************/
void SiiDrvRxMuteVideo(uint8_t switch_on)
{
	SiiRegBitsSet(RX_A__AUDP_MUTE, RX_M__AUDP_MUTE__VIDEO_MUTE, switch_on);
}

uint32_t SiiDrvRxGetOutVideoPath(void)
{
#if defined(__KERNEL__)
	if (output_format >= 0 && output_format < PATH__MAXVALUE)
		return output_format;
#endif
	return PATH__RGB;
}

/*****************************************************************************/
/**
 *  @brief		RX Initialization
 *
 *  @param[in]		inputIndex			input index
 *  @param[in]		startInStandbyMode	true to start in standby mode;
 *not if false
 *
 *  @return	Status
 *  @retval	true		Success
 *  @retval	false		Failure
 *
 *****************************************************************************/
bool SiiDrvRxInitialize(void)
{
	bool success = true;
#if 0
	uint32_t outVideoPath;
	uint8_t value;

	SiiRegBitsSet(RX_A__SWRST2, RX_M__SWRST2__AUDIO_FIFO_AUTO, true);
	SiiRegWrite(REG_RX_CTRL1, 0x3F);
	SiiRegWrite(RX_A__VSI_CTRL1, 0x0C);
	SiiRegWrite(RX_A__ACR_CTRL3,
		    RX_CTS_THRESHOLD | RX_M__ACR_CTRL3__MCLK_LOOPBACK);

	// init threshold for PLL unlock interrupt
	SiiRegWriteBlock(RX_A__LK_WIN_SVAL, default_sval_regs,
			 sizeof(default_sval_regs));

	// set Video bus width and Video data edge
	SiiRegModify(RX_A__SYS_CTRL1, RX_M__SYS_CTRL1__EDGE,
		     ((SI_INVERT_RX_OUT_PIX_CLOCK_BY_DEFAULT == ENABLE)
			  ? RX_M__SYS_CTRL1__EDGE
			  : 0));

	// AudioVideo Mute ON
	SiiRegWrite(RX_A__AUDP_MUTE,
		    RX_M__AUDP_MUTE__VIDEO_MUTE | RX_M__AUDP_MUTE__AUDIO_MUTE);

	// set BCH threshold and reset BCH counter
	SiiRegWrite(RX_A__BCH_THRES, 0x02);
	// Capture and clear BCH T4 errors.
	SiiRegWrite(RX_A__ECC_CTRL, RX_M__ECC_CTRL__CAPTURE_CNT);

	outVideoPath = SiiDrvRxGetOutVideoPath();

	switch (outVideoPath) {
	case PATH__RGB:
		value = 0x00;
		break;
	case PATH__YCbCr444:
		value = 0x80;
		break;
	case PATH__YCbCr422_16B:
		value = 0xC0;
		break;
	case PATH__YCbCr422_20B:
		value = 0xC8;
		break;
	case PATH__YCbCr422_16B_SYNC:
	case PATH__YCbCr422_MUX8B_SYNC:
		// AVC will do YCMUX only when input repetition is 2 or 4.
		// If input repetition is 1, YCMUX block will disable
		// automatically
		value = 0xF0;
		break;
	case PATH__YCbCr422_20B_SYNC:
	case PATH__YCbCr422_MUX10B_SYNC:
		value = 0xF8;
		break;
	case PATH__YCbCr422_MUX8B:
		value = 0xE0;
		break;
	case PATH__YCbCr422_MUX10B:
		value = 0xE8;
		break;
	default:
		value = 0x00;
		break;
	}
	SiiRegWrite(RX_A__VID_AOF, value);
	// enable AVC, default output is RGB, could be change via reigster
	// RX_A__VID_AOF.
	SiiRegWrite(RX_A__AEC_CTRL, RX_M__AEC_CTRL__AVC_EN);

	SiiRegWrite(REG_COMB_CTRL, 0x8C);
	SiiRegWrite(REG_DPLL_CFG3, 0x40);
	SiiRegWrite(REG_DPLL_BW_CFG2, 0x00);

	RxIsr_Init();

	RxAudio_Init();

	VMD_Init();

	// set HDCP Error Threshold
	SiiRegWriteWord(RX_A__ECC0_HDCP_THRES, RX_C__ECC0_HDCP_THRES_VALUE);

	SiiRegModify(RX_A__SYS_CTRL1, RX_M__SYS_CTRL1__PDALL,
		     SET_BITS); // Power on device
#else
	// FAE Phillip Keene Init Data
	SiiRegWrite(RX_A__SYS_CTRL1,
		    0x05); // 24/30/36bit bus mode &normal operation
	SiiRegWrite(RX_A__SWRST2, 0x40); // Auto audio FIFO reset whenever audio
					 // FIFO overflows or under runs
	SiiRegWrite(RX_A__SYS_SWTCH, 0x91); // enable DDC DSDA delay & DDC #0
					    // enable & TMDS core #0 enable
#if 0
	SiiDrvSwitchDeviceHpdControl(true, false); //  HPD on for temporary use
#endif

	SiiRegWrite(RX_A__PD_SYS3, 0x01); // audio normal operation
	SiiRegWrite(RX_A__I2S_CTRL2,
		    0xf9); // enable MCLKOUT/SD0/SD1/SD2/SD3 & pass data from
			   // SPDIF if it is PCM data otherwise output 0
	SiiRegWrite(RX_A__I2S_CTRL1, 0x40); // IIS format and timing configure
	SiiRegWrite(RX_A__AUDRX_CTRL,
		    0x04); // I2S output mode. 1 - SCK and WS toggle; SD is "on"
			   // or "off" depending on the value of the reg_sd0_en

	SiiDrvSoftwareReset(RX_M__SRST__SRST);
	SiiRegWrite(RX_A__AEC_CTRL, 0xe4);

	// device "Sequoia"
	SiiRegWrite(RX_A__VID_CTRL,
		    0x00); // video mode output color space select
	SiiRegWrite(RX_A__VID_MODE2, 0x00); // video output format select
	SiiRegWrite(RX_A__VID_MODE, 0x00);  // color space converter
	SiiRegWrite(RX_A__VID_AOF,
		    0x00); // video output format digital RGB output

	SiiRegWrite(REG_RX_CTRL1, 0x3f);
	SiiRegWrite(REG_RX_CTRL5,
		    0xc8); // these two registers need to set in es0.1 chip

	//Bit2:1 open drain   Bit1:1 trigger_low  0 trigger_high
	SiiRegWrite(RX_A__INT_CTRL, 0x4);//trigger_low

	RxIsr_Init();

	RxAudio_Init();
#endif
	return success;
}

/*****************************************************************************/
/**
 *  @brief		RX Software Reset
 *
 *  @param[in]		reset_mask		reset mask
 *
 *****************************************************************************/
void SiiDrvSoftwareReset(uint8_t reset_mask)
{
	uint8_t reg_val = SiiRegRead(RX_A__SRST);

	SiiRegWrite(RX_A__SRST, reg_val | reset_mask);
	SiiRegWrite(RX_A__SRST, reg_val & (~reset_mask));

	// Clear 5v plug-out interrupts if any
	SiiRegWrite(RX_A__INTR6, RX_M__INTR6__CABLE_UNPLUG);

	// Clear 5v plug-in interrupt if any
	SiiRegWrite(RX_A__INTR8, RX_M__INTR8__CABLE_IN);
}

#define AVC_RANGE_FIX
void SiiDrvRxVideoPathSet(void)
{
#ifdef AVC_RANGE_FIX
	uint32_t outVideoPath = SiiDrvRxGetOutVideoPath();

	if ((ColorSpace_RGB == RxAVI_GetColorSpace()) &&
	    (outVideoPath == PATH__RGB) &&
	    (SiiRegRead(RX_A__VID_BLANK1) == 0x10)) {
		// AVC will change the Blank level automatically, we can use it
		// to know if input is limited range or not if input is RGB and
		// limited range, Output is YCbCr, need to convert to RGB full
		// range, but AVC does not do this.
		SiiRegWrite(RX_A__AVC_EN1, RX_M__AVC_EN1__YCbCr2RGB_RANGE);
		SiiRegModify(RX_A__VID_MODE2,
			     RX_M__VID_MODE2__YCBCR_2_RGB_RANGE_EN, SET_BITS);

		// Also enable dither when doing color space range expansion
		SiiRegWrite(RX_A__AVC_EN2, RX_M__AVC_EN2__DITHER);
		SiiRegModify(RX_A__VID_MODE, RX_M__VID_MODE__DITHER, SET_BITS);
	} else if ((ColorSpace_RGB != RxAVI_GetColorSpace()) &&
		   (Range_Full == RxAVI_GetRangeQuantization())) {
		// Change the Black level, AVC setting is not correct here.
		SiiRegWrite(RX_A__AVC_EN1, CLEAR_BITS);
		SiiRegWrite(RX_A__AVC_EN2, RX_M__AVC_EN2__BLANK_DATA);
		SiiRegWrite(RX_A__VID_BLANK1,
			    (ColorSpace_YCbCr422 == RxAVI_GetColorSpace())
				? 0x00
				: 0x80);
		SiiRegWrite(RX_A__VID_BLANK2, 0x00);
		SiiRegWrite(RX_A__VID_BLANK3, 0x80);
		if (outVideoPath != PATH__RGB) {
			// if input is YCbCr and full range, output is YCbCr,
			// need to convert to limited range, but AVC does not do
			// this.
			SiiRegModify(RX_A__AVC_EN1,
				     RX_M__AVC_EN1__RGB2YCbCr_RANGE, SET_BITS);
			SiiRegModify(RX_A__VID_MODE,
				     RX_M__VID_MODE__RGB_2_YCBCR_RANGE,
				     SET_BITS);

			if (outVideoPath == PATH__YCbCr444 ||
			    outVideoPath == PATH__YCbCr422_16B ||
			    outVideoPath == PATH__YCbCr422_16B_SYNC ||
			    outVideoPath == PATH__YCbCr422_MUX8B ||
			    outVideoPath == PATH__YCbCr422_MUX8B_SYNC) {
				// YCbCr 422 10 bit, do not need to do dither
				// Also enable dither when doing color space
				// range compression
				SiiRegModify(RX_A__AVC_EN2,
					     RX_M__AVC_EN2__DITHER, SET_BITS);
				SiiRegModify(RX_A__VID_MODE,
					     RX_M__VID_MODE__DITHER, SET_BITS);
			}
		}
	} else {
		SiiRegWrite(RX_A__AVC_EN1, CLEAR_BITS);
		SiiRegWrite(RX_A__AVC_EN2, CLEAR_BITS);
	}
#endif
}
