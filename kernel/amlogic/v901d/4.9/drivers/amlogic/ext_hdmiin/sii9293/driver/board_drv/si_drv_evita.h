/*
 * drivers/amlogic/ext_hdmiin/sii9293/driver/board_drv/si_drv_evita.h
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

#ifndef SII_DRV_EVITA_H
#define SII_DRV_EVITA_H

#include "si_common.h"

bool SiiDrvEvitaInit(void);

void SiiDrvEvitaAviIfUpdate(void);

void SiiDrvEvitaAudioIfUpdate(uint8_t *pPacket);

#endif // SII_DRV_EVITA_H
