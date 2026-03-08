/*
 * board_fdt_fixup.c
 *
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include "amzn_secure_boot.h"
#include <common.h>
#include <malloc.h>
#include <fdt_support.h>
#include <libfdt.h>
#include <partition_table.h>
#if defined(UFBL_FEATURE_UNLOCK)
extern int amzn_device_unlock_status;
#endif
#ifdef CONFIG_IDME
#include <idme.h>
#endif
extern bool secure_boot_enabled(void);
void amzn_disable_partition_verity_maybe(void *fdt, const char * dir_name);
#if defined(CONFIG_UBOOT_LOGGER)
#include <uboot_log.h>
#include <image.h>
#endif
#if defined(CONFIG_UBOOT_LOGGER)
extern unsigned *get_uboot_log_buf_addr();
extern unsigned get_uboot_log_buf_size();

extern  ulong getenv_bootm_low(void);
extern  phys_size_t getenv_bootm_size(void);
extern  phys_size_t getenv_bootm_mapsize(void);
#endif
#define MAX_ROOT_ID_SIZE 120

#define BUILD_VARIANT_UNKNOWN  -1
#define BUILD_VARIANT_USER      0
#define BUILD_VARIANT_USERDEBUG 1

/*
 * Get build variant
 * user return 0
 * userdebug return 1
 * unknown return -1
 *
 */
static int get_build_variant(const char *line)
{
	int ret = BUILD_VARIANT_UNKNOWN;
	const int max_cmdline_len = 2048;
	/* Check the kernel is user or userdebug */
	char *buildvar  = NULL;

	/* Check the command line length */
	if (strlen(line) >= max_cmdline_len) {
		printf("The command line is too long\n");
		return BUILD_VARIANT_UNKNOWN;
	}

	/* Get buildvariant value */
	buildvar = strstr(line, "buildvariant=");
	if (buildvar == NULL) {
		printf("Unable to get build variant\n");
		goto exit;
	}

	buildvar += strlen("buildvariant=");

	/* Check the build variant */
	char *new_var = strdup(buildvar);
	char *var_end = strchr(new_var, ' ');
	if (var_end != NULL)
		*var_end='\0';

	if (strncmp(new_var, "userdebug", strlen("userdebug")) == 0) {
		ret = BUILD_VARIANT_USERDEBUG;
	} else if (strncmp(new_var, "user", strlen("user")) == 0) {
		ret = BUILD_VARIANT_USER;
	} else {
		printf("Unknown build variant:%s\n", new_var);
		goto free;
	}

free:
	free(new_var);
exit:
	return ret;
}

/*
 * read fos_flags from idme
 */
#define UBOOT_DM_VERITY_ENABLE
unsigned long get_fos_flags(void)
{
	unsigned long flags = 0;

#ifdef  CONFIG_IDME
	char fos_buf[16];
	int ret = 0;
	ret = idme_get_var_external("fos_flags", fos_buf, sizeof(fos_buf));

	if (ret < 0) {
		printf("get idme fos_flags Error\n");
		return 0;
	}

	flags = simple_strtoul(fos_buf, NULL, 16);
#endif
	printf("fos_flags=%x\n", flags);
	return flags;
}

#define IDME_FLAGS_LEN 32
static unsigned int get_kernel_log_level(char *cmdline)
{
	char flags_buf[IDME_FLAGS_LEN+1] = {0};
	unsigned kernel_loglevel = 0;
	unsigned long long fos_flags = 0;

	if (BUILD_VARIANT_USERDEBUG == get_build_variant(cmdline)) {
		kernel_loglevel = 7;
		printf("Userdebug !\n");
	} else {
		if ((get_fos_flags() & FOS_FLAGS_CONSOLE_ON) == FOS_FLAGS_CONSOLE_ON) {
			printf("Enable uart !\n");
			kernel_loglevel = 7;
		} else {
			printf("Disable uart !\n");
			kernel_loglevel = 0;
		}
	}
	return kernel_loglevel;
}

/*
 * Checks whether dm-verity is disabled
 * For locked production device , always return false
 * For unlocked/engineering device, check amazon fos_flags
 * 	if bit7 is set, return true
 * 	if bit7 is clear, return false
 */
