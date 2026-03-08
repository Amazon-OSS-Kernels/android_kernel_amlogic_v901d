/*
 * secure_boot.c
 *
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include <asm-generic/gpio.h>
#include <asm/arch/secure_apb.h>
#include <asm/io.h>
#include <common.h>
#include <ctype.h>
#include "amzn_secure_boot.h"
#if defined(UFBL_FEATURE_UNLOCK)
#include <amzn_unlock.h>
#include <u-boot/sha256.h>
#endif

#if defined(UFBL_FEATURE_ONETIME_UNLOCK)
#include <amzn_onetime_unlock.h>
#include "onetime_unlock_key.h"
#endif

#warning Override the UFBL amzn_target_sha256
void amzn_target_sha256(const void *data, size_t n, void *digest)
{
	/* Using a default hw implementation for sha256 */
	sha256_csum_wd(data, n,digest,0 );
}

bool secure_boot_enabled(void)
{
	const unsigned long cfg10 = readl(AO_SEC_SD_CFG10);
	return ( (cfg10 & (0x1<< 4)) ? true : false );
	/* 4th bit indicates secure boot status */
}

bool anti_rollback_enabled(void)
{
	const unsigned long cfg10 = readl(AO_SEC_SD_CFG10);
	return ( (cfg10 & (0x1<< 25)) ? true : false );
}


const char *amzn_target_device_name(void)
{
	static char target_name[16]={0};
	int i=0;
	strncpy(target_name,CONFIG_DEVICE_PRODUCT,strlen(CONFIG_DEVICE_PRODUCT));
	target_name[15] = '\0'; /* make sure the string is ended with \0 in case the target name is 16 bytes */
	while(i<strlen(target_name)){
		target_name[i]=tolower(target_name[i]);
		i++;
	}
	printf("target_device_name is %s\n",target_name);
	return target_name;
}

int amzn_target_device_type(void)
{

	/* Is anti-rollback enabled? */
	if (anti_rollback_enabled() == true) {
		return AMZN_PRODUCTION_DEVICE;
	}
	else {
		return AMZN_ENGINEERING_DEVICE;
	}
}

bool amzn_target_is_lockdown()
{
	bool ret = true;
	/* Is this an engineering device? */
	if (amzn_target_device_type() == AMZN_ENGINEERING_DEVICE)
		ret = false;

	/* Are we un-locked? */
	if (amzn_target_is_unlocked())
		ret = false;

	if (amzn_target_is_onetime_unlocked())
		ret = false;

	return ret;
}
#if defined(UFBL_FEATURE_UNLOCK)
#define CHIPID_UPPER		(6)
#define CHIPID_LOWER		(7)
#define CHIPID_BUF_SIZE		(16)
#define HASH_BUF_SIZE		(32)

int amzn_get_unlock_code(unsigned char *code, unsigned int *len)
{
	sha256_context ctx;
	uint8_t buff[CHIPID_BUF_SIZE] = {0};
	uint8_t hash[HASH_BUF_SIZE] = {0};

	if (!code || !len || *len < (16 + 1))
		return -1;

	if (get_chip_id(&buff[0], sizeof(buff)))
		return -1;
	/**
	 * To sync with Amazon serial number from kernel's /proc/cpuinfo,
	 * the unlock_code is low 64 bit of sha256(SoC Chipid 128bits).
	 */
	sha256_starts(&ctx);
	sha256_update(&ctx, &buff[0], sizeof(buff));
	sha256_finish(&ctx, &hash[0]);
	u32 *hashcode = (u32 *) &hash[0];
	snprintf(code, CHIPID_BUF_SIZE+1, "%08x%08x",be32_to_cpu(hashcode[CHIPID_UPPER]),
			be32_to_cpu(hashcode[CHIPID_LOWER]));

	*len = 16;
	return 0;
}

