/*
* Copyright (C) 2017 Amlogic, Inc. All rights reserved.
* *
This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
* *
This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
* *
You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
* *
Description:
*/

#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <idme.h>
#include <android_image.h>

#define IDME_FLAGS_LEN 32


#define BUILD_VARIANT_UNKNOWN  -1
#define BUILD_VARIANT_USER      0
#define BUILD_VARIANT_USERDEBUG 1


/*
* get build variant
* user return 0
* userdebug return 1
* unkonw return -1
*
*/
static int getbuildvariant(const char * line)
{
    int ret = BUILD_VARIANT_UNKNOWN;
    /* Check the kernel is user or userdebug */
    char * buildvar  = NULL;

    // Check the command line length
    if (strlen(line) >= ANDR_BOOT_ARGS_SIZE) {
        printf("the command line is too long\n");
        return BUILD_VARIANT_UNKNOWN;
    }

    // Get buildvariant value
    buildvar = strstr(line, "buildvariant=");
    if (buildvar == NULL) {
        printf("can't get build variant\n");
        goto exit;
    }

    buildvar += strlen("buildvariant=");

    /*
        Check the build variant.
    */
    char * new_var = strdup(buildvar);
    char * var_end = strchr(new_var, ' ');
    if (var_end != NULL)
        *var_end='\0';

    if (strncmp(new_var, "userdebug", strlen("userdebug")) == 0) {
//        printf(".....USERDEBUG\n");
        ret = BUILD_VARIANT_USERDEBUG;
    } else if (strncmp(new_var, "user", strlen("user")) == 0) {
//        printf(".....USER\n");
        ret = BUILD_VARIANT_USER;
    } else {
        printf("unknown build variant:%s\n", new_var);
        goto free;
    }

free:
    free(new_var);
exit:
    return ret;
}

/*
    Check first boot.
*/
static int isfirstboot(void)
{
    char buf[IDME_FLAGS_LEN+1] = { 0 };
    int count = 0;
    if (0!=idme_get_var_external("bootcount", buf, sizeof(buf))) {
        return 0;
    }

    count = simple_strtoul(buf, NULL, 10);
    printf("bootcount=%d\n", count);
    return (count <= 1);
}

/*
    Check USB status.
*/
int android_image_update_usb_mode(const char * line)
{
    char flags_buf[IDME_FLAGS_LEN+1] = {0};
    unsigned long long dev_flags = 0;
    unsigned long long fos_flags = 0;

    int usb_mode = 1; // 1 is device, 0 host

    // Check the command line length
    if (strlen(line) >= ANDR_BOOT_ARGS_SIZE) {
        printf("the command line is too long\n");
        return -1;
    }

    // get dev_flags
    if(0 != idme_get_var_external("dev_flags", flags_buf, IDME_FLAGS_LEN)) {
        printf("Error getting dev_flags value\n");
        return -1;
    }
    dev_flags = simple_strtoul(flags_buf, NULL, 16);
    usb_mode =((dev_flags & DEV_FLAGS_USB_DEVICE) == DEV_FLAGS_USB_DEVICE);

    printf("dev_flags:%llx\n", dev_flags);

    /*
    */
    if (isfirstboot()) {
        printf("First boot.\n");
        // if build variant is user
        if (BUILD_VARIANT_USER == getbuildvariant(line)) {
            if (0 != idme_get_var_external("fos_flags", flags_buf, IDME_FLAGS_LEN)) {
                printf("Error getting fos_flags value\n");
                return -1;
            }
            printf("flags_buf:%s\n", flags_buf);

            fos_flags = simple_strtoul(flags_buf, NULL, 16);
            if ((fos_flags & FOS_FLAGS_ADB_ON) == FOS_FLAGS_ADB_ON) {
                // set dev_flags USB device
                printf("fos_flag adb on\n");
                dev_flags |= DEV_FLAGS_USB_DEVICE;
                snprintf(flags_buf, IDME_FLAGS_LEN+1, "%llx", dev_flags);
                idme_update_var_ex("dev_flags", flags_buf, IDME_FLAGS_LEN);
            }
        } else if (BUILD_VARIANT_USERDEBUG == getbuildvariant(line)) {
            // set dev_flags USB device
            printf("dev_flags usb device\n");
            dev_flags |= DEV_FLAGS_USB_DEVICE;
            snprintf(flags_buf, IDME_FLAGS_LEN+1, "%llx", dev_flags);
            idme_update_var_ex("dev_flags", flags_buf, IDME_FLAGS_LEN);
        }
    }

    // get dev_flags
    if(0 != idme_get_var_external("dev_flags", flags_buf, IDME_FLAGS_LEN)) {
        printf("Error getting dev_flags value\n");
        return -1;
    }
    dev_flags = simple_strtoul(flags_buf, NULL, 16);
    usb_mode =((dev_flags & DEV_FLAGS_USB_DEVICE) == DEV_FLAGS_USB_DEVICE);


    // Check recovery usb mode
    char * recovery_otg_device = NULL;
    recovery_otg_device = getenv("recovery_otg_device");
    if (recovery_otg_device != NULL) {
        printf("recovery_otg_device=%s\n", recovery_otg_device);
        if (strcmp(recovery_otg_device, "host") == 0) {
            usb_mode = 0;
        } else if (strcmp(recovery_otg_device, "device") == 0) {
            usb_mode = 1;
        }
    }

    char * otg_device  = NULL;
    // Get buildvariant value
    otg_device = strstr(line, "otg_device=");
    if (otg_device == NULL) {
        printf("can't get build variant\n");
        return -1;
    }
    otg_device += strlen("otg_device=");

    printf("USB mode:%s\n", (usb_mode == 1) ? "device" : "host");
    *otg_device = usb_mode+48;
    return 0;
}

