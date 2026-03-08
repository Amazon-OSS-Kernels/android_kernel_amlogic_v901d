/*
 * soc_state_pwm.h
 *
 * soc_pwm_signal header to export PWM APIs
 *
 * IMPORTANT: You must only call these functions in kernel modules that are
 *            initialized after rootfs_initcall as the pwm driver needs
 *            to be initialized first.  Failure to do so may have undefined
 *            results.
 *
 * Copyright (C) 2021 Amazon Technologies Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SIGN_OF_LIFE_PLATFORM_H
#define __SIGN_OF_LIFE_PLATFORM_H

#define NORMAL_BOOT_PWM_FREQ_DEFAULT        200
#define RECOVERY_BOOT_PWM_FREQ_DEFAULT      160
#define POSTRECOVERY_BOOT_PWM_FREQ_DEFAULT  180

// Boot mode
enum boot_mode_type {
	MODE_UNAVAILABLE = 0,
	MODE_NORMAL = 1,
	MODE_RECOVERY = 2,
	MODE_POSTRECOVERY = 3,
};

enum boot_mode_type get_boot_mode_type(void);
int send_pwm_signal(unsigned int pwm_freq);

#endif
