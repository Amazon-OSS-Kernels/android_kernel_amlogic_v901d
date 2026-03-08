/*
 * Copyright (c) 2015 - 2021 Amazon.com, Inc. or its affiliates.  All rights reserved.
 */

#include "amzn_onetime_unlock.h"
#ifndef SUPPORT_UBOOT
#include <debug.h>
#endif
#include "ufbl_debug.h" /* for dprintf */
#ifdef SUPPORT_BOLT
#include <platform/bcm_platform.h>
#endif

/* Compiler may already define this.
 * If it doesn't, try this other compiler specific setting.
 * __attribute__((weak)) is specific to the GNU toolchain.
 */
#ifndef __WEAK
#define __WEAK __attribute__((weak))
#endif

#if defined(UFBL_FEATURE_ONETIME_UNLOCK)

static onetime_unlock_cert_t one_tu_cert = {
        {0},
        0,
        {0},
        {0}
};

static unsigned char signed_one_tu_code[RSA_2048_SIG_LEN] = {0};
static unsigned char signed_code_available = 0;

/**
 * Get one time unlock codes.
 * @param codes Double Pointer to unlock codes buffer
 * @param reboot_cnt Pointer to reboot_cnt, to be filled
 * @return 0 - success, fail otherwise
 */
__WEAK int amzn_get_one_tu_code(unsigned char *code, unsigned int *len)
{
    (void)code; /* avoid unused variable warning */
    (void)len;
    return -1;
}

/**
 * Get one time unlock root public key (per product key).
 * @param key Double Pointer to root public key buffer
 * @param key_len Pointer to key length
 * @return 0 - success, fail otherwise
 */
__WEAK int amzn_get_onetime_unlock_root_pubkey(const unsigned char **key, unsigned int *key_len)
{
    (void)(key);
    (void)(key_len);
    return -1;
}

/**
 * Verify one time unlock device specific meta info.
 * @param data Pointer to device meta info buffer
 * @param len Device meta info buffer size
 * @return 0 - success, fail otherwise
 */
__WEAK int amzn_verify_device_meta(const unsigned char *data, unsigned int len)
{
    (void)(data);
    (void)(len);
    return 0;
}

/**
 * Get one time unlock public key blacklist,
 * it is a buffer of SHA256 hash values.
 * @param buf Double pointer to blacklist buffer
 * @param len Pointer to output blacklist length
 * @return 0 - success, fail otherwise
 */
__WEAK int amzn_get_onetime_unlock_pk_blacklist(const unsigned char **buf, unsigned int *len)
{
    (void)(buf);
    (void)(len);
    return -1;
}

/**
 * Get one time unlock certificate.
 */
int amzn_get_onetime_unlock_cert(onetime_unlock_cert_t **cert, unsigned int *len)
{
    if (!len) return -1;
    *len = sizeof(one_tu_cert);
    *cert = &one_tu_cert;
    return 0;
}

/**
 * Set one time unlock certificate.
 */
int amzn_set_onetime_unlock_cert(const char *buf, unsigned int size)
{
    unsigned char *b64;
    size_t out_len = (size_t)size;
    b64 = (unsigned char *)buf;

    if (amzn_onetime_unlock_b64_decode(b64, size, b64, &out_len)) {
        dprintf(CRITICAL, "[%s:%d] certification b64 decode failed\n", __func__, __LINE__);
        return -1;
    }

    if (!b64 || (out_len != sizeof(one_tu_cert)))
        return -1;
    memcpy((const char *)&one_tu_cert, b64, out_len);
    return 0;
}

/**
 * Set one time unlock code (signature).
 */
int amzn_set_onetime_unlock_code(const char *buf, unsigned int size)
{
    unsigned char *b64;
    size_t out_len = (size_t)size;
    b64 = (unsigned char *)buf;

    if (amzn_onetime_unlock_b64_decode(b64, size, b64, &out_len)) {
        dprintf(CRITICAL, "[%s:%d] unlock code b64 decode failed\n", __func__, __LINE__);
        return -1;
    }

    if (!b64 || out_len != RSA_2048_SIG_LEN)
        return -1;
    memcpy(signed_one_tu_code, b64, out_len);
    signed_code_available = 1;
    return 0;
}

/**
 * Verify one time unlock code and certificate.
 */
int amzn_target_is_onetime_unlocked(void)
{
    unsigned char is_unlocked = 0;

    if(signed_code_available == 0)
        return is_unlocked;

    if (!amzn_verify_onetime_unlock_code((void*)signed_one_tu_code, RSA_2048_SIG_LEN)) {
        is_unlocked = 1;
    }
    return is_unlocked;
}
#endif
