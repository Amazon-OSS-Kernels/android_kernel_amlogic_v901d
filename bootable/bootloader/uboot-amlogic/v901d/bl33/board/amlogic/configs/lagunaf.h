/*
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef __BOARD_H__
#define __BOARD_H__

#include <asm/arch/cpu.h>

#define CONFIG_SYS_GENERIC_BOARD  1
#ifndef CONFIG_AML_MESON
#warning "include warning"
#endif


//#define DEBUG 1
#define CONFIG_SYS_VSNPRINTF 1
#define CONFIG_CMD_WATCHDOG 1

/*
 * platform power init config
 */
#define CONFIG_PLATFORM_POWER_INIT
#define CONFIG_VCCK_INIT_VOLTAGE	870		// VCCK power up voltage
#define CONFIG_VDDEE_INIT_VOLTAGE	840		// VDDEE power up voltage
#define CONFIG_VDDEE_SLEEP_VOLTAGE	770		// VDDEE suspend voltage

/* configs for CEC */
#define CONFIG_CEC_OSD_NAME		"AML_TV"
#define CONFIG_CEC_WAKEUP
/*if use bt-wakeup,open it*/
#define CONFIG_BT_WAKEUP
/*if use wifi-wakeup,open it*/
#define CONFIG_WIFI_WAKEUP
/* SMP Definitinos */
#define CPU_RELEASE_ADDR		secondary_boot_func

/* support uboot usb update*/
#define CONFIG_UBOOT_USB_UPDATE

/* support U-disk triggered standby for ABC*/
#define UBOOT_TARGET_PRODUCT_NAME_ABC

/* config saradc*/
#define CONFIG_CMD_SARADC 1
#define CONFIG_SARADC_CH  2

/*dsp boot*/
#define CONFIG_CMD_STARTDSP 1
#define CONFIG_CMD_DSPJTAGRESET 1

#define CONFIG_CMD_CEC      1


/* Bootloader Control Block function
   That is used for recovery and the bootloader to talk to each other
  */
#define CONFIG_BOOTLOADER_CONTROL_BLOCK

/*a/b update */
#define CONFIG_CMD_BOOTCTOL_AVB

/* Serial config */
#define CONFIG_CONS_INDEX 2
#define CONFIG_BAUDRATE  115200
#define CONFIG_AML_MESON_SERIAL   1
#define CONFIG_SERIAL_MULTI		1
//--------------please add amzn  configuration below
//add for idme
#define CONFIG_OF_BOARD_SETUP 1
#define CONFIG_IDME 1
#define CONFIG_HARDWARE_ID 1

 /* define IDME dev_flags bits used in uboot here */
#define DEV_FLAGS_USB_NUM_VAL			2
#define DEV_FLAGS_BYPASS_SECONDARY_BOOT		4
#define DEV_FLAGS_SELINUX_FORCE_ENFORCING	32
#define DEV_FLAGS_SELINUX_FORCE_PERMISSIVE	64

#define DEV_FLAGS_USB_DEVICE	4096

#define REF_BOARD_ID "00980000000A0019"
#define HVT_BOARD_ID "4EC00001000B0020"
#define EVT_BOARD_ID "4EC00002000B0020"
#define DVT_BOARD_ID "4EC00003000B0020"
#define PVT_BOARD_ID "4EC00014000B0020"

#define REF_BOARD_ID_TYPE 0
#define HVT_BOARD_ID_TYPE 1
#define EVT_BOARD_ID_TYPE 2
#define DVT_BOARD_ID_TYPE 3
#define PVT_BOARD_ID_TYPE 4

/* define IDME usr_flags bits used in uboot here */
#define USR_FLAGS_STOREDEMO_MODE		4
/* define IDME dev_flags bits used in uboot here */
#define DEV_FLAGS_BYPASS_SECONDARY_BOOT		4

//#define CONFIG_IRBLASTER_FACTORY_TEST 1
#define DTB_BIND_KERNEL
#define CONFIG_PTBL_MBR                1
#define CONFIG_USE_BOOTIMAGE_DTB


/* enableparam store in logo last 64k area*/
// #define CONFIG_LOGOPARAM_ENABLE

//Enable ir remote wake up for bl30
#define CONFIG_IR_REMOTE_POWER_UP_KEY_VAL1 0xef10fe01 //amlogic tv ir --- power
#define CONFIG_IR_REMOTE_POWER_UP_KEY_VAL2 0XBB44FB04 //amlogic tv ir --- ch+
#define CONFIG_IR_REMOTE_POWER_UP_KEY_VAL3 0xF20DFE01 //amlogic tv ir --- ch-
#define CONFIG_IR_REMOTE_POWER_UP_KEY_VAL4 0XBA45BD02 //amlogic small ir--- power
#define CONFIG_IR_REMOTE_POWER_UP_KEY_VAL5 0xe51afb04
/*https://www.amazon.com*/

#define CONFIG_IR_REMOTE_POWER_UP_KEY_VAL6	0xb9467d02 /* ABC power key */
#define CONFIG_IR_REMOTE_POWER_UP_KEY_VAL7	0xa05f7d02 /* ABC netflix key */
#define CONFIG_IR_REMOTE_POWER_UP_KEY_VAL8	0x5ea17d02 /* ABC prime video key */
#define CONFIG_IR_REMOTE_POWER_UP_KEY_VAL9	0x5da27d02 /* ABC music key */
#define CONFIG_IR_REMOTE_POWER_UP_KEY_VAL10	0x5ca37d02 /* ABC custom button4 key */
#define CONFIG_IR_REMOTE_POWER_UP_KEY_VAL11	0x609f7d02 /* ABC home key */
#define CONFIG_IR_REMOTE_POWER_UP_KEY_VAL12	0xb54a7d02 /* ABC enter key */
#define CONFIG_IR_REMOTE_POWER_UP_KEY_VAL13	0x5fa07d02 /* ABC voice search key */

