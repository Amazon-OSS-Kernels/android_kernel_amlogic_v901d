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
#include <command.h>
#include <environment.h>
#include <malloc.h>
#include <asm/byteorder.h>
#include <config.h>
#include <asm/arch/io.h>

#ifdef CONFIG_BOOTLOADER_CONTROL_BLOCK
extern int store_read_ops(
    unsigned char *partition_name,
    unsigned char * buf, uint64_t off, uint64_t size);
extern int store_write_ops(
    unsigned char *partition_name,
    unsigned char * buf, uint64_t off, uint64_t size);

#define COMMANDBUF_SIZE 32
#define STATUSBUF_SIZE      32
#define RECOVERYBUF_SIZE 768

#define BOOTINFO_OFFSET 864
#define SLOTBUF_SIZE    32
#define MISCBUF_SIZE  1088

#define CMD_WIPE_DATA          "wipe_data"
#define CMD_SYSTEM_CRASH    "system_crash"
#define CMD_RUN_RECOVERY   "boot-recovery"
#define CMD_RESIZE_DATA    "resize2fs_data"
#define CMD_FOR_RECOVERY "recovery_"
#define CMD_FASTBOOTD          "fastbootd"

struct bootloader_message {
    char command[32];
    char status[32];
    char recovery[768];

    // The 'recovery' field used to be 1024 bytes.  It has only ever
    // been used to store the recovery command line, so 768 bytes
    // should be plenty.  We carve off the last 256 bytes to store the
    // stage string (for multistage packages) and possible future
    // expansion.
    char stage[32];
    char slot_suffix[32];
    char reserved[192];
};

#define MAX_READ_TIMES 5

#define BCB_ACTION_ERASE 1 /*erase bcb*/
#define BCB_ACTION_NONE 0  /*None*/

/*If the partition has been read five times, the partition will be erased
*/
static int check_bcb_erase(char * miscbuf, int size) {
    struct bootloader_message * msg = NULL;
    unsigned int * read_times = NULL;

    if (!miscbuf) {
        printf("miscbuf is null.\n");
        return BCB_ACTION_NONE;
    }
    msg = malloc(sizeof(struct bootloader_message));
    if (!msg){
        printf("malloc failed.\n");
        return BCB_ACTION_NONE;
    }
    memcpy(msg, miscbuf, sizeof(struct bootloader_message));

    read_times = (int *)msg->reserved;
    printf("Read times %d.\n", (*read_times)++);

    // save read times.
    store_write_ops((unsigned char *)"misc", (unsigned char *)msg, 0, sizeof(struct bootloader_message));
    free(msg);
    return ((*read_times > MAX_READ_TIMES) ? BCB_ACTION_ERASE : BCB_ACTION_NONE);
}

static int clear_misc_partition(char *clearbuf, int size)
{
    char *partition = "misc";

    memset(clearbuf, 0, size);
    if (store_write_ops((unsigned char *)partition,
        (unsigned char *)clearbuf, 0, size) < 0) {
        printf("failed to clear %s.\n", partition);
        return -1;
    }

    return 0;
}

