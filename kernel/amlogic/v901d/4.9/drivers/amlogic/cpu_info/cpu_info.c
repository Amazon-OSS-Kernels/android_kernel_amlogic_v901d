/*
 * drivers/amlogic/cpu_info/cpu_info.c
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

#define pr_fmt(fmt) "cpuinfo: " fmt

#include <linux/export.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
 #include <linux/module.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/secmon.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/arm-smccc.h>

#ifdef CONFIG_AMZN_CHIP_ID_FORMAT
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/amlogic/secmon.h>
#include <linux/amlogic/cpu_version.h>
#include <crypto/hash.h>
#include <crypto/sha.h>
#endif

static unsigned char cpuinfo_chip_id[16] = { 0 };
#ifdef CONFIG_AMZN_CHIP_ID_FORMAT
unsigned int amzn_serial_low;
unsigned int amzn_serial_high;

#define CHIP_ID_LEN	sizeof(cpuinfo_chip_id)
#define CHIP_ID_UPPER	(6)
#define CHIP_ID_LOWER	(7)
#endif

static noinline int fn_smc(u64 function_id,
			   u64 arg0,
			   u64 arg1,
			   u64 arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc((unsigned long)function_id,
						(unsigned long)arg0,
						(unsigned long)arg1,
						(unsigned long)arg2,
						0, 0, 0, 0, &res);
	return res.a0;
}

static int cpuinfo_probe(struct platform_device *pdev)
{
	void __iomem *shm_out;
	struct device_node *np = pdev->dev.of_node;
	int cmd = 0, ret = 0;
#ifdef CONFIG_AMZN_CHIP_ID_FORMAT
	int err = 0;
	struct crypto_shash *tfm = NULL;
	struct shash_desc *desc = NULL;
	u32 hash[SHA256_DIGEST_SIZE / sizeof(u32)];
#endif

	if (of_property_read_u32(np, "cpuinfo_cmd", &cmd))
		return -EINVAL;

	shm_out = get_secmon_sharemem_output_base();
	if (!shm_out) {
		pr_err("failed to allocate shared memory\n");
		ret = -ENOMEM;
		goto error;
	}

	sharemem_mutex_lock();
	ret = fn_smc(cmd, 2, 0, 0);
	if (ret == 0) {
		int version = *((unsigned int *)shm_out);

		if (version == 2)
			memcpy((void *)&cpuinfo_chip_id[0],
			       (void *)shm_out + 4,
			       16);
		else {
			/**
			 * Legacy 12-byte chip ID read out, transform data
			 * to expected order format.
			 */
			uint8_t *ch;
			int i;

			cpuinfo_chip_id[0] = get_meson_cpu_version(
				MESON_CPU_VERSION_LVL_MAJOR);
			cpuinfo_chip_id[1] = get_meson_cpu_version(
				MESON_CPU_VERSION_LVL_MINOR);
			cpuinfo_chip_id[2] = get_meson_cpu_version(
				MESON_CPU_VERSION_LVL_PACK);
			cpuinfo_chip_id[3] = 0;

			/* Transform into expected order for display */
			ch = (uint8_t *)(shm_out + 4);
			for (i = 0; i < 12; i++)
				cpuinfo_chip_id[i + 4] = ch[11 - i];
		}
	} else {
		ret = -EPROTO;
		goto error;
	}
	sharemem_mutex_unlock();

	#ifdef CONFIG_AMZN_CHIP_ID_FORMAT
	tfm = crypto_alloc_shash("sha256", 0, 0);
	if (IS_ERR(tfm)) {
		pr_err("Failed to allocate tfm\n");
		ret = -ENOMEM;
		goto error;
	}

	desc = kzalloc(sizeof(*desc) + crypto_shash_descsize(tfm), GFP_KERNEL);
	if (!desc) {
//		pr_err("Failed to allocate desc\n");
		ret = -ENOMEM;
		goto error;
	}

	desc->tfm = tfm;
	desc->flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	err = crypto_shash_init(desc);
	if (err < 0) {
		pr_err("crypto_shash_init error\n");
		ret = err;
		goto error;
	}

	/* Compute the SHA256 */
	crypto_shash_update(desc, cpuinfo_chip_id, sizeof(cpuinfo_chip_id));
	crypto_shash_final(desc, (u8 *)hash);

	/*
	 * Our internal chip ID is computed
	 * as the following:
	 *
	 * lower 64-bit of sha256(128-bit chip ID in big endian form)
	 */
	amzn_serial_high = be32_to_cpu(hash[CHIP_ID_UPPER]);
	amzn_serial_low = be32_to_cpu(hash[CHIP_ID_LOWER]);
#endif
	ret = 0;
	pr_info("probe done\n");

error:
#ifdef CONFIG_AMZN_CHIP_ID_FORMAT
	kfree(desc);

	if (tfm)
		crypto_free_shash(tfm);
#endif

	return ret;
}

void cpuinfo_get_chipid(unsigned char *cid, unsigned int size)
{
	memcpy(&cid[0], cpuinfo_chip_id, size);
}

static const struct of_device_id cpuinfo_dt_match[] = {
	{ .compatible = "amlogic, cpuinfo" },
	{ /* sentinel */ },
};

static  struct platform_driver cpuinfo_platform_driver = {
	.probe		= cpuinfo_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "cpuinfo",
		.of_match_table	= cpuinfo_dt_match,
	},
};

static int __init meson_cpuinfo_init(void)
{
	return  platform_driver_register(&cpuinfo_platform_driver);
}
module_init(meson_cpuinfo_init);