/*config the default parameters for adc power key*/
#define CONFIG_ADC_POWER_KEY_CHAN   2  /*channel range: 0-7*/
#define CONFIG_ADC_POWER_KEY_VAL    0  /*sample value range: 0-1023*/
#define CONFIG_AMZN_FDT_FIXUP			1
/* args/envs */
#define CONFIG_SYS_MAXARGS  64
#define CONFIG_EXTRA_ENV_SETTINGS \
        "firstboot=1\0"\
        "upgrade_step=0\0"\
        "jtag=disable\0"\
        "loadaddr=1080000\0"\
        "model_name=FHD\0" \
        "panel_type=lvds_1\0" \
        "lcd_ctrl=0x00000000\0" \
        "lcd_ctrl_quiescent=0x00000000\0" \
        "outputmode=panel\0" \
        "hdmimode=1080p60hz\0" \
        "cvbsmode=576cvbs\0" \
        "display_width=1920\0" \
        "display_height=1200\0" \
        "display_bpp=24\0" \
        "display_color_index=24\0" \
        "display_layer=osd0\0" \
        "display_color_fg=0xffff\0" \
        "display_color_bg=0\0" \
        "dtb_mem_addr=0x1000000\0" \
        "fb_addr=0x3d800000\0" \
        "fb_width=1920\0" \
        "fb_height=1200\0" \
        "frac_rate_policy=1\0" \
        "fastboot_burning=fastboot\0" \
        "recovery_otg_device=none\0" \
        "fdt_high=0x20000000\0"\
        "try_auto_burn=update 700 750;\0"\
        "sdcburncfg=aml_sdc_burn.ini\0"\
        "sdc_burning=sdc_burn ${sdcburncfg}\0"\
        "wipe_data=successful\0"\
        "wipe_cache=successful\0"\
        "EnableSelinux=enforcing\0" \
        "recovery_part=recovery\0"\
        "recovery_offset=0\0"\
        "cvbs_drv=0\0"\
        "lock=10001000\0"\
        "osd_reverse=0\0"\
        "video_reverse=0\0"\
        "active_slot=normal\0"\
        "boot_part=boot\0"\
        "transition_done=0\0"\
        "logo_name=bootup\0"\
        "bypass_standby=0\0"\
        "powermode=standby\0"\
        "Irq_check_en=0\0"\
        "hwid=0\0"\
        "bl_level=100\0"\
        "bl_off=none\0"\
        "bl_status=1\0"\
        "hw_version=EVT\0"\
        "edid_14_dir=/tvconfig/hdmi/port_14.bin\0" \
        "edid_20_dir=/tvconfig/hdmi/port_20.bin\0" \
        "edid_select=0\0" \
        "port_map=0x4321\0" \
        "cec_fun=0x2F\0" \
        "logic_addr=0x0\0" \
        "cec_ac_wakeup=1\0" \
        "rpmb_state=0\0" \
        "cpu_version=rev_a\0" \
        "Dolby_enabled=no\0" \
        "DTS_enabled=no\0" \
        "UbootBuildTag=none\0" \
        "bl2_version=none\0" \
        "fs_type=""rootfstype=ramfs""\0"\
        "cec_init="\
            "echo cec_init ac_wakeup=${cec_ac_wakeup}; "\
            "if test ${cec_ac_wakeup} = 1; then "\
                "cec ${logic_addr} ${cec_fun}; "\
                "if test ${edid_select} = 1111; then "\
                    "hdmirx init ${port_map} ${edid_20_dir}; "\
                "else "\
                    "hdmirx init ${port_map} ${edid_14_dir}; "\
                "fi;"\
            "fi;"\
            "\0"\
        "initargs="\
            "init=/init console=ttyS0,115200 no_console_suspend earlycon=aml-uart,0xff803000 printk.devkmsg=on ramoops.pstore_en=1 ramoops.record_size=0x8000 ramoops.console_size=0x4000 "\
            "\0"\
        "upgrade_check="\
            "echo upgrade_step=${upgrade_step}; "\
            "if itest ${upgrade_step} == 3; then "\
                "run init_display; run storeargs;watchdog off;run update;"\
            "else fi;"\
            "\0"\
        "storeargs="\
            "setenv bootargs ${initargs} ${fs_type} logo=${display_layer},loaded,${fb_addr} powermode=${powermode} vout=${outputmode},enable panel_type=${panel_type} lcd_ctrl=${lcd_ctrl} hdmimode=${hdmimode} cvbsmode=${cvbsmode} osd_reverse=${osd_reverse} video_reverse=${video_reverse} androidboot.selinux=${EnableSelinux} androidboot.firstboot=${firstboot} androidboot.rpmb_state=${rpmb_state} jtag=${jtag} fb_width=${fb_width} fb_height=${fb_height} ; "\
            "setenv bootargs ${bootargs} bl_status=${bl_status}; "\
            "setenv bootargs ${bootargs} androidboot.serialno=123456789ABCDEFG androidboot.hardware=amlogic;"\
            "setenv bootargs ${bootargs} androidboot.Dolby_enabled=${Dolby_enabled}; "\
            "setenv bootargs ${bootargs} androidboot.Dts_enabled=${DTS_enabled}; "\
            "setenv bootargs ${bootargs} androidboot.cpu_version=${cpu_version}; "\
            "setenv bootargs ${bootargs} androidboot.hwid=${hwid}; "\
            "setenv bootargs ${bootargs} androidboot.dts=${aml_dt}; "\
            "setenv bootargs ${bootargs} androidboot.hw_version=${hw_version}; "\
            "setenv bootargs ${bootargs} androidboot.UbootBuildTag=${UbootBuildTag};"\
            "setenv bootargs ${bootargs} androidboot.bl2_version=${bl2_version};"\
            "if test ${reboot_mode_android} = quiescent; then "\
                "setenv bootargs ${bootargs} androidboot.quiescent=1;" \
            "fi;"\
   /*         "run cmdline_keys;"\ */\
            "\0"\
        "switch_bootmode="\
            "get_rebootmode;"\
            "if test ${reboot_mode} = factory_reset; then "\
                    "setenv reboot_mode_android ""normal"";"\
                    "run storeargs;"\
                    "run recovery_from_flash;"\
            "else if test ${reboot_mode} = update; then "\
                    "setenv reboot_mode_android ""normal"";"\
                    "run storeargs;"\
                    /*"run update;"*/ \
            "else if test ${reboot_mode} = quiescent; then "\
                    "setenv lcd_ctrl ${lcd_ctrl_quiescent};" \
                    "setenv reboot_mode_android ""quiescent"";"\
                    "run storeargs;"\
            "else if test ${reboot_mode} = recovery_quiescent; then "\
                    "setenv lcd_ctrl ${lcd_ctrl_quiescent};" \
                    "setenv reboot_mode_android ""quiescent"";"\
                    "run storeargs;"\
                    "run recovery_from_flash;"\
            "else if test ${reboot_mode} = watchdog_reboot; then "\
                    "if test ${bl_status} = 0; then "\
                        "setenv lcd_ctrl ${lcd_ctrl_quiescent};" \
                        "setenv reboot_mode_android ""quiescent"";"\
                        "run storeargs;"\
                    "fi; "\
            "else if test ${reboot_mode} = kernel_panic; then "\
                    "if test ${bl_status} = 0; then " \
                        "setenv lcd_ctrl ${lcd_ctrl_quiescent};" \
                        "setenv reboot_mode_android ""quiescent"";"\
                        "run storeargs;"\
                    "fi; "\
            "else if test ${reboot_mode} = crash_dump; then "\
                    "if test ${bl_status} = 0; then "\
                        "setenv lcd_ctrl ${lcd_ctrl_quiescent};" \
                        "setenv reboot_mode_android ""quiescent"";"\
                        "run storeargs;"\
                    "fi; "\
            "else if test ${reboot_mode} = cold_boot; then "\
                    "setenv reboot_mode_android ""normal"";"\
                    "run storeargs;"\
            "else if test ${reboot_mode} = fastboot; then "\
                "setenv reboot_mode_android ""normal"";"\
                "run storeargs;"\
                "echo Disable watchdog; "\
                "watchdog off;" \
                "fastboot;"\
            "fi;fi;fi;fi;fi;fi;fi;fi;fi;"\
            "\0" \
        "storeboot="\
            "boot_cooling;"\
            "get_system_as_root_mode;"\
            "echo system_mode: ${system_mode};"\
            "if test ${system_mode} = 1; then "\
                    "setenv fs_type ""ro rootwait skip_initramfs"";"\
                    "run storeargs;"\
            "fi;"\
            "decrement_retry_count;"\
            "get_valid_slot;"\
            "get_avb_mode;"\
            "echo active_slot: ${active_slot} avb2: ${avb2};"\
            "if test ${active_slot} != normal; then "\
                    "setenv bootargs ${bootargs} androidboot.slot_suffix=${active_slot};"\
            "fi;"\
