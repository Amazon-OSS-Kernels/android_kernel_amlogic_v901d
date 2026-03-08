/*
 * (C) Copyright 2008 - 2009
 * Windriver, <www.windriver.com>
 * Tom Rix <Tom.Rix@windriver.com>
 *
 * Copyright 2011 Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * Copyright 2014 Linaro, Ltd.
 * Rob Herring <robh@kernel.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <config.h>
#include <common.h>
#include <errno.h>
#include <malloc.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/composite.h>
#include <linux/compiler.h>
#include <version.h>
#include <g_dnl.h>
#include <asm/arch/cpu.h>
#ifdef CONFIG_FASTBOOT_FLASH_MMC_DEV
#include <fb_mmc.h>
#include <fb_storage.h>
#include <fb_fastboot.h>
#include <emmc_partitions.h>
#endif
#ifdef CONFIG_FASTBOOT_FLASH_NAND_DEV
#include <fb_nand.h>
#endif
#include <partition_table.h>
#include <android_image.h>
#include <image.h>
#ifdef CONFIG_AML_ANTIROLLBACK
#include <anti-rollback.h>
#endif

#if defined(CONFIG_IDME)
#include <idme.h>
#endif

#include <linux/ctype.h>

#if defined(UFBL_FEATURE_FASTBOOT_LOCKDOWN)
#include "amzn_fastboot_lockdown.h"
extern int is_locked_production_device();
#else
#error "UFBL_FEATURE_FASTBOOT_LOCKDOWN is required"
#endif

#if defined(UFBL_FEATURE_UNLOCK)
#include "amzn_unlock.h"
#else
#error "UFBL_FEATURE_UNLOCK is required"
#endif

#if defined(UFBL_FEATURE_ONETIME_UNLOCK)
#include <amzn_onetime_unlock.h>
#endif

#if defined(UFBL_FEATURE_SECURE_BOOT)
#include "amzn_secure_boot.h"
#else
#error "UFBL_FEATURE_SECURE_BOOT is required"
#endif

DECLARE_GLOBAL_DATA_PTR;

#define FASTBOOT_VERSION		"0.4"

#define FASTBOOT_INTERFACE_CLASS	0xff
#define FASTBOOT_INTERFACE_SUB_CLASS	0x42
#define FASTBOOT_INTERFACE_PROTOCOL	0x03

#define ENDPOINT_MAXIMUM_PACKET_SIZE_2_0  (0x0200)

#ifdef CONFIG_DEVICE_PRODUCT
#define DEVICE_PRODUCT	CONFIG_DEVICE_PRODUCT
#endif
#define DEVICE_SERIAL	"1234567890"

#define FB_ERR(fmt ...) printf("[ERR]%sL%d:", __func__, __LINE__),printf(fmt)
#define FB_MSG(fmt ...) printf("[MSG]"fmt)
#define FB_WRN(fmt ...) printf("[WRN]"fmt)
#define FB_DBG(...)
#define FB_HERE()    printf("f(%s)L%d\n", __func__, __LINE__)

#ifdef CONFIG_FASTBOOT_SLOT_SWITCHING
#define UBOOT_RUN_CMD_LEN 128
#endif

/* The 64 defined bytes plus \0 */

#define EP_BUFFER_SIZE	4096

struct f_fastboot {
	struct usb_function usb_function;

	/* IN/OUT EP's and corresponding requests */
	struct usb_ep *in_ep, *out_ep;
	struct usb_request *in_req, *out_req;
};

static inline struct f_fastboot *func_to_fastboot(struct usb_function *f)
{
	return container_of(f, struct f_fastboot, usb_function);
}

static struct f_fastboot *fastboot_func;
static unsigned int download_size;
static unsigned int download_bytes;


static struct usb_endpoint_descriptor ep_in = {
	.bLength            = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    = USB_DT_ENDPOINT,
	.bEndpointAddress   = USB_DIR_IN,
	.bmAttributes       = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     = ENDPOINT_MAXIMUM_PACKET_SIZE_2_0,
	.bInterval          = 0x00,
};

static struct usb_endpoint_descriptor ep_out = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_OUT,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize		= ENDPOINT_MAXIMUM_PACKET_SIZE_2_0,
	.bInterval		= 0x00,
};

static struct usb_interface_descriptor interface_desc = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bInterfaceNumber	= 0x00,
	.bAlternateSetting	= 0x00,
	.bNumEndpoints		= 0x02,
	.bInterfaceClass	= FASTBOOT_INTERFACE_CLASS,
	.bInterfaceSubClass	= FASTBOOT_INTERFACE_SUB_CLASS,
	.bInterfaceProtocol	= FASTBOOT_INTERFACE_PROTOCOL,
};

static struct usb_descriptor_header *fb_runtime_descs[] = {
	(struct usb_descriptor_header *)&interface_desc,
	(struct usb_descriptor_header *)&ep_in,
	(struct usb_descriptor_header *)&ep_out,
	NULL,
};

/*
 * static strings, in UTF-8
 */
static const char fastboot_name[] = "Android Fastboot";

static struct usb_string fastboot_string_defs[] = {
	[0].s = fastboot_name,
	{  }			/* end of list */
};

static struct usb_gadget_strings stringtab_fastboot = {
	.language	= 0x0409,	/* en-us */
	.strings	= fastboot_string_defs,
};

static struct usb_gadget_strings *fastboot_strings[] = {
	&stringtab_fastboot,
	NULL,
};

#define DRAM_UBOOT_RESERVE		0x01000000
unsigned int ddr_size_usable(unsigned int addr_start)
{
	unsigned int ddr_size=0;
	unsigned int free_size = 0;
	int i;

	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++)
		ddr_size += gd->bd->bi_dram[i].size;

	free_size = (ddr_size - DRAM_UBOOT_RESERVE - addr_start - CONFIG_SYS_MALLOC_LEN - CONFIG_SYS_MEM_TOP_HIDE);
#if defined CONFIG_FASTBOOT_MAX_DOWN_SIZE
	if (free_size > CONFIG_FASTBOOT_MAX_DOWN_SIZE)
		free_size = CONFIG_FASTBOOT_MAX_DOWN_SIZE;
#endif
	return free_size;
}

static void rx_handler_command(struct usb_ep *ep, struct usb_request *req);

static char response_str[RESPONSE_LEN + 1];

static int fastboot_tx_write_str(const char *buffer);
static int fastboot_tx_write(const char *buffer, unsigned int buffer_size);

void fastboot_fail(const char *s)
{
	strncpy(response_str, "FAIL", 5);
	if (s)strncat(response_str, s, RESPONSE_LEN - 4 - 1) ;
	fastboot_tx_write_str(response_str);
}

void fastboot_okay(const char *s)
{
	strncpy(response_str, "OKAY", 5);
	if (s)strncat(response_str, s, RESPONSE_LEN - 4 - 1) ;
	fastboot_tx_write_str(response_str);
}

void fastboot_busy(const char* s)
{
	strncpy(response_str, "INFO", 4 + 1);//add terminated 0
	if (s)strncat(response_str, s, RESPONSE_LEN - 4 - 1) ;
}
int fastboot_is_busy(void)
{
	return !strncmp("INFO", response_str, strlen("INFO"));
}

//cb for bulk in_req->complete
static void fastboot_complete(struct usb_ep *ep, struct usb_request *req)
{
	int status = req->status;

	if ( fastboot_is_busy() && fastboot_func) {
		struct usb_ep* out_ep = fastboot_func->out_ep;
		struct usb_request* out_req = fastboot_func->out_req;
		rx_handler_command(out_ep, out_req);
		return;
	}
	if (!status)
		return;
	printf("status: %d ep '%s' trans: %d\n", status, ep->name, req->actual);
}

static int fastboot_bind(struct usb_configuration *c, struct usb_function *f)
{
	int id;
	struct usb_gadget *gadget = c->cdev->gadget;
	struct f_fastboot *f_fb = func_to_fastboot(f);

	/* DYNAMIC interface numbers assignments */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	interface_desc.bInterfaceNumber = id;

	id = usb_string_id(c->cdev);
	if (id < 0)
		return id;
	fastboot_string_defs[0].id = id;
	interface_desc.iInterface = id;

	f_fb->in_ep = usb_ep_autoconfig(gadget, &ep_in);
	if (!f_fb->in_ep)
		return -ENODEV;
	f_fb->in_ep->driver_data = c->cdev;

	f_fb->out_ep = usb_ep_autoconfig(gadget, &ep_out);
	if (!f_fb->out_ep)
		return -ENODEV;
	f_fb->out_ep->driver_data = c->cdev;

	return 0;
}

static void fastboot_unbind(struct usb_configuration *c, struct usb_function *f)
{
	memset(fastboot_func, 0, sizeof(*fastboot_func));
}

static void fastboot_disable(struct usb_function *f)
{
	struct f_fastboot *f_fb = func_to_fastboot(f);

	usb_ep_disable(f_fb->out_ep);
	usb_ep_disable(f_fb->in_ep);

	if (f_fb->out_req) {
		free(f_fb->out_req->buf);
		usb_ep_free_request(f_fb->out_ep, f_fb->out_req);
		f_fb->out_req = NULL;
	}
	if (f_fb->in_req) {
		free(f_fb->in_req->buf);
		usb_ep_free_request(f_fb->in_ep, f_fb->in_req);
		f_fb->in_req = NULL;
	}
}

static struct usb_request *fastboot_start_ep(struct usb_ep *ep)
{
	struct usb_request *req;

	req = usb_ep_alloc_request(ep, 0);
	if (!req)
		return NULL;