static int do_RunBcbCommand(
    cmd_tbl_t * cmdtp,
    int flag,
    int argc,
    char * const argv[])
{
    int i = 0;
    char command[COMMANDBUF_SIZE] = {0};
    char status[STATUSBUF_SIZE] = {0};
    char recovery[RECOVERYBUF_SIZE] = {0};
    char miscbuf[MISCBUF_SIZE] = {0};
    char clearbuf[COMMANDBUF_SIZE+STATUSBUF_SIZE+RECOVERYBUF_SIZE] = {0};

    if (argc != 2) {
        return cmd_usage(cmdtp);
    }

    printf("Command: ");
    for (i = 0; i < argc; i++) {
        printf("%s ", argv[i]);
    }
    printf("\n");

    char *partition = "misc";
    char *command_mark = (char *)argv[1];

    if (strlen(command_mark) > sizeof(command)) {
        //printf("Bcb command mark range out of length(%d > %d).\n",
            //strlen(command_mark), sizeof(command));
        goto ERR;
    }

    if (!memcmp(command_mark, CMD_WIPE_DATA, strlen(command_mark))) {
        printf("Start to write --wipe_data to %s\n", partition);
        memcpy(miscbuf, CMD_RUN_RECOVERY, sizeof(CMD_RUN_RECOVERY));
        memcpy(miscbuf+sizeof(command)+sizeof(status), "recovery\n--wipe_data", sizeof("recovery\n--wipe_data"));
        store_write_ops((unsigned char *)partition, (unsigned char *)miscbuf, 0, sizeof(miscbuf));
    } else if (!memcmp(command_mark, CMD_SYSTEM_CRASH, strlen(command_mark))) {
        printf("Start to write --system_crash to %s\n", partition);
        memcpy(miscbuf, CMD_RUN_RECOVERY, sizeof(CMD_RUN_RECOVERY));
        memcpy(miscbuf+sizeof(command)+sizeof(status), "recovery\n--system_crash", sizeof("recovery\n--system_crash"));
        store_write_ops((unsigned char *)partition, (unsigned char *)miscbuf, 0, sizeof(miscbuf));
    } else if (!memcmp(command_mark, CMD_RESIZE_DATA, strlen(command_mark))) {
        printf("Start to write --resize2fs_data to %s\n", partition);
        memcpy(miscbuf, CMD_RUN_RECOVERY, sizeof(CMD_RUN_RECOVERY));
        memcpy(miscbuf+sizeof(command)+sizeof(status), "recovery\n--resize2fs_data", sizeof("recovery\n--resize2fs_data"));
        store_write_ops((unsigned char *)partition, (unsigned char *)miscbuf, 0, sizeof(miscbuf));
    } else if (!memcmp(command_mark, CMD_FOR_RECOVERY, strlen(CMD_FOR_RECOVERY))) {
        memcpy(miscbuf, CMD_RUN_RECOVERY, sizeof(CMD_RUN_RECOVERY));
        sprintf(recovery, "%s%s", "recovery\n--", command_mark);
        memcpy(miscbuf+sizeof(command)+sizeof(status), recovery, strlen(recovery));
        store_write_ops((unsigned char *)partition, (unsigned char *)miscbuf, 0, sizeof(miscbuf));
        return 0;
    } else if (!memcmp(command_mark, CMD_FASTBOOTD, strlen(command_mark))) {
        printf("write cmd to enter fastbootd \n");
        memcpy(miscbuf, CMD_RUN_RECOVERY, sizeof(CMD_RUN_RECOVERY));
        memcpy(miscbuf+sizeof(command)+sizeof(status), "recovery\n--fastboot", sizeof("recovery\n--fastboot"));
        store_write_ops((unsigned char *)partition, (unsigned char *)miscbuf, 0, sizeof(miscbuf));
    }

    printf("Start read %s partition datas!\n", partition);
    if (store_read_ops((unsigned char *)partition,
        (unsigned char *)miscbuf, 0, sizeof(miscbuf)) < 0) {
        printf("failed to store read %s.\n", partition);
        goto ERR;
    }

    // judge misc partition whether has datas
    char tmpbuf[MISCBUF_SIZE];
    memset(tmpbuf, 0, sizeof(tmpbuf));
    if (!memcmp(tmpbuf, miscbuf, strlen(miscbuf))) {
        printf("BCB hasn't any datas,exit!\n");
        return 0;
    }

    // check reboot mode, if cold_boot and bypass_standby = 0; exit run bcb.
    run_command("get_rebootmode", 0);
    char * rebootmode = getenv("reboot_mode");
    if ((rebootmode != NULL) && (strcmp(rebootmode, "cold_boot") == 0)) {
        char * bypass_standby = getenv("bypass_standby");
        if ((bypass_standby != NULL) && (strcmp(bypass_standby, "0") == 0)) {
            printf("Cold reboot, don't run bcb\n");
            return 0;
        }
    }

    // if the bcb have read 5 time, erase bcb.
    if (check_bcb_erase(miscbuf, MISCBUF_SIZE) == BCB_ACTION_ERASE) {
        printf("Bcb read time > %d, don't run bcb. erase bcb\n", MAX_READ_TIMES);
        run_command("amlmmc erase misc 0 800", 0);
        return 0;
    }

    memcpy(command, miscbuf, sizeof(command));
    memcpy(status, miscbuf+sizeof(command), sizeof(status));
    memcpy(recovery, miscbuf+sizeof(command)+sizeof(status), sizeof(recovery));
    memcpy(clearbuf, miscbuf, sizeof(clearbuf));

    printf("get bootloader message from misc partition:\n");
    printf("[commannd:%*s]\n[status:%*s]\n",
    (int) sizeof(command), command,(int)sizeof(status), status);

    /*
    print recovery string. printf funtion print too long string (>CONFIG_SYS_PBSIZE), uboot crash.
    */
    printf("[recovery:");
    char printf_str[CONFIG_SYS_CBSIZE] = {0};
    memset(printf_str, 0, sizeof(printf_str));

	/* Check which size is bigger ans that the smaller one
	 * in fact, the CONFIG_SYS_PBSIZE is defined as 1024, so
	 * we should use RECOVERYBUF_SIZE, use the comparasion for
	 * more general solution */
	if (RECOVERYBUF_SIZE > CONFIG_SYS_CBSIZE) {
		memcpy(printf_str, recovery, (CONFIG_SYS_CBSIZE - 1));
		printf_str[CONFIG_SYS_CBSIZE - 1] = '\0';
	} else {
		memcpy(printf_str, recovery, (RECOVERYBUF_SIZE - 1));
		printf_str[RECOVERYBUF_SIZE - 1] = '\0';
	}

    printf("%s", printf_str);
    printf("]\n");

    if (!memcmp(command, CMD_RUN_RECOVERY, strlen(CMD_RUN_RECOVERY))) {
        if (run_command("run recovery_from_flash", 0) < 0) {
            printf("run_command for cmd:run recovery_from_flash failed.\n");
            return -1;
        }
        printf("run command:run recovery_from_flash successful.\n");
        return 0;
    }

    return 0;

 ERR:
    return -1;
}
#else
static int do_RunBcbCommand(
    cmd_tbl_t * cmdtp,
    int flag,
    int argc,
    char * const argv[]) {
    if (argc != 2) {
        return cmd_usage(cmdtp);
    }

    // Do-Nothing!
    return 0;
}
#endif /* CONFIG_BOOTLOADER_CONTROL_BLOCK */


// BCB: Bootloader Control Block
U_BOOT_CMD(
    bcb, 2, 0, do_RunBcbCommand,
    "bcb",
    "\nThis command will run some commands which saved in misc\n"
    "partition by mark to decide whether execute command!\n"
    "Command format:\n"
    "  bcb bcb_mark\n"
    "Example:\n"
    "  /dev/block/misc partiton is saved some contents:\n"
    "  uboot-command\n" // command mark
    "  N/A\n"
    "  setenv aa 11;setenv bb 22;setenv cc 33;saveenv;\n"   // command
    "So you can execute command: bcb uboot-command"
);
