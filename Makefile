obj-m += mxc_v4l2_capture.o
obj-m += ipu_prp_vf_sdc.o ipu_prp_vf_sdc_bg.o
obj-m += ipu_fg_overlay_sdc.o ipu_bg_overlay_sdc.o
obj-m += ipu_prp_enc.o ipu_still.o
obj-m += ipu_csi_enc.o ipu_still.o

CFLAGS_mxc_v4l2_capture.o := -DDEBUG

#ov5640_camera_int-objs := ov5640.o
#obj-m += ov5640_camera_int.o

#ov5642_camera-objs := ov5642.o
#obj-m += ov5642_camera.o

ov5640_camera_mipi-objs := ov5640_mipi.o
obj-m += ov5640_camera_mipi.o

#adv7180_tvin-objs := adv7180.o
#obj-m += adv7180_tvin.o

obj-$(CONFIG_VIDEO_V4L2_MXC_INT_DEVICE) += v4l2-int-device.o

SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers
