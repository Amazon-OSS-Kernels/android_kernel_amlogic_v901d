# Copyright (c) 2015 - 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved

#
# Common list of objects files we want to include
#
SECURE_BOOT_OBJECTS += \
    image_verify.o

SECURE_BOOT_CFLAGS += \
    -DSUPPORT_UBOOT \
    -std=c99 \
    -ffunction-sections -fdata-sections \
    -DUFBL_FEATURE_SECURE_BOOT

SECURE_BOOT_HEADERS += \
    -I$(UFBL_PATH)/include

ifeq (true,$(strip $(FEATURE_LIBTOMCRYPT)))
SECURE_BOOT_OBJECTS += \
    image_verify_ltc.o

SECURE_BOOT_CFLAGS += \
    -DUFBL_FEATURE_SECURE_BOOT_LTC \
    -DLTC_SOURCE -DLTC_NO_FILE \
    -DUSE_LTM -DLTM_DESC \
    -D_WCHAR_T -DLTC_CLEAN_STACK

SECURE_BOOT_HEADERS = \
    -I$(UFBL_PATH)/features/libtomcrypt/ \
    -I$(UFBL_PATH)/features/libtomcrypt/src/headers
endif

ifeq (true,$(strip $(FEATURE_MBEDTLS)))
SECURE_BOOT_OBJECTS += \
    image_verify_mbedtls.o

SECURE_BOOT_CFLAGS += \
    -DUFBL_FEATURE_SECURE_BOOT_MBEDTLS \
    -D__KERNEL_STRICT_NAMES \
    -DMBEDTLS_CONFIG_FILE='<config-ufbl.h>'

SECURE_BOOT_HEADERS = \
    -I$(UFBL_PATH)/features/mbedtls-gplv2/src/configs \
    -I$(UFBL_PATH)/features/mbedtls-gplv2/src/include
endif

# Begin rules for kbuild U-Boot
ifeq (yes,$(strip $(UFBL_UBOOT_IS_KBUILD)))

ccflags-y += $(SECURE_BOOT_CFLAGS) $(SECURE_BOOT_HEADERS)
obj-y += $(SECURE_BOOT_OBJECTS)

else # default rules

include $(TOPDIR)/config.mk

SECURE_BOOT_LIB = $(obj)libsecure_boot.o

CFLAGS += \
    $(SECURE_BOOT_CFLAGS) \
    $(SECURE_BOOT_HEADERS)

COBJS  := $(SECURE_BOOT_OBJECTS)
SRCS   := $(COBJS:.o=.c)
OBJS   := $(addprefix $(obj),$(COBJS))

$(SECURE_BOOT_LIB): $(OBJS)
	$(call cmd_link_o_target, $(OBJS))

endif # (yes,$(strip $(UFBL_UBOOT_IS_KBUILD)))