	req->length = EP_BUFFER_SIZE;
	req->buf = memalign(CONFIG_SYS_CACHELINE_SIZE, EP_BUFFER_SIZE);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	memset(req->buf, 0, req->length);
	return req;
}

static int fastboot_set_alt(struct usb_function *f,
			    unsigned interface, unsigned alt)
{
	int ret;
	struct f_fastboot *f_fb = func_to_fastboot(f);

	debug("%s: func: %s intf: %d alt: %d\n",
	      __func__, f->name, interface, alt);

	/* make sure we don't enable the ep twice */
	ret = usb_ep_enable(f_fb->out_ep, &ep_out);
	if (ret) {
		puts("failed to enable out ep\n");
		return ret;
	}

	f_fb->out_req = fastboot_start_ep(f_fb->out_ep);
	if (!f_fb->out_req) {
		puts("failed to alloc out req\n");
		ret = -EINVAL;
		goto err;
	}
	f_fb->out_req->complete = rx_handler_command;

	ret = usb_ep_enable(f_fb->in_ep, &ep_in);
	if (ret) {
		puts("failed to enable in ep\n");
		goto err;
	}

	f_fb->in_req = fastboot_start_ep(f_fb->in_ep);
	if (!f_fb->in_req) {
		puts("failed alloc req in\n");
		ret = -EINVAL;
		goto err;
	}
	f_fb->in_req->complete = fastboot_complete;

	ret = usb_ep_queue(f_fb->out_ep, f_fb->out_req, 0);
	if (ret)
		goto err;

	return 0;
err:
	fastboot_disable(f);
	return ret;
}

static int  fastboot_setup(struct usb_function *f,
	const struct usb_ctrlrequest *ctrl)
{
	int value = -EOPNOTSUPP;
	struct f_fastboot *f_fb = func_to_fastboot(f);

	/* composite driver infrastructure handles everything; interface
	 * activation uses set_alt().
	 */
	if (((ctrl->bRequestType & USB_RECIP_MASK) == USB_RECIP_ENDPOINT)
		&& (ctrl->bRequest == USB_REQ_CLEAR_FEATURE)
		&& (ctrl->wValue== USB_ENDPOINT_HALT)) {
		switch (ctrl->wIndex & 0xfe) {
		case USB_DIR_OUT:
			value = ctrl->wLength;
			usb_ep_clear_halt(f_fb->out_ep);
			break;

		case USB_DIR_IN:
			value = ctrl->wLength;
			usb_ep_clear_halt(f_fb->in_ep);
			break;
		default:
			printf("unknown usb_ctrlrequest\n");
			break;
		}
	}

	return value;
}

static int fastboot_add(struct usb_configuration *c)
{
	struct f_fastboot *f_fb;
	int status;

	if (fastboot_func == NULL) {
		f_fb = memalign(CONFIG_SYS_CACHELINE_SIZE, sizeof(*f_fb));
		if (!f_fb)
			return -ENOMEM;

		fastboot_func = f_fb;
		memset(f_fb, 0, sizeof(*f_fb));
	} else {
		f_fb = fastboot_func;
	}

	f_fb->usb_function.name = "f_fastboot";
	f_fb->usb_function.hs_descriptors = fb_runtime_descs;
	f_fb->usb_function.bind = fastboot_bind;
	f_fb->usb_function.unbind = fastboot_unbind;
	f_fb->usb_function.set_alt = fastboot_set_alt;
	f_fb->usb_function.disable = fastboot_disable;
	f_fb->usb_function.strings = fastboot_strings;
	f_fb->usb_function.setup = fastboot_setup;

	status = usb_add_function(c, &f_fb->usb_function);
	if (status) {
		free(f_fb);
		fastboot_func = NULL;
	}

	return status;
}
DECLARE_GADGET_BIND_CALLBACK(usb_dnl_fastboot, fastboot_add);

static int fastboot_tx_write(const char *buffer, unsigned int buffer_size)
{
	struct usb_request *in_req = fastboot_func->in_req;
	int ret;

	memcpy(in_req->buf, buffer, buffer_size);
	in_req->length = buffer_size;
	ret = usb_ep_queue(fastboot_func->in_ep, in_req, 0);
	if (ret)
		printf("Error %d on queue\n", ret);
	return 0;
}

static int fastboot_tx_write_str(const char *buffer)
{
	return fastboot_tx_write(buffer, strlen(buffer));
}


static void compl_do_reset(struct usb_ep *ep, struct usb_request *req)
{
	do_reset(NULL, 0, 0, NULL);
}

static void compl_do_reboot_bootloader(struct usb_ep *ep, struct usb_request *req)
{
	if (dynamic_partition)
		run_command("reboot bootloader", 0);
	else
		run_command("reboot fastboot", 0);
}

static void compl_do_reboot_fastboot(struct usb_ep *ep, struct usb_request *req)
{
	run_command("reboot fastboot", 0);
}

static void cb_reboot(struct usb_ep *ep, struct usb_request *req)
{
	char *cmd = req->buf;

	printf("cmd cb_reboot is %s\n", cmd);

	strsep(&cmd, "-");
	if (!cmd) {
		fastboot_func->in_req->complete = compl_do_reset;
		fastboot_tx_write_str("OKAY");
		return;
	}

	printf("cmd cb_reboot is %s\n", cmd);
	if (strcmp(cmd, "bootloader") == 0)
		fastboot_func->in_req->complete = compl_do_reboot_bootloader;
	else if (strcmp(cmd, "fastboot") == 0)
		fastboot_func->in_req->complete = compl_do_reboot_fastboot;

	fastboot_tx_write_str("OKAY");
}

static int strcmp_l1(const char *s1, const char *s2)
{
	if (!s1 || !s2)
		return -1;
	return strncmp(s1, s2, strlen(s1));
}

void dump_lock_info(LockData_t* info)
{
	printf("info->version_major = %d\n", info->version_major);
	printf("info->version_minor = %d\n", info->version_minor);
	printf("info->lock_state = %d\n", info->lock_state);
	printf("info->lock_critical_state = %d\n", info->lock_critical_state);
	printf("info->lock_bootloader = %d\n", info->lock_bootloader);
}


static int check_lock(void)
{
#if 0
	char *lock_s;
	LockData_t* info;

	lock_s = getenv("lock");
	if (!lock_s) {
		printf("lock state is NULL \n");
		lock_s = "10000000";
		setenv("lock", "10000000");
		saveenv();
	}
	printf("lock state: %s\n", lock_s);

	info = (LockData_t*)malloc(sizeof(struct LockData));
	memset(info,0,LOCK_DATA_SIZE);
	info->version_major = (int)(lock_s[0] - '0');
	info->version_minor = (int)(lock_s[1] - '0');
	info->lock_state = (int)(lock_s[4] - '0');
	info->lock_critical_state = (int)(lock_s[5] - '0');
	info->lock_bootloader = (int)(lock_s[6] - '0');

	dump_lock_info(info);

	if (( info->lock_state == 1 ) || ( info->lock_critical_state == 1 ))
		return 1;
	else
		return 0;
#else
	return 0;
#endif
}

static const char* getvar_list[] = {
	"version-baseband", "version-bootloader", "version", "hw-revision", "max-download-size",
	"serialno", "product", "off-mode-charge", "variant", "battery-soc-ok",
	"battery-voltage", "partition-type:boot", "partition-size:boot",
	"partition-type:system", "partition-size:system", "partition-type:vendor", "partition-size:vendor",
	"partition-type:odm", "partition-size:odm", "partition-type:data", "partition-size:data",
	"erase-block-size", "logical-block-size", "secure", "unlocked",
};
static const char* getvar_list_ab[] = {
	"version-baseband", "version-bootloader", "version", "hw-revision", "max-download-size",
	"serialno", "product", "off-mode-charge", "variant", "battery-soc-ok",
	"battery-voltage", "partition-type:boot", "partition-size:boot",
	"partition-type:system", "partition-size:system", "partition-type:vendor", "partition-size:vendor",
	"partition-type:odm", "partition-size:odm", "partition-type:data", "partition-size:data",
	"partition-type:cache", "partition-size:cache",
	"erase-block-size", "logical-block-size", "secure", "unlocked",
	"slot-count", "slot-suffixes","current-slot", "has-slot:bootloader", "has-slot:boot",
	"has-slot:system", "has-slot:vendor", "has-slot:odm", "has-slot:vbmeta", "has-slot:fip",
	"has-slot:metadata", "has-slot:product", "has-slot:dtbo", "has-slot:tvconfig", "has-slot:oemconfig",
	"slot-successful:a", "slot-unbootable:a", "slot-retry-count:a",
	"slot-successful:b", "slot-unbootable:b", "slot-retry-count:b",
};

#if defined(CONFIG_IDME)
char * get_hw_reverision(){
	char buf[24] = {0};
	if (!idme_get_var_external("board_id", buf, sizeof(buf))){
		if (!strncmp(HVT_BOARD_ID, buf, strlen(HVT_BOARD_ID))){
			return "HVT";
		}else if(!strncmp(EVT_BOARD_ID, buf, strlen(EVT_BOARD_ID))){
			return "EVT";
		}else if(!strncmp(DVT_BOARD_ID, buf, strlen(DVT_BOARD_ID))){
			return "DVT";
		}else if(!strncmp(PVT_BOARD_ID, buf, strlen(PVT_BOARD_ID))){
			return "PVT";
		}else {
			return "REF";
		}
	}
	printf("can not get board_id \n");
        return "REF";
}
#endif