/*            "if test ${avb2} = 0; then "\
                "if test ${active_slot} = _a; then "\
                    "setenv bootargs ${bootargs} root=/dev/mmcblk0p16;"\
                "else if test ${active_slot} = _b; then "\
                    "setenv bootargs ${bootargs} root=/dev/mmcblk0p17;"\
                "fi;fi;"\
            "fi;"\ */ \
            "echo Disable watchdog; "\
            "watchdog off;" \
            "if imgread kernel ${boot_part} ${loadaddr}; then bootm ${loadaddr}; fi;"\
            "fastboot;"\
            "\0"\
        "factory_reset_poweroff_protect="\
            "echo wipe_data=${wipe_data}; echo wipe_cache=${wipe_cache};"\
            "if test ${wipe_data} = failed; then "\
                "run init_display; run storeargs;"\
                "if mmcinfo; then "\
                    "run recovery_from_sdcard;"\
                "fi;"\
                "if usb start 0; then "\
                    "run recovery_from_udisk;"\
                "fi;"\
                "run recovery_from_flash;"\
            "fi; "\
            "if test ${wipe_cache} = failed; then "\
                "run init_display; run storeargs;"\
                "if mmcinfo; then "\
                    "run recovery_from_sdcard;"\
                "fi;"\
                "if usb start 0; then "\
                    "run recovery_from_udisk;"\
                "fi;"\
                "run recovery_from_flash;"\
            "fi; \0" \
        "update="\
            /*first usb burning, second sdc_burn, third ext-sd autoscr/recovery, last udisk autoscr/recovery*/\
            "run fastboot_burning; "\
            "run recovery_from_flash;"\
            "\0"\
        "recovery_from_sdcard="\
            "if fatload mmc 0 ${loadaddr} aml_autoscript; then autoscr ${loadaddr}; fi;"\
            "if fatload mmc 0 ${loadaddr} recovery.img; then "\
                    "if fatload mmc 0 ${dtb_mem_addr} dtb.img; then echo sd dtb.img loaded; fi;"\
                    "wipeisb; "\
                    "setenv recovery_otg_device host;" \
                    "echo Disable watchdog; "\
                    "watchdog off;" \
                    "bootm ${loadaddr};fi;"\
            "\0"\
        "recovery_from_udisk="\
            "if fatload usb 0 ${loadaddr} aml_autoscript; then autoscr ${loadaddr}; fi;"\
            "if fatload usb 0 ${loadaddr} recovery.img; then "\
                "if fatload usb 0 ${dtb_mem_addr} dtb.img; then echo udisk dtb.img loaded; fi;"\
                "wipeisb; "\
                "setenv recovery_otg_device host;" \
                "echo Disable watchdog; "\
                "watchdog off;" \
                "bootm ${loadaddr};fi;"\
            "\0"\
        "recovery_from_flash="\
            "get_valid_slot;"\
            "echo Disable usb scanning;"\
            "echo active_slot: ${active_slot};"\
            "if test ${active_slot} = normal; then "\
                "setenv bootargs ${bootargs} aml_dt=${aml_dt} recovery_part={recovery_part} recovery_offset={recovery_offset};"\
                "if itest ${upgrade_step} == 3; then "\
                    "if ext4load mmc 1:2 ${dtb_mem_addr} /recovery/dtb.img; then echo cache dtb.img loaded; fi;"\
                    "if ext4load mmc 1:2 ${loadaddr} /recovery/recovery.img; then echo cache recovery.img loaded; wipeisb; setenv recovery_otg_device host; bootm ${loadaddr}; fi;"\
                "else fi;"\
                "echo Disable watchdog; "\
                "watchdog off;" \
                "if imgread kernel ${recovery_part} ${loadaddr} ${recovery_offset}; then wipeisb; setenv recovery_otg_device host; bootm ${loadaddr}; fi;"\
            "else "\
                "setenv bootargs ${bootargs} aml_dt=${aml_dt} recovery_part=${boot_part} recovery_offset=${recovery_offset};"\
                 "echo Disable watchdog; "\
                 "watchdog off;" \
                "if imgread kernel ${boot_part} ${loadaddr}; then setenv recovery_otg_device host; bootm ${loadaddr}; fi;"\
            "fi;"\
            "\0"\
