
obj-m += mxc_v4l2_capture.o

ov5640_camera_mipi-objs := ov5640_mipi.o
obj-m += ov5640_camera_mipi.o
SRC := $(shell pwd)

# KERNEL_SRC & KERNEL_BUILD must be defined in the build environment.
# KERNEL_SRC := <yocto-root>/build/tmp/work-shared/<machine>/kernel-source
# KERNEL_BUILD := <yocto-root>/build/tmp/work-shared/<machine>/kernel-build-artifacts

# Note: We must link to the Module.symvers file from the built kernel to ensure the module
# is compiled with the correct dependency information.

# Note: There is a known bug in Yocto 1.8 where the Module.symvers file may not be generated correctly.
# See http://comments.gmane.org/gmane.linux.embedded.yocto.general/24484

all:
	cp -u $(KERNEL_BUILD)/Module.symvers .
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) O=$(KERNEL_BUILD) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) clean
