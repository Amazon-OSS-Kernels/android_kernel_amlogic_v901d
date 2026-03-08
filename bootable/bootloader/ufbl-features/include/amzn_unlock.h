/*
 * Copyright (C) 2015 - 2021 Amazon.com Inc. or its affiliates.  All Rights Reserved.
 */
#ifndef AMZN_UNLOCK_H
#define AMZN_UNLOCK_H

#define UNLOCK_CODE_LEN         (32)
#define SIGNED_UNLOCK_CODE_LEN  (256)

#define DEVICE_META_SIZE        (32)
#define MAX_PUBKEY_LEN          (300)
#define SHA256_DIGEST_SIZE      (32)
#define RSA_2048_SIG_LEN        (256)

#define BASE64_LEN(x)           ((x + 2) / 3 * 4)

typedef enum
{
	ERR_UNLOCK_SUCCESS = 0,
	ERR_UNLOCK_BAD_ARGUMENT,
	ERR_UNLOCK_NO_MEMORY,
	ERR_UNLOCK_BAD_KEY,
	ERR_UNLOCK_RSA_VERIFY_FAIL,
	ERR_UNLOCK_HASH_FAIL,
	ERR_UNLOCK_GET_CODE_FAIL,
} unlock_error_code;

typedef enum
{
	TEMP_UNLOCK_SUCCESS = 0,
	ERR_TEMP_UNLOCK_GENERAL_FAIL,
	ERR_TEMP_UNLOCK_BAD_ARGUMENT,
	ERR_TEMP_UNLOCK_NO_MEMORY,
	ERR_TEMP_UNLOCK_BAD_KEY,
	ERR_TEMP_UNLOCK_RSA_VERIFY_FAIL,
	ERR_TEMP_UNLOCK_HASH_FAIL,
	ERR_TEMP_UNLOCK_GET_CODE_FAIL,
	ERR_TEMP_UNLOCK_GET_CERT_FAIL,
	ERR_TEMP_UNLOCK_CERT_VERITY_FAIL,
	ERR_TEMP_UNLOCK_DEVICE_META_VERIFY_FAIL,
	ERR_TEMP_UNLOCK_GET_SEC_BLACKLIST_FAIL,
	ERR_TEMP_UNLOCK_IN_BLACKLIST,
} temp_unlock_error_code;

int amzn_verify_unlock(const unsigned char *sig, unsigned int sig_len);
int amzn_target_is_unlocked(void);
int amzn_get_unlock_code(unsigned char *code, unsigned int *len);
const unsigned char * amzn_get_unlock_key(unsigned int *key_len);

/* Support for boot count limited unlock codes */
int amzn_verify_limited_unlock(const unsigned char *sig, unsigned int sig_len, unsigned int ctr_min, unsigned int ctr_max);
int amzn_get_limited_unlock_code(unsigned char *code, unsigned int *len, unsigned int ctr);

#endif