/*        "init_display="\
            "osd open;osd clear;logo_display $logo_name;bmp scale;vout output ${outputmode}"\
            "\0"\ */\
        "OSD_QUIESCENT_MODE=" \
            "setenv bl_off once;"\
            "osd open;osd clear;logo_display $logo_name;bmp scale;vout output ${outputmode};"\
            "setenv bl_off none;"\
            "setenv reboot_mode_android ""quiescent"";"\
            "setenv lcd_ctrl ${lcd_ctrl_quiescent};" \
            "run storeargs;"\
            "lcd bl off;"\
            "\0"\
        "OSD_NORMAL_MODE=" \
            "osd open;osd clear;logo_display $logo_name;bmp scale;vout output ${outputmode};"\
            "setenv reboot_mode_android ""normal"";"\
            "run storeargs;"\
            "setenv bootargs ${bootargs} ;"\
            "\0"\
        "init_display="\
            "get_rebootmode;"\
            "echo reboot_mode :::: ${reboot_mode};"\
            "if test ${reboot_mode} = quiescent; then "\
                    "run OSD_QUIESCENT_MODE;"\
            "else if test ${reboot_mode} = recovery_quiescent; then "\
                    "run OSD_QUIESCENT_MODE;"\
            "else if test ${reboot_mode} = watchdog_reboot; then "\
                    "if test ${bl_status} = 0; then "\
                        "run OSD_QUIESCENT_MODE;"\
                    "else "\
                        "run OSD_NORMAL_MODE;"\
                    "fi;"\
            "else if test ${reboot_mode} = kernel_panic; then "\
                    "if test ${bl_status} = 0; then "\
                        "run OSD_QUIESCENT_MODE;"\
                    "else "\
                        "run OSD_NORMAL_MODE;"\
                    "fi;"\
           "else if test ${reboot_mode} = crash_dump; then "\
                    "if test ${bl_status} = 0; then "\
                        "run OSD_QUIESCENT_MODE;"\
                    "else "\
                        "run OSD_NORMAL_MODE;"\
                    "fi;"\
            "else "\
                "run OSD_NORMAL_MODE;"\
            "fi;fi;fi;fi;fi"\
            "\0"\
 /*       "cmdline_keys="\
            "if keyman init 0x1234; then "\
                "if keyman read usid ${loadaddr} str; then "\
                    "setenv bootargs ${bootargs} androidboot.serialno=${usid};"\
                    "setenv serial ${usid};"\
                "else "\
                    "setenv bootargs ${bootargs} androidboot.serialno=1234567890;"\
                    "setenv serial 1234567890;"\
                "fi;"\
                "if keyman read mac ${loadaddr} str; then "\
                    "setenv bootargs ${bootargs} mac=${mac} androidboot.mac=${mac};"\
                "fi;"\
                "if keyman read deviceid ${loadaddr} str; then "\
                    "setenv bootargs ${bootargs} androidboot.deviceid=${deviceid};"\
                "fi;"\
            "fi;"\
            "\0"\  */ \
        "check_display="\
            "get_rebootmode;"\
            "if test ${reboot_mode} = cold_boot; then "\
                "if itest ${bypass_standby} != 1; then "\
                    "echo not init_display; "\
                "else "\
                    "run init_display; "\
                "fi; "\
            "else "\
                "run init_display; "\
            "fi; "\
            "\0"\
        "bcb_cmd="\
            "get_rebootmode;"\
            "get_valid_slot;"\
            "\0"\
        "upgrade_key="\
            "if gpio input GPIOAO_3; then "\
                "echo detect upgrade key; run update;"\
            "fi;"\
            "\0"\
        "system_factory_off="\
            "get_rebootmode; "\
            "if test ${reboot_mode} = cold_boot; then "\
                "run adckey_update;"\
                "echo reboot_mode=${reboot_mode}; "\
                "if test ${reboot_mode} = cold_boot && itest ${bypass_standby} != 1; then "\
                    "run cec_init;"\
                    "setMtkBT; "\
                    "lcd disable; "\
                    "lcd bl off; "\
                    "echo Disable watchdog; "\
                     "watchdog off;" \
                    "systemoff; "\
                "fi;"\
            "fi;"\
            "\0"\
        "irremote_update="\
            "if irkey 2500000 0xe31cfb04 0xb748fb04; then "\
                "echo read irkey ok!; " \
                "if itest ${irkey_value} == 0xe31cfb04; then " \
                    /*"run update;"*/\
                "else if itest ${irkey_value} == 0xb748fb04; then " \
                    /*"run update;\n"*/ \
                "fi;fi;" \
            "fi;\0" \
        "adckey_update="\
            "if saradc open 2; then "\
                "if saradc getval; then "\
                    "if itest ${saradc_val} >= 0; then "\
                        "if itest ${saradc_val} <= 40; then "\
                            "run init_display;"\
                            "run storeargs;"\
                            "echo Disable watchdog; "\
                            "watchdog off;" \
                            "run recovery_from_flash;"\
                        "fi;fi;fi;"\
            "fi;\0" \
        "upgrade_usb="\
            "echo Disable watchdog; "\
            "watchdog off;" \
            "uboot_update;"\
            "\0"\

