/*
 * Copyright (c) 2021 Amazon.com, Inc. or its affiliates.  All rights reserved.
 *
 */

#include <../drivers/usb/gadget/v2_burning/v2_sdc_burn/optimus_sdc_burn_i.h>
#include "fat_file_helper.h"

long call_fat_fopen(const char *filename) {
    return do_fat_fopen(filename);
}

long call_fat_fread(int fd, __u8 *buffer, unsigned long maxsize) {
    return do_fat_fread(fd, buffer, maxsize);
}

void call_fat_fclose(int fd) {
    do_fat_fclose(fd);
}

int call_fat_fseek(int fd, const __u64 offset, int wherehence) {
    return do_fat_fseek(fd, offset, wherehence);
}
