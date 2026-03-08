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

#include "mhl_linuxdrv.h"
#include "osal/include/osal.h"
#include "si_common.h"
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

/***** local macro definitions ***********************************************/

/***** local variable declarations *******************************************/
/***** global variable declarations *******************************************/

struct MHL_DRIVER_CONTEXT_T gDriverContext = {0};
struct device_info *devinfo;

/* Module parameters that can be provided on insmod */
int debug_level;
module_param(debug_level, int, 0644);
MODULE_PARM_DESC(debug_level, "debug level (default: 0)");

int output_format; /* Set device output format */
module_param(output_format, int, 0444);
MODULE_PARM_DESC(output_format, "Output format(default: 0-RGB)");

int input_dev_rap = 1; /* RAP Input Device */
module_param(input_dev_rap, int, 0444);
MODULE_PARM_DESC(input_dev_rap, "RAP Input Device (default: 1)");

int input_dev_rcp = 1; /* RCP Input Device */
module_param(input_dev_rcp, int, 0444);
MODULE_PARM_DESC(input_dev_rcp, "RCP Input Device (default: 1)");

int input_dev_ucp = 1; /* UCP Input Device */
module_param(input_dev_ucp, int, 0444);
MODULE_PARM_DESC(input_dev_ucp, "UCP Input Device (default: 1)");

const char strVersion[] = "CP5293-v1.00.03 (release 1237)";

#ifdef CONFIG_IDME
#define LEN_FHD			3
#define LEN_APTIV		5
extern const char *idme_get_model_name(void);
#endif

enum display_panel {
	FHD,
	APTIV,
};

u8 display_panel_type = FHD;


/*****************************************************************************
 *  @brief Start the MHL transmitter device
 *
 *  This function is called during driver startup to initialize control of the
 *  MHL transmitter device by the driver.
 *
 *  @return     0 if successful, negative error code otherwise
 *
 *****************************************************************************/
int32_t StartMhlTxDevice(void)
{
	uint32_t halStatus;
	uint32_t osalStatus;

	pr_info("Starting %s\n", MHL_DEVICE_NAME);

	// Initialize the OS Abstraction Layer (OSAL) support.
	osalStatus = SiiOsInit(0);
	if (osalStatus != SII_OS_STATUS_SUCCESS) {
		SII_DEBUG_PRINT(
		    MSG_ERR, "Initialization of OSAL failed, error code: %d\n",
		    osalStatus);
		return -EIO;
	}

	halStatus = HalInit();
	if (halStatus != HAL_RET_SUCCESS) {
		SII_DEBUG_PRINT(
		    MSG_ERR, "Initialization of HAL failed, error code: %d\n",
		    halStatus);
		SiiOsTerm();
		return -EIO;
	}

	halStatus = HalOpenI2cDevice(MHL_DEVICE_NAME, MHL_DRIVER_NAME);
	if (halStatus != HAL_RET_SUCCESS) {
		SII_DEBUG_PRINT(
		    MSG_ERR,
		    "Opening of I2c device %s failed, error code: %d\n",
		    MHL_DEVICE_NAME, halStatus);
		HalTerm();
		SiiOsTerm();
		return -EIO;
	}

	HalAcquireIsrLock();
	/* Initialize the 5293 & power up. */
	// SiiMhlTxInitialize(&DebugSW);
	if (!SiiDrvDeviceInitialize()) {
		DEBUG_PRINT(MSG_ALWAYS,
		"SiiDrvDeviceInitialize failed!\n");
		return -EIO;
	}

	HalReleaseIsrLock();
#if 1 // dennis
	halStatus = HalInstallIrqHandler(SiiDrvDeviceManageInterrupts);
	if (halStatus != HAL_RET_SUCCESS) {
		SII_DEBUG_PRINT(MSG_ERR,
		"Initialization of HAL interrupt support failed, error code: %d\n",
		halStatus);
		HalCloseI2cDevice();
		HalTerm();
		SiiOsTerm();
		return -EIO;
	}
#endif
	return 0;
}

/**
 * @brief Stop the MHL transmitter device
 * This function shuts down control of the transmitter device so that
 * the driver can exit
 *
 *
 * @return 0 if successful, negative error code otherwise
 *
 * @param void
 *
 */
int32_t StopMhlTxDevice(void)
{
	uint32_t halStatus;

	pr_info("Stopping %s\n", MHL_DEVICE_NAME);

	HalAcquireIsrLock();
	HalRemoveIrqHandler();

	SiiDrvDeviceRelease();
	HalReleaseIsrLock();

	halStatus = HalCloseI2cDevice();
	if (halStatus != HAL_RET_SUCCESS) {
		SII_DEBUG_PRINT(
		    MSG_ERR, "Closing of I2c device failed, error code: %d\n",
		    halStatus);
		return -EIO;
	}

	halStatus = HalTerm();
	if (halStatus != HAL_RET_SUCCESS) {
		SII_DEBUG_PRINT(MSG_ERR,
				"Termination of HAL failed, error code: %d\n",
				halStatus);
		return -EIO;
	}

	SiiOsTerm();
	return 0;
}

/***** public functions ******************************************************/

#define MAX_EVENT_STRING_LEN 512

/* MHL device initialization and release */
int mhl_dev_add(struct device_info *dev_info)
{
	int retval = 0;

	if (dev_info == NULL) {
		pr_info("Invalid devinfo pointer\n");
		retval = -EFAULT;
		goto failed;
	}
	dev_info->mhl = kzalloc(sizeof(*dev_info->mhl), GFP_KERNEL);
	if (dev_info->mhl == NULL) {
		retval = -ENOMEM;
		goto failed;
	}

	dev_info->mhl->devnum = MKDEV(MAJOR(dev_info->devnum), 2);

	dev_info->mhl->cdev = cdev_alloc();
	dev_info->mhl->cdev->owner = THIS_MODULE;
	retval = cdev_add(dev_info->mhl->cdev, dev_info->mhl->devnum, 1);
	if (retval)
		goto failed;

	return 0;

failed:
	return retval;
}

/*
 *  MHL Attributes
 */
static ssize_t get_connection_state(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int retval;

	if (SiiMhlRxCbusConnected())
		retval = scnprintf(buf, PAGE_SIZE, "%s", "connected");
	else
		retval = scnprintf(buf, PAGE_SIZE, "%s", "not connected");

	return retval;
}

/*
 * Declare the sysfs entries for MHL Attributes.
 * These macros create instances of:
 *   dev_attr_connection_state
 */
static DEVICE_ATTR(connection_state, 0444, get_connection_state, NULL);