/* TODO: Update code to use FOS_FLAGS_DM_VERITY_OFF inside idme.h */
#define	PLATFORM_FOS_FLAGS_DM_VERITY_OFF		(1 << 7)
 // static struct uboot_log ulog ;
bool amzn_dm_verity_is_off(int unlock_status)
{
	int lock_state;

	lock_state = ((unlock_status == 0) &&
			(amzn_target_device_type() != AMZN_ENGINEERING_DEVICE));

	if (lock_state) {
		/* Locked device: dm-verity is on and cannot be off */
		return false;
	} else if (get_fos_flags() & PLATFORM_FOS_FLAGS_DM_VERITY_OFF) {
		/*
		 * Unlocked/Engineering device with bit 7 set
		 * in fos_flags, dm-verity is off
		 */
		return true;
	} else {
		/* dm-verity is on otherwise */
		return false;
	}
}

static int get_root_partition_id(char *root_device_id)
{
	int err = 0;
#ifdef CONFIG_IDME
	int is_diag_bootmode = 0;
	int is_transition_bootmode = 0;
	int bootmode = IDME_BOOTMODE_NORMAL;

	bootmode = idme_boot_mode();
	is_diag_bootmode = (bootmode == IDME_BOOTMODE_DIAG);
	is_transition_bootmode = (bootmode == IDME_BOOTMODE_TRANSITION);

	if (is_diag_bootmode || is_transition_bootmode) {
#if defined UBOOT_DM_VERITY_ENABLE
		if(amzn_dm_verity_is_off(amzn_device_unlock_status) == true){
			err = snprintf(root_device_id, MAX_ROOT_ID_SIZE,"mmcblk0p%d ",
				(get_partition_num_by_name("dfs")+1));
		}
		else {
			err = snprintf(root_device_id, MAX_ROOT_ID_SIZE,"dm-0 dm=\"system none ro,0 1 android-verity /dev/mmcblk0p%d\"",
				(get_partition_num_by_name("dfs")+1));
		}
#else
			err = snprintf(root_device_id, MAX_ROOT_ID_SIZE,"mmcblk0p%d ",
				(get_partition_num_by_name("dfs")+1));
#endif
	} else {
#if defined UBOOT_DM_VERITY_ENABLE
		if(amzn_dm_verity_is_off(amzn_device_unlock_status) == true) {
			if (active_slot == 'a') {
				err = snprintf(root_device_id, MAX_ROOT_ID_SIZE,"mmcblk0p%d ",
					(get_partition_num_by_name("system_a")+1));
			} else if (active_slot == 'b') {
				err = snprintf(root_device_id, MAX_ROOT_ID_SIZE,"mmcblk0p%d ",
					(get_partition_num_by_name("system_b")+1));
			} else {
				err = snprintf(root_device_id, MAX_ROOT_ID_SIZE,"mmcblk0p%d ",
					(get_partition_num_by_name("system")+1));
			}
		} else {
			if (active_slot == 'a') {
				err = snprintf(root_device_id, MAX_ROOT_ID_SIZE,"dm-0 dm=\"system none ro,0 1 android-verity /dev/mmcblk0p%d\"",
					(get_partition_num_by_name("system_a")+1));
			} else if (active_slot == 'b') {
				err = snprintf(root_device_id, MAX_ROOT_ID_SIZE,"dm-0 dm=\"system none ro,0 1 android-verity /dev/mmcblk0p%d\"",
					(get_partition_num_by_name("system_b")+1));
			} else {
				err = snprintf(root_device_id, MAX_ROOT_ID_SIZE,"dm-0 dm=\"system none ro,0 1 android-verity /dev/mmcblk0p%d\"",
					(get_partition_num_by_name("system")+1));
			}
		}
#else
			err = snprintf(root_device_id, MAX_ROOT_ID_SIZE,"mmcblk0p%d ",
				(get_partition_num_by_name("system")+1));
#endif
	}

#else
	err = snprintf(root_device_id, MAX_ROOT_ID_SIZE,"mmcblk0p%d ",
		(get_partition_num_by_name("system")+1));
#endif
	return err;
}

