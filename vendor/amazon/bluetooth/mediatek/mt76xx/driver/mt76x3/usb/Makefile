ifeq ($(LINUX_SRC), )
	export KERNEL_SRC := /lib/modules/$(shell uname -r)/build
else
	export KERNEL_SRC := $(LINUX_SRC)
endif
###############################################################################
# USB
###############################################################################
USB_MOD_NAME = btmtk_usb_mt76x3
USB_CFILES := \
	btmtk_usb_main.c

$(USB_MOD_NAME)-objs := $(USB_CFILES:.c=.o)

ccflags-y += -DANDROID_OS
###############################################################################
# Common
###############################################################################
obj-m := $(USB_MOD_NAME).o

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

usb:
	make -C $(KERNEL_SRC) M=$(PWD) $(USB_MOD_NAME).ko

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean

###############################################################################
# Check coding style
###############################################################################
ccs:
	./scripts/checkpatch.pl -f btmtk_usb_main.c
	./scripts/checkpatch.pl -f btmtk_usb_main.h
	./scripts/checkpatch.pl -f btmtk_config.h
	./scripts/checkpatch.pl -f btmtk_define.h