static struct attribute *mhl_attrs[] = {
	&dev_attr_connection_state.attr,
	NULL, /* need to NULL terminate the list of attributes */
};

static struct attribute_group mhl_attr_group = {
	.attrs = mhl_attrs,
};

/*
 *  MHL Devcap Group Attributes
 */
static ssize_t get_mhl_devcap_local(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int retval = 0;

	if (HalAcquireIsrLock() != HAL_RET_SUCCESS) {
		retval = -ERESTARTSYS;
		goto done;
	}

	retval = scnprintf(buf, PAGE_SIZE, "0x%02X",
			   SiiRegRead(REG_CBUS_DEVICE_CAP_0 +
				      gDriverContext.devcap_local_offset));

	HalReleaseIsrLock();

done:
	return retval;
}

static ssize_t set_mhl_devcap_local(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	ssize_t retval = count;
	unsigned long value = 0;
	int rv = 0;

	rv = kstrtoul(buf, 0, &value);
	if (rv) {
		pr_info("Invalid MHL Local Device Capability Value %s", buf);
		retval = rv;
		goto done;
	}

	if (value > 0xFF) {
		pr_info("Invalid MHL Local Device Capability Value %lu\n",
			value);
		retval = -EINVAL;
		goto done;
	}

	if (HalAcquireIsrLock() != HAL_RET_SUCCESS) {
		retval = -ERESTARTSYS;
		goto done;
	}
	SiiRegWrite(REG_CBUS_DEVICE_CAP_0 + gDriverContext.devcap_local_offset,
		    value);

	HalReleaseIsrLock();
done:
	return retval;
}

static ssize_t get_mhl_devcap_local_offset(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0x%02X",
			 gDriverContext.devcap_local_offset);
}

static ssize_t set_mhl_devcap_local_offset(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	ssize_t retval = count;
	int rv = 0;
	unsigned long offset = 0;

	rv = kstrtoul(buf, 0, &offset);
	if (rv) {
		pr_info("Invalid MHL Local Device Capability Offset %s", buf);
		retval = rv;
		goto done;
	}

	if (offset > 0x0F) {
		pr_info("Invalid MHL Local Device Capability Offset %lu\n",
			offset);
		retval = -EINVAL;
		goto done;
	}

	gDriverContext.devcap_local_offset = offset;

done:
	return retval;
}

static ssize_t get_mhl_devcap_remote(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int retval = 0;

	if (!SiiMhlRxCbusConnected()) {
		pr_info(
		"MHL Remote Device Capabilities not available in cbus not connected.\n");
		retval = -ENODEV;
		goto done;
	}

	retval = scnprintf(
	    buf, PAGE_SIZE, "0x%02X",
	    SiiCbusRemoteDcapGet(gDriverContext.devcap_remote_offset));

done:
	return retval;
}

static ssize_t get_mhl_devcap_remote_offset(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0x%02X",
			 gDriverContext.devcap_remote_offset);
}

static ssize_t set_mhl_devcap_remote_offset(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	ssize_t retval = count;
	int rv = 0;
	unsigned long offset = 0;

	rv = kstrtoul(buf, 0, &offset);
	if (rv) {
		pr_info("Invalid MHL Remote Device Capability Offset %s", buf);
		retval = rv;
		goto done;
	}

	if (offset > 0x0F) {
		pr_info("Invalid MHL Remote Device Capability Offset %lu\n",
			offset);
		retval = -EINVAL;
		goto done;
	}

	gDriverContext.devcap_remote_offset = offset;

done:
	return retval;
}

/*
 * Declare the sysfs entries forMHL Devcap Group Attributes.
 */
static struct device_attribute dev_attr_devcap_local =
	 __ATTR(local, 0644, get_mhl_devcap_local, set_mhl_devcap_local);
static struct device_attribute dev_attr_devcap_local_offset =
	__ATTR(local_offset, 0644, get_mhl_devcap_local_offset,
		set_mhl_devcap_local_offset);
static struct device_attribute dev_attr_devcap_remote =
	__ATTR(remote, 0444, get_mhl_devcap_remote, NULL);
static struct device_attribute dev_attr_devcap_remote_offset =
	__ATTR(remote_offset, 0644, get_mhl_devcap_remote_offset,
		set_mhl_devcap_remote_offset);

static struct attribute *mhl_devcap_attrs[] = {
	&dev_attr_devcap_local.attr,
	&dev_attr_devcap_local_offset.attr,
	&dev_attr_devcap_remote.attr,
	&dev_attr_devcap_remote_offset.attr,
	NULL, /* need to NULL terminate the list of attributes */
};

static struct attribute_group mhl_devcap_attr_group = {
	.name = __stringify(devcap),
	.attrs = mhl_devcap_attrs,
};

/*
 * MHL RAP Group Attributes
 */
static ssize_t get_mhl_rap_in(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0x%02X",
			 gDriverContext.rap_in_keycode);
}

static ssize_t set_mhl_rap_in_status(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned long param;
	ssize_t status = count;

	if (!SiiMhlRxCbusConnected()) {
		pr_info("Command not available in cbus not connected.\n");
		return -ENODEV;
	}

	if (HalAcquireIsrLock() != HAL_RET_SUCCESS)
		return -ERESTARTSYS;

	param = kstrtoul(buf, 0, 0);
	switch (param) {
	case 0x00:
	case 0x03:
		SiiMhlRxSendRapk(param);
		break;
	default:
		DEBUG_PRINT(MSG_ERR, "Invalid parameter %s received\n", buf);
		status = -EINVAL;
	}
	HalReleaseIsrLock();
	return status;
}

static ssize_t get_mhl_rap_out(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0x%02X",
			 gDriverContext.rap_out_keycode);
}

static ssize_t set_mhl_rap_out(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	unsigned long param;
	ssize_t status = count;

	if (!SiiMhlRxCbusConnected()) {
		pr_info("Command not available in cbus not connected.\n");
		return -ENODEV;
	}

	if (HalAcquireIsrLock() != HAL_RET_SUCCESS)
		return -ERESTARTSYS;

	param = kstrtoul(buf, 0, 0);
	switch (param) {
	case MHL_RAP_CMD_POLL:
	case MHL_RAP_CONTENT_ON:
	case MHL_RAP_CONTENT_OFF:
		SiiMhlRxSendRAPCmd(param);
		gDriverContext.rap_out_keycode = param;
		gDriverContext.rap_out_statecode = MHL_MSC_MSG_RAP_NO_ERROR;
		break;
	default:
		DEBUG_PRINT(MSG_ERR, "Invalid parameter %s received\n", buf);
		status = -EINVAL;
	}
	HalReleaseIsrLock();
	return status;
}