int fixup_ublog_resmem(void *fdt , void * bl_addr , int bl_size)
{
	int nodeoffset;
	char uboot_log_reg[64]={0};
	struct uboot_log * log_buffer ;
	int err;
	struct  reg_info      /* RAM configuration */
	{
		unsigned int addr;
		unsigned int size;
	}  reg;
	printf("[bl_log] fixup ublog memory \n");
	nodeoffset = fdt_node_offset_by_compatible(fdt, -1, "bl_log");
	if (nodeoffset < 0) {
		printf( "[bl_log] %s: getting node from dtb fails\n", __func__);
		return -1; /* return -1, if fail */
	}
	reg.addr = cpu_to_fdt32(bl_addr);
	reg.size = cpu_to_fdt32(bl_size);
	//err = sprintf(uboot_log_reg, "<0x%p 0x%d>", log_buffer, get_uboot_log_buf_size());
	//    printf("uboot/reg = <0x%x 0x%d>",reg.addr,reg.size);
	if (fdt_setprop(fdt, nodeoffset, "reg", &reg, sizeof(reg)) < 0) {
		printf( "[bl_log] %s: set reg fails\n", __func__);
		return -1; /* return -1, if fail */
	}
	return 0;
}


int board_fixup_fdt(void *blob,bootm_headers_t *images)
{
	int err, nodeoffset, len = 0;
	const char *str = NULL;
	char *new_str = NULL;
	size_t new_str_size = 0;

	amzn_disable_partition_verity_maybe(blob,"/firmware/android/fstab/vendor");
	amzn_disable_partition_verity_maybe(blob,"/firmware/android/fstab/product");
	err = fdt_check_header(blob);

	if (err < 0) {
		printf("%s: Invalid FDT blob: %s\n",
				__FUNCTION__, fdt_strerror(err));
		return err;
	}

	err = fdt_path_offset(blob, "/chosen");
	if (err < 0) {
		printf("%s: Cannot find /chosen node: %s\n",
				__FUNCTION__, fdt_strerror(err));
		return err;
	}
	nodeoffset = err;

	/* Retrieve the cmdline string */
	str = fdt_getprop_namelen(blob, nodeoffset,
			"bootargs", strlen("bootargs"), &len);

	if (!str) {
		printf("%s: Unable to locate bootargs property",
				__FUNCTION__);
		return -FDT_ERR_NOTFOUND;
	}

	/* Construct a new string adding our own fields */
	new_str_size = strlen(str) +
		strlen(" androidboot.secure_cpu=1 androidboot.prod=1" \
				" androidboot.arb_enabled=1") + 1;
	new_str = malloc(new_str_size);

	if (!new_str) {
		printf("%s: Unable to allocate memory\n",
				__FUNCTION__);
		return -FDT_ERR_NOSPACE;
	}

	err = snprintf(new_str, new_str_size,
			"%s androidboot.secure_cpu=%d androidboot.prod=%d" \
			" androidboot.arb_enabled=%d", str,
			(secure_boot_enabled() == true) ? 1 : 0,
			(amzn_target_device_type() == AMZN_ENGINEERING_DEVICE) ? 0 : 1,
			run_command("query ARB", 0));

	if (err < 0) {
		printf("%s: snprintf error\n", __FUNCTION__);
	} else if (err >= new_str_size) {
		printf("%s: Truncated bootargs string\n", __FUNCTION__);
		free(new_str);
		return -FDT_ERR_TRUNCATED;
	}

#if defined(CONFIG_UBOOT_LOGGER)
#define MAX_UBOOT_LOG_CMDLINE_SIZE 36
	char uboot_log_cmd[MAX_UBOOT_LOG_CMDLINE_SIZE]={0};
	struct uboot_log * log_buffer ;
	struct uboot_log * log_buffer_ori ;


	log_buffer = (struct uboot_log * )(ulong)lmb_alloc_base(&images->lmb, UBOOT_LOG_BUF_SIZE, 0xf,	getenv_bootm_mapsize() + getenv_bootm_low());
	log_buffer_ori=get_uboot_log_buf_addr();

	lmb_reserve(&images->lmb, log_buffer_ori,  UBOOT_LOG_BUF_SIZE);
	if (log_buffer){
		printf("copy logbuff to kernel !--------\n");
		memcpy(log_buffer,log_buffer_ori,UBOOT_LOG_BUF_SIZE);
		err = sprintf(uboot_log_cmd, " uboot_log=%p,%d", log_buffer, get_uboot_log_buf_size());
	}else{
		printf("log_buffer alloc failed");

	}

//	lmb_dump_all(&images->lmb);
	err = sprintf(uboot_log_cmd, " uboot_log=%p,%d", log_buffer, get_uboot_log_buf_size());
	if (err < 0) {
		printf("%s: snprintf error\n", __FUNCTION__);
	} else if (err >= MAX_UBOOT_LOG_CMDLINE_SIZE) {
		printf("%s: Truncated bootargs string\n", __FUNCTION__);
		free(new_str);
		return -FDT_ERR_TRUNCATED;
	}

	new_str_size += strlen(uboot_log_cmd) +1;
	new_str = realloc(new_str, new_str_size);
	if (!new_str) {
		printf("%s: Unable to allocate memory\n",
				__FUNCTION__);
		return -FDT_ERR_NOSPACE;
	}
	strncat(new_str, uboot_log_cmd, strlen(uboot_log_cmd) + 1);
	if(fixup_ublog_resmem(blob, log_buffer, get_uboot_log_buf_size()) != 0){
		printf("uboot log memory reserves fail\n");
	}
	printf("reset uboot log memory\n");
#endif

	char root_device_id[MAX_ROOT_ID_SIZE]={0};
	err = get_root_partition_id(root_device_id);
	if (err < 0) {
		printf("%s: unable to get root device ID \n",
				__FUNCTION__ );
		return err;
	}

/*
#ifdef CONFIG_IDME
	bootmode = idme_boot_mode();
	is_diag_bootmode = (bootmode == IDME_BOOTMODE_DIAG);
	is_transition_bootmode = (bootmode == IDME_BOOTMODE_TRANSITION);

	if (is_diag_bootmode || is_transition_bootmode){
		err = snprintf(root_device_id, MAX_ROOT_ID_SIZE," root=/dev/mmcblk0p%d ",
				(get_partition_num_by_name("dfs")+1));
	}else {
		err = snprintf(root_device_id, MAX_ROOT_ID_SIZE," root=/dev/mmcblk0p%d ",
				(get_partition_num_by_name("system")+1));
	}
#else
	err = snprintf(root_device_id, MAX_ROOT_ID_SIZE," root=/dev/mmcblk0p%d ",
		(get_partition_num_by_name("system")+1));
#endif


	if (err < 0) {
		printf("%s: snprintf error\n", __FUNCTION__);
	} else if (err >= MAX_ROOT_ID_SIZE) {
		printf("%s: Truncated bootargs string\n", __FUNCTION__);
		free(new_str);
		return -FDT_ERR_TRUNCATED;
	}

	new_str_size += strlen(root_device_id) +1;
	new_str = realloc(new_str, new_str_size);
	if (!new_str) {
		printf("%s: Unable to allocate memory\n",
				__FUNCTION__);
		return -FDT_ERR_NOSPACE;
	}
	strncat(new_str, root_device_id, strlen(root_device_id) + 1);
*/

#if defined(UFBL_FEATURE_UNLOCK)
#define MAX_UNLOCK_STR_SIZE 150
	char unlock_str[MAX_UNLOCK_STR_SIZE] = {0};

	err = snprintf(unlock_str, MAX_UNLOCK_STR_SIZE," androidboot.unlocked_kernel=%s androidboot.veritymode=%s root=/dev/%s",
			(amzn_device_unlock_status == 1) ? "true" : "false",
#if defined UBOOT_DM_VERITY_ENABLE
			(amzn_dm_verity_is_off(amzn_device_unlock_status) == true) ? "disabled" : "eio",
			root_device_id);
#else
			(amzn_dm_verity_is_off(amzn_device_unlock_status) == true) ? "disabled" : "disabled",
			root_device_id);