const unsigned char *amzn_get_unlock_key(unsigned int *key_len)
{
	/* ABC_unlock.pub.der */
	static const  unsigned char lagunaf_unlock_pub_der[] = {
		0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
		0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00, 0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01,
		0x00, 0xd6, 0xe3, 0x46, 0x74, 0xc1, 0xbb, 0x33, 0xe8, 0x68, 0x6b, 0xed, 0xb3, 0xf2, 0x77, 0xe9,
		0x69, 0x32, 0x8f, 0xb7, 0xa7, 0x72, 0x1a, 0x14, 0x4a, 0xef, 0xfc, 0x30, 0x42, 0xb7, 0xfd, 0x49,
		0xfe, 0x9e, 0xe2, 0x73, 0xc2, 0x63, 0xea, 0xf8, 0xfd, 0x73, 0x77, 0xff, 0xca, 0x8c, 0x6a, 0x7c,
		0xec, 0x03, 0x9c, 0xe6, 0xd6, 0x9d, 0xd6, 0xa7, 0xd6, 0x7f, 0x33, 0x1e, 0x25, 0xbe, 0x81, 0x42,
		0xd8, 0xff, 0xa4, 0xdd, 0x78, 0x2a, 0xd1, 0x6f, 0xcb, 0x0f, 0xf2, 0x1c, 0x7b, 0xaa, 0xcf, 0xc4,
		0x56, 0x97, 0xc0, 0x6b, 0x41, 0xc3, 0x18, 0x4f, 0xa7, 0x6f, 0x56, 0x3e, 0xd6, 0xd7, 0x72, 0x6f,
		0xc6, 0x89, 0xe0, 0xb3, 0x18, 0xc3, 0x66, 0xe4, 0xf6, 0x8d, 0xc4, 0xad, 0x85, 0x09, 0xa3, 0xae,
		0xf4, 0x76, 0x73, 0xea, 0x6a, 0x34, 0xad, 0xaa, 0x52, 0x41, 0x93, 0x0e, 0xfe, 0x8d, 0x95, 0xba,
		0xcc, 0xf2, 0xfa, 0xab, 0xf3, 0xcc, 0xf7, 0x97, 0x32, 0xdf, 0x82, 0x82, 0x17, 0xe9, 0xe3, 0x0c,
		0x26, 0xa7, 0xee, 0x8a, 0x23, 0xc3, 0x1d, 0x2d, 0x16, 0x47, 0x97, 0xd1, 0x45, 0x01, 0x92, 0x80,
		0xd3, 0x2d, 0x3e, 0x99, 0x5b, 0xeb, 0x69, 0x76, 0x3a, 0xfe, 0xba, 0x80, 0xa6, 0x96, 0x70, 0x69,
		0xc8, 0x11, 0xc8, 0x66, 0x27, 0xec, 0x06, 0x44, 0x14, 0x7c, 0xec, 0x3b, 0xcf, 0x20, 0xc5, 0x64,
		0x1a, 0x8d, 0x10, 0xc2, 0x52, 0x47, 0x52, 0x22, 0xfb, 0xc4, 0xfb, 0x03, 0x4d, 0x01, 0x89, 0xa2,
		0x42, 0x1b, 0xb7, 0x3e, 0x90, 0x9d, 0xfc, 0x23, 0xba, 0xa4, 0xd6, 0x33, 0x15, 0x69, 0x56, 0x3c,
		0xb8, 0xcd, 0x2e, 0x0c, 0x1e, 0xd5, 0x1a, 0xc8, 0xa8, 0x70, 0x0b, 0x10, 0x27, 0xf6, 0xac, 0x5e,
		0xb4, 0xac, 0x6b, 0x02, 0x9d, 0x7a, 0x47, 0x45, 0x40, 0xe8, 0x42, 0x27, 0x33, 0xcd, 0xd9, 0xdf,
		0x31, 0x02, 0x03, 0x01, 0x00, 0x01
		};

	const int unlock_key_size = sizeof(lagunaf_unlock_pub_der);
	if (!key_len)
		return NULL;

	*key_len = unlock_key_size;

	return lagunaf_unlock_pub_der;
}

#if defined(UFBL_FEATURE_ONETIME_UNLOCK)
int amzn_get_one_tu_code(unsigned char *code, unsigned int *len)
{
	static unsigned char code_generated = 0;
	static unsigned char one_tu_code[ONETIME_UNLOCK_CODE_LEN + 1] = {0};

	if (!code || !len || *len < ONETIME_UNLOCK_CODE_LEN)
		return -1;

	if (!code_generated) {
/**
 * Different SoC/Product may have different scheme to add entropy into PRNG.
 * For ABC, take unlock code plus get_timer (8 bytes) as entropy
 */
#define ENTROPY_LEN (UNLOCK_CODE_LEN + 8)
		static unsigned char entropy[ENTROPY_LEN] = {0};
		unsigned int unlock_code_len = UNLOCK_CODE_LEN;
		if (amzn_get_unlock_code(entropy, &unlock_code_len)) {
			return -1;
		}
		sprintf(&entropy[unlock_code_len], "%08x", get_timer(0));
/**
 * amzn_get_onetime_random_number will return a binary string which is not readable.
 * The binary string cannot be returned via fastboot so using base64 encode it.
 */
// compute how many random bytes do we need so that the converted size is the target length
#define RANDOM_BYTES_SIZE (ONETIME_UNLOCK_CODE_LEN + 3) / 4 * 3
		uint8_t random_bytes[RANDOM_BYTES_SIZE] = {0};
		unsigned int out_len = sizeof(one_tu_code);

		if (amzn_get_onetime_random_number(entropy, strlen(entropy),
						random_bytes, sizeof(random_bytes)))
			return -1;

		if (amzn_onetime_unlock_b64_encode(random_bytes, sizeof(random_bytes),
						one_tu_code, &out_len)) {
			return -1;
		}
		code_generated = 1;
	}
	memcpy(code, one_tu_code, ONETIME_UNLOCK_CODE_LEN);
	*len = ONETIME_UNLOCK_CODE_LEN;
	return 0;
}

int amzn_get_onetime_unlock_root_pubkey(const unsigned char **key, unsigned int *key_len)
{
	static const unsigned char onetime_unlock_key[] = ONETIME_UNLOCK_KEY;
	const int onetime_unlock_key_size = sizeof(onetime_unlock_key);

	if (!key || !key_len)
		return -1;

	*key_len = onetime_unlock_key_size;
	*key = onetime_unlock_key;
	return 0;
}
#endif //UFBL_FEATURE_ONETIME_UNLOCK

#endif