static void cb_getvar(struct usb_ep *ep, struct usb_request *req)
{
	char *cmd = req->buf;
	char cmdBuf[RESPONSE_LEN];
	char* response = response_str;
	char *s;
	char *s1;
	char *s2;
	char *s3;
	size_t chars_left;

	run_command("get_valid_slot", 0);

	strcpy(response, "OKAY");
	chars_left = sizeof(response_str) - strlen(response) - 1;

	memcpy(cmdBuf, cmd, strnlen(cmd, RESPONSE_LEN-1)+1);
	cmdBuf[RESPONSE_LEN - 1] = 0;
	cmd = cmdBuf;
	strsep(&cmd, ":");
	printf("cb_getvar: %s\n", cmd);
	if (!cmd) {
		error("missing variable\n");
		fastboot_tx_write_str("FAILmissing var");
		return;
	}
	if (!strncmp(cmd, "all", 3)) {
		static int cmdIndex = 0;
		int getvar_num = 0;
		if (has_boot_slot == 1) {
 			strcpy(cmd, getvar_list_ab[cmdIndex]);
 			getvar_num = (sizeof(getvar_list_ab) / sizeof(getvar_list_ab[0]));
 		} else {
 			strcpy(cmd, getvar_list[cmdIndex]);//only support no-arg cmd
 			getvar_num = (sizeof(getvar_list) / sizeof(getvar_list[0]));
 		}
		printf("getvar_num: %d\n", getvar_num);
		if ( ++cmdIndex >= getvar_num) cmdIndex = 0;
		else fastboot_busy(NULL);
		FB_MSG("all cmd:%s\n", cmd);
		strncat(response, cmd, chars_left);
		strncat(response, ":", 1);
		chars_left -= strlen(cmd) + 1;
	}

	if (!strcmp_l1("version-baseband", cmd)) {
		strncat(response, "N/A", chars_left);
	} else if (!strcmp_l1("version-bootloader", cmd)) {
		strncat(response, U_BOOT_VERSION, chars_left);
	} else if (!strcmp_l1("hw-revision", cmd)) {
#if defined(CONFIG_IDME)
		strncat(response, get_hw_reverision(), chars_left);
#else
		strncat(response, "EVT", chars_left);
#endif
	} else if (!strcmp_l1("version", cmd)) {
		strncat(response, FASTBOOT_VERSION, chars_left);
	} else if (!strcmp_l1("bootloader-version", cmd)) {
		strncat(response, U_BOOT_VERSION, chars_left);
	} else if (!strcmp_l1("off-mode-charge", cmd)) {
		strncat(response, "0", chars_left);
	} else if (!strcmp_l1("variant", cmd)) {
		strncat(response, "US", chars_left);
	} else if (!strcmp_l1("battery-soc-ok", cmd)) {
		strncat(response, "yes", chars_left);
	} else if (!strcmp_l1("battery-voltage", cmd)) {
		strncat(response, "4.2V", chars_left);
	} else if (!strcmp_l1("is-userspace", cmd)) {
		strncat(response, "no", chars_left);
	} else if (!strcmp_l1("is-logical", cmd)) {
		strsep(&cmd, ":");
		printf("partition is %s\n", cmd);
		if (!dynamic_partition) {
			strncat(response, "no", chars_left);
		} else {
			if ((strcmp(cmd, "system") == 0) ||  (strcmp(cmd, "vendor") == 0)
				|| (strcmp(cmd, "odm") == 0) || (strcmp(cmd, "product") == 0)) {
				strncat(response, "yes", chars_left);
			} else {
				strncat(response, "no", chars_left);
			}
		}
	} else if (!strcmp_l1("super-partition-name", cmd)) {
		char *slot_name;
		slot_name = getenv("slot-suffixes");
		if (has_boot_slot == 0) {
			strncat(response, "super", chars_left);
		} else {
			printf("slot-suffixes: %s\n", slot_name);
			if (strcmp(slot_name, "0") == 0) {
				printf("active_slot is %s\n", "a");
				strncat(response, "super_a", chars_left);
			} else if (strcmp(slot_name, "1") == 0) {
				printf("active_slot is %s\n", "b");
				strncat(response, "super_b", chars_left);
			}
		}
	} else if (!strcmp_l1("downloadsize", cmd) ||
		!strcmp_l1("max-download-size", cmd)) {
		char str_num[12];

		sprintf(str_num, "0x%08x", ddr_size_usable(CONFIG_USB_FASTBOOT_BUF_ADDR));
		strncat(response, str_num, chars_left);
	} else if (!strcmp_l1("serialno", cmd)) {
		//s = getenv("serial");
		s = get_usid_string();
#if defined(CONFIG_IDME)
		char buf[24] = {0};
		if (!idme_get_var_external("serial", buf, sizeof(buf)))
			s = buf;
#endif
		if (s)
			strncat(response, s, chars_left);
		else
			strncat(response, DEVICE_SERIAL, chars_left);
	} else if (!strcmp_l1("product", cmd)) {
#ifdef DEVICE_PRODUCT
		s1 = DEVICE_PRODUCT;
		printf("DEVICE_PRODUCT: %s\n", s1);
#else
		s1 = getenv("device_product");
		printf("device_product: %s\n", s1);
#endif
		strncat(response, s1, chars_left);
	} else if (!strcmp_l1("secure", cmd)) {
		strncat(response,
				(secure_boot_enabled() == true) ? "yes" : "no",
				chars_left);
#if defined(UFBL_FEATURE_UNLOCK)
	} else if (!strcmp_l1("unlock_code", cmd)) {
			unsigned char unlock_code[UNLOCK_CODE_LEN + 1] = {0};
			int code_len = UNLOCK_CODE_LEN;
			char str_num[UNLOCK_CODE_LEN + 1] = {0};
			if (amzn_get_unlock_code(unlock_code, &code_len)) {
				strncat(response, "cannot get unlock code", chars_left);
			} else {
				snprintf(str_num, UNLOCK_CODE_LEN, "%s", unlock_code);
				strncat(response, str_num, chars_left);
			}

	} else if (!strcmp_l1("unlock_status", cmd)) {
		char str_num[2];
		snprintf(str_num, sizeof(str_num), "%d", amzn_target_is_unlocked() ? 1 : 0);
		strncat(response, str_num, chars_left);
	} else if (!strcmp_l1("unlock_version", cmd)) {
		strncat(response, "1", chars_left);
#endif
	}
#if defined(UFBL_FEATURE_ONETIME_UNLOCK)
	else if (!strcmp_l1("otu_code", cmd)) {
		unsigned char one_tu_code[ONETIME_UNLOCK_CODE_LEN + 1] = {0};
		unsigned int unlock_code_len = sizeof(one_tu_code);
		char str_num[ONETIME_UNLOCK_CODE_LEN + 1] = {0};
		if (amzn_get_one_tu_code(one_tu_code, &unlock_code_len)) {
			strncat(response, "cannot get onetime unlock code", chars_left);
		} else {
			snprintf(str_num, sizeof(str_num), "%s", one_tu_code);
			strncat(response, str_num, chars_left);
		}
	}
#endif
	else if (!strcmp_l1("slot-count", cmd)) {
		strncat(response, "2", chars_left);
	} else if (!strcmp_l1("slot-suffixes", cmd)) {
		s2 = getenv("slot-suffixes");
		printf("slot-suffixes: %s\n", s2);
		if (s2)
			strncat(response, s2, chars_left);
		else
			strncat(response, "0", chars_left);
	} else if (!strcmp_l1("current-slot", cmd)) {
		s3 = getenv("slot-suffixes");
		printf("slot-suffixes: %s\n", s3);
		if (strcmp(s3, "0") == 0) {
			printf("active_slot is %s\n", "a");
			strncat(response, "a", chars_left);
		} else if (strcmp(s3, "1") == 0) {
			printf("active_slot is %s\n", "b");
			strncat(response, "b", chars_left);
		}
	} else if (!strcmp_l1("has-slot:bootloader", cmd)) {
		printf("do not have slot for bootloader, but there is fip slot\n");
		strncat(response, "no", chars_left);
	} else if (!strcmp_l1("has-slot:fip", cmd)) {
		printf("has fip slot\n");
		strncat(response, "yes", chars_left);
	} else if (!strcmp_l1("has-slot:boot", cmd)) {
		if (has_boot_slot == 1) {
			printf("has boot slot\n");
			strncat(response, "yes", chars_left);
		} else
			strncat(response, "no", chars_left);
	} else if (!strcmp_l1("has-slot:system", cmd)) {
		if (dynamic_partition) {
			strncat(response, "no", chars_left);
		} else {
			if (has_system_slot == 1) {
				printf("has system slot\n");
				strncat(response, "yes", chars_left);
			} else
				strncat(response, "no", chars_left);
		}
	} else if (!strcmp_l1("has-slot:vendor", cmd)) {
		if (dynamic_partition) {
			strncat(response, "no", chars_left);
		} else {
			if (has_boot_slot == 1) {
				printf("has vendor slot\n");
				strncat(response, "yes", chars_left);
			} else
				strncat(response, "no", chars_left);
		}
	} else if (!strcmp_l1("has-slot:vbmeta", cmd)) {
		if (dynamic_partition) {
			strncat(response, "no", chars_left);
		} else {
			if (has_boot_slot == 1) {
				printf("has vbmeta slot\n");
				strncat(response, "yes", chars_left);
			} else
				strncat(response, "no", chars_left);
		}
	} else if (!strcmp_l1("has-slot:product", cmd)) {
		if (dynamic_partition) {
			strncat(response, "no", chars_left);
		} else {
			if (has_boot_slot == 1) {
				printf("has product slot\n");
				strncat(response, "yes", chars_left);
			} else
				strncat(response, "no", chars_left);
		}
	} else if (!strcmp_l1("has-slot:super", cmd)) {
		if (!dynamic_partition) {
			strncat(response, "no", chars_left);
		} else {
			if (has_boot_slot == 1) {
				printf("has product slot\n");
				strncat(response, "yes", chars_left);
			} else
				strncat(response, "no", chars_left);
		}
	} else if (!strcmp_l1("has-slot:metadata", cmd)) {
		if (has_boot_slot == 1) {
			printf("has metadata slot\n");
			strncat(response, "yes", chars_left);
		} else
			strncat(response, "no", chars_left);
	} else if (!strcmp_l1("has-slot:dtbo", cmd)) {
		if (has_boot_slot == 1) {
			printf("has dtbo slot\n");
			strncat(response, "yes", chars_left);
		} else
			strncat(response, "no", chars_left);
	} else if (!strcmp_l1("has-slot:odm", cmd)) {
		if (has_boot_slot == 1) {
			printf("has odm slot\n");
			strncat(response, "yes", chars_left);
		} else
			strncat(response, "no", chars_left);
	} else if (!strcmp_l1("has-slot:tvconfig", cmd)) {
		if (has_boot_slot == 1) {
			printf("has tvconfig slot\n");
			strncat(response, "yes", chars_left);
		} else
			strncat(response, "no", chars_left);
	} else if (!strcmp_l1("has-slot:oemconfig", cmd)) {
		if (has_boot_slot == 1) {
			printf("has oemconfig slot\n");
			strncat(response, "yes", chars_left);
		} else
			strncat(response, "no", chars_left);
	} else if (!strncmp("partition-size", cmd, strlen("partition-size"))) {
		char str_num[48];
		struct partitions *pPartition;
		uint64_t sz = 0;
		int ret = -1;
		strsep(&cmd, ":");
		if ((cmd == NULL) || (strcmp(cmd, "") == 0)) {
			printf("partition name is NULL\n");
			strcpy(response, "FAILpartition name is NULL");
			fastboot_tx_write_str(response);
			return;
		}
		printf("partition is %s\n", cmd);
		if (strcmp(cmd, "userdata") == 0) {
			strcpy(cmd, "data");
			printf("partition is %s\n", cmd);
		}
		if (!strncmp("mbr", cmd, strlen("mbr"))) {
			strcpy(response, "FAILVariable not implemented");
		} else {
			if (!strncmp("bootloader-", cmd, strlen("bootloader-"))) {
				strsep(&cmd, "-");
				ret = mmc_boot_size(cmd, &sz);
				printf("ret = %d\n", ret);
				if (ret == 0) {
					printf("size:%016llx\n", sz);
					sprintf(str_num, "%016llx", sz);
				} else {
					printf("get partitize error\n");
					sprintf(str_num, "FAILget partitize error");
				}
			} else {
				pPartition = find_mmc_partition_by_name(cmd);
				if (pPartition) {
					printf("size:%016llx\n", pPartition->size);
					if (strcmp(cmd, "data") == 0) {
						printf("reserve 0x4000 for fde data\n");
						sz = pPartition->size - 0x4000;
						printf("data size :%016llx\n", sz);
						sprintf(str_num, "%016llx", sz);
					} else {
						sprintf(str_num, "%016llx", pPartition->size);
					}
				} else {
					printf("find_mmc_partition_by_name fail\n");
					sprintf(str_num, "FAILget fail");
				}
			}
			strncat(response, str_num, chars_left);
		}
	} else if (!strcmp_l1("partition-type:cache", cmd)) {
		if (has_boot_slot == 0) {
			strncat(response, "ext4", chars_left);
		}
	} else if (!strcmp_l1("partition-type:data", cmd)) {
		strncat(response, "ext4", chars_left);
	} else if (!strcmp_l1("partition-type:userdata", cmd)) {
		strncat(response, "ext4", chars_left);
	} else if (!strcmp_l1("partition-type:system", cmd)) {
		strncat(response, "ext4", chars_left);
	} else if (!strcmp_l1("partition-type:vendor", cmd)) {
		strncat(response, "ext4", chars_left);
	} else if (!strcmp_l1("partition-type:odm", cmd)) {
		strncat(response, "ext4", chars_left);
	} else if (!strcmp_l1("partition-type:tee", cmd)) {
		strncat(response, "ext4", chars_left);
	} else if (!strcmp_l1("partition-type:param", cmd)) {
		strncat(response, "ext4", chars_left);
	} else if (!strcmp_l1("partition-type:product", cmd)) {
		strncat(response, "ext4", chars_left);
	} else if (!strcmp_l1("partition-type:metadata", cmd)) {
		strncat(response, "ext4", chars_left);
	} else if (!strncmp("partition-type", cmd, strlen("partition-type"))) {
		strncat(response, "raw", chars_left);
	} else if (!strcmp_l1("erase-block-size", cmd) ||
		!strcmp_l1("logical-block-size", cmd)) {
		strncat(response, "2000", chars_left);
	} else if (!strcmp_l1("secure", cmd)) {
		if (check_lock()) {
			strncat(response, "yes", chars_left);
		} else {
			strncat(response, "no", chars_left);
		}
	} else if (!strcmp_l1("unlocked", cmd)) {
		if (check_lock()) {
			strncat(response, "no", chars_left);
		} else {
			strncat(response, "yes", chars_left);
		}
	} else if (!strcmp_l1("slot-successful", cmd)) {
		char str[128];
		strsep(&cmd, ":");
		printf("cmd is %s\n", cmd);
		int ret;
		if (has_boot_slot == 1) {
			printf("has boot slot\n");
			if (strcmp(cmd, "a") && strcmp(cmd, "b")) {
				printf("we only have a/b slot now, variable error\n");
				strcpy(response, "FAILVariable error, only have a/b slot now");
				goto exit;
			}

			sprintf(str, "get_slot_state %s suc_stete", cmd);
			printf("command:    %s\n", str);
			ret = run_command(str, 0);
			printf("ret = %d\n", ret);
			if (ret == 0)
				strncat(response, "no", chars_left);
			else
				strncat(response, "yes", chars_left);
		} else
			strcpy(response, "FAILVariable not implemented in non ab mode");
	} else if (!strcmp_l1("slot-unbootable", cmd)) {
		char str[128];
		strsep(&cmd, ":");
		printf("cmd is %s\n", cmd);
		int ret;
		if (has_boot_slot == 1) {
			printf("has boot slot\n");
			if (strcmp(cmd, "a") && strcmp(cmd, "b")) {
				printf("we only have a/b slot now, variable error\n");
				strcpy(response, "FAILVariable error, only have a/b slot now");
				goto exit;
			}
			sprintf(str, "get_slot_state %s boot_state", cmd);
			printf("command:    %s\n", str);
			ret = run_command(str, 0);
			printf("ret = %d\n", ret);
			if (ret == 0)
				strncat(response, "yes", chars_left);
			else
				strncat(response, "no", chars_left);
		} else
			strcpy(response, "FAILVariable not implemented in non ab mode");
	} else if (!strcmp_l1("slot-retry-count", cmd)) {
		strsep(&cmd, ":");
		printf("cmd is %s\n", cmd);
		if (has_boot_slot == 1) {
			char *str_num = NULL;
			printf("has boot slot\n");
			if (strcmp(cmd, "a") && strcmp(cmd, "b")) {
				printf("we only have a/b slot now, variable error\n");
				strcpy(response, "FAILVariable error, only have a/b slot now");
				goto exit;
			}
			if (strcmp(cmd, "a") == 0) {
				str_num = getenv("retry-count_a");
			}else if (strcmp(cmd, "b") == 0) {
				str_num = getenv("retry-count_b");
			}
			if (str_num)
				strncat(response, str_num, chars_left);
			else
				strcpy(response, "FAILGet retry-count error");
		} else
			strcpy(response, "FAILVariable not implemented in non ab mode");
	} else {
		error("unknown variable: %s\n", cmd);
		strcpy(response, "FAILVariable not implemented");
	}

exit:
	fastboot_tx_write_str(response);
}

