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

#if !defined(MHL_DRIVER_H)
#define MHL_DRIVER_H

#include "sii_hal.h"
#include <linux/device.h>
#include "si_rx_video_mode_detection.h"
#include <linux/of.h>
#include <linux/of_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif /* _defined (__cplusplus) */

/***** macro definitions *****************************************************/
#if defined(MAKE_5293_DRIVER)

#define MHL_DRIVER_NAME "sii5293drv"
#define MHL_DEVICE_NAME "sii-5293"
#define CLASS_NAME "sii9293"
#define DEVNAME "sii5293"
#define MHL_DEVNAME "mhl"

#define NUMBER_OF_DEVS 2

#define DEVICE_EVENT "DEVICE_EVENT"
#define MHL_EVENT "MHL_EVENT"

/* Device events */
#define DEV_CONNECTION_CHANGE_EVENT "connection_change"
#define DEV_INPUT_VIDEO_MODE_EVENT "input_video_stable"

/* MHL events */
#define MHL_CONNECTED_EVENT "connected"
#define MHL_DISCONNECTED_EVENT "disconnected"
#define MHL_RAP_RECEIVED_EVENT "received_rap"
#define MHL_RAP_ACKED_EVENT "received_rapk"
#define MHL_RCP_RECEIVED_EVENT "received_rcp"
#define MHL_RCP_ACKED_EVENT "received_rcpk"
#define MHL_RCP_ERROR_EVENT "received_rcpe"
#define MHL_UCP_RECEIVED_EVENT "received_ucp"
#define MHL_UCP_ACKED_EVENT "received_ucpk"
#define MHL_UCP_ERROR_EVENT "received_ucpe"

#else

#error "Need to add name and description strings for new drivers here!"

#endif

#define MHL_DRIVER_MINOR_MAX 1

/***** public type definitions ***********************************************/
enum {
	GPIOZ_0 = 0,
	GPIOZ_1 = 1,
	GPIOZ_2 = 2,
	GPIOZ_3 = 3,
	GPIOZ_4 = 4,
	GPIOZ_5 = 5,
	GPIOZ_6 = 6,
	GPIOZ_7 = 7,
	GPIOZ_8 = 8,
	GPIOZ_9 = 9,
	GPIOZ_10 = 10,

	GPIOH_0 = 11,
	GPIOH_1 = 12,
	GPIOH_2 = 13,
	GPIOH_3 = 14,
	GPIOH_4 = 15,
	GPIOH_5 = 16,
	GPIOH_6 = 17,
	GPIOH_7 = 18,
	GPIOH_8 = 19,
	GPIOH_9 = 20,
	GPIOH_10 = 21,
	GPIOH_11 = 22,
	GPIOH_12 = 23,
	GPIOH_13 = 24,
	GPIOH_14 = 25,
	GPIOH_15 = 26,
	GPIOH_16 = 27,
	GPIOH_17 = 28,
	GPIOH_18 = 29,
	GPIOH_19 = 30,
	GPIOH_20 = 31,
	GPIOH_21 = 32,
	GPIOH_22 = 33,
	GPIOH_23 = 34,
	GPIOH_24 = 35,

	BOOT_0 = 36,
	BOOT_1 = 37,
	BOOT_2 = 38,
	BOOT_3 = 39,
	BOOT_4 = 40,
	BOOT_5 = 41,
	BOOT_6 = 42,
	BOOT_7 = 43,
	BOOT_8 = 44,
	BOOT_9 = 45,
	BOOT_10 = 46,
	BOOT_11 = 47,
	BOOT_12 = 48,
	BOOT_13 = 49,

	GPIOC_0 = 50,
	GPIOC_1 = 51,
	GPIOC_2 = 52,
	GPIOC_3 = 53,
	GPIOC_4 = 54,
	GPIOC_5 = 55,
	GPIOC_6 = 56,
	GPIOC_7 = 57,
	GPIOC_8 = 58,
	GPIOC_9 = 59,
	GPIOC_10 = 60,
	GPIOC_11 = 61,
	GPIOC_12 = 62,
	GPIOC_13 = 63,
	GPIOC_14 = 64,

