/*
* Copyright (C) 2017 Amlogic, Inc. All rights reserved.
* *
This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
* *
This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
* *
You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
* *
Description:
*/

#include <common.h>
#include <asm/arch/io.h>
#include <amlogic/aml_efuse.h>
#include <asm/arch/secure_apb.h>

//weak function for EFUSE license query
//all following functions are defined with "weak" for customization of each SoC
//EFUSE_LICX	--> AO_SEC_SD_CFG10/9 --> EFUSE mirror
int  __attribute__((weak)) IS_FEAT_BOOT_VERIFY(void)
{
	#ifndef ADDR_IS_FEAT_BOOT_VERIFY
	  #ifdef EFUSE_LIC0
		  #define ADDR_IS_FEAT_BOOT_VERIFY (EFUSE_LIC0)
		  #define OSET_IS_FEAT_BOOT_VERIFY (0)
	  #else
		  #define ADDR_IS_FEAT_BOOT_VERIFY (AO_SEC_SD_CFG10)
		  #define OSET_IS_FEAT_BOOT_VERIFY (4)
	  #endif
	#endif

	return ((readl(ADDR_IS_FEAT_BOOT_VERIFY) >> OSET_IS_FEAT_BOOT_VERIFY) & 1);

	#undef ADDR_IS_FEAT_BOOT_VERIFY
	#undef OSET_IS_FEAT_BOOT_VERIFY
}
int  __attribute__((weak)) IS_FEAT_BOOT_ENCRYPT(void)
{
	#ifndef ADDR_IS_FEAT_BOOT_ENCRYPT
	  #ifdef EFUSE_LIC0
		#define ADDR_IS_FEAT_BOOT_ENCRYPT (EFUSE_LIC0)
		#define OSET_IS_FEAT_BOOT_ENCRYPT (1)
	  #else
		#define ADDR_IS_FEAT_BOOT_ENCRYPT (AO_SEC_SD_CFG10)
		#define OSET_IS_FEAT_BOOT_ENCRYPT (28)
	  #endif
	#endif

	return ((readl(ADDR_IS_FEAT_BOOT_ENCRYPT) >> OSET_IS_FEAT_BOOT_ENCRYPT) & 1);

	#undef ADDR_IS_FEAT_BOOT_ENCRYPT
	#undef OSET_IS_FEAT_BOOT_ENCRYPT
}