#endif
	

	if (err < 0) {
		printf("%s: snprintf error\n", __FUNCTION__);
	} else if (err >= MAX_UNLOCK_STR_SIZE) {
		printf("%s: Truncated bootargs string\n", __FUNCTION__);
		free(new_str);
		return -FDT_ERR_TRUNCATED;
	}

	new_str_size += strlen(unlock_str) +1;
	new_str = realloc(new_str, new_str_size);
	if (!new_str) {
		printf("%s: Unable to allocate memory\n",
				__FUNCTION__);
		return -FDT_ERR_NOSPACE;
	}
	strncat(new_str, unlock_str, strlen(unlock_str) + 1);
#endif

#if defined(CONFIG_IDME)
	const char *console_config = "console=ttyS0,115200";
	unsigned int pos = NULL;
	if (get_kernel_log_level(new_str) == 0) {
		pos = strstr(new_str, console_config);
		if (pos == NULL) {
			printf("Unable to find console, exit\n");
		} else {
			memmove(pos, pos+strlen(console_config), (new_str+strlen(new_str)-pos));
		}
	}
#endif
	err = fdt_setprop(blob, nodeoffset,
			"bootargs", new_str, strlen(new_str) + 1);
	if (err < 0)
		printf("%s: fdt_setprop failed: %s",
				__FUNCTION__, fdt_strerror(err));

	free(new_str);

	return err;
}


