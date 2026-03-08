################################################################################
#
#  build_kernel_config.sh
#
#  Copyright (c) 2021-2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
################################################################################

KERNEL_SUBPATH="kernel/amlogic/v901d/4.9"
DEFCONFIG_NAME="lagunaf_defconfig"
TARGET_ARCH="arm"
MAKE_DTBS=y

# Expected image files are seperated with ":"
KERNEL_IMAGES="arch/arm/boot/Image:arch/arm/boot/zImage"

################################################################################
# NOTE: You must fill in the following with the path to a copy of
#       a gcc-linaro-6.3.1-2017.02-x86_64_arm-linux-gnueabihf compiler
################################################################################
CROSS_COMPILER_PATH=""
TOOLCHAIN_PREFIX="arm-linux-gnueabihf-"
