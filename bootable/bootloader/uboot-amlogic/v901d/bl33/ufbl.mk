# UFBL
SRCTREE               := $(srctree)
OBJTREE               := $(objtree)
GET_LOCAL_DIR    = $(patsubst %/,%,$(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))
KBUILD_LOCAL_DIR_FUNC := $$(patsubst \%/,\%,$$(dir $$(word $$(words $$(MAKEFILE_LIST)),$$(MAKEFILE_LIST))))
KBUILD_LOCAL_DIR       = $(call KBUILD_LOCAL_DIR_FUNC)


$(info ---current BOOTLOADER_PROJECT_TARGET :::$(BOOTLOADER_TARGET_BOARD))

TOPDIR	:= $(shell cd $(srctree) && /bin/pwd)
obj	:= $(shell cd $(buildtree) && /bin/pwd)/
export GET_LOCAL_DIR TOPDIR
BOOTLOADER_PROJECT_TARGET=$(BOOTLOADER_TARGET_BOARD)

$(info $(BOOTLOADER_PROJECT_TARGET) is UFBL project target)

export SRCTREE OBJTREE GET_LOCAL_DIR KBUILD_LOCAL_DIR_FUNC KBUILD_LOCAL_DIR TOPDIR
UFBL_ENABLE     := yes
UFBL_PATH       := $(TOPDIR)/../../../ufbl-features
UFBL_LIB_PATH   := $(UFBL_PATH)/features
UFBL_INC_PATH   := $(UFBL_PATH)/include
UFBL_PROJECT    := lagunaf
TARGET_PRODUCT  := lagunaf

export UFBL_ENABLE UFBL_PATH UFBL_LIB_PATH UFBL_PROJECT BOARD_AMAZON_KERNEL_SIGNING


# ---back UFBL_LIBS_PATH := $(dir $(sort $(UFBL_LIBS)))


KBUILD_CFLAGS += -DUFBL_FEATURE_SECURE_BOOT
UBOOTINCLUDE  += -Iinclude $(if $(KBUILD_SRC), -I$(srctree)/include)
UBOOTINCLUDE  += -I$(srctree)/arch/$(ARCH)/include
#UBOOTINCLUDE  += -I$(srctree)//include/linux
UBOOTINCLUDE  += -include $(srctree)/include/linux/kconfig.h -I$(UFBL_INC_PATH)



CFLAGS += "-Wno-error"
include $(UFBL_PATH)/build/ufbl_uboot.mk

UFBL_LIBS_PATH := $(dir $(sort $(UFBL_LIBS)))

libs-y += $(UFBL_LIBS_PATH)
#include $(UFBL_PATH)/build/ufbl_uboot.mk