static ssize_t get_mhl_rap_out_status(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0x%02X",
			 gDriverContext.rap_out_statecode);
}

static ssize_t get_mhl_rap_input_dev(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d", input_dev_rap);
}

static ssize_t set_mhl_rap_input_dev(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int status;
	unsigned long param;

	/* Assume success */
	status = count;
	param = kstrtoul(buf, 0, 0);
	switch (param) {
	case 0:
	case 1:
		input_dev_rap = param;
		break;
	default:
		DEBUG_PRINT(MSG_ERR, "Invalid parameter %s received\n", buf);
		status = -EINVAL;
	}
	return status;
}

/*
 * Declare the sysfs entries for MHL RAP Group Attributes.
 */
static struct device_attribute dev_attr_rap_in =
	__ATTR(in, 0444, get_mhl_rap_in, NULL);
static struct device_attribute dev_attr_rap_in_status =
	__ATTR(in_status, 0200, NULL, set_mhl_rap_in_status);
static struct device_attribute dev_attr_rap_out =
	__ATTR(out, 0644, get_mhl_rap_out, set_mhl_rap_out);
static struct device_attribute dev_attr_rap_out_status =
	__ATTR(out_status, 0444, get_mhl_rap_out_status, NULL);
static struct device_attribute dev_attr_rap_input_dev = __ATTR(
	input_dev, 0644, get_mhl_rap_input_dev, set_mhl_rap_input_dev);

static struct attribute *mhl_rap_attrs[] = {
	&dev_attr_rap_in.attr,
	&dev_attr_rap_in_status.attr,
	&dev_attr_rap_out.attr,
	&dev_attr_rap_out_status.attr,
	&dev_attr_rap_input_dev.attr,
	NULL, /* need to NULL terminate the list of attributes */
	};

static struct attribute_group mhl_rap_attr_group = {
	.name = __stringify(rap),
	.attrs = mhl_rap_attrs,
};

/*
 * MHL RCP Group Attributes
 */
static ssize_t get_mhl_rcp_in(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0x%02X",
			 gDriverContext.rcp_in_keycode);
}

static ssize_t set_mhl_rcp_in_status(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned long param;
	ssize_t status = count;

	if (!SiiMhlRxCbusConnected()) {
		pr_info("Command not available in cbus not connected.\n");
		return -ENODEV;
	}

	if (HalAcquireIsrLock() != HAL_RET_SUCCESS)
		return -ERESTARTSYS;

	param = kstrtoul(buf, 0, 0);
	switch (param) {
	case 0x00:
		SiiMhlRxSendRcpk(gDriverContext.rcp_in_keycode);
		break;
	case 0x01:
	case 0x02:
		SiiMhlRxSendRcpe(param);
		SiiMhlRxSendRcpk(gDriverContext.rcp_in_keycode);
		break;
	default:
		DEBUG_PRINT(MSG_ERR, "Invalid parameter %s received\n", buf);
		status = -EINVAL;
	}
	HalReleaseIsrLock();
	return status;
}

static ssize_t get_mhl_rcp_out(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0x%02X",
			 gDriverContext.rcp_out_keycode);
}

static ssize_t set_mhl_rcp_out(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	unsigned long param;
	ssize_t status = count;

	if (!SiiMhlRxCbusConnected()) {
		pr_info("Command not available in cbus not connected.\n");
		return -ENODEV;
	}

	if (HalAcquireIsrLock() != HAL_RET_SUCCESS)
		return -ERESTARTSYS;

	param = kstrtoul(buf, 0, 0);
	if (param > 0xFF) {
		DEBUG_PRINT(MSG_ERR, "Invalid parameter %s received\n", buf);
		status = -EINVAL;
	} else {
		SiiMhlRxSendRCPCmd(param);
		gDriverContext.rcp_out_keycode = param;
		gDriverContext.rcp_out_statecode = MHL_MSC_MSG_RCP_NO_ERROR;
	}
	HalReleaseIsrLock();
	return status;
}

static ssize_t get_mhl_rcp_out_status(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0x%02X",
			 gDriverContext.rcp_out_statecode);
}

static ssize_t get_mhl_rcp_input_dev(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d", input_dev_rcp);
}

static ssize_t set_mhl_rcp_input_dev(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int status;
	unsigned long param;

	/* Assume success */
	status = count;
	param = kstrtoul(buf, 0, 0);
	switch (param) {
	case 0:
	case 1:
		input_dev_rcp = param;
		break;
	default:
		DEBUG_PRINT(MSG_ERR, "Invalid parameter %s received\n", buf);
		status = -EINVAL;
	}
	return status;
}

/*
 * Declare the sysfs entries for MHL RCP Group Attributes.
 */
static struct device_attribute dev_attr_rcp_in =
	__ATTR(in, 0444, get_mhl_rcp_in, NULL);
static struct device_attribute dev_attr_rcp_in_status =
	__ATTR(in_status, 0200, NULL, set_mhl_rcp_in_status);
static struct device_attribute dev_attr_rcp_out =
	__ATTR(out, 0644, get_mhl_rcp_out, set_mhl_rcp_out);
static struct device_attribute dev_attr_rcp_out_status =
	__ATTR(out_status, 0444, get_mhl_rcp_out_status, NULL);
static struct device_attribute dev_attr_rcp_input_dev = __ATTR(
	input_dev, 0644, get_mhl_rcp_input_dev, set_mhl_rcp_input_dev);

static struct attribute *mhl_rcp_attrs[] = {
	&dev_attr_rcp_in.attr,
	&dev_attr_rcp_in_status.attr,
	&dev_attr_rcp_out.attr,
	&dev_attr_rcp_out_status.attr,
	&dev_attr_rcp_input_dev.attr,
	NULL, /* need to NULL terminate the list of attributes */
};

static struct attribute_group mhl_rcp_attr_group = {
	.name = __stringify(rcp),
	.attrs = mhl_rcp_attrs,
};

/*
 * MHL UCP Group Attributes
 */
static ssize_t get_mhl_ucp_in(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0x%02X",
			 gDriverContext.ucp_in_keycode);
}