	GPIOW_0 = 65,
	GPIOW_1 = 66,
	GPIOW_2 = 67,
	GPIOW_3 = 68,
	GPIOW_4 = 69,
	GPIOW_5 = 70,
	GPIOW_6 = 71,
	GPIOW_7 = 72,
	GPIOW_8 = 73,
	GPIOW_9 = 74,
	GPIOW_10 = 75,
	GPIOW_11 = 76,

	GPIODV_0 = 77,
	GPIODV_1 = 78,
	GPIODV_2 = 79,
	GPIODV_3 = 80,
	GPIODV_4 = 81,
	GPIODV_5 = 82,
	GPIODV_6 = 83,
	GPIODV_7 = 84,
	GPIODV_8 = 85,
	GPIODV_9 = 86,
	GPIODV_10 = 87,
	GPIODV_11 = 88,
	GPIO_MAX,
};

enum {
	MHL_CONN = 1, // default value is 0, so that when comes the notify first
		      // time will always effective.
	HDMI_CONN,
	NO_CONN,
};

struct MHL_DRIVER_CONTEXT_T {
	uint8_t chip_revision;	// chip revision
	bool pwr5v_state;	// power 5v state
	bool mhl_cable_state; // mhl cable state
	uint32_t connection_state;
	uint8_t input_video_mode; // last determined video mode
	uint8_t avi_vic;	  // vic value from AVI Inforframe
	uint8_t debug_i2c_address;
	uint8_t debug_i2c_offset;
	uint8_t debug_i2c_xfer_length;
	uint8_t devcap_remote_offset; // last Device Capability register
	uint8_t devcap_local_offset;  // last Device Capability register
	uint8_t rap_in_keycode;	      // last RAP key code received.
	uint8_t rap_out_keycode;      // last RAP key code transmitted.
	uint8_t rap_out_statecode;    // last RAP state code transmitted
	uint8_t rcp_in_keycode;	      // last RCP key code received.
	uint8_t rcp_out_keycode;      // last RCP key code transmitted.
	uint8_t rcp_out_statecode;    // last RCP state code transmitted
	uint8_t ucp_in_keycode;	      // last UCP key code received.
	uint8_t ucp_out_keycode;      // last UCP key code transmitted.
	uint8_t ucp_out_statecode;    // last UCP state code transmitted
};

struct mhl_device_info {
	dev_t devnum;
	struct cdev *cdev;
	struct device *device;
};

struct sii5293_config {
	/* data */
	unsigned int i2c_bus_index;
	unsigned int gpio_intr;			 // interrupt pin
	enum of_gpio_flags gpio_intr_irq_flags;	 // interrupt pin irq flag
	unsigned int gpio_reset;		 // hardware reset pin
	enum of_gpio_flags gpio_reset_irq_flags; // hardware reset pin irq flag
};

struct device_info {
	dev_t devnum;
	struct cdev *cdev;
	struct device *device;
	struct class *dev_class;

	struct mhl_device_info *mhl;
	struct sii5293_config config;
	uint8_t my_rap_input_device;
	uint8_t my_rcp_input_device;
	uint8_t my_ucp_input_device;
};

/***** global variables ********************************************/

extern struct MHL_DRIVER_CONTEXT_T gDriverContext;
extern struct device_info *devinfo;

/***** public function prototypes ********************************************/
/**
 * \defgroup driver_public_api Driver Public API
 * @{
 */
int send_sii5293_uevent(struct device *device, const char *event_cat,
			const char *event_type, const char *event_data);

void SiiConnectionStateNotify(bool connect);

#ifdef __cplusplus
}
#endif /* _defined (__cplusplus) */

#endif /* _defined (MHL_DRIVER_H) */
