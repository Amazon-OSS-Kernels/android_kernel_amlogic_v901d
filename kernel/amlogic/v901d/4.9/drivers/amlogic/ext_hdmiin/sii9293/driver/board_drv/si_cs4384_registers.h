/*
 * drivers/amlogic/ext_hdmiin/sii9293/driver/board_drv/si_cs4384_registers.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
 */

#ifndef __SI_CS4384_REGISTERS_H__
#define __SI_CS4384_REGISTERS_H__

#include "si_drv_cra_cfg.h"

//------------------------------------------------------------------------------
// Registers in Page F      (0x30)
//------------------------------------------------------------------------------

#define CS_4384_CTRL1 (PP_PAGE_AUDIO | 0x02)
#define CS_4384_CTRL2 (PP_PAGE_AUDIO | 0x03)
#define CS_4384_CTRL3 (PP_PAGE_AUDIO | 0x05)

#endif // __SI_CS4384_REGISTERS_H__