static unsigned int rx_bytes_expected(void)
{
	int rx_remain = download_size - download_bytes;
	if (rx_remain < 0)
		return 0;
	if (rx_remain > EP_BUFFER_SIZE)
		return EP_BUFFER_SIZE;
	return rx_remain;
}
#if defined(CONFIG_IDME)

#define DEV_FLAGS_ADB_USB_ON    0x1000

#define UNLOCK_AUTO_UPDATE_IDME_FOSFLAGS  1

/* Automatically set adb-related flag bits in idme after successful unlock */
int idme_auto_set_after_unlock(void)
{
	int ret;
	char flags_str[19] = { 0 };
	unsigned long long flags = 0;

/* update fos_flags automatically. */
#if UNLOCK_AUTO_UPDATE_IDME_FOSFLAGS
	if (0 != idme_get_var_external("fos_flags", flags_str, 16)) {
		printf("Error getting fos_flags value\n");
		return -1;
	}

	flags = simple_strtoul(flags_str, NULL, 16);

	/* Set adb-on and adb-root bit in fos_flags. */
	flags |= FOS_FLAGS_ADB_ON | FOS_FLAGS_ADB_ROOT;
	memset(flags_str, 0, sizeof(flags_str));
	snprintf(flags_str, sizeof(flags_str), "%llx", flags);
	ret = idme_update_var_ex("fos_flags", flags_str, sizeof(flags_str) - 1);
	if (ret) {
		printf("failed to update fos_flags\n");
		return -1;
	}
	printf("update fos_flags successfully\n");
#endif

	/* Read and update dev_flags automatically. */
	if (0 != idme_get_var_external("dev_flags", flags_str, 16)) {
		printf("Error getting dev_flags value\n");
		return -1;
	}

	flags = simple_strtoul(flags_str, NULL, 16);

	/* Set adb-usb bit in dev_flags. */
	flags |= DEV_FLAGS_ADB_USB_ON;
	memset(flags_str, 0, sizeof(flags_str));
	snprintf(flags_str, sizeof(flags_str), "%llx", flags);
	ret = idme_update_var_ex("dev_flags", flags_str, sizeof(flags_str) - 1);
	if (ret) {
		printf("failed to update dev_flags\n");
		return -1;
	}

	printf("update dev_flags successfully\n");

	return 0;
}

