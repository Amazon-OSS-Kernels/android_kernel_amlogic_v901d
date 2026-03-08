/*
 * Copyright (c) 2015 - 2021 Amazon.com, Inc. or its affiliates.  All rights reserved.
 */
#ifndef AMZN_ONETIME_UNLOCK_H
#define AMZN_ONETIME_UNLOCK_H

#include <stddef.h>
#include "amzn_unlock.h"

#define ONETIME_UNLOCK_CODE_LEN   (32)

typedef struct {
    unsigned char device_meta[DEVICE_META_SIZE];
    unsigned int pk_len;
    unsigned char pubkey[MAX_PUBKEY_LEN];
    unsigned char signature[RSA_2048_SIG_LEN];
}__attribute__((packed, aligned(4))) onetime_unlock_cert_t;

/* Support for one time unlock */
int amzn_get_one_tu_code(unsigned char *code, unsigned int *len);
int amzn_get_onetime_unlock_root_pubkey(const unsigned char **key, unsigned int *key_len);
int amzn_verify_device_meta(const unsigned char *data, unsigned int len);
int amzn_get_onetime_unlock_pk_blacklist(const unsigned char **buf, unsigned int *len);

int amzn_get_onetime_random_number(const unsigned char *entropy, size_t entropy_size,
    unsigned char *output_buf, size_t output_size);
int amzn_get_onetime_unlock_cert(onetime_unlock_cert_t **cert, unsigned int *len);
int amzn_set_onetime_unlock_cert(const char *b64_buf, unsigned int size);
int amzn_set_onetime_unlock_code(const char *b64_buf, unsigned int size);

int amzn_target_is_onetime_unlocked(void);
int amzn_onetime_unlock_b64_decode(const unsigned char *b64_str, size_t len_in,
    unsigned char *b64_dec, size_t *len_out);
int amzn_onetime_unlock_b64_encode(const unsigned char *in, size_t len,
    unsigned char *out, size_t *outlen);
int amzn_verify_onetime_unlock_code(const unsigned char *sig, unsigned int sig_len);
#endif
