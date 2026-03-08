/*
 * (C) Copyright 2018
 * Zhigang.Yu@amlogic.com
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * Portions copyright 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

/*
 * Uboot update
 */
#include <common.h>
#include <command.h>
#include <s_record.h>
#include <net.h>
#include <ata.h>
#include <asm/io.h>
#include <part.h>
#include <fat.h>
#include <fs.h>
#include <zip_archive.h>
#include <../drivers/usb/gadget/v2_burning/v2_sdc_burn/optimus_sdc_burn_i.h>
#include <partition_table.h>
#include <usb.h>
#include <usb/usb_unlock_key.h>
#if defined(CONFIG_IDME)
#include <idme.h>
#endif

#ifdef CONFIG_UBOOT_USB_UPDATE

#define NAME_MAX    (32)
#define VALUE_MAX    (1024)
#define MAX_LINE_BUFF (NAME_MAX+VALUE_MAX)

static u32 fb_width;
static u32 fb_height;
static u32 display_bpp;
static unsigned char *fb_addr;
extern unsigned long get_fb_addr(void);
static int metadata_recovery = 0;
static int update_ui_success = 0;

#define color_red (0xff << 16)
#define color_green (0xff << 8)
#define color_blue (0xff << 0)
#define color_white (color_red | color_green | color_blue)
#define RSA_2048_SIG_LEN 256
#define UPDATE_FILE_NAME "update.zip"
#define UPDATE_FILE_SIZE_MAX 1500000000 // Max size 1.5GB
#define UPDATE_FILE_SIGNATURE "update.zip.signed"
#define VERSION_PROP_FILENAME "version.prop"
#define VERSION_PROP "ro.build.version.incremental="
#define LOAD_ADDR 0x10800000
#define UPDATE_FILE_LOAD_ADDR 0x19000000 // Leave a buffer of 136MB to hold the extracted chunk
#define CMD_LOAD_SYSTEM_A_PROP "ext4load mmc 1:11 0x10800000 system/build.prop"
#define CMD_LOAD_SYSTEM_B_PROP "ext4load mmc 1:12 0x10800000 system/build.prop"
#define CMD_SET_ACTIVE_SLOT_A "set_active_slot a"
#define CMD_SET_ACTIVE_SLOT_B "set_active_slot b"
#define CMD_MARK_SLOT_SUCCESSFUL_A "mark_slot_successful a"
#define CMD_MARK_SLOT_SUCCESSFUL_B "mark_slot_successful b"
#define CMD_RESET_AB_METADATA "reset_ab_metadata"
#define CMD_USB_STOP "usb stop"
#define DEALERSHIP_UNBRICK_VERSION 9999999999999

enum {
    UpdateOkay = 0,
    UpdateError = -1,
    UpdateVersionSame = -2,
    UpdateVersionError = -3,
};

static int strcmp_l2(const char *s1, const char *s2)
{
    if (!s1 || !s2)
        return -1;
    return strncmp(s1, s2, strlen(s2));
}

static int show_color_block(u32 color, int pos_x, int pos_y, int w, int h)
{
    int i, j;
    unsigned char *fbp = NULL;
    int byte_per_pixel;

    if (!fb_addr) {
        printf("framebuffer wasn't initialized\n");
        goto error;
    }

    if (pos_x < 0 || w < 0 ||
            pos_y < 0 || h < 0 ||
            (pos_x + w) > fb_width ||
            (pos_y + h) > fb_height) {
        printf("position is out of range, image upgrade failed\n");
        goto error;
    }

    byte_per_pixel = display_bpp / 8;
    fbp = fb_addr + (fb_width * pos_y + pos_x) * byte_per_pixel;

    for (i = 0; i < h ; i++) {
        for (j = 0; j < w; j++) {
            *(fbp + 0) = color & 0xff;
            *(fbp + 1) = (color >> 8) & 0xff;
            *(fbp + 2) = (color >> 16) & 0xff;
            fbp += byte_per_pixel;
        }
        fbp += (fb_width - w) * byte_per_pixel;
    }

    flush_cache(fb_addr, fb_width * fb_height * byte_per_pixel);

    return 0;
error:
    return -1;
}