/* Automatically clean adb-related flag bits in idme before relock */
int idme_auto_clean_before_relock(void)
{
	int ret;
	char flags_str[19] = { 0 };
	unsigned long long flags = 0;

/* update fos_flags automatically. */
#if UNLOCK_AUTO_UPDATE_IDME_FOSFLAGS
	if (0 != idme_get_var_external("fos_flags", flags_str, 16)) {
		printf("Error getting fos_flags value\n");
		return -1;
	}

	flags = simple_strtoul(flags_str, NULL, 16);

	/* clear adb-on and adb-root bit in fos_flags. */
	flags &= ~(FOS_FLAGS_ADB_ON | FOS_FLAGS_ADB_ROOT);
	memset(flags_str, 0, sizeof(flags_str));
	snprintf(flags_str, sizeof(flags_str), "%llx", flags);
	ret = idme_update_var_ex("fos_flags", flags_str, sizeof(flags_str) - 1);
	if (ret) {
		printf("failed to update fos_flags\n");
		return -1;
	}
	printf("update fos_flags successfully\n");
#endif

	/* Read and update dev_flags automatically. */
	if (0 != idme_get_var_external("dev_flags", flags_str, 16)) {
		printf("Error getting dev_flags value\n");
		return -1;
	}

	flags = simple_strtoul(flags_str, NULL, 16);

	/* Clear adb-usb bit in dev_flags. */
	flags &= ~DEV_FLAGS_ADB_USB_ON;
	memset(flags_str, 0, sizeof(flags_str));
	snprintf(flags_str, sizeof(flags_str), "%llx", flags);
	ret = idme_update_var_ex("dev_flags", flags_str, sizeof(flags_str) - 1);
	if (ret) {
		printf("failed to update dev_flags\n");
		return -1;
	}

	printf("update dev_flags successfully\n");

	return 0;
}
static void cb_oem_idme(struct usb_ep *ep, struct usb_request *req)
{
	char response[RESPONSE_LEN];
	char *cmd = req->buf;

	cmd = strstr(cmd, "idme");
	cmd = strstr(cmd, " ");
	if (!fastboot_idme((const char*)cmd))
		strcpy(response, "OKAYidme done");
	else
		strcpy(response, "FAILidme fail");

	fastboot_tx_write_str(response);
}

static inline int is_blank(int c)
{
	return (c == ' ' || c == '\t');
}

static void cmd_oem_bootmode(struct usb_ep *ep, struct usb_request *req)
{
	char response[64] = {0};
	unsigned int bootmode_int = 0;
	char *arg = req->buf;
	int ret = 0;

	arg = strstr(arg, "bootmode");
	arg = strstr(arg, " ");
	if (!arg || *arg == '\0'){
		fastboot_fail("Must specify bootmode");
		return;
	}

	while (*arg && isblank(*arg))
		arg++;

	/* Only accept valid bootmode values */
	bootmode_int = *arg - '0';
	if (bootmode_int > IDME_BOOTMODE_MAX) {
		fastboot_fail("invalid bootmode given");
		return;
	}

	ret = idme_update_var_ex("bootmode", arg, 1);

	if (ret) {
		snprintf(response, 64, "Could not set bootmode, error=%d", ret);
		fastboot_fail(response);
		return;
	}

	snprintf(response, 64, "bootmode set to %c", *arg);
	fastboot_okay(response);
}

/*
 * 'fastboot oem flags <type:> <modifiers> <value1[|value2|value3|...]>'
 *
 * type:
 *  fos_flags:, fos: f: (default if not specified)
 *  dev_flags:, dev:, d:
 *  usr_flags:, usr:, u:
 *
 * modifiers:
 *  + means to OR value with the existing value
 *    (flags = flags | value)
 *  - means to AND the inverse of value with the existing value
 *    (flags = flags & ~value)
 *  = means to set the value (flags = value). If = is not set, it is assumed
 *
 * value:
 * If begins with 0x, it is treated as a hexadecimal value and set directly
 * If the value field is a number, it is converted from decimal to hex and set
 * Multiple values may be ORd together as in value1|value2|value3
 * Symbolic names are supported, and must match the symbolic name used in the header file
 *
 * If no type, modifiers or value is specified, print out the current value.
 * Also, accept the special keyword 'print'
 */

typedef enum {
	FLAGS_CMD_NONE = 0,
	FLAGS_CMD_HELP = 1,
	FLAGS_CMD_PRINT = 2,
	FLAGS_CMD_SET = 3
} flags_cmd_t;

typedef enum {
	FLAGS_TYPE_NONE = 0,
	FLAGS_TYPE_FOS = 1,
	FLAGS_TYPE_DEV = 2,
	FLAGS_TYPE_USR = 3,
	FLAGS_TYPE_ALL = 99
} flags_type_t;

typedef enum {
	FLAGS_MOD_NONE = 0,
	FLAGS_MOD_SET = 1,
	FLAGS_MOD_ADD = 2,
	FLAGS_MOD_REMOVE = 3
} flags_modifier_t;

static inline int ishexprefix(char* s)
{
	return s && *s && ('0' == *s) && ('x' == *(s+1) || 'X' == *(s+1));
}

static inline int issymprefix(char* s)
{
	return s && *s &&
		(!strncmp(s,"FOS_FLAGS_",10) ||
		 !strncmp(s,"DEV_FLAGS_",10) ||
		 !strncmp(s,"USR_FLAGS_",10) );
}

static int hexval(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return 0;
}

static inline unsigned long long atoull(const char *num)
{
	unsigned long long value = 0;

	if (num[0] == '0' && num[1] == 'x') {
		// hex
		num += 2;
		while (*num && isxdigit(*num))
			value = value * 16 + hexval(*num++);
	} else {
		// decimal
		while (*num && isdigit(*num))
			value = value * 10 + *num++ - '0';
	}

	return value;
}

static inline unsigned long long getnumval(char *s)
{
	if(s && *s) {
		if(ishexprefix(s)) {
			// For hex, normalize to lower case for atoull function
			*(s+1) = tolower(*(s+1));
		}

		return atoull(s);
	}

	return 0;
}

