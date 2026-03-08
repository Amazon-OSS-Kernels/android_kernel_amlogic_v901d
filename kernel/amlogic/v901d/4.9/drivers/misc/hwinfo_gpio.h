/*
 * linux/drivers/misc/hwinfo_gpio.h
 *
 * Copyright (C) 2008 Hewlett-Packard Development Company, L.P.
 *	David Altobelli <david.altobelli@hp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __HWINFO_GPIO_H
#define __HWINFO_GPIO_H

extern int g_audio_clk_mode;
#ifdef CONFIG_IDME
extern const char *idme_get_model_name(void);
#endif
#endif
