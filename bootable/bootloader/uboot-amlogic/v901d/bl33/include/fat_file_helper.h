/*
 * Copyright (c) 2021 Amazon.com, Inc. or its affiliates.  All rights reserved.
 */

#ifndef _FAT_FILE_HELPER_H_
#define _FAT_FILE_HELPER_H_

/*
 * Wrapper function to call fat functions in
 * /drivers/usb/gadget/v2_burning/v2_sdc_burn/optimus_sdc_burn_i.h
 */
long call_fat_fopen(const char *filename);
long call_fat_fread(int fd, __u8 *buffer, unsigned long maxsize);
void call_fat_fclose(int fd);
int call_fat_fseek(int fd, const __u64 offset, int wherehence);

#endif