static inline unsigned long long getsymval(char *s)
{
        // TBD
	return 0;
}
/*int is_locked_production_device()
{
	return false;
}
*/
static void cmd_oem_flags(struct usb_ep *ep, struct usb_request *req)
{
	/* sizeof(flags) + NULL = 17 */
	char response[64] = { 0 };
	int ret = 0;
	char * target_flags = NULL;
	char new_flags_str[19] = { 0 };
	char cur_flags_str[19] = { 0 };
	unsigned long long cur_flags = 0;
	unsigned long long new_flags = 0;
	unsigned long long value = 0;
	char *arg = req->buf;

	arg = strstr(arg, "flags");
	arg = strstr(arg, " ");

	/* allow modifying to printing usr_flags on locked hw */
	/* Restrict modifying fos_flags or dev_flags on locked hw */

	flags_cmd_t flags_cmd = FLAGS_CMD_PRINT;
	flags_modifier_t flags_modifier = FLAGS_MOD_SET;
	flags_type_t flags_type = FLAGS_TYPE_NONE;

	/* Begin parsing */
	/* Default to a printout, when just oem flags is called */
	if (!arg || !*arg) {
		flags_cmd = FLAGS_CMD_PRINT;
		flags_type = is_locked_production_device()? FLAGS_TYPE_USR : FLAGS_TYPE_FOS;
		flags_modifier = FLAGS_MOD_NONE;
		goto parse_done;
	}

	while (*arg && isblank(*arg))
		arg++;

	if (!*arg)
		goto parse_done;

	/* find type: fos_flags, dev_flags or usr_flags */
	if ((!strncmp(arg, "f:", 2) && (arg+=strlen("f:"))) ||
			(!strncmp(arg, "fos:", 4) && (arg+=strlen("fos:"))) ||
			(!strncmp(arg, "fos_flags:", 10) && (arg+=strlen("fos_flags:"))))
		flags_type = FLAGS_TYPE_FOS;
	else if ((!strncmp(arg, "d:", 2) && (arg+=strlen("d:"))) ||
			(!strncmp(arg, "dev:", 4) && (arg+=strlen("dev:"))) ||
			(!strncmp(arg, "dev_flags:", 10) && (arg+=strlen("dev_flags:"))))
		flags_type = FLAGS_TYPE_DEV;
	else if ((!strncmp(arg, "u:", 2) && (arg+=strlen("u:"))) ||
			(!strncmp(arg, "usr:", 4) && (arg+=strlen("usr:"))) ||
			(!strncmp(arg, "usr_flags:", 10) && (arg+=strlen("usr_flags:"))))
		flags_type = FLAGS_TYPE_USR;

	/* Find modifier +, -, = (= default) */
	while (*arg && is_blank(*arg))
		arg++;
	if(!*arg)
		goto parse_done;

	switch (*arg) {
	case '+':
		flags_modifier = FLAGS_MOD_ADD;
		arg++;
		break;
	case '-':
		flags_modifier = FLAGS_MOD_REMOVE;
		arg++;
		break;
	case '=':
		flags_modifier = FLAGS_MOD_SET;
		arg++;
		break;
	}

	/* Find value, or special case */
	while (*arg && isblank(*arg))
		arg++;
	if (!*arg)
		goto parse_done;

	if (!strncmp(arg,"print", 5) ||
			!strncmp(arg,"PRINT", 5)) {
		arg+=strlen("print");
		flags_cmd = FLAGS_CMD_PRINT;
		flags_modifier = FLAGS_MOD_NONE;
		while (*arg && isblank(*arg)) arg++;

		if (FLAGS_TYPE_NONE == flags_type)
			flags_type = FLAGS_TYPE_FOS;

		if (is_locked_production_device() || ('u' == *arg) || ('U' == *arg))
			flags_type = FLAGS_TYPE_USR;
		else if (('f' == *arg) || ('F' == *arg))
			flags_type = FLAGS_TYPE_FOS;
		else if (('d' == *arg) || ('D' == *arg))
			flags_type = FLAGS_TYPE_DEV;

		goto parse_done;
	} else if (!strcmp(arg,"help") ||
			!strcmp(arg,"HELP") ||
			!strcmp(arg,"?")) {
		flags_cmd = FLAGS_CMD_HELP;
		flags_modifier = FLAGS_MOD_NONE;
		flags_type = FLAGS_TYPE_NONE;
		goto parse_done;
	} else {
		/*
		 * see if we have a value string
		 * while(*arg && isblank(*arg)) arg++; if(!*arg) goto parse_done;
		 */
		value = 0;
		char* cur_ptr = NULL;
		while (*arg) {
			/* skip whitespace */
			while(*arg && isblank(*arg)) arg++;

			cur_ptr = arg;
			while (*arg && *arg != '|' && !isblank(*arg)) arg++;
			if (*arg) {
				*arg = 0;
				/* Prepare for next round */
				arg++;
				while (*arg && (isblank(*arg) || '|' == *arg))
					arg++;
			}

			flags_cmd = FLAGS_CMD_SET;
			if (issymprefix(cur_ptr))
				value |= getsymval(cur_ptr);
			else
				value |= getnumval(cur_ptr);
		}
	}

parse_done:
	if (is_locked_production_device() && flags_cmd >= FLAGS_CMD_SET) {
		fastboot_fail("oem flags command is restricted on locked hw");
		return;
	}

	if (FLAGS_CMD_SET == flags_cmd && FLAGS_TYPE_NONE == flags_type)
		flags_type =  FLAGS_TYPE_FOS;

	switch (flags_type) {
	case FLAGS_TYPE_DEV:
		target_flags = "dev_flags";
		break;
	case FLAGS_TYPE_USR:
		target_flags = "usr_flags";
		break;
	default:
		target_flags = "fos_flags";
		break;
	}

	cur_flags = 0;
	memset(cur_flags_str, 0, sizeof(cur_flags_str));
	if(0 != idme_get_var_external(target_flags, cur_flags_str+2, 16)) {
		printf("Error getting %s value\n", target_flags);
		cur_flags_str[0]=0;
	}
	/* Need to prefix with 0x for atoull to handle hex. */
	cur_flags_str[0] = '0'; cur_flags_str[1] = 'x';
	cur_flags = atoull(cur_flags_str);

	switch(flags_cmd) {
	case FLAGS_CMD_PRINT:
		if ( FLAGS_TYPE_FOS ==  flags_type ) {
			int size_needed = fos_flags_to_str( cur_flags, NULL, 0 );
			char* b = (char*)malloc(size_needed);
			fos_flags_to_str( cur_flags, b, size_needed );
			printf("%s\n", b);
			free(b);
		}
		snprintf(response, 64, "%s is 0x%llx\n", target_flags, cur_flags);
		fastboot_okay(response);
		return;
		break;
	case FLAGS_CMD_SET:
		switch (flags_modifier) {
			case FLAGS_MOD_ADD:
				new_flags = (cur_flags | value);
				break;
			case FLAGS_MOD_REMOVE:
				new_flags = (cur_flags & ~value);
				break;
			default:
				new_flags = value;
				break;
		}

		memset(new_flags_str, 0, sizeof(new_flags_str));
		snprintf(new_flags_str, sizeof(new_flags_str), "%llx", new_flags);

		ret = idme_update_var_ex(target_flags, new_flags_str, sizeof(new_flags_str)-1);
		if (ret) {
			snprintf(response, 64, "could not set %s, error=%d", target_flags, ret);
			fastboot_fail(response);
			return;
		}

		snprintf(response, 64, "%s set to %s", target_flags, new_flags_str);
		printf("%s\n", response);
		fastboot_okay(response);
		return;
		break;
	default:
		fastboot_okay("oem flags [<type> <modifier>] <value>");
		break;
	} /* switch(flags_cmd) */

	fastboot_fail("Unhandled");

}
#endif

#if defined(UFBL_FEATURE_UNLOCK)
static void cb_oem_relock(struct usb_ep *ep, struct usb_request *req)
{
	char code[SIGNED_UNLOCK_CODE_LEN] = {0};
	char response[RESPONSE_LEN] = {0};

	if (idme_update_var_ex("unlock_code", code, sizeof(code))) {
		strcpy(response, "FAILrelock failed");
	} else {
		idme_auto_clean_before_relock();
		strcpy(response, "OKAY");
	}

	fastboot_tx_write_str(response);
	return;
}
#endif

#define BYTES_PER_DOT	0x20000
static void rx_handler_dl_image(struct usb_ep *ep, struct usb_request *req)
{
	char response[RESPONSE_LEN];
	unsigned int transfer_size = download_size - download_bytes;
	const unsigned char *buffer = req->buf;
	unsigned int buffer_size = req->actual;
	unsigned int pre_dot_num, now_dot_num;

	if (req->status != 0) {
		printf("Bad status: %d\n", req->status);
		return;
	}

	if (buffer_size < transfer_size)
		transfer_size = buffer_size;

	memcpy((void *)CONFIG_USB_FASTBOOT_BUF_ADDR + download_bytes,
	       buffer, transfer_size);

	pre_dot_num = download_bytes / BYTES_PER_DOT;
	download_bytes += transfer_size;
	now_dot_num = download_bytes / BYTES_PER_DOT;

	if (pre_dot_num != now_dot_num) {
		putc('.');
		if (!(now_dot_num % 74))
			putc('\n');
	}

	/* Check if transfer is done */
	if (download_bytes >= download_size) {
		/*
		 * Reset global transfer variable, keep download_bytes because
		 * it will be used in the next possible flashing command
		 */
		download_size = 0;
		req->complete = rx_handler_command;
		req->length = EP_BUFFER_SIZE;

		sprintf(response, "OKAY");
		fastboot_tx_write_str(response);

		printf("\ndownloading of %d bytes finished\n", download_bytes);
	} else {
		req->length = rx_bytes_expected();
		if (req->length < ep->maxpacket)
			req->length = ep->maxpacket;
	}

	req->actual = 0;
	usb_ep_queue(ep, req, 0);
}

static void cb_download(struct usb_ep *ep, struct usb_request *req)
{
	char *cmd = req->buf;
	char response[RESPONSE_LEN];

	printf("cmd cb_download is %s\n", cmd);

	strsep(&cmd, ":");
	download_size = simple_strtoul(cmd, NULL, 16);
	download_bytes = 0;

	printf("Starting download of %d bytes\n", download_size);

	if (0 == download_size) {
		sprintf(response, "FAILdata invalid size");
	} else if (download_size > ddr_size_usable(CONFIG_USB_FASTBOOT_BUF_ADDR)) {
		download_size = 0;
		sprintf(response, "FAILdata too large");
	} else {
		sprintf(response, "DATA%08x", download_size);
		req->complete = rx_handler_dl_image;
		req->length = rx_bytes_expected();
		if (req->length < ep->maxpacket)
			req->length = ep->maxpacket;
	}
	fastboot_tx_write_str(response);
}

typedef struct andr_img_hdr boot_img_hdr;

static void do_bootm_on_complete(struct usb_ep *ep, struct usb_request *req)
{
	char boot_addr_start[12];
	unsigned    kernel_size;
	unsigned    ramdisk_size;
	boot_img_hdr *hdr_addr = NULL;
	int genFmt = 0;
	unsigned actualBootImgSz = 0;
	unsigned dtbSz = 0;
	unsigned char* loadaddr = 0;

	puts("Booting kernel...\n");

	sprintf(boot_addr_start, "bootm 0x%lx", load_addr);
	printf("boot_addr_start %s\n", boot_addr_start);

	loadaddr = (unsigned char*)CONFIG_USB_FASTBOOT_BUF_ADDR;
	hdr_addr = (boot_img_hdr*)loadaddr;

	genFmt = genimg_get_format(hdr_addr);
	if (IMAGE_FORMAT_ANDROID != genFmt) {
		printf("Fmt unsupported!genFmt 0x%x != 0x%x\n", genFmt, IMAGE_FORMAT_ANDROID);
		return;
	}

	kernel_size     =(hdr_addr->kernel_size + (hdr_addr->page_size-1)+hdr_addr->page_size)&(~(hdr_addr->page_size -1));
	ramdisk_size    =(hdr_addr->ramdisk_size + (hdr_addr->page_size-1))&(~(hdr_addr->page_size -1));
	dtbSz           = hdr_addr->second_size;
	actualBootImgSz = kernel_size + ramdisk_size + dtbSz;
	printf("kernel_size 0x%x, page_size 0x%x, totalSz 0x%x\n", hdr_addr->kernel_size, hdr_addr->page_size, kernel_size);
	printf("ramdisk_size 0x%x, totalSz 0x%x\n", hdr_addr->ramdisk_size, ramdisk_size);
	printf("dtbSz 0x%x, Total actualBootImgSz 0x%x\n", dtbSz, actualBootImgSz);

	if (actualBootImgSz < CONFIG_FASTBOOT_MAX_DOWN_SIZE) {
		memcpy((void *)load_addr, (void *)CONFIG_USB_FASTBOOT_BUF_ADDR, actualBootImgSz);
		flush_cache(load_addr,(unsigned long)actualBootImgSz);
	}

	run_command(boot_addr_start, 0);

	/* This only happens if image is somehow faulty so we start over */
	do_reset(NULL, 0, 0, NULL);
}