#ifdef CONFIG_PXP_EMULATOR
#define CONFIG_PREBOOT "echo preboot for pxp"
#define CONFIG_BOOTCOMMAND "echo bootcmd for pxp"
#else
#define CONFIG_PREBOOT  \
	"mw ff638630 0 2;"\
	"run bcb_cmd; "\
	"run factory_reset_poweroff_protect;"\
	"run upgrade_check;"\
	"run check_display;"\
	"run storeargs;"\
	"bcb uboot-command;"\
	"run system_factory_off;"\
	"run upgrade_usb;"\
	"run switch_bootmode;"
#endif // #ifdef CONFIG_PXP_EMULATOR

#define CONFIG_BOOTCOMMAND "run storeboot"

//#define CONFIG_ENV_IS_NOWHERE  1
#define CONFIG_NO_ENV_PART   1
#define CONFIG_ENV_SIZE   (64*1024)
#define CONFIG_FIT 1
#define CONFIG_OF_LIBFDT 1
#define CONFIG_ANDROID_BOOT_IMAGE 1
#define CONFIG_ANDROID_IMG 1
#define CONFIG_SYS_BOOTM_LEN (64<<20) /* Increase max gunzip size*/

/* cpu */
#define CONFIG_CPU_CLK					1608 //MHz. Range: 360-2000, should be multiple of 24

/* ATTENTION */
/* DDR configs move to board/amlogic/[board]/firmware/timing.c */

#define CONFIG_NR_DRAM_BANKS			1
/* ddr functions */
#define CONFIG_DDR_FULL_TEST			0 //0:disable, 1:enable. ddr full test
#define CONFIG_CMD_DDR_D2PLL			0 //0:disable, 1:enable. d2pll cmd
#define CONFIG_CMD_DDR_TEST				0 //0:disable, 1:enable. ddrtest cmd
#define CONFIG_CMD_DDR_TEST_G12			1 //0:disable, 1:enable. G12 ddrtest cmd
#define CONFIG_DDR_LOW_POWER			0 //0:disable, 1:enable. ddr clk gate for lp
#define CONFIG_DDR_ZQ_PD				0 //0:disable, 1:enable. ddr zq power down
#define CONFIG_DDR_USE_EXT_VREF			0 //0:disable, 1:enable. ddr use external vref
#define CONFIG_DDR4_TIMING_TEST			0 //0:disable, 1:enable. ddr4 timing test function
#define CONFIG_DDR_PLL_BYPASS			0 //0:disable, 1:enable. ddr pll bypass function
#define CONFIG_DDR_NONSEC_SCRAMBLE		0 //0:disable, 1:enable. non-sec region scramble function
//#define  CONFIG_SILENT_CONSOLE                  1
//#define  CONFIG_UBOOT_LOGGER                    1
//#define  DEBUG                                   1
/* storage: emmc/nand/sd */
#define		CONFIG_STORE_COMPATIBLE 1
#define 	CONFIG_ENV_OVERWRITE
#define 	CONFIG_CMD_SAVEENV
/* fixme, need fix*/

