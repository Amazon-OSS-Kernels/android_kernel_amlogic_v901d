/*
 * idme_default_table_lagunaf.h
 *
 * Copyright 2017-2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

/*!
 * @file idme_default_table_lagunaf.h
 * @brief This file contains idme default table values
 *  in the userstore partition
 *
 */

#ifndef __IDME_DEFAULT_TABLE_LAGUNAF_H__
#define __IDME_DEFAULT_TABLE_LAGUNAF_H__

const struct idme_init_values idme_default_values[] = {
	{ { "board_id", 16, 1, 0444 },
		/* Default Board ID value */
		"ffffff0000000000"
	},
	{ { "serial", 16, 1, 0444 },
		/* Default DSN value */
		"0"
	},
	{ { "mac_addr", 16, 1, 0444 },
		/* Default MAC address */
		"0"
	},
	{ { "mac_sec", 32, 1, 0444 },
		/* Default MAC secret */
		"0"
	},
	{ { "bt_mac_addr", 16, 1, 0444 },
		/* Default BT MAC address */
		"0"
	},
	{ { "product_name", 32, 1, 0444 },
		/* Product name, acos 2.4 */
		"0"
	},
	{ { "productid", 32, 1, 0444 },
		/* Default Primary Product ID */
		"0"
	},
	{ { "productid2", 32, 1, 0444 },
		/* Default Secondary Product ID */
		"0"
	},
	{ { "bootmode", 4, 1, 0444 },
		/* Default Bootmode */
		"1"
	},
	{ { "postmode", 4, 1, 0444 },
		/* Default Postmode */
		"0"
	},
	{ { "bootcount", 8, 1, 0444 },
		/* Initial Bootcount */
		"0"
	},
	{ { "manufacturing", 512, 1, 0444 },
		/* Manufacturer-specific data */
		""
	},
	{ { "eth_mac_addr", 16, 1, 0444 },
		/* Default MAC address for ethernet */
		"0"
	},
	{ { "eth_ip_addr", 16, 1, 0444 },
		/* Initial default IPAddress */
		"0"
	},
	{ { "device_type_id", 32, 1, 0444 },
		/* Default device type value */
		"0"
	},
	{ { "unlock_code", 1024, 1, 0444 },
		/* Unlock code */
		""
	},
	{ { "sensorcal", 160, 1, 0444 },
		/* Sensor Calibration Data */
		"0"
	},
	{ { "wifi_mfg", 512, 1, 0444 },
		/* wifi Calibration Data */
		"0"
	},
	{ { "bt_mfg", 128, 1, 0444 },
		/* bt Calibration Data */
		"0"
	},
	{ { "fos_flags", 8, 1, 0444 },
		/* device specific flag */
		"40"
	},
	{ { "dev_flags", 8, 1, 0444 },
		/* device specific flag */
		"0"
	},
	{ { "usr_flags", 8, 1, 0444 },
		/* device specific flag */
		"0"
	},
	{ { "model_name", 256, 1, 0444 },
		/* device specific flag */
		"/config/model/Customer_1.ini"
	},
	{ { "keys", 81920, 0, 0444 },
		/* keys backup, 80K */
		0
	},
	{ { "mfr_name", 32, 1, 0444 },
		/* Manufacturer name */
		"0"
	},
	{ { "mfr_model", 32, 1, 0444 },
		/* Manufacturer model */
		"0"
	},
        { { "product_model", 32, 1, 0444 },
                /* product model */
                "0"
        },
	{ { "transition_done", 4, 1, 0444 },
		/* transition done flag */
		"0"
	},
	{ { "region", 32, 1, 0444 },
		 /* region flag, default to  US */
		"US"
	},
	{ { "", 0, 0, 0 }, 0 },
};


#endif /* __IDME_DEFAULT_TABLE_abc123_H__ */
