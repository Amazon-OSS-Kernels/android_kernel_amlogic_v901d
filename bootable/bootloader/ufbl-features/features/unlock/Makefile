# Copyright (c) 2016 - 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved

#
# Common list of objects files we want to include
#
UNLOCK_OBJECTS = \
    unlock.o

# Common set of CFLAGS we need
UNLOCK_CFLAGS = \
    -DSUPPORT_UBOOT \
    -std=c99 \
    -ffunction-sections -fdata-sections \
    -DUFBL_FEATURE_UNLOCK

ifeq (true,$(strip $(FEATURE_LIBTOMCRYPT)))
UNLOCK_OBJECTS += \
    unlock_ltc.o

UNLOCK_CFLAGS += \
    -DUFBL_FEATURE_UNLOCK_LTC \
    -DLTC_SOURCE \
    -DLTC_NO_FILE \
    -DUSE_LTM -DLTM_DESC \
    -D_WCHAR_T -DLTC_CLEAN_STACK

UNLOCK_HEADERS = \
    -I$(UFBL_PATH)/features/libtomcrypt/ \
    -I$(UFBL_PATH)/features/libtomcrypt/src/headers
endif

ifeq (true,$(strip $(FEATURE_MBEDTLS)))
UNLOCK_OBJECTS += \
    unlock_common.o \
    unlock_mbedtls.o

UNLOCK_CFLAGS += \
    -DUFBL_FEATURE_UNLOCK_MBEDTLS \
    -DMBEDTLS_CONFIG_FILE='<config-ufbl.h>'

UNLOCK_HEADERS = \
    -I$(UFBL_PATH)/features/mbedtls-gplv2/include \
    -I$(UFBL_PATH)/features/mbedtls-gplv2/src/configs \
    -I$(UFBL_PATH)/features/mbedtls-gplv2/src/include
endif

# Begin rules for kbuild U-Boot
ifeq (yes,$(strip $(UFBL_UBOOT_IS_KBUILD)))

ccflags-y    += $(UNLOCK_CFLAGS) $(UNLOCK_HEADERS)
obj-y        += $(UNLOCK_OBJECTS)

else # default rules

include $(TOPDIR)/config.mk

UNLOCK_LIB = $(obj)libunlock.o

# Default CFLAGS
CFLAGS += \
    $(UNLOCK_CFLAGS) \
    $(UNLOCK_HEADERS)

COBJS := $(UNLOCK_OBJECTS)
SRCS  := $(COBJS:.o=.c)
OBJS  := $(addprefix $(obj),$(COBJS))

$(UNLOCK_LIB): $(OBJS)
	$(call cmd_link_o_target, $(OBJS))

endif # (yes,$(strip $(UFBL_UBOOT_IS_KBUILD)))
