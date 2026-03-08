/*
 * Copyright (C) 2015-2021 Amazon.com Inc. or its affiliates.  All Rights Reserved.
 */
#ifndef AMZN_UNLOCK_INTERNAL_H
#define AMZN_UNLOCK_INTERNAL_H

int amzn_verify_code_internal(const unsigned char *data, unsigned int data_len,
	const unsigned char *sig, unsigned int sig_len,
	const unsigned char *key, unsigned int key_len);

#endif /* AMZN_UNLOCK_INTERNAL_H */
