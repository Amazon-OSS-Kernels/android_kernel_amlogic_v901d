/*
 * arch/arm/cpu/armv8/txlx/cec/cec.c
 *
 * Copyright (C) 2012 AMLOGIC, INC. All Rights Reserved.
 * Author: hongmin hua <hongmin hua@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */


#include <common.h>
#include <command.h>
#include <asm/arch/io.h>
#include <asm/arch/secure_apb.h>
/*#include <asm/arch/cec_tx_reg.h>*/
/*#include <amlogic/aml_cec.h>*/
#include "cec.h"

int cec_hw_init(int logic_addr, unsigned char fun_cfg)
{
	#if 0
	if (fun_cfg & (1 << CEC_FUNC_MASK)) {
		/*cec_hw_reset();*/
		/*cec_set_log_addr(logic_addr);*/
	}
	#endif
	writel(fun_cfg, P_AO_DEBUG_REG0);
	writel(logic_addr, AO_DEBUG_REG1);
	printf("cec func: log_addr:%#x,%#x\n", readl(P_AO_DEBUG_REG0),
		readl(AO_DEBUG_REG1));
	return 0;
}