static void cb_boot(struct usb_ep *ep, struct usb_request *req)
{
	fastboot_func->in_req->complete = do_bootm_on_complete;
	fastboot_tx_write_str("OKAY");
}

static void do_exit_on_complete(struct usb_ep *ep, struct usb_request *req)
{
	puts("Booting kernel..\n");
	run_command("run storeboot", 0);

	/* This only happens if image is somehow faulty so we start over */
	do_reset(NULL, 0, 0, NULL);
}


static void cb_continue(struct usb_ep *ep, struct usb_request *req)
{
	fastboot_func->in_req->complete = do_exit_on_complete;
	fastboot_tx_write_str("OKAY");
}

static void cb_flashing(struct usb_ep *ep, struct usb_request *req)
{
	char *cmd;
	char* response = response_str;
	char* lock_s;
	LockData_t* info;
	size_t chars_left;
	char lock_d[LOCK_DATA_SIZE];

	lock_s = getenv("lock");
	if (!lock_s) {
		printf("lock state is NULL \n");
		strcpy(lock_d, "10000000");
		lock_s = "10000000";
		setenv("lock", "10000000");
		saveenv();
	} else {
		printf("lock state: %s\n", lock_s);
		if (strlen(lock_s) > 15)
			strncpy(lock_d, lock_s, 15);
		else
			strncpy(lock_d, lock_s, strlen(lock_s));
	}

	info = malloc(sizeof(struct LockData));
	if (!info) {
		error("malloc error\n");
		fastboot_tx_write_str("FAILmalloc error");
		return;
	}
	memset(info,0,LOCK_DATA_SIZE);
	info->version_major = (int)(lock_d[0] - '0');
	info->version_minor = (int)(lock_d[1] - '0');
	info->lock_state = (int)(lock_d[4] - '0');
	info->lock_critical_state = (int)(lock_d[5] - '0');
	info->lock_bootloader = (int)(lock_d[6] - '0');
	dump_lock_info(info);

	strcpy(response, "OKAY");
	chars_left = sizeof(response_str) - strlen(response) - 1;
	cmd = req->buf;
	strsep(&cmd, " ");
	printf("cb_flashing: %s\n", cmd);
	if (!cmd) {
		error("missing variable\n");
		fastboot_tx_write_str("FAILmissing var");
		free(info);
		return;
	}

	if (!strcmp_l1("unlock_critical", cmd)) {
		info->lock_critical_state = 0;
	} else if (!strcmp_l1("lock_critical", cmd)) {
		info->lock_critical_state = 1;
	} else if (!strcmp_l1("get_unlock_ability", cmd)) {
		char str_num[8];
		sprintf(str_num, "%d", info->lock_state);
		strncat(response, str_num, chars_left);
	} else if (!strcmp_l1("get_unlock_bootloader_nonce", cmd)) {
		char str_num[8];
		sprintf(str_num, "%d", info->lock_critical_state);
		strncat(response, str_num, chars_left);
	} else if (!strcmp_l1("unlock_bootloader", cmd)) {
		strncat(response, "please run flashing unlock & flashing unlock_critical before write", chars_left);
	} else if (!strcmp_l1("lock_bootloader", cmd)) {
		info->lock_bootloader = 1;
	} else if (!strcmp_l1("unlock", cmd)) {
		if (info->lock_state == 1 ) {
			char *avb_s;
			avb_s = getenv("avb2");
			if (avb_s == NULL) {
				run_command("get_avb_mode;", 0);
				avb_s = getenv("avb2");
			}
			printf("avb2: %s\n", avb_s);
			if (strcmp(avb_s, "1") == 0) {
#ifdef CONFIG_AML_ANTIROLLBACK
				if (avb_unlock()) {
					printf("unlocking device.  Erasing userdata partition!\n");
					run_command("store erase partition data", 0);
				} else {
					printf("unlock failed!\n");
				}
#else
				printf("unlocking device.  Erasing userdata partition!\n");
				run_command("store erase partition data", 0);
#endif
			}
		}
		info->lock_state = 0;
	} else if (!strcmp_l1("lock", cmd)) {
		if (info->lock_state == 0 ) {
			char *avb_s;
			avb_s = getenv("avb2");
			if (avb_s == NULL) {
				run_command("get_avb_mode;", 0);
				avb_s = getenv("avb2");
			}
			printf("avb2: %s\n", avb_s);
			if (strcmp(avb_s, "1") == 0) {
#ifdef CONFIG_AML_ANTIROLLBACK
				if (avb_lock()) {
					printf("lock failed!\n");
				} else {
					printf("locking device.  Erasing userdata partition!\n");
					run_command("store erase partition data", 0);
				}
#else
				printf("locking device.  Erasing userdata partition!\n");
				run_command("store erase partition data", 0);
#endif
			}
		}
		info->lock_state = 1;
	} else {
		error("unknown variable: %s\n", cmd);
		strcpy(response, "FAILVariable not implemented");
	}

	dump_lock_info(info);
	sprintf(lock_d, "%d%d00%d%d%d0", info->version_major, info->version_minor, info->lock_state, info->lock_critical_state, info->lock_bootloader);
	printf("lock_d state: %s\n", lock_d);
	setenv("lock", lock_d);
	saveenv();
	printf("response: %s\n", response);
	free(info);
	fastboot_tx_write_str(response);
}


#ifdef CONFIG_FASTBOOT_FLASH
static void cb_flash(struct usb_ep *ep, struct usb_request *req)
{
	char *cmd = req->buf;
	char* response = response_str;

	printf("cmd cb_flash is %s\n", cmd);

	strsep(&cmd, ":");
	if (!cmd) {
		error("missing partition name\n");
		fastboot_tx_write_str("FAILmissing partition name");
		return;
	}
#if defined(UFBL_FEATURE_UNLOCK)
	if (!strcmp_l1("unlock", cmd)) {
		if (amzn_verify_unlock((void *)CONFIG_USB_FASTBOOT_BUF_ADDR, download_bytes)) {
			fastboot_tx_write_str("FAILunlock code error");
		} else if (idme_update_var_ex("unlock_code", (void *)CONFIG_USB_FASTBOOT_BUF_ADDR,
					SIGNED_UNLOCK_CODE_LEN)) {
			fastboot_tx_write_str("FAILunlock failed");
		} else {
			idme_auto_set_after_unlock();
			fastboot_tx_write_str("OKAY");
		}
		return;
	}
#endif
#if defined(UFBL_FEATURE_ONETIME_UNLOCK)
	else if (!strcmp_l1("otucert", cmd)) {
		if (amzn_set_onetime_unlock_cert((void *)CONFIG_USB_FASTBOOT_BUF_ADDR, download_bytes)) {
			fastboot_tx_write_str("FAILset onetime unlock cert failed");
		} else {
			fastboot_tx_write_str("OKAY");
		}
		return;
		}
		else if (!strcmp_l1("otucode", cmd)) {
			if (amzn_set_onetime_unlock_code((void *)CONFIG_USB_FASTBOOT_BUF_ADDR, download_bytes)) {
				fastboot_tx_write_str("FAILset signed onetime unlock code failed");
			} else {
				fastboot_tx_write_str("OKAY");
			}
		return;
	}
#endif
	if (check_lock()) {
		error("device is locked, can not run this cmd.Please flashing unlock & flashing unlock_critical\n");
		fastboot_tx_write_str("FAILlocked device");
		return;
	}

	if (dynamic_partition) {
		if ((strcmp(cmd, "system") == 0) || (strcmp(cmd, "vendor") == 0)
			|| (strcmp(cmd, "odm") == 0) || (strcmp(cmd, "product") == 0)) {
			error("system/vendor/odm/product is logic partition, can not write here\n");
			fastboot_tx_write_str("FAILlogic partition");
			return;
		}
	}

	printf("partition is %s\n", cmd);
	if (strcmp(cmd, "userdata") == 0) {
		strcpy(cmd, "data");
		printf("partition is %s\n", cmd);
	}

	if (strcmp(cmd, "dts") == 0) {
		strcpy(cmd, "dtb");
		printf("partition is %s\n", cmd);
	}

	//strcpy(response, "FAILno flash device defined");
	if (is_mainstorage_emmc()) {
#ifdef CONFIG_FASTBOOT_FLASH_MMC_DEV
		fb_mmc_flash_write(cmd, (void *)CONFIG_USB_FASTBOOT_BUF_ADDR,
				   download_bytes);
#endif
	} else if (is_mainstorage_nand()) {
#ifdef CONFIG_FASTBOOT_FLASH_NAND_DEV
		fb_nand_flash_write(cmd, (void *)CONFIG_USB_FASTBOOT_BUF_ADDR,
					download_bytes);
#else
		fastboot_fail("not support nftl\n");
#endif
	} else {
		printf("error: no valid fastboot device\n");
		fastboot_fail("no vaild device\n");
	}
//	fastboot_tx_write_str(response);
}
#endif