static ssize_t set_mhl_ucp_in_status(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned long param;
	ssize_t status = count;

	if (!SiiMhlRxCbusConnected()) {
		pr_info("Command not available in cbus not connected.\n");
		return -ENODEV;
	}

	if (HalAcquireIsrLock() != HAL_RET_SUCCESS)
		return -ERESTARTSYS;

	param = kstrtoul(buf, 0, 0);
	switch (param) {
	case 0x00:
		SiiMhlRxSendUcpk(gDriverContext.ucp_in_keycode);
		break;
	case 0x01:
		SiiMhlRxSendUcpe(param);
		SiiMhlRxSendUcpk(gDriverContext.ucp_in_keycode);
		break;
	default:
		DEBUG_PRINT(MSG_ERR, "Invalid parameter %s received\n", buf);
		status = -EINVAL;
	}
	HalReleaseIsrLock();
	return status;
}

static ssize_t get_mhl_ucp_out(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0x%02X",
			 gDriverContext.ucp_out_keycode);
}

static ssize_t set_mhl_ucp_out(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	unsigned long param;
	ssize_t status = count;

	if (!SiiMhlRxCbusConnected()) {
		pr_info("Command not available in cbus not connected.\n");
		return -ENODEV;
	}

	if (HalAcquireIsrLock() != HAL_RET_SUCCESS)
		return -ERESTARTSYS;

	param = kstrtoul(buf, 0, 0);
	if (param > 0xFF) {
		DEBUG_PRINT(MSG_ERR, "Invalid parameter %s received\n", buf);
		status = -EINVAL;
	} else {
		SiiMhlRxSendUCPCmd(param);
		gDriverContext.ucp_out_keycode = param;
		gDriverContext.ucp_out_statecode = MHL_MSC_MSG_UCP_NO_ERROR;
	}
	HalReleaseIsrLock();
	return status;
}

static ssize_t get_mhl_ucp_out_status(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0x%02X",
			 gDriverContext.ucp_out_statecode);
}

static ssize_t get_mhl_ucp_input_dev(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d", input_dev_ucp);
}

static ssize_t set_mhl_ucp_input_dev(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int status;
	unsigned long param;

	/* Assume success */
	status = count;
	param = kstrtoul(buf, 0, 0);
	switch (param) {
	case 0:
	case 1:
		input_dev_ucp = param;
		break;
	default:
		DEBUG_PRINT(MSG_ERR, "Invalid parameter %s received\n", buf);
		status = -EINVAL;
	}
	return status;
}

/*
 * Declare the sysfs entries for MHL UCP Group Attributes.
 */
static struct device_attribute dev_attr_ucp_in =
	__ATTR(in, 0444, get_mhl_ucp_in, NULL);
static struct device_attribute dev_attr_ucp_in_status =
	__ATTR(in_status, 0200, NULL, set_mhl_ucp_in_status);
static struct device_attribute dev_attr_ucp_out =
	__ATTR(out, 0644, get_mhl_ucp_out, set_mhl_ucp_out);
static struct device_attribute dev_attr_ucp_out_status =
	__ATTR(out_status, 0444, get_mhl_ucp_out_status, NULL);
static struct device_attribute dev_attr_ucp_input_dev = __ATTR(
	input_dev, 0644, get_mhl_ucp_input_dev, set_mhl_ucp_input_dev);

static struct attribute *mhl_ucp_attrs[] = {
	&dev_attr_ucp_in.attr,
	&dev_attr_ucp_in_status.attr,
	&dev_attr_ucp_out.attr,
	&dev_attr_ucp_out_status.attr,
	&dev_attr_ucp_input_dev.attr,
	NULL, /* need to NULL terminate the list of attributes */
};

static struct attribute_group mhl_ucp_attr_group = {
	.name = __stringify(ucp),
	.attrs = mhl_ucp_attrs,
};

int mhl_dev_init(struct device_info *dev_info)
{
	int retval = 0;

	if ((dev_info == NULL) || (dev_info->mhl == NULL)) {
		pr_info("Invalid devinfo pointer\n");
		retval = -EFAULT;
		goto failed;
	}

	// dev_info->dev_class->dev_attrs = NULL;
	dev_info->mhl->device =
	    device_create(dev_info->dev_class, dev_info->device,
			  dev_info->mhl->devnum, NULL, MHL_DEVNAME);
	if (IS_ERR(dev_info->mhl->device)) {
		retval = PTR_ERR(dev_info->mhl->device);
		goto failed;
	}

	retval =
	    sysfs_create_group(&dev_info->mhl->device->kobj, &mhl_attr_group);
	if (retval < 0)
		pr_info("failed to create MHL attribute group - continuing without\n");

	retval = sysfs_create_group(&dev_info->mhl->device->kobj,
				    &mhl_devcap_attr_group);
	if (retval < 0)
		pr_info("failed to create MHL devcap attribute group - continuing without\n");

	retval = sysfs_create_group(&dev_info->mhl->device->kobj,
				    &mhl_rap_attr_group);
	if (retval < 0)
		pr_info("failed to create MHL rap attribute group - continuing without\n");

	retval = sysfs_create_group(&dev_info->mhl->device->kobj,
				    &mhl_rcp_attr_group);
	if (retval < 0)
		pr_info("failed to create MHL rcp attribute group - continuing without\n");

	retval = sysfs_create_group(&dev_info->mhl->device->kobj,
				    &mhl_ucp_attr_group);
	if (retval < 0)
		pr_info("failed to create MHL ucp attribute group - continuing without\n");

	return 0;

failed:
	return retval;
}

void mhl_dev_exit(struct device_info *dev_info)
{
	if ((dev_info == NULL) || (dev_info->mhl == NULL)) {
		pr_info("Invalid devinfo pointer\n");
		return;
	}

	sysfs_remove_group(&dev_info->mhl->device->kobj, &mhl_attr_group);
	sysfs_remove_group(&dev_info->mhl->device->kobj,
			   &mhl_devcap_attr_group);
	sysfs_remove_group(&dev_info->mhl->device->kobj, &mhl_rap_attr_group);
	sysfs_remove_group(&dev_info->mhl->device->kobj, &mhl_rcp_attr_group);
	sysfs_remove_group(&dev_info->mhl->device->kobj, &mhl_ucp_attr_group);

	device_unregister(dev_info->mhl->device);
	device_destroy(dev_info->dev_class, dev_info->mhl->devnum);
}

/*
 * Sii5293 Attributes
 */
static ssize_t get_chip_version(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (gDriverContext.chip_revision == 0xFF)
		return scnprintf(buf, PAGE_SIZE, "%s", "");
	else
		return scnprintf(buf, PAGE_SIZE, "%d",
				 gDriverContext.chip_revision);
}

