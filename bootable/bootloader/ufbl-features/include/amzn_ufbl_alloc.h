/*
 * Copyright 2019 - 2022 Amazon.com, Inc. or its Affiliates. All rights reserved.
 */

#ifndef __AMZN_UFBL_ALLOC_H
#define __AMZN_UFBL_ALLOC_H

#if defined(SUPPORT_KERNELFLINGER)

#define amzn_plat_alloc AllocatePool
#define amzn_plat_free FreePool

#else /* defined(SUPPORT_KERNELFLINGER) */
#ifdef SUPPORT_BOLT
#include <platform/bcm_platform.h>
#else
#if defined(UFBL_FEATURE_SECURE_BOOT_MBEDTLS) || defined(UFBL_PLATFORM_AML)
typedef unsigned long ulong;
#endif
#ifdef UFBL_PLATFORM_MSTAR
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#endif /* SUPPORT_BOLT */
#define amzn_plat_alloc malloc
#define amzn_plat_free free

#endif /* defined(SUPPORT_KERNELFLINGER) */

#endif /* AMZN_UFBL_ALLOC */
