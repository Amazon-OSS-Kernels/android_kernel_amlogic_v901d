/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#ifndef SI_DRV_RX_ISR_H
#define SI_DRV_RX_ISR_H

void RxIsr_Init(void);

void SiiRxInterruptHandler(void);

bool SiiDrvCableStatusGet(bool *pData);

bool SiiDrvVidStableGet(bool *pData);

#endif // SI_DRV_RX_ISR_H