static ssize_t get_input_video_mode(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	uint8_t vic4x3, vic16x9;

	if (gDriverContext.input_video_mode == SI_VIDEO_MODE_NOT_STABLE) {
		return scnprintf(buf, PAGE_SIZE, "not stable\n");
	} else if (gDriverContext.input_video_mode == SI_VIDEO_MODE_NON_STD) {
		return scnprintf(buf, PAGE_SIZE, "out of range\n");
	} else if (gDriverContext.input_video_mode == SI_VIDEO_MODE_PC_OTHER) {
		return scnprintf(buf, PAGE_SIZE, "pc resolution\n");
	} else if (gDriverContext.input_video_mode &
		   SI_VIDEO_MODE_3D_RESOLUTION_MASK) {
		return scnprintf(buf, PAGE_SIZE,
			"Current video is 3D format. Its VIC is %d and Type is %s\n",
			(int)gDriverContext.avi_vic, SiiRx3DTypeGet());
	} else {
		gDriverContext.input_video_mode &= 0x7f;
		if (gDriverContext.input_video_mode >=
		    NMB_OF_CEA861_VIDEO_MODES) {
			return scnprintf(
			    buf, PAGE_SIZE, "HDMI VIC %d\n",
			    VideoModeTable[gDriverContext.input_video_mode]
				.HdmiVic);
		} else {
			vic4x3 = VideoModeTable[gDriverContext.input_video_mode]
				     .Vic4x3;
			vic16x9 =
			    VideoModeTable[gDriverContext.input_video_mode]
				.Vic16x9;
			if (vic4x3 && vic16x9)
				return scnprintf(buf, PAGE_SIZE,
						 "CEA VIC %d %d\n", vic4x3,
						 vic16x9);
			else if (vic4x3)
				return scnprintf(buf, PAGE_SIZE,
						 "CEA VIC %d\n", vic4x3);
			else if (vic16x9)
				return scnprintf(buf, PAGE_SIZE,
						 "CEA VIC %d\n", vic16x9);
			else
				return scnprintf(buf, PAGE_SIZE, "%s\n", "");
		}
	}
}

static ssize_t get_device_connection_state(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	ssize_t status = 0;

	switch (gDriverContext.connection_state) {
	case MHL_CONN:
		status =
		    scnprintf(buf, PAGE_SIZE, "%s", "mhl source connected");
		break;
	case HDMI_CONN:
		status =
		    scnprintf(buf, PAGE_SIZE, "%s", "hdmi source connected");
		break;
	case NO_CONN:
		status = scnprintf(buf, PAGE_SIZE, "%s", "no source connected");
		break;
	default:
		status = scnprintf(buf, PAGE_SIZE, "%s", "");
		break;
	}
	return status;
}

static ssize_t get_debug_level(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d", debug_level);
}

static ssize_t set_debug_level(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	ssize_t retval = count;
	int rv = 0;
	long new_debug_level = 0;

	rv = kstrtol(buf, 0, &new_debug_level);
	if (rv) {
		pr_info("Invalid Debug Level input: %s", buf);
		retval = rv;
		goto done;
	}

	if (new_debug_level < -1 || new_debug_level > 2) {
		pr_err("Invalid Debug Level input: %d\n", (int)new_debug_level);
		retval = -EINVAL;
		goto done;
	}

	debug_level = (int)new_debug_level;

done:
	return retval;
}

/*
 * Declare the sysfs entries for Sii5293 Attributes.
 * These macros create instances of:
 *   dev_attr_chip_version
 *   dev_attr_input_video_mode
 *   dev_attr_device_connection_state
 *   dev_attr_debug_level
 */
static DEVICE_ATTR(chip_version, 0444, get_chip_version, NULL);
static DEVICE_ATTR(input_video_mode, 0444, get_input_video_mode, NULL);
static DEVICE_ATTR(device_connection_state, 0444,
		   get_device_connection_state, NULL);
static DEVICE_ATTR(debug_level, 0644, get_debug_level,
		   set_debug_level);

static struct attribute *sii5293_attrs[] = {
	&dev_attr_chip_version.attr,
	&dev_attr_input_video_mode.attr,
	&dev_attr_device_connection_state.attr,
	&dev_attr_debug_level.attr,
	NULL, /* need to NULL terminate the list of attributes */
};

static struct attribute_group sii5293_attr_group = {
	.attrs = sii5293_attrs,
};

#ifdef DEBUG

static ssize_t get_pwr5v_state(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	if (gDriverContext.pwr5v_state)
		return scnprintf(buf, PAGE_SIZE, "%s", "in");
	else
		return scnprintf(buf, PAGE_SIZE, "%s", "out");
}

static ssize_t get_mhl_cable_state(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	if (gDriverContext.mhl_cable_state)
		return scnprintf(buf, PAGE_SIZE, "%s", "in");
	else
		return scnprintf(buf, PAGE_SIZE, "%s", "out");
}

static ssize_t get_rx_term_state(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int status = 0;

	if (HalAcquireIsrLock() != HAL_RET_SUCCESS)
		return -ERESTARTSYS;

	status =
	    scnprintf(buf, PAGE_SIZE, "%d", SiiRegRead(REG_RX_CTRL5) & 0x03);

	HalReleaseIsrLock();
	return status;
}

static ssize_t set_rx_term_state(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	int status = 0;
	long new_term = 0;

	status = kstrtol(buf, 0, &new_term);
	if (status) {
		pr_info("Invalid Debug Level input: %s", buf);
		return status;
	}

	if (new_term < 0 || new_term > 3) {
		pr_info("Invalid Debug Level input: %d", (int)new_term);
		return -EINVAL;
	}

	if (HalAcquireIsrLock() != HAL_RET_SUCCESS)
		return -ERESTARTSYS;

	SiiRegModify(REG_RX_CTRL5, 0x03, new_term);

	HalReleaseIsrLock();
	return count;
}

static ssize_t get_hdcp_state(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	int status = 0;

	if (HalAcquireIsrLock() != HAL_RET_SUCCESS)
		return -ERESTARTSYS;

	status = scnprintf(buf, PAGE_SIZE, "%s",
				SiiRegRead(RX_A__HDCP_STAT) &
					RX_M__HDCP_STAT__AUTHENTICATED
					? "authenticated"
					: "not authenticated");

	HalReleaseIsrLock();
	return status;
}