/* If dm-verity is OFF by fos_flags, we need to remove the 'verity' for vendor product parititon */
void amzn_disable_partition_verity_maybe(void *fdt,  const char *dir_name) {
	int offset = 0;
	int len = 0;
	const void* vendor_fsmgr_flags = NULL;
	const char* node_name = "fsmgr_flags";
	// under /sys/firmware/devicetree/base
	const char* node_dir_name = dir_name;
	char* buffer = NULL;
	char* target_buffer = NULL;
	int unlocked = amzn_device_unlock_status;

	if (node_dir_name == NULL) {
		return;
	}
#if defined UBOOT_DM_VERITY_ENABLE
	/* do nothing if dm-verity is ON */
	if (amzn_dm_verity_is_off(unlocked) == 0) {
		printf("[DM-VERITY] do nothing , dm-verity is ON\n");
		return;
	}
#endif
	offset = fdt_path_offset(fdt, node_dir_name);
	if (offset < 0) {
		printf("[DM-VERITY] Couldn't find %s node, do nothing!\n",node_dir_name);
		return;
	}

	vendor_fsmgr_flags = fdt_getprop(fdt, offset, node_name, &len);
	if (!vendor_fsmgr_flags || !len) {
		printf("[DM-VERITY] couldn't get node %s/%sm, do nothing!\n",
				node_dir_name, node_name);
		return;
	}

	buffer = malloc(len+1);
	if (!buffer) {
		printf("[DM-VERITY] allocate temp buffer failed, do nothing!\n");
		return;
	}
	memcpy(buffer, vendor_fsmgr_flags, len);
	buffer[len] = '\0';

	len = strlen(buffer);

	printf("[DM-VERITY] Found vendor fsmgr_flags value is [%s]\n", buffer);

	char *found_ptr = strstr(buffer, "verify");
	int len_verify = strlen("verify");
	int i = 0;
	if (found_ptr && (found_ptr == buffer || *(found_ptr - 1) == ',') &&
	    (found_ptr + len_verify == buffer + len || *(found_ptr + len_verify) == ',')) {
		printf("[DM-VERITY] found verify, erase it!\n");
		char *from = found_ptr + len_verify;
		char *to = found_ptr;
		while (from < buffer + len) {
			if (from == found_ptr + len_verify &&
			    *from == ',' &&
			    (found_ptr == buffer || *(found_ptr - 1) == ',')) {
				// skip coping the unnecessary ','
				++from;
			} else {
				*to = *from;
				++to;
				++from;
			}
		}
		*to = '\0';
		if (to > buffer && *(to-1) == ',') {  // remove the possible trailing ','
			*(to-1) = '\0';
		}
	} else {
		printf("[DM-VERITY] don't find verify, do nothing!\n");
		free(buffer);
		return;
	}
	printf("[DM-VERITY] change fsmgr_flags to [%s]\n", buffer);
	fdt_setprop(fdt, offset, "fsmgr_flags", buffer, strlen(buffer) + 1);
	free(buffer);

}