#if (defined(CONFIG_ENV_IS_IN_AMLNAND) || defined(CONFIG_ENV_IS_IN_MMC)) && defined(CONFIG_STORE_COMPATIBLE)
#error env in amlnand/mmc already be compatible;
#endif

/*
*				storage
*		|---------|---------|
*		|					|
*		emmc<--Compatible-->nand
*					|-------|-------|
*					|				|
*					MTD<-Exclusive->NFTL
*/
/* axg only support slc nand */
/* swither for mtd nand which is for slc only. */
/* support for mtd */
//#define CONFIG_AML_MTD 1
/* support for nftl */
//#define CONFIG_AML_NAND	1

#if defined(CONFIG_AML_NAND) && defined(CONFIG_AML_MTD)
#error CONFIG_AML_NAND/CONFIG_AML_MTD can not support at the sametime;
#endif

#ifdef CONFIG_AML_MTD

/* bootlaoder is construct by bl2 and fip
 * when DISCRETE_BOOTLOADER is enabled, bl2 & fip
 * will not be stored continuously, and nand layout
 * would be bl2|rsv|fip|normal, but not
 * bl2|fip|rsv|noraml anymore
 */
#define CONFIG_DISCRETE_BOOTLOADER

#ifdef  CONFIG_DISCRETE_BOOTLOADER
#define CONFIG_TPL_SIZE_PER_COPY          0x200000
#define CONFIG_TPL_COPY_NUM               4
#define CONFIG_TPL_PART_NAME              "tpl"
/* for bl2, restricted by romboot */
#define CONFIG_BL2_COPY_NUM               8
#endif /* CONFIG_DISCRETE_BOOTLOADER */

#define CONFIG_CMD_NAND 1
#define CONFIG_MTD_DEVICE y
/* mtd parts of ourown.*/
#define CONFIFG_AML_MTDPART	1
/* mtd parts by env default way.*/
/*
#define MTDIDS_NAME_STR		"aml_nand.0"
#define MTDIDS_DEFAULT		"nand1=" MTDIDS_NAME_STR
#define MTDPARTS_DEFAULT	"mtdparts=" MTDIDS_NAME_STR ":" \
					"3M@8192K(logo),"	\
					"10M(recovery),"	\
					"8M(kernel),"	\
					"40M(rootfs),"	\
					"-(data)"
*/
#define CONFIG_CMD_UBI
#define CONFIG_CMD_UBIFS
#define CONFIG_RBTREE
#define CONFIG_CMD_NAND_TORTURE 1
#define CONFIG_CMD_MTDPARTS   1
#define CONFIG_MTD_PARTITIONS 1
#define CONFIG_SYS_MAX_NAND_DEVICE  2
#define CONFIG_SYS_NAND_BASE_LIST   {0}
#endif
/* endof CONFIG_AML_MTD */
#define		CONFIG_AML_SD_EMMC 1
#ifdef		CONFIG_AML_SD_EMMC
	#define 	CONFIG_GENERIC_MMC 1
	#define 	CONFIG_CMD_MMC 1
	#define CONFIG_CMD_GPT 1
	#define	CONFIG_SYS_MMC_ENV_DEV 1
	#define CONFIG_EMMC_DDR52_EN 0
	#define CONFIG_EMMC_DDR52_CLK 35000000
    /* !! For tm2 revA ONLY !!*/
    #define CONFIG_EMMC_KEEP_BOOT1 1
#endif
#define		CONFIG_PARTITIONS 1
#define 	CONFIG_SYS_NO_FLASH  1

/* meson SPI */
#define CONFIG_AML_SPIFC
#define CONFIG_AML_SPICC
#if defined CONFIG_AML_SPIFC || defined CONFIG_AML_SPICC
	#define CONFIG_OF_SPI
	#define CONFIG_DM_SPI
	#define CONFIG_CMD_SPI
#endif
/* SPI flash config */
#ifdef CONFIG_AML_SPIFC
	#define CONFIG_SPI_FLASH
	#define CONFIG_DM_SPI_FLASH
	#define CONFIG_CMD_SF
	/* SPI flash surpport list */
	#define CONFIG_SPI_FLASH_ATMEL
	#define CONFIG_SPI_FLASH_EON
	#define CONFIG_SPI_FLASH_GIGADEVICE
	#define CONFIG_SPI_FLASH_MACRONIX
	#define CONFIG_SPI_FLASH_SPANSION
	#define CONFIG_SPI_FLASH_STMICRO
	#define CONFIG_SPI_FLASH_SST
	#define CONFIG_SPI_FLASH_WINBOND
	#define CONFIG_SPI_FRAM_RAMTRON
	#define CONFIG_SPI_M95XXX
	#define CONFIG_SPI_FLASH_ESMT
	/* SPI nand flash support */
	#define CONFIG_SPI_NAND
	#define CONFIG_BL2_SIZE (64 * 1024)
#endif