static ssize_t get_hpd_state(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	int status = 0;
	uint8_t regVal;

	if (HalAcquireIsrLock() != HAL_RET_SUCCESS)
		return -ERESTARTSYS;

	regVal = SiiRegRead(REG_HP_CTRL) & 0x03;

	HalReleaseIsrLock();

	if (regVal == 2) {
		if (SiiMhlRxCbusConnected())
			status = scnprintf(buf, PAGE_SIZE, "%s", "high");
		else
			status = scnprintf(buf, PAGE_SIZE, "%s", "low");
	} else if (regVal == 0) {
		status = scnprintf(buf, PAGE_SIZE, "%s", "low");
	} else if (regVal == 1) {
		status = scnprintf(buf, PAGE_SIZE, "%s", "high");
	} else {
		status = scnprintf(buf, PAGE_SIZE, "%s", "");
	}

	return status;
}

static ssize_t set_hpd_state(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int status = 0;
	long new_hpd = 0;

	status = kstrtol(buf, 0, &new_hpd);
	if (status) {
		pr_info("Invalid Debug Level input: %s", buf);
		return status;
	}

	if (new_hpd < 0 || new_hpd > 2) {
		pr_info("Invalid Debug Level input: %d", (int)new_hpd);
		return -EINVAL;
	}

	if (HalAcquireIsrLock() != HAL_RET_SUCCESS)
		return -ERESTARTSYS;

	SiiRegModify(REG_HP_CTRL, 0x03, new_hpd);

	HalReleaseIsrLock();
	return count;
}

#define MAX_DEBUG_TRANSFER_SIZE 16
static ssize_t get_debug_register(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	uint8_t data[MAX_DEBUG_TRANSFER_SIZE];
	uint8_t idx, j;
	int status = -EINVAL;

	DEBUG_PRINT(MSG_DBG, "called\n");

	if (gDriverContext.debug_i2c_address == 0)
		gDriverContext.debug_i2c_address = DEV_PAGE_PP_0;

	if (HalAcquireIsrLock() != HAL_RET_SUCCESS)
		return -ERESTARTSYS;

	status = scnprintf(buf, PAGE_SIZE,
		"address:0x%02X offset:0x%02X length:0x%02X data:",
		gDriverContext.debug_i2c_address,
		gDriverContext.debug_i2c_offset,
		gDriverContext.debug_i2c_xfer_length);
	if (gDriverContext.debug_i2c_xfer_length == 0) {
		for (j = 0; j < 16; j++) {
			if (0 !=
			    CraReadBlockI2c(DEV_I2C_0,
					    gDriverContext.debug_i2c_address,
					    16 * j, data, 16))
				return -EINVAL;
			status += scnprintf(&buf[status], PAGE_SIZE, "\n");
			for (idx = 0; idx < 16; idx++) {
				status += scnprintf(&buf[status], PAGE_SIZE,
						    "0x%02X ", data[idx]);
			}
		}
	} else {
		if (CraReadBlockI2c(DEV_I2C_0,
			gDriverContext.debug_i2c_address,
			gDriverContext.debug_i2c_offset, data,
			gDriverContext.debug_i2c_xfer_length) != 0)
			return -EINVAL;
		for (idx = 0; idx < gDriverContext.debug_i2c_xfer_length;
		     idx++) {
			status += scnprintf(&buf[status], PAGE_SIZE, "0x%02X ",
					    data[idx]);
		}
	}

	HalReleaseIsrLock();

	return status;
}

static ssize_t set_debug_register(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	static char *white_space = "' ', '\t'";
	unsigned long address = 0x100; /* initialize with invalid values */
	unsigned long offset = 0x100;
	unsigned long length = 0x100;
	unsigned long value;
	uint8_t data[MAX_DEBUG_TRANSFER_SIZE];
	uint8_t idx;
	char *str;
	char *endptr;
	int status = -EINVAL;

	DEBUG_PRINT(MSG_DBG,
	"received string: %s\n",
	buf);

	/*
	 * Parse the input string and extract the scratch pad register selection
	 * parameters
	 */
	str = strstr(buf, "address=");
	if (str != NULL) {
		address = kstrtoul(str + 8, 0, 0);
		if (address > 0xFF) {
			DEBUG_PRINT(MSG_ERR,
			"Invalid page address: 0x%02lX specified\n",
			address);
			goto err_exit;
		}
	} else {
		DEBUG_PRINT(MSG_ERR,
		 "Invalid string format, can't find address parameter\n");
		goto err_exit;
	}

	str = strstr(buf, "offset=");
	if (str != NULL) {
		offset = kstrtoul(str + 7, 0, 0);
		if (offset > 0xFF) {
			DEBUG_PRINT(MSG_ERR,
			"Invalid page offset: 0x%02lX specified\n",
			offset);
			goto err_exit;
		}
	} else {
		DEBUG_PRINT(MSG_ERR,
		 "Invalid string format, can't find offset value\n");
		goto err_exit;
	}

	str = strstr(buf, "length=");
	if (str != NULL) {
		length = kstrtoul(str + 7, 0, 0);
		if (length > MAX_DEBUG_TRANSFER_SIZE) {
			DEBUG_PRINT(MSG_ERR,
			"Transfer size 0x%02lX is too large\n",
			length);
			goto err_exit;
		}
	} else {
		DEBUG_PRINT(MSG_ERR,
		 "Invalid string format, can't find length value\n");
		goto err_exit;
	}

	str = strstr(buf, "data=");
	if (str != NULL) {
		str += 5;
		endptr = str;
		for (idx = 0; idx < length; idx++) {
			endptr += strspn(endptr, white_space);
			str = endptr;
			if (*str == 0) {
				DEBUG_PRINT(MSG_ERR,
					    "Too few data values provided\n");
				goto err_exit;
			}

			value = kstrtoul(str, &endptr, 0);

			if (value > 0xFF) {
				DEBUG_PRINT(MSG_ERR,
				 "Invalid register data value detected\n");
				goto err_exit;
			}

			data[idx] = value;
		}
	} else {
		idx = 0;
	}

	if ((offset + length) > 0x100) {
		DEBUG_PRINT(
		    MSG_ERR,
		    "Invalid offset/length combination entered 0x%02X/0x%02X",
		    offset, length);
		goto err_exit;
	}

	gDriverContext.debug_i2c_address = address;
	gDriverContext.debug_i2c_offset = offset;
	gDriverContext.debug_i2c_xfer_length = length;

	if (idx == 0) {
		DEBUG_PRINT(MSG_ERR,
"No data specified, storing address offset and length for subsequent debug read\n");
		goto err_exit;
	}

	if (HalAcquireIsrLock() != HAL_RET_SUCCESS)
		return -ERESTARTSYS;

	status = CraWriteBlockI2c(DEV_I2C_0, address, offset, data, length);

	if (status == 0)
		status = count;

	HalReleaseIsrLock();

err_exit:
	return status;
}