static int show_flash_progress(int cur_step, int total_steps, int result)
{
    int x, y, w, h, color;
    const int progress_bar_x = 100;
    const int progress_bar_y = 800;
    const int progress_bar_w = fb_width - 200;
    const int progress_bar_h = 100;

    if (cur_step < 0 || total_steps < 0 || cur_step > total_steps ||
            (cur_step != 0 && total_steps == 0))
        goto error;

    x = progress_bar_x;
    y = progress_bar_y;
    h = progress_bar_h;
    w = !(cur_step | total_steps) ? progress_bar_w :
        progress_bar_w * cur_step / total_steps;
    color = !(cur_step | total_steps) ? color_white :
        result ? color_red : color_green;

    show_color_block(color, x, y, w, h);

    return 0;
error:
    return -1;
}

static int update_ui_init(void)
{
    char *str = NULL;

    fb_addr = (unsigned char *)get_fb_addr();

    str = getenv("fb_width");
    fb_width = str ? simple_strtoul(str, NULL, 10) : 0;

    str = getenv("fb_height");
    fb_height = str ? simple_strtoul(str, NULL, 10) : 0;

    str = getenv("display_bpp");
    display_bpp = str ? simple_strtoul(str, NULL, 10) : 0;
    if (display_bpp != 24) {
        printf("only 24bpp is supported now\n");
        goto error;
    }

    if (!(fb_width && fb_width && display_bpp && fb_addr))
        goto error;

    return 0;
error:
    return -1;
}

int check_file(const char *file) {
    int ret = file_exists("usb", "0", file, FS_TYPE_FAT);
    return ret;
}

#define AO_RTI_PINMUX_REG1 (0xff800000 + (0x006 << 2)) //0xff800018 or 0x06 [bit15:12]=3
#define AO_PWM_MISC_REG_AB (0xff807000 + (0x002 << 2)) //0xff807008
#define AO_PWM_PWM_A (0xff807000 + (0x000 << 2)) //PWM_A_DUTY_CYCLE

static void pled_mux_to_pwm(void)
{
    unsigned int value;
    // GPIOAO_11 mux as PWM_AO_A
    value = readl(AO_RTI_PINMUX_REG1);
    value &= ~(0xf << 12);
    value |= (3 << 12);
    writel(value, AO_RTI_PINMUX_REG1);
}

static void send_pwm(unsigned int pwm_lo, unsigned int pwm_hi)
{
    unsigned int value, div;

    pled_mux_to_pwm();

    value = readl(AO_PWM_MISC_REG_AB);
    // clk div
    value &= ~(0x7f << 8);
    div = 0x24;
    value |= div << 8; //bit 8 is PWM_A_CLK_DIV: Selects the divider (N+1) for PWM_A clock.

    // clk enable
    value |= 1 << 15; //bit 15 is PWM_A_CLK_EN
    writel(value, AO_PWM_MISC_REG_AB);

    // duty cycle
    value = (pwm_hi << 16) | pwm_lo;
    writel(value, AO_PWM_PWM_A);

    // enable PWM_AO_A
    value = readl(AO_PWM_MISC_REG_AB);
    value |= 0x3; //PWM_A_EN(bit 0) and PWM_B_EN(bit 1) both set means PWM_A and PWM_B outputs are configured to generate PWM output.
    writel(value, AO_PWM_MISC_REG_AB);
}