#ifdef CONFIG_FASTBOOT_SLOT_SWITCHING
static void cb_oem_mark_slot_successful(struct usb_ep *ep, struct usb_request *req)
{
	char *cmd = req->buf;
	//char response[RESPONSE_LEN];
	int ret = 0;
	char str[UBOOT_RUN_CMD_LEN] = {0};

	printf("cmd cb_oem_mark_slot_successful is %s\n", cmd);
	cmd = strstr(cmd, "mark_slot_successful");
	strsep(&cmd, " ");
	if (!cmd) {
		error("missing slot name\n");
		fastboot_tx_write_str("FAILmissing slot name");
		return;
	}

	sprintf(str, "mark_slot_successful %s", cmd);
	printf("command:    %s\n", str);
	ret = run_command(str, 0);
	printf("ret = %d\n", ret);
	if (ret == 0)
		fastboot_tx_write_str("OKAY");
	else
		fastboot_tx_write_str("FAILmark slot successful error");
}

static void cb_set_active(struct usb_ep *ep, struct usb_request *req)
{
	char *cmd = req->buf;
	//char response[RESPONSE_LEN];
	int ret = 0;
	char str[UBOOT_RUN_CMD_LEN] = {0};

	printf("cmd cb_set_active is %s\n", cmd);
	strsep(&cmd, ":");
	if (!cmd) {
		error("missing slot name\n");
		fastboot_tx_write_str("FAILmissing slot name");
		return;
	}

	if (check_lock()) {
		error("device is locked, can not run this cmd.Please flashing unlock & flashing unlock_critical\n");
		fastboot_tx_write_str("FAILlocked device");
		return;
	}

	sprintf(str, "set_active_slot %s", cmd);
	printf("command:    %s\n", str);
	ret = run_command(str, 0);
	printf("ret = %d\n", ret);
	if (ret == 0)
		fastboot_tx_write_str("OKAY");
	else
		fastboot_tx_write_str("FAILset slot error");
}
#endif

static void cb_flashall(struct usb_ep *ep, struct usb_request *req)
{
	char* response = response_str;
	char *cmd = req->buf;

	printf("cmd cb_flashall is %s\n", cmd);

	if (check_lock()) {
		error("device is locked, can not run this cmd.Please flashing unlock & flashing unlock_critical\n");
		fastboot_tx_write_str("FAILlocked device");
		return;
	}

	//strcpy(response, "FAILno flash device defined");
	if (is_mainstorage_emmc()) {
#ifdef CONFIG_FASTBOOT_FLASH_MMC_DEV
		fb_mmc_flash_write(cmd, (void *)CONFIG_USB_FASTBOOT_BUF_ADDR,
				   download_bytes);
#endif
	} else if (is_mainstorage_nand()) {
#ifdef CONFIG_FASTBOOT_FLASH_NAND_DEV
		fb_nand_flash_write(cmd, (void *)CONFIG_USB_FASTBOOT_BUF_ADDR,
					download_bytes);
#else
		fastboot_fail("not support nftl\n");
#endif
	} else {
		printf("error: no valid fastboot device\n");
		fastboot_fail("no vaild device\n");
	}
//	fastboot_tx_write_str(response);
}

static void cb_erase(struct usb_ep *ep, struct usb_request *req)
{
	char* response = response_str;
	char *cmd = req->buf;

	printf("cmd cb_erase is %s\n", cmd);

	strsep(&cmd, ":");
	if (!cmd) {
		error("missing partition name\n");
		fastboot_tx_write_str("FAILmissing partition name");
		return;
	}

	if (check_lock()) {
		error("device is locked, can not run this cmd.Please flashing unlock & flashing unlock_critical\n");
		fastboot_tx_write_str("FAILlocked device");
		return;
	}

	printf("partition is %s\n", cmd);
	if (strcmp(cmd, "userdata") == 0) {
		strcpy(cmd, "data");
		printf("partition is %s\n", cmd);
	}

	if (strcmp(cmd, "dts") == 0) {
		strcpy(cmd, "dtb");
		printf("partition is %s\n", cmd);
	}

	//strcpy(response, "FAILno erase device defined");
	if (is_mainstorage_emmc()) {
#ifdef CONFIG_FASTBOOT_FLASH_MMC_DEV
		fb_mmc_erase_write(cmd, (void *)CONFIG_USB_FASTBOOT_BUF_ADDR);
#endif
	} else if (is_mainstorage_nand()) {
#ifdef CONFIG_FASTBOOT_FLASH_NAND_DEV
		fb_nand_erase(cmd, (void *)CONFIG_USB_FASTBOOT_BUF_ADDR);
#else
		fastboot_fail("not support nftl\n");
#endif
	} else {
		printf("error: no valid fastboot device\n");
		fastboot_fail("no vaild device\n");
	}
//	fastboot_tx_write_str(response);
}

static void cb_devices(struct usb_ep *ep, struct usb_request *req)
{
	char response[RESPONSE_LEN];
	char *cmd = req->buf;

	printf("cmd is %s\n", cmd);

	strcpy(response, "AMLOGIC");

	fastboot_tx_write_str(response);
}

static void cb_oem_cmd(struct usb_ep *ep, struct usb_request *req)
{
	char response[RESPONSE_LEN/2 + 1];
	char* cmd = req->buf;
	printf("oem cmd[%s]\n", cmd);
	static int i = 0;

	memcpy(response, cmd, strnlen(cmd, RESPONSE_LEN/2)+1);//+1 to terminate str
	cmd = response;
	strsep(&cmd, " ");
	FB_MSG("To run cmd[%s]\n", cmd);
	run_command(cmd, 0);

    if (++i > 3) i = 0;

	i ? fastboot_busy("AMLOGIC") : fastboot_okay(response);
	fastboot_tx_write_str(response_str);
	return ;
}

struct cmd_dispatch_info {
	char *cmd;
	void (*cb)(struct usb_ep *ep, struct usb_request *req);
};

static const struct cmd_dispatch_info cmd_dispatch_info[] = {
	{
		.cmd = "reboot",
		.cb = cb_reboot,
	}, {
		.cmd = "getvar:",
		.cb = cb_getvar,
	}, {
		.cmd = "download:",
		.cb = cb_download,
	}, {
		.cmd = "boot",
		.cb = cb_boot,
	}, {
		.cmd = "continue",
		.cb = cb_continue,
	}, {
		.cmd = "flashing",
		.cb = cb_flashing,
	},
#ifdef CONFIG_FASTBOOT_FLASH
	{
		.cmd = "flash",
		.cb = cb_flash,
	},
#endif
	{
		.cmd = "update",
		.cb = cb_download,
	},
	{
		.cmd = "flashall",
		.cb = cb_flashall,
	},
	{
		.cmd = "erase",
		.cb = cb_erase,
	},
	{
		.cmd = "devices",
		.cb = cb_devices,
	},
	{
		.cmd = "reboot-bootloader",
		.cb = cb_reboot,
	},
#ifdef CONFIG_FASTBOOT_SLOT_SWITCHING
	{
		.cmd = "set_active",
		.cb = cb_set_active,
	},
	{
		.cmd = "oem mark_slot_successful",
		.cb = cb_oem_mark_slot_successful,
	},
#endif
#if 0
	{
		.cmd = "reboot-fastboot",
		.cb = cb_reboot,
	},
#endif
#if defined(CONFIG_IDME)
	{
		.cmd = "oem idme",
		.cb = cb_oem_idme,
	},

#endif
	{
		.cmd = "oem bootmode",
		.cb = cmd_oem_bootmode,
	},
	{
		.cmd = "oem flags",
		.cb = cmd_oem_flags,
	},
#if defined(UFBL_FEATURE_UNLOCK)
	{
		.cmd = "oem relock",
		.cb = cb_oem_relock,
	},
#endif
};
/*
int fastboot_info(const char *info)
{
	return fastboot_tx_write(info, strlen(info));
}
*/
//cb for out_req->complete
static void rx_handler_command(struct usb_ep *ep, struct usb_request *req)
{
	char *cmdbuf = req->buf;
	void (*func_cb)(struct usb_ep *ep, struct usb_request *req) = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(cmd_dispatch_info); i++) {
		if (!strcmp_l1(cmd_dispatch_info[i].cmd, cmdbuf)) {
			func_cb = cmd_dispatch_info[i].cb;
			break;
		}
	}

	if (!func_cb) {
		error("unknown command: %s\n", cmdbuf);
		fastboot_tx_write_str("FAILunknown command");
#if defined(UFBL_FEATURE_FASTBOOT_LOCKDOWN)
	} else if (is_restricted_command_on_locked_hw(cmdbuf)) {
		error("I'm sorry, but command %s is blocked at fastboot\n",
				cmd_dispatch_info[i].cmd);
		fastboot_tx_write_str("FAILblocked command");
#endif
	}else {
		if (req->actual < req->length) {
			u8 *buf = (u8 *)req->buf;
			buf[req->actual] = 0;
			func_cb(ep, req);
		} else {
			error("buffer overflow\n");
			fastboot_tx_write_str("FAILbuffer overflow");
		}
	}

	if (req->status == 0 && !fastboot_is_busy()) {
		*cmdbuf = '\0';
		req->actual = 0;
		usb_ep_queue(ep, req, 0);
	}
}