static DEVICE_ATTR(hpd_state, 0444, get_hpd_state, set_hpd_state);
static DEVICE_ATTR(hdcp_state, 0444, get_hdcp_state, NULL);
static DEVICE_ATTR(pwr5v_state, 0444, get_pwr5v_state, NULL);
static DEVICE_ATTR(mhl_cable_state, 0444, get_mhl_cable_state, NULL);
static DEVICE_ATTR(rx_term_state, 0444, get_rx_term_state,
		   set_rx_term_state);
static DEVICE_ATTR(register, 0644, get_debug_register,
		   set_debug_register);

static struct attribute *sii5293_debug_attrs[] = {
	&dev_attr_pwr5v_state.attr,
	&dev_attr_mhl_cable_state.attr,
	&dev_attr_rx_term_state.attr,
	&dev_attr_hpd_state.attr,
	&dev_attr_hdcp_state.attr,
	&dev_attr_register.attr,
	NULL, /* need to NULL terminate the list of attributes */
};

static struct attribute_group sii5293_debug_attr_group = {
	.name = __stringify(debug),
	.attrs = sii5293_debug_attrs,
};
#endif

// sii5293 platform-driver

#ifdef CONFIG_OF
static int sii5293_get_of_data(struct device_node *pnode)
{
	struct device_node *sii5293_node = pnode;
	//	struct i2c_board_info board_info;
	//	struct i2c_adapter *adapter;
	unsigned int i2c_index;
	int irq_gpio;
	enum of_gpio_flags irq_flags;

	const char *str;
	int ret = 0;

	pr_err("[%s]: sii5293_get_of_data!\n", __func__);
	// for i2c bus
	ret = of_property_read_string(sii5293_node, "i2c_bus", &str);
	if (ret) {
		pr_err("[%s]: failed to get i2c_bus str!\n", __func__);
		return -1;
	}

	//#define AML_I2C_MASTER_AO                       0
	//#define AML_I2C_MASTER_A                        1
	//#define AML_I2C_MASTER_B                        2
	//#define AML_I2C_MASTER_C                        3
	//#define AML_I2C_MASTER_D                        4
	if (!strncmp(str, "i2c_bus_ao", 9))
		i2c_index = 0;
	else if (!strncmp(str, "i2c_bus_a", 9))
		i2c_index = 1;
	else if (!strncmp(str, "i2c_bus_b", 9))
		i2c_index = 2;
	else if (!strncmp(str, "i2c_bus_c", 9))
		i2c_index = 3;
	else if (!strncmp(str, "i2c_bus_d", 9))
		i2c_index = 4;
	else
		return -1;

	devinfo->config.i2c_bus_index = i2c_index;
#if 1
	// for gpio_reset
	pr_err("[%s]read gpio_reset !\n", __func__);
	irq_gpio =
	    of_get_named_gpio_flags(sii5293_node, "reset-gpios", 0, &irq_flags);
	pr_err("valid gpio_reset GPIO:%d irq_flags %x\n", irq_gpio, irq_flags);
	devinfo->config.gpio_reset = gpio_to_irq(irq_gpio);
	devinfo->config.gpio_reset_irq_flags = irq_flags;

	// for irq
	pr_err("[%s]read gpio_intr !\n", __func__);
	irq_gpio =
	    of_get_named_gpio_flags(sii5293_node, "irq-gpios", 0, &irq_flags);
	pr_err("valid gpio_intr GPIO:%d irq_flags %x\n", irq_gpio, irq_flags);
	devinfo->config.gpio_intr = gpio_to_irq(irq_gpio);
	devinfo->config.gpio_intr_irq_flags = irq_flags;
#endif

#if 0
	memset(&board_info, 0x0, sizeof(board_info));
	strncpy(board_info.type, HDMIRX_SII9233A_NAME, I2C_NAME_SIZE);
	adapter = i2c_get_adapter(i2c_index);
	board_info.addr = SII9233A_I2C_ADDR;
	board_info.platform_data = &hdmirx_info;

	hdmirx_info.i2c_client = i2c_new_device(adapter, &board_info);
	pr_err("[%s] new i2c device i2c_client = 0x%x\n",
		 hdmirx_info.i2c_client);
#endif
	pr_err("sii5293 get i2c_idx = %d, gpio_reset = %d, gpio_irq = %d\n",
	       devinfo->config.i2c_bus_index, devinfo->config.gpio_reset,
	       devinfo->config.gpio_intr);
	return 0;
}
#endif

static int sii5293_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_err("[%s]: sii5293_probe!\n", __func__);
	devinfo->config.i2c_bus_index = 0xff;
	devinfo->config.gpio_reset = 0;
	devinfo->config.gpio_intr = 0;

#ifdef CONFIG_OF
	sii5293_get_of_data(pdev->dev.of_node);
#endif

	return ret;
}

static int sii5293_remove(struct platform_device *pdev) { return 0; }

#ifdef CONFIG_OF
static const struct of_device_id sii5293_dt_match[] = {
	{
	.compatible = "amlogic,sii9293",
	},
};
#endif

static struct platform_driver sii5293_driver = {
	.probe = sii5293_probe,
	.remove = sii5293_remove,
	.driver = {
	.name = MHL_DEVICE_NAME,
	.owner = THIS_MODULE,
#ifdef CONFIG_OF
	.of_match_table = sii5293_dt_match,
#endif
	}
};
static void check_panel_model_name(void)
{
#ifdef CONFIG_IDME
	const char *model_name;

	model_name = idme_get_model_name();

	if (!strncmp(model_name, "FHD", LEN_FHD))
		display_panel_type = FHD;

	if (!strncmp(model_name, "APTIV", LEN_APTIV))
		display_panel_type = APTIV;

	printk(KERN_ERR "display_panel_name = %d\n", display_panel_type);
#endif
}


/*
 * modelue init interface
 */