#if defined CONFIG_AML_MTD || defined CONFIG_SPI_NAND
	#define CONFIG_CMD_NAND 1
	#define CONFIG_MTD_DEVICE y
	#define CONFIG_RBTREE
	#define CONFIG_CMD_NAND_TORTURE 1
	#define CONFIG_CMD_MTDPARTS   1
	#define CONFIG_MTD_PARTITIONS 1
	#define CONFIG_SYS_MAX_NAND_DEVICE  2
	#define CONFIG_SYS_NAND_BASE_LIST   {0}
#endif

/* vpu */
#define CONFIG_AML_VPU 1
//#define CONFIG_VPU_CLK_LEVEL_DFT 7

/* DISPLAY & HDMITX */
//#define CONFIG_AML_HDMITX20 1
#define CONFIG_AML_CANVAS 1
#define CONFIG_AML_VOUT 1
#define CONFIG_AML_OSD 1
#define CONFIG_OSD_SCALE_ENABLE 1
#define CONFIG_CMD_BMP 1

#if defined(CONFIG_AML_VOUT)
//#define CONFIG_AML_CVBS 1
#endif

#define CONFIG_AML_LCD    1
#define CONFIG_AML_LCD_TABLET 1
#define CONFIG_AML_LCD_TV 1
//#define CONFIG_AML_LCD_EXTERN 1
#define CONFIG_AML_BL_EXTERN  1
#define CONFIG_AML_SEDES 1
#define CONFIG_AML_BL_EXTERN_I2C_M0516LDN 1
#define CONFIG_AML_BL_EXTERN_I2C_A8552 1

//ABC don't support  local dimming
//#define CONFIG_AML_LOCAL_DIMMING
//#define CONFIG_AML_LOCAL_DIMMING_GLOBAL

/* USB
 * Enable CONFIG_MUSB_HCD for Host functionalities MSC, keyboard
 * Enable CONFIG_MUSB_UDD for Device functionalities.
 */
/* #define CONFIG_MUSB_UDC		1 */
#define CONFIG_CMD_USB 1
#if defined(CONFIG_CMD_USB)
	#define CONFIG_GXL_XHCI_BASE            0xff500000
	#define CONFIG_GXL_USB_PHY2_BASE        0xffe09000
	#define CONFIG_GXL_USB_PHY3_BASE        0xffe09080
	#define CONFIG_USB_PHY_20				0xff636000
	#define CONFIG_USB_PHY_21				0xff63A000
	#define CONFIG_USB_PHY_22				0xff658000
	#define CONFIG_USB_STORAGE      1
	#define CONFIG_USB_XHCI		1
	#define CONFIG_USB_XHCI_AMLOGIC_V2 1
	#define CONFIG_USB_GPIO_PWR  			GPIOAO(GPIOAO_8)
	#define CONFIG_USB_GPIO_PWR_NAME		"GPIOAO_8"
	#define CONFIG_USB_AMLOGIC_PHY_V2		1
	#define CONFIG_USB_U2_PORT_NUM			3
	#define CONFIG_USB_POWER				1
	//#define CONFIG_USB_XHCI_AMLOGIC_USB3_V2		1
#endif //#if defined(CONFIG_CMD_USB)

#define CONFIG_TXLX_USB        1
#define CONFIG_USB_DEVICE_V2    1
#define USB_PHY2_PLL_PARAMETER_1	0x09400414
#define USB_PHY2_PLL_PARAMETER_2	0x927e0000
#define USB_PHY2_PLL_PARAMETER_3	0xAC5F69E5

//UBOOT fastboot config
#define CONFIG_CMD_FASTBOOT 1
#define CONFIG_FASTBOOT_FLASH_MMC_DEV 1
#define CONFIG_FASTBOOT_FLASH 1
#define CONFIG_USB_GADGET 1
#define CONFIG_USBDOWNLOAD_GADGET 1
#define CONFIG_SYS_CACHELINE_SIZE 64
#define CONFIG_FASTBOOT_MAX_DOWN_SIZE	0x8000000
#define CONFIG_DEVICE_PRODUCT	"lagunaf"
#define CONFIG_CMD_PART

//UBOOT Facotry usb/sdcard burning config
#define CONFIG_AML_V2_FACTORY_BURN              1       //support facotry usb burning
#define CONFIG_AML_FACTORY_BURN_LOCAL_UPGRADE   1       //support factory sdcard burning
#define CONFIG_POWER_KEY_NOT_SUPPORTED_FOR_BURN 1       //There isn't power-key for factory sdcard burning
#define CONFIG_SD_BURNING_SUPPORT_UI            1       //Displaying upgrading progress bar when sdcard/udisk burning

#define CONFIG_AML_SECURITY_KEY                 1
#define CONFIG_UNIFY_KEY_MANAGE                 1

/* net */
#define CONFIG_CMD_NET   1
#if defined(CONFIG_CMD_NET)
	#define CONFIG_DESIGNWARE_ETH 1
	#define CONFIG_PHYLIB	1
	#define CONFIG_NET_MULTI 1
	#define CONFIG_CMD_PING 1
	#define CONFIG_CMD_DHCP 1
	#define CONFIG_CMD_RARP 1
	#define CONFIG_HOSTNAME        arm_gxbb
//	#define CONFIG_RANDOM_ETHADDR  1				   /* use random eth addr, or default */
	#define CONFIG_ETHADDR         00:15:18:01:81:31   /* Ethernet address */
	#define CONFIG_IPADDR          10.18.9.97          /* Our ip address */
	#define CONFIG_GATEWAYIP       10.18.9.1           /* Our getway ip address */
	#define CONFIG_SERVERIP        10.18.9.113         /* Tftp server ip address */
	#define CONFIG_NETMASK         255.255.255.0