int map_image_to_partition(char *image_name, ZipExtractor *zip_extractor)
{
    if (!strcmp_l2(image_name, "bl2.bin.signed")) {
        zip_extractor->partition = strdup("bootloader");
        zip_extractor->partition_other = strdup("bootloader-boot0");
    } else if (!strcmp_l2(image_name, "dt.img")) {
        zip_extractor->partition = strdup("mbr");
        zip_extractor->partition_other = NULL;
    } else if (!strcmp_l2(image_name, "logo.img")) {
        zip_extractor->partition = strdup("logo");
        zip_extractor->partition_other = NULL;
    }
    // Format of all the other images will be imagename.img i.e system.img vendor.img
    else {
        char *token = strtok(image_name, ".");
        zip_extractor->partition = calloc(0, sizeof(char) * (strlen(token) + 2));
        zip_extractor->partition_other = NULL;
        if (zip_extractor->partition == NULL) {
            printf("%s: no memory\n", __func__);
            return UpdateError;
        }
        if (active_slot == 'a') {
            sprintf(zip_extractor->partition, "%s_b", token);
        } else if (active_slot == 'b') {
            sprintf(zip_extractor->partition, "%s_a", token);
        } else {
            printf("Error: active_slot is not set\n");
            return UpdateError;
        }
    }
    if (zip_extractor->partition == NULL) {
        printf("%s: no memory\n", __func__);
        return UpdateError;
    }
    return 0;
}

// mmc 1:11 is for system_a partition
// mmc 1:12 is for system_b partition
long extract_version_from_system() {
    char *cmd;
    char *version;
    int ret = -1;

    if (active_slot == 'a') {
        cmd = CMD_LOAD_SYSTEM_A_PROP;
        ret = run_command(CMD_LOAD_SYSTEM_A_PROP, 0);
    } else if (active_slot == 'b') {
        cmd = CMD_LOAD_SYSTEM_B_PROP;
        ret = run_command(CMD_LOAD_SYSTEM_B_PROP, 0);
    }

    if (ret) {
        printf("Command: %s failed\n", cmd);
        return UpdateError;
    }

    int filesize = (int)getenv_hex("filesize", 0);
    char *buffer = malloc(filesize + 4);
    if (buffer == NULL) {
        printf("No memory for malloc\n");
        return UpdateError;
    }

    memset(buffer, 0, filesize+4);
    memcpy(buffer, LOAD_ADDR, filesize);
    memset(LOAD_ADDR, 0, filesize);

    char *temp = strtok(buffer, "\n");
    while (temp)
    {
        // ro.build.version.incremental=0020401505412 is what we're looking to parse
        if (!strncmp(VERSION_PROP, temp, strlen(VERSION_PROP))) {
            int version_length = strlen(temp) - strlen(VERSION_PROP);
            version = malloc(sizeof(char) * version_length);
            if (version == NULL) {
                printf("No memory for version\n");
                return UpdateError;
            }
            memcpy(version, temp + strlen(VERSION_PROP), version_length);
            break;
        }
        temp = strtok(NULL, "\n");
    }

    return simple_strtol(version, NULL, 10);
}

int check_version(ZipArchiveHandle zip_handle)
{
    ZipEntry version_entry;
    ZipString version_string;

    version_string.name = strdup(VERSION_PROP_FILENAME);
    version_string.name_length = strlen(version_string.name);

    if (find_entry(zip_handle, version_string, &version_entry) < 0) {
        printf("Failed to find entry in zip: %s\n", VERSION_PROP_FILENAME);
        return UpdateError;
    }

    char *buffer = malloc(sizeof(char) * version_entry.uncompressed_length);
    if (buffer == NULL) {
        printf("No memory for malloc\n");
        return UpdateError;
    }

    if (extract_version(zip_handle, &version_entry, buffer) < 0) {
        printf("Failed to extract version to memory\n");
        return UpdateError;
    }

    long package_version = simple_strtol(buffer, NULL, 10);
    if (package_version == DEALERSHIP_UNBRICK_VERSION) {
        printf("Package version matches DEALERSHIP_UNBRICK_VERSION\n");
        return 0;
    }

    long system_version = extract_version_from_system();

    if (system_version == UpdateError) {
        printf("Failed to extract system version\n");
        return UpdateError;
    }
    printf("System_version: %ld package_version:%ld\n", system_version, package_version);

    if (system_version > package_version) {
        printf("Unable to update with older version number\n");
        return UpdateVersionError;
    } else if (system_version == package_version) {
        printf("System is already running this version\n");
        return UpdateVersionSame;
    }

    return 0;
}

