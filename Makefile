aranz_ov5640_camera_mipi-objs := ov5640_mipi.o v4l2-int-device.o
obj-m += aranz_ov5640_camera_mipi.o
SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) clean
	