#endif /* (CONFIG_CMD_NET) */

/* other devices */
/* I2C DM driver*/
#define CONFIG_DM_I2C
#define CONFIG_SYS_I2C_MESON		1

/* PWM DM driver*/
#define CONFIG_DM_PWM
#define CONFIG_PWM_MESON

#define CONFIG_EFUSE 1
#define CONFIG_AES   1
/* commands */
#define CONFIG_CMD_CACHE 1
#define CONFIG_CMD_BOOTI 1
#define CONFIG_CMD_EFUSE 1
#define CONFIG_CMD_I2C 1
#define CONFIG_CMD_MEMORY 1
#define CONFIG_CMD_FAT 1
#define CONFIG_CMD_GPIO 1
#define CONFIG_CMD_RUN
#define CONFIG_CMD_REBOOT 1
#define CONFIG_CMD_ECHO 1
#define CONFIG_CMD_JTAG	1
#define CONFIG_CMD_AUTOSCRIPT 1
#define CONFIG_CMD_MISC 1
#define CONFIG_CMD_PLLTEST 1
#define CONFIG_CMD_INI 1

/*file system*/
#define CONFIG_DOS_PARTITION 1
#define CONFIG_EFI_PARTITION 1
#define CONFIG_AML_PARTITION 1
#define CONFIG_MMC 1
#define CONFIG_FS_FAT 1
#define CONFIG_FS_EXT4 1
#define CONFIG_LZO 1

//#define CONFIG_MDUMP_COMPRESS 1
#define CONFIG_EXT4_WRITE 1
#define CONFIG_CMD_EXT4 1
#define CONFIG_CMD_EXT4_WRITE 1

/* Cache Definitions */
//#define CONFIG_SYS_DCACHE_OFF
//#define CONFIG_SYS_ICACHE_OFF

/* other functions */
#define CONFIG_NEED_BL301	1
#define CONFIG_NEED_BL32	1
#define CONFIG_CMD_RSVMEM	1
#define CONFIG_FIP_IMG_SUPPORT	1
#define CONFIG_BOOTDELAY	0 //delay 1s
#define CONFIG_SYS_LONGHELP 1
#define CONFIG_CMD_MISC     1
#define CONFIG_CMD_ITEST    1
#define CONFIG_CMD_CPU_TEMP 1
#define CONFIG_CMD_HDMIRX   1
#define CONFIG_SYS_MEM_TOP_HIDE 0x08000000 //hide 128MB for kernel reserve
#define CONFIG_CMD_LOADB    1
#define CONFIG_MULTI_DTB    1

//#define CONFIG_CMD_WOL_POWER 1
/* debug mode defines */
//#define CONFIG_DEBUG_MODE           1
#ifdef CONFIG_DEBUG_MODE
#define CONFIG_DDR_CLK_DEBUG        636
#define CONFIG_CPU_CLK_DEBUG        600
#endif

//support secure boot
#define CONFIG_AML_SECURE_UBOOT   1

#if defined(CONFIG_AML_SECURE_UBOOT)

//for SRAM size limitation just disable NAND
//as the socket board default has no NAND
//#undef CONFIG_AML_NAND

//unify build for generate encrypted bootloader "u-boot.bin.encrypt"
#define CONFIG_AML_CRYPTO_UBOOT   1

//unify build for generate encrypted kernel image
//SRC : "board/amlogic/(board)/boot.img"
//DST : "fip/boot.img.encrypt"
//#define CONFIG_AML_CRYPTO_IMG       1

#endif //CONFIG_AML_SECURE_UBOOT

#define CONFIG_SECURE_STORAGE 1

/* USB port for MT7668. */
#define BT_USB_PORT_NUM	1

//build with uboot auto test
//#define CONFIG_AML_UBOOT_AUTO_TEST 1

//board customer ID
//#define CONFIG_CUSTOMER_ID  (0x6472616F624C4D41)

#if defined(CONFIG_CUSTOMER_ID)
  #undef CONFIG_AML_CUSTOMER_ID
  #define CONFIG_AML_CUSTOMER_ID  CONFIG_CUSTOMER_ID
#endif

/* Choose One of Ethernet Type */
#undef CONFIG_ETHERNET_NONE
#define ETHERNET_INTERNAL_PHY
#undef ETHERNET_EXTERNAL_PHY

#define CONFIG_CMD_AML_MTEST 1
#if defined(CONFIG_CMD_AML_MTEST)
#if !defined(CONFIG_SYS_MEM_TOP_HIDE)
#error CONFIG_CMD_AML_MTEST depends on CONFIG_SYS_MEM_TOP_HIDE;
#endif
#if !(CONFIG_SYS_MEM_TOP_HIDE)
#error CONFIG_SYS_MEM_TOP_HIDE should not be zero;
#endif
#endif

//to support TLV which can support to transfer info from BL2 to BL3X
//#define CONFIG_AML_SUPPORT_TLV

//DDR address to contain info from BL2 to BL3X
//#define  AML_BL2_TMASTER_DDR_ADDR  (0x3000000)
#define CONFIG_HIGH_TEMP_COOL  130

//use hardware sha2
#define CONFIG_AML_HW_SHA2

//use sha2 command
#define CONFIG_CMD_SHA2
#endif