int extract_and_flash_images(loff_t update_file_size)
{
    int ret = -1;
    ZipArchiveHandle zip_handle;

    if (open_archive(&zip_handle, UPDATE_FILE_NAME, update_file_size) != 0) {
        printf("Failed to open zip package: %s\n", UPDATE_FILE_NAME);
        return UpdateError;
    }

    ret = check_version(zip_handle);
    if (ret == UpdateVersionError || ret == UpdateError) {
        printf("Cannot update to version\n");
        return UpdateError;
    } else if (ret == UpdateVersionSame && metadata_recovery == 0) {
        printf("Updated package version is the same as current version\n");
        return UpdateVersionSame;
    }

    ZipArchive *zip_archive = (ZipArchive *)zip_handle;
    int i = 0;

    for (i = 0; i < zip_archive->num_entries; i++) {
        ZipEntry image_entry;
        ZipExtractor zip_extractor;
        ZipString image_to_extract = zip_archive->zip_string_list[i];

        // Skip over version.prop
        if (!strcmp_l2(image_to_extract.name, VERSION_PROP_FILENAME))
            goto update_display_bar;

        printf("Extracting and flashing: %s\n", image_to_extract.name);

        if (find_entry(zip_handle, image_to_extract, &image_entry) < 0) {
            printf("Failed to find entry in zip: %s\n", image_to_extract.name);
            return UpdateError;
        }

        if (map_image_to_partition(image_to_extract.name, &zip_extractor) < 0) {
            printf("Failed to map image to partition\n");
            return UpdateError;
        }

        if (extract_image_to_partition(zip_handle, &image_entry, &zip_extractor) < 0) {
            printf("Extract to partition failed\n");
            return UpdateError;
        }
update_display_bar:
        if (update_ui_success)
            show_flash_progress(i + 1, zip_archive->num_entries, 0);
    }

    return 0;
}

int validate_update_file(loff_t update_file_size)
{
    unsigned int pub_key_len;
    loff_t signature_file_size;
    unsigned char signature[RSA_2048_SIG_LEN] = {0};
    static const unsigned char pub_key[] = USB_UPDATE_UNLOCK_KEY;
    pub_key_len = sizeof(pub_key);

    if (check_file(UPDATE_FILE_SIGNATURE) == 0) {
        printf("Signature file not found\n");
        return UpdateError;
    }

    if (fat_size(UPDATE_FILE_SIGNATURE, &signature_file_size) == UpdateError) {
        printf("Error getting %s filesize\n", UPDATE_FILE_SIGNATURE);
        return UpdateError;
    }

    if (signature_file_size > RSA_2048_SIG_LEN) {
        printf("Error: %s too large\n", UPDATE_FILE_SIGNATURE);
        return UpdateError;
    }

    loff_t read_size = file_fat_read(UPDATE_FILE_SIGNATURE, signature, RSA_2048_SIG_LEN);
    if (read_size != RSA_2048_SIG_LEN) {
        printf("Error: We want to read: %ld but got %ld\n", RSA_2048_SIG_LEN, read_size);
        return UpdateError;
    }

    init_usb_verifier_hash_state();

    usb_verifier_hash_process(UPDATE_FILE_LOAD_ADDR, (unsigned long) update_file_size);

    if (finalize_usb_verifier_hash_state(signature, RSA_2048_SIG_LEN, pub_key, pub_key_len) < 0) {
        printf("Verification on file: %s with signature: %s failed\n", UPDATE_FILE_NAME,
                                                                       UPDATE_FILE_SIGNATURE);
        return UpdateError;
    }

    return UpdateOkay;
}

