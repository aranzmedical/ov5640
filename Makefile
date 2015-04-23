#
# E-CON SYSTEM INDIA PRIVATE LIMITED
# Camera sensor driver for eSOMiMX6 OV5640 sensor camera (MIPI camera)
#

ov5640_camera_mipi-objs := ov5640_mipi.o
obj-m += ov5640_camera_mipi.o
SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) clean
	