static int __init SiiMhlInit(void)
{
	int32_t ret = -1;

	check_panel_model_name();

	if (display_panel_type == FHD) {
		printk(KERN_ERR "The display panel attached is FHD, force init fail\n");
		return -ENOMEM;
	}

	//debug_level = 0x2;//dennis
	pr_info("%s driver starting!\n", MHL_DRIVER_NAME);
	pr_info("Version: %s\n", strVersion);
	/* register chrdev */
	pr_info("register_chrdev %s\n", DEVNAME);

	devinfo = kzalloc(sizeof(*devinfo), GFP_KERNEL);
	if (devinfo == NULL) {
		//pr_info("Out of memory!\n");
		return ret;
	}
	devinfo->mhl = NULL;

	ret = alloc_chrdev_region(&devinfo->devnum, 0, NUMBER_OF_DEVS, DEVNAME);
	if (ret) {
		pr_info("register_chrdev %s failed, error code: %d\n",
			MHL_DRIVER_NAME, ret);
		goto free_devinfo;
	}

	devinfo->cdev = cdev_alloc();
	devinfo->cdev->owner = THIS_MODULE;
	ret = cdev_add(devinfo->cdev, devinfo->devnum, MHL_DRIVER_MINOR_MAX);
	if (ret) {
		pr_info("cdev_add %s failed %d\n", MHL_DRIVER_NAME, ret);
		goto free_chrdev;
	}

	ret = mhl_dev_add(devinfo);
	if (ret)
		goto free_cdev;

	devinfo->dev_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(devinfo->dev_class)) {
		pr_info("class_create failed %d\n", ret);
		ret = PTR_ERR(devinfo->dev_class);
		goto free_mhl_cdev;
	}

	// devinfo->dev_class->dev_attrs = driver_attribs;

	devinfo->device = device_create(devinfo->dev_class, NULL,
					devinfo->devnum, NULL, "%s", DEVNAME);
	if (IS_ERR(devinfo->device)) {
		pr_info("class_device_create failed %s %d\n", DEVNAME, ret);
		ret = PTR_ERR(devinfo->device);
		goto free_class;
	}

	// add sii5293 platform-driver to get dt resources.
	ret = platform_driver_register(&sii5293_driver);
	if (ret) {
		pr_info("sii5293, failed to register sii5293 module\n");
		goto free_class;
	}

	ret = sysfs_create_group(&devinfo->device->kobj, &sii5293_attr_group);
	if (ret) {
		pr_info("failed to create root attribute group - continuing without\n");
		goto free_dev;
	}
#ifdef DEBUG
	ret = sysfs_create_group(&devinfo->device->kobj,
				 &sii5293_debug_attr_group);
	if (ret) {
		pr_info("failed to create debug attribute group - continuing without\n");
		goto free_dev;
	}
#endif

	ret = mhl_dev_init(devinfo);
	if (ret)
		goto free_dev;

	ret = StartMhlTxDevice();
	if (ret == 0) {
		pr_notice("mhldrv initialized successfully\n");
		return 0;
	}

	// Transmitter startup failed so fail the driver load.
	mhl_dev_exit(devinfo);

free_dev:
	if (devinfo->device) {
#ifdef DEBUG
		sysfs_remove_group(&devinfo->device->kobj,
				   &sii5293_debug_attr_group);
#endif
		sysfs_remove_group(&devinfo->device->kobj, &sii5293_attr_group);
		device_unregister(devinfo->device);
		devinfo->device = NULL;
	}
	device_destroy(devinfo->dev_class, devinfo->devnum);

free_class:
	class_destroy(devinfo->dev_class);

free_mhl_cdev:
	cdev_del(devinfo->mhl->cdev);

free_cdev:
	cdev_del(devinfo->cdev);

free_chrdev:
	unregister_chrdev_region(devinfo->devnum, MHL_DRIVER_MINOR_MAX);

free_devinfo:
	if (devinfo) {
		//if (devinfo->mhl)
			//kfree(devinfo->mhl);
		kfree(devinfo);
	}
	devinfo = NULL;
	return ret;
}
/*
 * modelue remove interface
 */
static void __exit SiiMhlExit(void)
{
	pr_info("%s driver exiting!\n", MHL_DRIVER_NAME);
	StopMhlTxDevice();
	mhl_dev_exit(devinfo);
	if (devinfo->device) {
#ifdef DEBUG
		sysfs_remove_group(&devinfo->device->kobj,
				   &sii5293_debug_attr_group);
#endif
		sysfs_remove_group(&devinfo->device->kobj, &sii5293_attr_group);
		device_unregister(devinfo->device);
		devinfo->device = NULL;
	}
	device_destroy(devinfo->dev_class, devinfo->devnum);
	class_destroy(devinfo->dev_class);
	cdev_del(devinfo->cdev);
	unregister_chrdev_region(devinfo->devnum, MHL_DRIVER_MINOR_MAX);
	if (devinfo) {
		//if (devinfo->mhl)
			//kfree(devinfo->mhl);
		kfree(devinfo);
	}
	platform_driver_unregister(&sii5293_driver);
	devinfo = NULL;
	pr_info("%s driver successfully exited!\n", MHL_DRIVER_NAME);
}

module_init(SiiMhlInit);
module_exit(SiiMhlExit);

int send_sii5293_uevent(struct device *device, const char *event_cat,
			const char *event_type, const char *event_data)
{
	int retval = 0;
	char event_string[MAX_EVENT_STRING_LEN];
	char * const envp[] = {event_string, NULL};

	if ((event_cat == NULL) || (event_type == NULL)) {
		pr_info("Invalid parameters\n");
		retval = -EINVAL;
		return retval;
	}

	scnprintf(event_string, MAX_EVENT_STRING_LEN - 1,
		  "%s={\"event\":\"%s\",\"data\":%s}", event_cat, event_type,
		  (event_data != NULL) ? event_data : "null");

	kobject_uevent_env(&device->kobj, KOBJ_CHANGE, (char **)envp);

	return retval;
}

void SiiConnectionStateNotify(bool connect)
{
#define MAX_REPORT_DATA_STRING_SIZE 20
	uint32_t new_state;
	char str[MAX_REPORT_DATA_STRING_SIZE];

	if (connect) {
		if (SiiMhlRxCbusConnected()) {
			new_state = MHL_CONN;
			scnprintf(str, MAX_REPORT_DATA_STRING_SIZE,
				  "mhl source connected");
		} else {
			new_state = HDMI_CONN;
			scnprintf(str, MAX_REPORT_DATA_STRING_SIZE,
				  "hdmi source connected");
		}
	} else {
		new_state = NO_CONN;
		scnprintf(str, MAX_REPORT_DATA_STRING_SIZE,
			  "no source connected");
	}

	if (new_state != gDriverContext.connection_state) {
		gDriverContext.connection_state = new_state;
		sysfs_notify(&devinfo->device->kobj, NULL,
			     "device_connection_state");
		send_sii5293_uevent(devinfo->device, DEVICE_EVENT,
				    DEV_CONNECTION_CHANGE_EVENT, str);
	}
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Silicon Image <http://www.siliconimage.com>");
MODULE_DESCRIPTION("Silicon Image MHL/HDMI Receiver driver");
