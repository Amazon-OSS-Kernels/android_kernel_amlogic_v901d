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

#ifndef _OS_DATA_H
#define _OS_DATA_H

#ifdef PLATFORM3
#error This file is not designed for use on the specified platform.
#endif

#if !defined(__KERNEL__)
#include <sys/time.h>
#include <bits/local_lim.h>
#include <pthread.h>
#else
#include <linux/time.h>
#endif

#endif /* _OS_DATA_H */
