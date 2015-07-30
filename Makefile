# OV5640 Custom Camera Driver
# (c) ARANZ Medical Ltd. 2015
# Authors: Jared Sanson & Brenton Milne
# License: GPL
#
# Documentation for linux kernel makefiles:
# https://www.kernel.org/doc/Documentation/kbuild/makefiles.txt
#
# NOTE:
# KERNEL_SRC & KERNEL_BUILD must be defined in the build environment. Eg:
# 	KERNEL_SRC := <yocto-root>/build/tmp/work-shared/<machine>/kernel-source
# 	KERNEL_BUILD := <yocto-root>/build/tmp/work-shared/<machine>/kernel-build-artifacts
#

obj-$(OUT_OF_TREE_BUILD) += mxc_v4l2_capture.o

ov5640_camera_aranz-objs := ov5640_mipi.o
obj-m += ov5640_camera_aranz.o

SRC := $(shell pwd)


# Note: We must link to the Module.symvers file from the built kernel to ensure the module
# is compiled with the correct dependency information.

# Note: There is a known bug in Yocto 1.8 where the Module.symvers file may not be generated correctly.
# See http://comments.gmane.org/gmane.linux.embedded.yocto.general/24484

all:
	cp -u $(KBUILD_OUTPUT)/Module.symvers .
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) O=$(KBUILD_OUTPUT) $(MAKE_OPTS) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) clean


# Build in DEBUG mode if configured for debug
ccflags-$(CONFIG_VIDEO_MXC_DEBUG) := -DDEBUG

# Supress pedantic warnings that don't matter
CFLAGS_ov5640_mipi.o := -Wno-declaration-after-statement
