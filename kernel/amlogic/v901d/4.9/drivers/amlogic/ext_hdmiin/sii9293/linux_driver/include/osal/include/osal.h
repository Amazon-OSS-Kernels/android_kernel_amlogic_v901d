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

#ifndef _OSAL_H
#define _OSAL_H

/* TODO: check necessity of inclusion for KAL layer implementation */

#if !defined(__KERNEL__)
#include <os_compiler.h>
#include <os_linux.h>
#include "os_data.h"
#include <os_socket.h>
#include <os_file.h>
#include <os_string.h>
#if !defined(DO_NOT_USE_DMLS)
#include <os_dmls.h>
#endif

#else
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include "osal/include/os_types.h"
#include "osal/include/os_data.h"
#endif

#define MAX_TIMER_NAME_LEN 64
/** @brief Define signature of timer callback function used by
 * HalRequestTimerCallback().
 */
typedef void (*timerCallbackHandler_t)(void *);

/***** local type definitions ************************************************/

/* Define structure used to maintain list of outstanding timer objects. */
struct _SiiOsTimerInfo_t {
	struct list_head listEntry;
	struct work_struct workItem;
	uint8_t flags;
	char timerName[MAX_TIMER_NAME_LEN];
	struct hrtimer hrTimer;
	timerCallbackHandler_t callbackHandler;
	void *callbackParam;
	uint32_t timeMsec;
	bool bPeriodic;
};

uint32_t SiiOsInit(uint32_t maxChannels);

uint32_t SiiOsTerm(void);

/* Timer APIs */
uint32_t SiiOsTimerCreate
(
	const char *pName,
	void (*pTimerFunction)(void *pArg),
	void *pTimerArg,
	struct _SiiOsTimerInfo_t **pRetTimerId
);

uint32_t SiiOsTimerDelete(struct _SiiOsTimerInfo_t **pTimerId);

uint32_t SiiOsTimerStart(struct _SiiOsTimerInfo_t *timerId, uint32_t time_msec);

uint32_t SiiOsTimerStop(struct _SiiOsTimerInfo_t *timerId);

void *SiiOsAlloc(const char *pName, size_t size, uint32_t flags);

void *SiiOsCalloc(const char *pName, size_t size, uint32_t flags);

void SiiOsFree(void *pAddr);

#endif /* _OSAL_H */