int do_uboot_update(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
    int ret;
    loff_t update_file_size;
    long fd = -1;

    //usb start to ensure usb host inserted and inited
    if (usb_init() >= 0) {
#ifdef CONFIG_USB_STORAGE
        /* try to recognize storage devices immediately */
        if (usb_stor_scan(1) == -1) {
            printf("No USB storage device found\n");
            return 0;
        }
#else
        printf("USB storage not configured\n");
        return 0;
#endif
    } else {
        printf("Failed to initialize USB controller\n");
        return 0;
    }

    if (check_file(UPDATE_FILE_NAME) == 0) {
        printf("Update file not found\n");
        return 0;
    }

    send_pwm(5405, 5405);
    printf("Sent 60Hz at duty 50%% indicating start of USB update\n");

    // If active_slot is properly set
    if (active_slot == 'a' || active_slot == 'b') {
        if (update_ui_init() < 0) {
            printf("Image flashing GUI init failure\n");
            goto error;
        }
        show_flash_progress(0, 0, 0);
        update_ui_success = 1;
        printf("Image flashing GUI init done\n");
    } else {
        printf("Active_slot is not set, resetting misc\n");
        metadata_recovery = 1;
        ret = run_command(CMD_RESET_AB_METADATA, 0);
        ret = run_command("get_valid_slot", 0);
        if (ret) {
            printf("Error getting active_slot\n");
            goto error;
        }
    }

    optimus_device_probe("usb", "0");

    fd = do_fat_fopen(UPDATE_FILE_NAME);
    if (fd < 0) {
        printf("We failed to open\n");
        goto error;
    }

    if (fat_size(UPDATE_FILE_NAME, &update_file_size) == UpdateError) {
        printf("Error getting %s filesize\n", UPDATE_FILE_NAME);
        goto error;
    }

    if (update_file_size > UPDATE_FILE_SIZE_MAX) {
        printf("Error: %s too large\n", UPDATE_FILE_NAME);
        goto error;
    }

    long read_size = call_fat_fread(fd, (void *) UPDATE_FILE_LOAD_ADDR, update_file_size);
    if (read_size != update_file_size) {
        printf("Error: We want to read: %ld but got %ld\n", update_file_size, read_size);
        goto error;
    }

    flush_cache(UPDATE_FILE_LOAD_ADDR, update_file_size);
    mdelay(10);

    if (validate_update_file(update_file_size) == UpdateError) {
        printf("Signature Verification failed\n");
        goto error;
    }
    // Everything is in memory at this point, so close the opened fd to the USB
    do_fat_fclose(fd);

    ret = extract_and_flash_images(update_file_size);
    if (ret == UpdateError) {
        printf("Failure while extracting and flashing images\n");
        goto error;
    } else if (ret == UpdateVersionSame) {
        printf("Target update version is the same\n");
        if (update_ui_success)
            show_flash_progress(1, 1, 0);
    } else {
        if (active_slot == 'a') {
            ret = run_command(CMD_SET_ACTIVE_SLOT_B, 0);
            ret += run_command(CMD_MARK_SLOT_SUCCESSFUL_B, 0);
        } else if (active_slot == 'b') {
            ret = run_command(CMD_SET_ACTIVE_SLOT_A, 0);
            ret += run_command(CMD_MARK_SLOT_SUCCESSFUL_A, 0);
        } else {
            printf("Error: active_slot is not set\n");
            goto error;
        }

        if (ret) {
            printf("Changing and setting active_slot failed\n");
            goto error;
        }
    }

    if (usb_stor_scan(1) == -1) {
        printf("USB got unplugged sometime during the update\n");
        goto error;
    }

    if (run_command(CMD_USB_STOP, 0)) {
        printf("USB stop failed\n");
    }

    // Always show Update Bar fully completed at end
    if (update_ui_success)
        show_flash_progress(1, 1, 0);

    // Send end of update pwm 80Hz
    send_pwm(4054, 4054);
    printf("Sent 80Hz at duty 50%% indicating end of USB update\n");

    while(1)
        mdelay(1000);

    return 0;
error:
    if (run_command(CMD_USB_STOP, 0)) {
        printf("USB stop failed\n");
    }

    // Send end of error PWM 100Hz
    send_pwm(3243, 3243);
    printf("Sent 100Hz at duty 50%% indicating end of USB update\n");
    if (update_ui_success)
        show_flash_progress(1, 1, 1);

    while(1)
        mdelay(1000);
    return 0;
}
#else
int do_uboot_update (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]) {
    return 0;
}

#endif

U_BOOT_CMD(
	uboot_update, 2, 0,	do_uboot_update,
	"run uboot update from usb device",
	"[addr] - flash script starting at addr\n"
);

