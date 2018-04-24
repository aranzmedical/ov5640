/*
 * Copyright 2004-2014 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @file drivers/media/video/mxc/capture/mxc_v4l2_capture.c
 *
 * @brief Mxc Video For Linux 2 driver
 *
 * @ingroup MXC_V4L2_CAPTURE
 */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>      // initialization macros
#include <linux/module.h>    // dynamic loading of modules into the kernel
#include <linux/kernel.h>    // kernel stuff
#include <linux/gpio.h>      // GPIO functions/macros
#include <linux/interrupt.h> // interrupt functions/macros
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/pagemap.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/syscon/imx6q-iomuxc-gpr.h>
#include <linux/mutex.h>
#include <linux/mxcfb.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include "v4l2-int-device.h"
#include <linux/fsl_devices.h>
#include "mxc_v4l2_capture.h"
#include "ipu_prp_sw.h"

#define EPITCR		0x00
#define EPITSR		0x04
#define EPITLR		0x08
#define EPITCMPR	0x0c
#define EPITCNR		0x10

#define EPITCR_EN			(1 << 0)
#define EPITCR_ENMOD			(1 << 1)
#define EPITCR_OCIEN			(1 << 2)
#define EPITCR_RLD			(1 << 3)
#define EPITCR_PRESC(x)			(((x) & 0xfff) << 4)
#define EPITCR_SWR			(1 << 16)
#define EPITCR_IOVW			(1 << 17)
#define EPITCR_DBGEN			(1 << 18)
#define EPITCR_WAITEN			(1 << 19)
#define EPITCR_RES			(1 << 20)
#define EPITCR_STOPEN			(1 << 21)
#define EPITCR_OM_DISCON		(0 << 22)
#define EPITCR_OM_TOGGLE		(1 << 22)
#define EPITCR_OM_CLEAR			(2 << 22)
#define EPITCR_OM_SET			(3 << 22)
#define EPITCR_CLKSRC_OFF		(0 << 24)
#define EPITCR_CLKSRC_PERIPHERAL	(1 << 24)
#define EPITCR_CLKSRC_REF_HIGH		(2 << 24)
#define EPITCR_CLKSRC_REF_LOW		(3 << 24)

#define EPITSR_OCIF			(1 << 0)

//#define ARANZ_DEBUG

#define init_MUTEX(sem)         sema_init(sem, 1)

#define V4L2_CID_DRIVER_BASE            (V4L2_CID_USER_BASE | 0x1001)
//#define V4L2_CID_TEST_PATTERN           (V4L2_CID_DRIVER_BASE + 0)
#define V4L2_CID_FOCUS_TRIGGER          (V4L2_CID_DRIVER_BASE + 1)

#define V4L2_MODE_NORMAL          0
#define V4L2_MODE_SEQ1            1
#define V4L2_MODE_SEQ2            2

static struct platform_device_id imx_v4l2_devtype[] = {
	{
		.name = "v4l2-capture-imx5",
		.driver_data = IMX5_V4L2,
	}, {
		.name = "v4l2-capture-imx6",
		.driver_data = IMX6_V4L2,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(platform, imx_v4l2_devtype);

static const struct of_device_id mxc_v4l2_dt_ids[] = {
	{
		.compatible = "fsl,imx6q-v4l2-capture",
		.data = &imx_v4l2_devtype[IMX6_V4L2],
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, mxc_v4l2_dt_ids);

static int video_nr = -1;

/*! This data is used for the output to the display. */
#define MXC_V4L2_CAPTURE_NUM_OUTPUTS	6
#define MXC_V4L2_CAPTURE_NUM_INPUTS	2
static struct v4l2_output mxc_capture_outputs[MXC_V4L2_CAPTURE_NUM_OUTPUTS] = {
	{
	 .index = 0,
	 .name = "DISP3 BG",
	 .type = V4L2_OUTPUT_TYPE_ANALOG,
	 .audioset = 0,
	 .modulator = 0,
	 .std = V4L2_STD_UNKNOWN,
	 },
	{
	 .index = 1,
	 .name = "DISP3 BG - DI1",
	 .type = V4L2_OUTPUT_TYPE_ANALOG,
	 .audioset = 0,
	 .modulator = 0,
	 .std = V4L2_STD_UNKNOWN,
	 },
	{
	 .index = 2,
	 .name = "DISP3 FG",
	 .type = V4L2_OUTPUT_TYPE_ANALOG,
	 .audioset = 0,
	 .modulator = 0,
	 .std = V4L2_STD_UNKNOWN,
	 },
	{
	 .index = 3,
	 .name = "DISP4 BG",
	 .type = V4L2_OUTPUT_TYPE_ANALOG,
	 .audioset = 0,
	 .modulator = 0,
	 .std = V4L2_STD_UNKNOWN,
	 },
	{
	 .index = 4,
	 .name = "DISP4 BG - DI1",
	 .type = V4L2_OUTPUT_TYPE_ANALOG,
	 .audioset = 0,
	 .modulator = 0,
	 .std = V4L2_STD_UNKNOWN,
	 },
	{
	 .index = 5,
	 .name = "DISP4 FG",
	 .type = V4L2_OUTPUT_TYPE_ANALOG,
	 .audioset = 0,
	 .modulator = 0,
	 .std = V4L2_STD_UNKNOWN,
	 },
};

static struct v4l2_input mxc_capture_inputs[MXC_V4L2_CAPTURE_NUM_INPUTS] = {
	{
	 .index = 0,
	 .name = "CSI IC MEM",
	 .type = V4L2_INPUT_TYPE_CAMERA,
	 .audioset = 0,
	 .tuner = 0,
	 .std = V4L2_STD_UNKNOWN,
	 .status = 0,
	 },
	{
	 .index = 1,
	 .name = "CSI MEM",
	 .type = V4L2_INPUT_TYPE_CAMERA,
	 .audioset = 0,
	 .tuner = 0,
	 .std = V4L2_STD_UNKNOWN,
	 .status = V4L2_IN_ST_NO_POWER,
	 },
};

/*! List of TV input video formats supported. The video formats is corresponding
 * to the v4l2_id in video_fmt_t.
 * Currently, only PAL and NTSC is supported. Needs to be expanded in the
 * future.
 */
typedef enum {
	TV_NTSC = 0,		/*!< Locked on (M) NTSC video signal. */
	TV_PAL,			/*!< (B, G, H, I, N)PAL video signal. */
	TV_NOT_LOCKED,		/*!< Not locked on a signal. */
} video_fmt_idx;

/*! Number of video standards supported (including 'not locked' signal). */
#define TV_STD_MAX		(TV_NOT_LOCKED + 1)

/*! Video format structure. */
typedef struct {
	int v4l2_id;		/*!< Video for linux ID. */
	char name[16];		/*!< Name (e.g., "NTSC", "PAL", etc.) */
	u16 raw_width;		/*!< Raw width. */
	u16 raw_height;		/*!< Raw height. */
	u16 active_width;	/*!< Active width. */
	u16 active_height;	/*!< Active height. */
	u16 active_top;		/*!< Active top. */
	u16 active_left;	/*!< Active left. */
} video_fmt_t;

/*!
 * Description of video formats supported.
 *
 *  PAL: raw=720x625, active=720x576.
 *  NTSC: raw=720x525, active=720x480.
 */
static video_fmt_t video_fmts[] = {
	{			/*! NTSC */
	 .v4l2_id = V4L2_STD_NTSC,
	 .name = "NTSC",
	 .raw_width = 720,		/* SENS_FRM_WIDTH */
	 .raw_height = 525,		/* SENS_FRM_HEIGHT */
	 .active_width = 720,		/* ACT_FRM_WIDTH */
	 .active_height = 480,		/* ACT_FRM_HEIGHT */
	 .active_top = 0,
	 .active_left = 0,
	 },
	{			/*! (B, G, H, I, N) PAL */
	 .v4l2_id = V4L2_STD_PAL,
	 .name = "PAL",
	 .raw_width = 720,
	 .raw_height = 625,
	 .active_width = 720,
	 .active_height = 576,
	 .active_top = 0,
	 .active_left = 0,
	 },
	{			/*! Unlocked standard */
	 .v4l2_id = V4L2_STD_ALL,
	 .name = "Autodetect",
	 .raw_width = 720,
	 .raw_height = 625,
	 .active_width = 720,
	 .active_height = 576,
	 .active_top = 0,
	 .active_left = 0,
	 },
};

/*!* Standard index of TV. */
static video_fmt_idx video_index = TV_NOT_LOCKED;

static int mxc_v4l2_master_attach(struct v4l2_int_device *slave);
static void mxc_v4l2_master_detach(struct v4l2_int_device *slave);
static int start_preview(cam_data *cam);
static int stop_preview(cam_data *cam);
static void start_epit(cam_data *cam);
static void stop_epit(cam_data *cam);

/*! Information about this driver. */
static struct v4l2_int_master mxc_v4l2_master = {
	.attach = mxc_v4l2_master_attach,
	.detach = mxc_v4l2_master_detach,
};

/*****************************************************************************/
static void imx_v4l2_io3_toggle(cam_data *cam)
{
  if (cam->gpio_io3_state)
  {
		gpio_set_value(cam->gpio_io3, 0);
    cam->gpio_io3_state = 0;
  }
	else
  {
		gpio_set_value(cam->gpio_io3, 1);
    cam->gpio_io3_state = 1;
  }
}

/*****************************************************************************/
static void imx_v4l2_io4_toggle(cam_data *cam)
{
  if (cam->gpio_io4_state)
  {
		gpio_set_value(cam->gpio_io4, 0);
    cam->gpio_io4_state = 0;
  }
	else
  {
		gpio_set_value(cam->gpio_io4, 1);
    cam->gpio_io4_state = 1;
  }
}

static int setLaserFrameSettings(void *dev)
{
	struct v4l2_control c;
	cam_data *cam = (cam_data *) dev;
	if (cam == NULL)
  {
		return 1;
  }

	if(wait_event_interruptible(cam->seq_queue, cam->seq_counter != 0) == 0)
	{
		c.id = V4L2_CID_EXPOSURE;
		c.value = cam->seq_settings.laserExposure;
		vidioc_int_s_ctrl(cam->sensor, &c);

		//Set Frame 1 Gain.
		c.id = V4L2_CID_GAIN;
		c.value = cam->seq_settings.laserGain;
		vidioc_int_s_ctrl(cam->sensor, &c);
	}

	return 0;
}

/***************************************************************************
 * Functions for handling Frame buffers.
 **************************************************************************/

/*********************************************************************************************************************/
/*!
 * Free frame buffers
 *
 * @param cam      Structure cam_data *
 *
 * @return status  0 success.
 */
static int mxc_free_frame_buf(cam_data *cam)
{
	int i;

#ifdef ARANZ_DEBUG
	//pr_debug("%s\n", __func__);
  printk(KERN_ALERT "mxc_free_frame_buf.\n");
#endif

	for (i = 0; i < FRAME_NUM; i++) 
  {
		if (cam->frame[i].vaddress != 0) 
    {
			dma_free_coherent(0, cam->frame[i].buffer.length, cam->frame[i].vaddress, cam->frame[i].paddress);
      cam->frame[i].vaddress = 0;
		}
	}

	return 0;
}

static void mxc_free_frames(cam_data *cam);

/*********************************************************************************************************************/
/*!
 * Allocate frame buffers
 *
 * @param cam      Structure cam_data*
 * @param count    int number of buffer need to allocated
 *
 * @return status  -0 Successfully allocated a buffer, -ENOBUFS	failed.
 */
static int mxc_allocate_frame_buf(cam_data *cam, int count)
{
	int i;
	u32 map_sizeimage = 0;
	struct sensor_data *sensor = cam->sensor->priv;

  if (count > FRAME_NUM) 
  {
    return -ENOBUFS;
  }

	if (sensor && sensor->adata) 
  {
		const struct additional_data *adata = sensor->adata;
		map_sizeimage = adata->map_sizeimage;
	}
	else 
  {
		map_sizeimage = cam->v2f.fmt.pix.sizeimage;
	}

#ifdef ARANZ_DEBUG
  printk(KERN_ALERT "mxc_allocate_frame_buf: count: %d, size: %u\n", count, map_sizeimage);
#endif

  mxc_free_frames(cam); //Make sure the linked lists are reset.

  //Change this over to use 
  //struct dma_pool *	dma_pool_create(const char *name, struct device *dev, size_t size, size_t align, size_t alloc);
  //void * dma_pool_alloc(struct dma_pool *pool, gfp_t gfp_flags, dma_addr_t *dma_handle);
  //void dma_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t addr);
  //void dma_pool_destroy(struct dma_pool *pool);
  //No Need to use GFP_ATOMIC

	for (i = 0; i < count; i++) 
  {
		cam->frame[i].vaddress = dma_alloc_coherent(0, PAGE_ALIGN(map_sizeimage), &cam->frame[i].paddress, GFP_DMA | GFP_ATOMIC);
		if (cam->frame[i].vaddress == 0) 
    {
			pr_err("%s: failed.\n", __func__);
			mxc_free_frame_buf(cam);
			return -ENOBUFS;
		}
		cam->frame[i].buffer.index = i;
		cam->frame[i].buffer.flags = V4L2_BUF_FLAG_MAPPED;
		cam->frame[i].buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cam->frame[i].buffer.length = PAGE_ALIGN(map_sizeimage);
		cam->frame[i].buffer.memory = V4L2_MEMORY_MMAP;
		cam->frame[i].buffer.m.offset = cam->frame[i].paddress;
		cam->frame[i].index = i;

#ifdef ARANZ_DEBUG
    printk(KERN_ALERT "mxc_allocate_frame_buf: Index: %d, Address: %p : %u\n", i, cam->frame[i].vaddress, cam->frame[i].paddress);
#endif
	}

	if (cam->dummy_frame.vaddress == 0)
  {
    cam->dummy_frame.vaddress = dma_alloc_coherent(0, PAGE_ALIGN(SZ_16M), &cam->dummy_frame.paddress, GFP_DMA | GFP_ATOMIC);
    cam->dummy_frame.buffer.length = SZ_16M;

#ifdef ARANZ_DEBUG
    printk(KERN_ALERT "mxc_allocate_frame_buf: Dummy frame allocated\n");
#endif
  }
  else
  {
    printk(KERN_ALERT "mxc_allocate_frame_buf: Dummy frame allocation failed.\n");
  }

	return 0;
}

/*********************************************************************************************************************/
/*!
 * Free frame buffers status
 *
 * @param cam    Structure cam_data *
 *
 * @return none
 */
static void mxc_free_frames(cam_data *cam)
{
	int i;

#ifdef ARANZ_DEBUG
	//pr_debug("%s\n", __func__);
  printk(KERN_ALERT "mxc_free_frames.\n");
#endif

	for (i = 0; i < FRAME_NUM; i++)
  {
		cam->frame[i].buffer.flags = V4L2_BUF_FLAG_MAPPED;
  }

	cam->enc_counter = 0;
	INIT_LIST_HEAD(&cam->ready_q);
	INIT_LIST_HEAD(&cam->working_q);
	INIT_LIST_HEAD(&cam->done_q);
}

/*********************************************************************************************************************/
/*!
 * Return the buffer status
 *
 * @param cam	   Structure cam_data *
 * @param buf	   Structure v4l2_buffer *
 *
 * @return status  0 success, EINVAL failed.
 */
static int mxc_v4l2_buffer_status(cam_data *cam, struct v4l2_buffer *buf)
{
	pr_debug("%s\n", __func__);

	if (buf->index < 0 || buf->index >= FRAME_NUM) {
		pr_err("ERROR: v4l2 capture: mxc_v4l2_buffer_status buffers "
		       "not allocated\n");
		return -EINVAL;
	}

	memcpy(buf, &(cam->frame[buf->index].buffer), sizeof(*buf));
	return 0;
}

/*********************************************************************************************************************/
static int mxc_v4l2_release_bufs(cam_data *cam)
{
	pr_debug("%s\n", __func__);
	return 0;
}

/*********************************************************************************************************************/
static int mxc_v4l2_prepare_bufs(cam_data *cam, struct v4l2_buffer *buf)
{
#ifdef ARANZ_DEBUG
	//pr_debug("%s\n", __func__);
  printk(KERN_ALERT "mxc_v4l2_prepare_buf: %d\n", buf->index);
#endif

	if (buf->index < 0 || buf->index >= FRAME_NUM || buf->length < cam->v2f.fmt.pix.sizeimage) 
  {
		pr_err("ERROR: v4l2 capture: mxc_v4l2_prepare_bufs buffers not allocated,index=%d, length=%d\n", buf->index, buf->length);
		return -EINVAL;
	}

	cam->frame[buf->index].buffer.index = buf->index;
	cam->frame[buf->index].buffer.flags = V4L2_BUF_FLAG_MAPPED;
	cam->frame[buf->index].buffer.length = buf->length;
	cam->frame[buf->index].buffer.m.offset = cam->frame[buf->index].paddress = buf->m.offset;
	cam->frame[buf->index].buffer.type = buf->type;
	cam->frame[buf->index].buffer.memory = V4L2_MEMORY_USERPTR;
	cam->frame[buf->index].index = buf->index;

	return 0;
}

/***************************************************************************
 * Functions for handling the video stream.
 **************************************************************************/

/*********************************************************************************************************************/
/*!
 * Indicates whether the palette is supported.
 *
 * @param palette V4L2_PIX_FMT_RGB565, V4L2_PIX_FMT_BGR24 or V4L2_PIX_FMT_BGR32
 *
 * @return 0 if failed
 */
static inline int valid_mode(u32 palette)
{
	return ((palette == V4L2_PIX_FMT_RGB565) ||
		(palette == V4L2_PIX_FMT_BGR24) ||
		(palette == V4L2_PIX_FMT_RGB24) ||
		(palette == V4L2_PIX_FMT_BGR32) ||
		(palette == V4L2_PIX_FMT_RGB32) ||
		(palette == V4L2_PIX_FMT_YUV422P) ||
		(palette == V4L2_PIX_FMT_UYVY) ||
		(palette == V4L2_PIX_FMT_YUYV) ||
		(palette == V4L2_PIX_FMT_YUV420) ||
		(palette == V4L2_PIX_FMT_YVU420) ||
		(palette == V4L2_PIX_FMT_NV12 ||
		 palette == V4L2_PIX_FMT_SBGGR8));
}

/*********************************************************************************************************************/
/*!
 * Start the encoder job
 *
 * @param cam      structure cam_data *
 *
 * @return status  0 Success
 */
static int mxc_streamon(cam_data *cam)
{
	struct mxc_v4l_frame *frame;
	unsigned long lock_flags;
	int err = 0;

#ifdef ARANZ_DEBUG
  printk(KERN_ALERT "mxc_streamon: ipu%d/csi%d capture_on=%d %s\n", cam->ipu_id, cam->csi, cam->capture_on, mxc_capture_inputs[cam->current_input].name);
	//pr_debug("%s\n", __func__);
#endif

	if (NULL == cam) 
  {
		pr_err("ERROR! cam parameter is NULL\n");
		return -1;
	}

	if (cam->capture_on) 
  {
		pr_err("ERROR: v4l2 capture: Capture stream has been turned on\n");
		return -1;
	}

	if (list_empty(&cam->ready_q)) 
  {
		pr_err("ERROR: v4l2 capture: mxc_streamon buffer has not been queued yet\n");
		return -EINVAL;
	}

	if (cam->enc_update_eba && cam->ready_q.prev == cam->ready_q.next) 
  {
		pr_err("ERROR: v4l2 capture: mxc_streamon buffer need ping pong at least two buffers\n");
		return -EINVAL;
	}

	cam->capture_pid = current->pid;

	if (cam->overlay_on == true)
  {
		stop_preview(cam);
  }

	if (cam->enc_enable) 
  {
		err = cam->enc_enable(cam);
		if (err != 0)
    {
			return err;
    }
	}

	spin_lock_irqsave(&cam->queue_int_lock, lock_flags);
	cam->ping_pong_csi = 0;
	cam->local_buf_num = 0;
	if (cam->enc_update_eba) 
  {
		frame = list_entry(cam->ready_q.next, struct mxc_v4l_frame, queue);
    list_del(cam->ready_q.next);
		list_add_tail(&frame->queue, &cam->working_q);
		frame->ipu_buf_num = cam->ping_pong_csi;
		err = cam->enc_update_eba(cam->ipu, frame->buffer.m.offset, &cam->ping_pong_csi);
#ifdef ARANZ_DEBUG
    printk(KERN_ALERT "v4l2 capture: mxc_streamon: ENC Frame Queued: %d, PingPong: %d\n", frame->buffer.index, frame->ipu_buf_num);
#endif

		frame = list_entry(cam->ready_q.next, struct mxc_v4l_frame, queue);
		list_del(cam->ready_q.next);
		list_add_tail(&frame->queue, &cam->working_q);
		frame->ipu_buf_num = cam->ping_pong_csi;
    err |= cam->enc_update_eba(cam->ipu, frame->buffer.m.offset, &cam->ping_pong_csi);
#ifdef ARANZ_DEBUG
    printk(KERN_ALERT "v4l2 capture: mxc_streamon: ENC Frame Queued: %d, PingPong: %d\n", frame->buffer.index, frame->ipu_buf_num);
#endif

		spin_unlock_irqrestore(&cam->queue_int_lock, lock_flags);
	} 
  else 
  {
		spin_unlock_irqrestore(&cam->queue_int_lock, lock_flags);
		return -EINVAL;
	}

	if (cam->overlay_on == true)
  {
		start_preview(cam);
  }

	if (cam->enc_enable_csi) 
  {
		err = cam->enc_enable_csi(cam);
		if (err != 0)
    {
			return err;
    }
	}

	cam->capture_on = true;

  imx_v4l2_io3_toggle(cam);

	return err;
}

/*********************************************************************************************************************/
/*!
 * Shut down the encoder job
 *
 * @param cam      structure cam_data *
 *
 * @return status  0 Success
 */
static int mxc_streamoff(cam_data *cam)
{
	int err = 0;

#ifdef ARANZ_DEBUG
	//pr_debug("%s: ipu%d/csi%d capture_on=%d %s\n", __func__, cam->ipu_id, cam->csi, cam->capture_on, mxc_capture_inputs[cam->current_input].name);
  printk(KERN_ALERT "mxc_streamoff: ipu%d/csi%d capture_on=%d %s\n", cam->ipu_id, cam->csi, cam->capture_on, mxc_capture_inputs[cam->current_input].name);
#endif

	if (cam->capture_on == false)
  {
		return 0;
  }
	/* For both CSI--MEM and CSI--IC--MEM
	 * 1. wait for idmac eof
	 * 2. disable csi first
	 * 3. disable idmac
	 * 4. disable smfc (CSI--MEM channel)
	 */
	if (mxc_capture_inputs[cam->current_input].name != NULL) {
		if (cam->enc_disable_csi) {
			err = cam->enc_disable_csi(cam);
			if (err != 0)
				return err;
		}
		if (cam->enc_disable) {
			err = cam->enc_disable(cam);
			if (err != 0)
				return err;
		}
	}

  stop_epit(cam);

	mxc_free_frames(cam);
	mxc_capture_inputs[cam->current_input].status |= V4L2_IN_ST_NO_POWER;
	cam->capture_on = false;

  imx_v4l2_io3_toggle(cam);

	return err;
}

/*********************************************************************************************************************/
/*!
 * Valid and adjust the overlay window size, position
 *
 * @param cam      structure cam_data *
 * @param win      struct v4l2_window  *
 *
 * @return 0
 */
static int verify_preview(cam_data *cam, struct v4l2_window *win)
{
	int i = 0, width_bound = 0, height_bound = 0;
	int *width, *height;
	unsigned int ipu_ch = CHAN_NONE;
	struct fb_info *bg_fbi = NULL, *fbi = NULL;
	bool foregound_fb = false;
	mm_segment_t old_fs;

	pr_debug("%s\n", __func__);

	do {
		fbi = (struct fb_info *)registered_fb[i];
		if (fbi == NULL) {
			pr_err("ERROR: verify_preview frame buffer NULL.\n");
			return -1;
		}

		/* Which DI supports 2 layers? */
		if (((strncmp(fbi->fix.id, "DISP3 BG", 8) == 0) &&
					(cam->output < 3)) ||
		    ((strncmp(fbi->fix.id, "DISP4 BG", 8) == 0) &&
					(cam->output >= 3))) {
			if (fbi->fbops->fb_ioctl) {
				old_fs = get_fs();
				set_fs(KERNEL_DS);
				fbi->fbops->fb_ioctl(fbi, MXCFB_GET_FB_IPU_CHAN,
						(unsigned long)&ipu_ch);
				set_fs(old_fs);
			}
			if (ipu_ch == MEM_BG_SYNC) {
				bg_fbi = fbi;
				pr_debug("Found background frame buffer.\n");
			}
		}

		/* Found the frame buffer to preview on. */
		if (strcmp(fbi->fix.id,
			    mxc_capture_outputs[cam->output].name) == 0) {
			if (((strcmp(fbi->fix.id, "DISP3 FG") == 0) &&
						(cam->output < 3)) ||
			    ((strcmp(fbi->fix.id, "DISP4 FG") == 0) &&
						(cam->output >= 3)))
				foregound_fb = true;

			cam->overlay_fb = fbi;
			break;
		}
	} while (++i < FB_MAX);

	if (foregound_fb) {
		width_bound = bg_fbi->var.xres;
		height_bound = bg_fbi->var.yres;

		if (win->w.width + win->w.left > bg_fbi->var.xres ||
		    win->w.height + win->w.top > bg_fbi->var.yres) {
			pr_err("ERROR: FG window position exceeds.\n");
			return -1;
		}
	} else {
		/* 4 bytes alignment for BG */
		width_bound = cam->overlay_fb->var.xres;
		height_bound = cam->overlay_fb->var.yres;

		if (cam->overlay_fb->var.bits_per_pixel == 24)
			win->w.left -= win->w.left % 4;
		else if (cam->overlay_fb->var.bits_per_pixel == 16)
			win->w.left -= win->w.left % 2;

		if (win->w.width + win->w.left > cam->overlay_fb->var.xres)
			win->w.width = cam->overlay_fb->var.xres - win->w.left;
		if (win->w.height + win->w.top > cam->overlay_fb->var.yres)
			win->w.height = cam->overlay_fb->var.yres - win->w.top;
	}

	/* stride line limitation */
	win->w.height -= win->w.height % 8;
	win->w.width -= win->w.width % 8;

	if (cam->rotation >= IPU_ROTATE_90_RIGHT) {
		height = &win->w.width;
		width = &win->w.height;
	} else {
		width = &win->w.width;
		height = &win->w.height;
	}

	if (*width == 0 || *height == 0) {
		pr_err("ERROR: v4l2 capture: width or height"
			" too small.\n");
		return -EINVAL;
	}

	if ((cam->crop_bounds.width / *width > 8) ||
	    ((cam->crop_bounds.width / *width == 8) &&
	     (cam->crop_bounds.width % *width))) {
		*width = cam->crop_bounds.width / 8;
		if (*width % 8)
			*width += 8 - *width % 8;
		if (*width + win->w.left > width_bound) {
			pr_err("ERROR: v4l2 capture: width exceeds "
				"resize limit.\n");
			return -1;
		}
		pr_err("ERROR: v4l2 capture: width exceeds limit. "
			"Resize to %d.\n",
			*width);
	}

	if ((cam->crop_bounds.height / *height > 8) ||
	    ((cam->crop_bounds.height / *height == 8) &&
	     (cam->crop_bounds.height % *height))) {
		*height = cam->crop_bounds.height / 8;
		if (*height % 8)
			*height += 8 - *height % 8;
		if (*height + win->w.top > height_bound) {
			pr_err("ERROR: v4l2 capture: height exceeds "
				"resize limit.\n");
			return -1;
		}
		pr_err("ERROR: v4l2 capture: height exceeds limit "
			"resize to %d.\n",
			*height);
	}

	return 0;
}

/*********************************************************************************************************************/
/*!
 * start the viewfinder job
 *
 * @param cam      structure cam_data *
 *
 * @return status  0 Success
 */
static int start_preview(cam_data *cam)
{
	int err = 0;

	pr_debug("MVC: start_preview\n");

	if (cam->v4l2_fb.flags == V4L2_FBUF_FLAG_OVERLAY)
	#ifdef CONFIG_MXC_IPU_PRP_VF_SDC
		err = prp_vf_sdc_select(cam);
	#else
		err = foreground_sdc_select(cam);
	#endif
	else if (cam->v4l2_fb.flags == V4L2_FBUF_FLAG_PRIMARY)
	#ifdef CONFIG_MXC_IPU_PRP_VF_SDC
		err = prp_vf_sdc_select_bg(cam);
	#else
		err = bg_overlay_sdc_select(cam);
	#endif
	if (err != 0)
		return err;

	if (cam->vf_start_sdc) {
		err = cam->vf_start_sdc(cam);
		if (err != 0)
			return err;
	}

	if (cam->vf_enable_csi)
		err = cam->vf_enable_csi(cam);

	pr_debug("End of %s: v2f pix widthxheight %d x %d\n",
		 __func__,
		 cam->v2f.fmt.pix.width, cam->v2f.fmt.pix.height);
	pr_debug("End of %s: crop_bounds widthxheight %d x %d\n",
		 __func__,
		 cam->crop_bounds.width, cam->crop_bounds.height);
	pr_debug("End of %s: crop_defrect widthxheight %d x %d\n",
		 __func__,
		 cam->crop_defrect.width, cam->crop_defrect.height);
	pr_debug("End of %s: crop_current widthxheight %d x %d\n",
		 __func__,
		 cam->crop_current.width, cam->crop_current.height);

	return err;
}

/*********************************************************************************************************************/
/*!
 * shut down the viewfinder job
 *
 * @param cam      structure cam_data *
 *
 * @return status  0 Success
 */
static int stop_preview(cam_data *cam)
{
	int err = 0;

	if (cam->vf_disable_csi) {
		err = cam->vf_disable_csi(cam);
		if (err != 0)
			return err;
	}

	if (cam->vf_stop_sdc) {
		err = cam->vf_stop_sdc(cam);
		if (err != 0)
			return err;
	}

	if (cam->v4l2_fb.flags == V4L2_FBUF_FLAG_OVERLAY)
	#ifdef CONFIG_MXC_IPU_PRP_VF_SDC
		err = prp_vf_sdc_deselect(cam);
	#else
		err = foreground_sdc_deselect(cam);
	#endif
	else if (cam->v4l2_fb.flags == V4L2_FBUF_FLAG_PRIMARY)
	#ifdef CONFIG_MXC_IPU_PRP_VF_SDC
		err = prp_vf_sdc_deselect_bg(cam);
	#else
		err = bg_overlay_sdc_deselect(cam);
	#endif

	return err;
}

/***************************************************************************
 * VIDIOC Functions.
 **************************************************************************/

/*********************************************************************************************************************/
/*!
 * V4L2 - mxc_v4l2_g_fmt function
 *
 * @param cam         structure cam_data *
 *
 * @param f           structure v4l2_format *
 *
 * @return  status    0 success, EINVAL failed
 */
static int mxc_v4l2_g_fmt(cam_data *cam, struct v4l2_format *f)
{
	int retval = 0;

	pr_debug("%s: type=%d\n", __func__, f->type);

	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		pr_debug("   type is V4L2_BUF_TYPE_VIDEO_CAPTURE\n");
		f->fmt.pix = cam->v2f.fmt.pix;
		break;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		pr_debug("   type is V4L2_BUF_TYPE_VIDEO_OVERLAY\n");
		f->fmt.win = cam->win;
		break;
	default:
		pr_debug("   type is invalid\n");
		retval = -EINVAL;
	}

	pr_debug("End of %s: v2f pix widthxheight %d x %d\n",
		 __func__,
		 cam->v2f.fmt.pix.width, cam->v2f.fmt.pix.height);
	pr_debug("End of %s: crop_bounds widthxheight %d x %d\n",
		 __func__,
		 cam->crop_bounds.width, cam->crop_bounds.height);
	pr_debug("End of %s: crop_defrect widthxheight %d x %d\n",
		 __func__,
		 cam->crop_defrect.width, cam->crop_defrect.height);
	pr_debug("End of %s: crop_current widthxheight %d x %d\n",
		 __func__,
		 cam->crop_current.width, cam->crop_current.height);

	return retval;
}

/*********************************************************************************************************************/
/*!
 * V4L2 - mxc_v4l2_s_fmt function
 *
 * @param cam         structure cam_data *
 *
 * @param f           structure v4l2_format *
 *
 * @return  status    0 success, EINVAL failed
 */
static int mxc_v4l2_s_fmt(cam_data *cam, struct v4l2_format *f, bool try_fmt)
{
	int retval = 0;
	int size = 0;
	int bytesperline = 0;
	int *width, *height;

	pr_debug("%s\n", __func__);

	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		pr_debug("   type=V4L2_BUF_TYPE_VIDEO_CAPTURE\n");
		if (!valid_mode(f->fmt.pix.pixelformat)) {
			pr_err("ERROR: v4l2 capture: mxc_v4l2_s_fmt: format "
			       "not supported\n");
			return -EINVAL;
		}

		/*
		 * Force the capture window resolution to be crop bounds
		 * for CSI MEM input mode.
		 */
		if (strcmp(mxc_capture_inputs[cam->current_input].name,
			   "CSI MEM") == 0) {
			f->fmt.pix.width = cam->crop_current.width;
			f->fmt.pix.height = cam->crop_current.height;
		}

		if (cam->rotation >= IPU_ROTATE_90_RIGHT) {
			height = &f->fmt.pix.width;
			width = &f->fmt.pix.height;
		} else {
			width = &f->fmt.pix.width;
			height = &f->fmt.pix.height;
		}

		/* stride line limitation */
		*width -= *width % 8;
		*height -= *height % 8;

		if (*width == 0 || *height == 0) {
			pr_err("ERROR: v4l2 capture: width or height"
				" too small.\n");
			return -EINVAL;
		}

		if ((cam->crop_current.width / *width > 8) ||
		    ((cam->crop_current.width / *width == 8) &&
		     (cam->crop_current.width % *width))) {
			*width = cam->crop_current.width / 8;
			if (*width % 8)
				*width += 8 - *width % 8;
			pr_err("ERROR: v4l2 capture: width exceeds limit "
				"resize to %d.\n",
			       *width);
		}

		if ((cam->crop_current.height / *height > 8) ||
		    ((cam->crop_current.height / *height == 8) &&
		     (cam->crop_current.height % *height))) {
			*height = cam->crop_current.height / 8;
			if (*height % 8)
				*height += 8 - *height % 8;
			pr_err("ERROR: v4l2 capture: height exceeds limit "
			       "resize to %d.\n",
			       *height);
		}

		switch (f->fmt.pix.pixelformat) {
		case V4L2_PIX_FMT_RGB565:
			size = f->fmt.pix.width * f->fmt.pix.height * 2;
			bytesperline = f->fmt.pix.width * 2;
			break;
		case V4L2_PIX_FMT_BGR24:
			size = f->fmt.pix.width * f->fmt.pix.height * 3;
			bytesperline = f->fmt.pix.width * 3;
			break;
		case V4L2_PIX_FMT_RGB24:
			size = f->fmt.pix.width * f->fmt.pix.height * 3;
			bytesperline = f->fmt.pix.width * 3;
			break;
		case V4L2_PIX_FMT_BGR32:
			size = f->fmt.pix.width * f->fmt.pix.height * 4;
			bytesperline = f->fmt.pix.width * 4;
			break;
		case V4L2_PIX_FMT_RGB32:
			size = f->fmt.pix.width * f->fmt.pix.height * 4;
			bytesperline = f->fmt.pix.width * 4;
			break;
		case V4L2_PIX_FMT_YUV422P:
			size = f->fmt.pix.width * f->fmt.pix.height * 2;
			bytesperline = f->fmt.pix.width;
			break;
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_YUYV:
			size = f->fmt.pix.width * f->fmt.pix.height * 2;
			bytesperline = f->fmt.pix.width * 2;
			break;
		case V4L2_PIX_FMT_YUV420:
		case V4L2_PIX_FMT_YVU420:
			size = f->fmt.pix.width * f->fmt.pix.height * 3 / 2;
			bytesperline = f->fmt.pix.width;
			break;
		case V4L2_PIX_FMT_NV12:
			size = f->fmt.pix.width * f->fmt.pix.height * 3 / 2;
			bytesperline = f->fmt.pix.width;
			break;
		case V4L2_PIX_FMT_SBGGR8:
			size = f->fmt.pix.width * f->fmt.pix.height;
			bytesperline = f->fmt.pix.width;
			break;
		default:
			break;
		}

		if (f->fmt.pix.bytesperline < bytesperline)
			f->fmt.pix.bytesperline = bytesperline;
		else
			bytesperline = f->fmt.pix.bytesperline;

		if (try_fmt) {
			/* XXX: workaround for gstreamer */
			if (f->fmt.pix.sizeimage < size ||
					f->fmt.pix.sizeimage % size)
				f->fmt.pix.sizeimage = size;
			else
				size = f->fmt.pix.sizeimage;

			break;
		}
		else {
			if (f->fmt.pix.sizeimage < size)
				f->fmt.pix.sizeimage = size;
			else
				size = f->fmt.pix.sizeimage;
		}

		cam->v2f.fmt.pix = f->fmt.pix;

		if (cam->v2f.fmt.pix.priv != 0) {
			if (copy_from_user(&cam->offset,
					   (void *)cam->v2f.fmt.pix.priv,
					   sizeof(cam->offset))) {
				retval = -EFAULT;
				break;
			}
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		pr_debug("   type=V4L2_BUF_TYPE_VIDEO_OVERLAY\n");
		retval = verify_preview(cam, &f->fmt.win);
		if (!try_fmt)
			cam->win = f->fmt.win;
		break;
	default:
		retval = -EINVAL;
	}

	pr_debug("End of %s: v2f pix widthxheight %d x %d\n",
		 __func__,
		 cam->v2f.fmt.pix.width, cam->v2f.fmt.pix.height);
	pr_debug("End of %s: crop_bounds widthxheight %d x %d\n",
		 __func__,
		 cam->crop_bounds.width, cam->crop_bounds.height);
	pr_debug("End of %s: crop_defrect widthxheight %d x %d\n",
		 __func__,
		 cam->crop_defrect.width, cam->crop_defrect.height);
	pr_debug("End of %s: crop_current widthxheight %d x %d\n",
		 __func__,
		 cam->crop_current.width, cam->crop_current.height);

	return retval;
}

/*********************************************************************************************************************/
static int mxc_v4l2_query_ctrl(cam_data *cam, struct v4l2_queryctrl *qc)
{
        int status = 0;

        pr_debug("In MVC:mxc_v4l2_query_ctrl\n");
        if (cam->sensor) {
                status = vidioc_int_queryctrl(cam->sensor, qc);
        } else {
                pr_err("ERROR: v4l2 capture: slave not found!\n");
                status = -ENODEV;
        }
        return status;
}

/*********************************************************************************************************************/
/*!
 * get control param
 *
 * @param cam         structure cam_data *
 *
 * @param c           structure v4l2_control *
 *
 * @return  status    0 success, EINVAL failed
 */
static int mxc_v4l2_g_ctrl(cam_data *cam, struct v4l2_control *c)
{
	int status = 0;

	pr_debug("%s\n", __func__);

	/* probably don't need to store the values that can be retrieved,
	 * locally, but they are for now. */
	switch (c->id) {
//	case V4L2_CID_HFLIP:
//		/* This is handled in the ipu. */
//		if (cam->rotation == IPU_ROTATE_HORIZ_FLIP)
//			c->value = 1;
//		break;
//	case V4L2_CID_VFLIP:
//		/* This is handled in the ipu. */
//		if (cam->rotation == IPU_ROTATE_VERT_FLIP)
//			c->value = 1;
//		break;
	case V4L2_CID_MXC_ROT:
		/* This is handled in the ipu. */
		c->value = cam->rotation;
		break;
	case V4L2_CID_BRIGHTNESS:
		if (cam->sensor) {
			c->value = cam->bright;
			status = vidioc_int_g_ctrl(cam->sensor, c);
			cam->bright = c->value;
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			status = -ENODEV;
		}
		break;
	case V4L2_CID_HUE:
		if (cam->sensor) {
			c->value = cam->hue;
			status = vidioc_int_g_ctrl(cam->sensor, c);
			cam->hue = c->value;
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			status = -ENODEV;
		}
		break;
	case V4L2_CID_CONTRAST:
		if (cam->sensor) {
			c->value = cam->contrast;
			status = vidioc_int_g_ctrl(cam->sensor, c);
			cam->contrast = c->value;
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			status = -ENODEV;
		}
		break;
	case V4L2_CID_SATURATION:
		if (cam->sensor) {
			c->value = cam->saturation;
			status = vidioc_int_g_ctrl(cam->sensor, c);
			cam->saturation = c->value;
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			status = -ENODEV;
		}
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		if (cam->sensor) {
			c->value = cam->aw_mode;
			status = vidioc_int_g_ctrl(cam->sensor, c);
			cam->aw_mode = c->value;
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			status = -ENODEV;
		}
		break;
	case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
		if (cam->sensor) {
			c->value = cam->wb_temp;
			status = vidioc_int_g_ctrl(cam->sensor, c);
			cam->wb_temp = c->value;
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			status = -ENODEV;
		}
		break;
	case V4L2_CID_COLORFX:
		if (cam->sensor) {
			c->value = cam->effects;
			status = vidioc_int_g_ctrl(cam->sensor, c);
			cam->effects = c->value;
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			status = -ENODEV;
			cam->focus_step = c->value;
		}
		break;
	case V4L2_CID_TEST_PATTERN:
		if (cam->sensor) {
			c->value = cam->pattern;
			status = vidioc_int_g_ctrl(cam->sensor, c);
			cam->pattern = c->value;
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			status = -ENODEV;
		}
		break;
	case V4L2_CID_FOCUS_ABSOLUTE:
		if (cam->sensor) {
			c->value = cam->focus_step;
			status = vidioc_int_g_ctrl(cam->sensor, c);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			status = -ENODEV;
		}
		break;
	case V4L2_CID_FOCUS_TRIGGER:
			if (cam->sensor) {
			status = vidioc_int_g_ctrl(cam->sensor, c);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			status = -ENODEV;
		}
		break;
	case V4L2_CID_RED_BALANCE:
		if (cam->sensor) {
			c->value = cam->red;
			status = vidioc_int_g_ctrl(cam->sensor, c);
			cam->red = c->value;
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			status = -ENODEV;
		}
		break;
	case V4L2_CID_BLUE_BALANCE:
		if (cam->sensor) {
			c->value = cam->blue;
			status = vidioc_int_g_ctrl(cam->sensor, c);
			cam->blue = c->value;
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			status = -ENODEV;
		}
		break;
	case V4L2_CID_BLACK_LEVEL:
		if (cam->sensor) {
			c->value = cam->ae_mode;
			status = vidioc_int_g_ctrl(cam->sensor, c);
			cam->ae_mode = c->value;
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			status = -ENODEV;
		}
		break;
	default:
		// ARANZ - Forward any other controls to the image sensor
		if (cam->sensor) {
			status = vidioc_int_g_ctrl(cam->sensor, c);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			status = -ENODEV;
		}
		//pr_err("ERROR: v4l2 capture: unsupported ioctrl!\n");
	}

	return status;
}

/*********************************************************************************************************************/
static int mxc_v4l2_send_command(cam_data *cam, struct v4l2_send_command_control *c) 
{
	int ret = 0;

	if (vidioc_int_send_command(cam->sensor, c)) {
		ret = -EINVAL;
	}
	return ret;
}

/*********************************************************************************************************************/
/*!
 * V4L2 - set_control function
 *          V4L2_CID_PRIVATE_BASE is the extention for IPU preprocessing.
 *          0 for normal operation
 *          1 for vertical flip
 *          2 for horizontal flip
 *          3 for horizontal and vertical flip
 *          4 for 90 degree rotation
 * @param cam         structure cam_data *
 *
 * @param c           structure v4l2_control *
 *
 * @return  status    0 success, EINVAL failed
 */
static int mxc_v4l2_s_ctrl(cam_data *cam, struct v4l2_control *c)
{
	int i, ret = 0;
	int tmp_rotation = IPU_ROTATE_NONE;
	struct sensor_data *sensor_data;

	pr_debug("%s\n", __func__);

	switch (c->id) 
  {
	case V4L2_CID_MXC_ROT:
	case V4L2_CID_MXC_VF_ROT:
		/* This is done by the IPU */
		switch (c->value) {
		case V4L2_MXC_ROTATE_NONE:
			tmp_rotation = IPU_ROTATE_NONE;
			break;
		case V4L2_MXC_ROTATE_VERT_FLIP:
			tmp_rotation = IPU_ROTATE_VERT_FLIP;
			break;
		case V4L2_MXC_ROTATE_HORIZ_FLIP:
			tmp_rotation = IPU_ROTATE_HORIZ_FLIP;
			break;
		case V4L2_MXC_ROTATE_180:
			tmp_rotation = IPU_ROTATE_180;
			break;
		case V4L2_MXC_ROTATE_90_RIGHT:
			tmp_rotation = IPU_ROTATE_90_RIGHT;
			break;
		case V4L2_MXC_ROTATE_90_RIGHT_VFLIP:
			tmp_rotation = IPU_ROTATE_90_RIGHT_VFLIP;
			break;
		case V4L2_MXC_ROTATE_90_RIGHT_HFLIP:
			tmp_rotation = IPU_ROTATE_90_RIGHT_HFLIP;
			break;
		case V4L2_MXC_ROTATE_90_LEFT:
			tmp_rotation = IPU_ROTATE_90_LEFT;
			break;
		case V4L2_MXC_CAM_ROTATE_NONE:
			if (vidioc_int_s_ctrl(cam->sensor, c)) {
				ret = -EINVAL;
			}
			break;
		case V4L2_MXC_CAM_ROTATE_VERT_FLIP:
			if (vidioc_int_s_ctrl(cam->sensor, c)) {
				ret = -EINVAL;
			}
			break;
		case V4L2_MXC_CAM_ROTATE_HORIZ_FLIP:
			if (vidioc_int_s_ctrl(cam->sensor, c)) {
				ret = -EINVAL;
			}
			break;
		case V4L2_MXC_CAM_ROTATE_180:
			if (vidioc_int_s_ctrl(cam->sensor, c)) {
				ret = -EINVAL;
			}
			break;
		default:
			ret = -EINVAL;
		}
		#ifdef CONFIG_MXC_IPU_PRP_VF_SDC
		if (c->id == V4L2_CID_MXC_VF_ROT)
			cam->vf_rotation = tmp_rotation;
		else
			cam->rotation = tmp_rotation;
		#else
			cam->rotation = tmp_rotation;
		#endif

		break;
	case V4L2_CID_HUE:
		if (cam->sensor) {
			cam->hue = c->value;
			ret = vidioc_int_s_ctrl(cam->sensor, c);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			ret = -ENODEV;
		}
		break;
	case V4L2_CID_CONTRAST:
		if (cam->sensor) {
			cam->contrast = c->value;
			ret = vidioc_int_s_ctrl(cam->sensor, c);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			ret = -ENODEV;
		}
		break;
	case V4L2_CID_BRIGHTNESS:
		if (cam->sensor) {
			cam->bright = c->value;
			ret = vidioc_int_s_ctrl(cam->sensor, c);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			ret = -ENODEV;
		}
		break;
	case V4L2_CID_SATURATION:
		if (cam->sensor) {
			cam->saturation = c->value;
			ret = vidioc_int_s_ctrl(cam->sensor, c);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			ret = -ENODEV;
		}
		break;
	case V4L2_CID_RED_BALANCE:
		if (cam->sensor) {
			cam->red = c->value;
			ret = vidioc_int_s_ctrl(cam->sensor, c);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			ret = -ENODEV;
		}
		break;
	case V4L2_CID_BLUE_BALANCE:
		if (cam->sensor) {
			cam->blue = c->value;
			ret = vidioc_int_s_ctrl(cam->sensor, c);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			ret = -ENODEV;
		}
		break;
	case V4L2_CID_TEST_PATTERN:
		if (cam->sensor) {
			cam->pattern = c->value;
			ret = vidioc_int_s_ctrl(cam->sensor, c);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			ret = -ENODEV;
		}
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		if (cam->sensor) {
			cam->aw_mode = c->value;
			ret = vidioc_int_s_ctrl(cam->sensor, c);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			ret = -ENODEV;
		}
		break;
	case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
		if (cam->sensor) {
			cam->wb_temp = c->value;
			ret = vidioc_int_s_ctrl(cam->sensor, c);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			ret = -ENODEV;
		}
		break;
	case V4L2_CID_COLORFX:
		if (cam->sensor) {
			cam->effects = c->value;
			ret = vidioc_int_s_ctrl(cam->sensor, c);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			ret = -ENODEV;
		}
		break;
	case V4L2_CID_FOCUS_ABSOLUTE:
		if (cam->sensor) {
			cam->focus_step = c->value;
			ret = vidioc_int_s_ctrl(cam->sensor, c);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			ret = -ENODEV;
		}
		break;
	case V4L2_CID_FOCUS_TRIGGER:
		if (cam->sensor) {
			ret = vidioc_int_s_ctrl(cam->sensor, c);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			ret = -ENODEV;
		}
		break;
	case V4L2_CID_EXPOSURE:
		if (cam->sensor) {
			cam->ae_mode = c->value;
			ret = vidioc_int_s_ctrl(cam->sensor, c);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			ret = -ENODEV;
		}
		break;
	case V4L2_CID_MXC_FLASH:
#ifdef CONFIG_MXC_IPU_V1
		ipu_csi_flash_strobe(true);
#endif
		break;

	case V4L2_CID_AUTO_FOCUS_START: {
		ret = vidioc_int_s_ctrl(cam->sensor, c);
		break;
	}

	case V4L2_CID_AUTO_FOCUS_STOP: {
		if (vidioc_int_s_ctrl(cam->sensor, c)) {
			ret = -EINVAL;
		}
		break;
	}

	case V4L2_CID_MXC_SWITCH_CAM:
		if (cam->sensor == cam->all_sensors[c->value])
			break;

		/* power down other cameraes before enable new one */
		for (i = 0; i < cam->sensor_index; i++) {
			if (i != c->value) {
				vidioc_int_dev_exit(cam->all_sensors[i]);
				vidioc_int_s_power(cam->all_sensors[i], 0);
				if (cam->mclk_on[cam->mclk_source]) {
					ipu_csi_enable_mclk_if(cam->ipu,
							CSI_MCLK_I2C,
							cam->mclk_source,
							false, false);
					cam->mclk_on[cam->mclk_source] =
								false;
				}
			}
		}
		sensor_data = cam->all_sensors[c->value]->priv;
		if (sensor_data->io_init)
			sensor_data->io_init();
		cam->sensor = cam->all_sensors[c->value];
		cam->mclk_source = sensor_data->mclk_source;
		ipu_csi_enable_mclk_if(cam->ipu, CSI_MCLK_I2C,
				       cam->mclk_source, true, true);
		cam->mclk_on[cam->mclk_source] = true;
		vidioc_int_s_power(cam->sensor, 1);
		vidioc_int_dev_init(cam->sensor);
		break;
	default:
		// ARANZ - Forward any other controls to the image sensor
        if (cam->sensor) {
        	ret = vidioc_int_s_ctrl(cam->sensor, c);
        } else {
        	pr_err("ERROR: v4l2 capture: slave not found!\n");
        	ret = -ENODEV;
        }
		//pr_debug("   default case\n");
		//ret = -EINVAL;
		break;
	}

	return ret;
}

/*********************************************************************************************************************/
void setup_ifparm(cam_data *cam, int init_defrect)
{
	struct v4l2_format cam_fmt;
	ipu_csi_signal_cfg_t csi_param;
	struct v4l2_ifparm ifparm;
	int swidth, sheight;
	int sleft, stop;

	vidioc_int_g_ifparm(cam->sensor, &ifparm);
	memset(&csi_param, 0, sizeof(csi_param));
	csi_param.csi = cam->csi;
	csi_param.mclk = ifparm.u.bt656.clock_curr;

	pr_debug("   clock_curr=mclk=%d\n", ifparm.u.bt656.clock_curr);
	switch (ifparm.if_type) {
	case V4L2_IF_TYPE_BT1120_PROGRESSIVE_DDR:
		csi_param.clk_mode = IPU_CSI_CLK_MODE_CCIR1120_PROGRESSIVE_DDR;
		break;
	case V4L2_IF_TYPE_BT1120_PROGRESSIVE_SDR:
		csi_param.clk_mode = IPU_CSI_CLK_MODE_CCIR1120_PROGRESSIVE_SDR;
		break;
	case V4L2_IF_TYPE_BT1120_INTERLACED_DDR:
		csi_param.clk_mode = IPU_CSI_CLK_MODE_CCIR1120_INTERLACED_DDR;
		break;
	case V4L2_IF_TYPE_BT1120_INTERLACED_SDR:
		csi_param.clk_mode = IPU_CSI_CLK_MODE_CCIR1120_INTERLACED_SDR;
		break;
	case V4L2_IF_TYPE_BT656_PROGRESSIVE:
		csi_param.clk_mode = IPU_CSI_CLK_MODE_CCIR656_PROGRESSIVE;
		break;
	case V4L2_IF_TYPE_BT656_INTERLACED:
		csi_param.clk_mode = IPU_CSI_CLK_MODE_CCIR656_INTERLACED;
		break;
	case V4L2_IF_TYPE_BT656:
//		csi_param.clk_mode = IPU_CSI_CLK_MODE_CCIR656_PROGRESSIVE;
//		break;
	default:
		if (ifparm.u.bt656.clock_curr == 0) {
			csi_param.clk_mode = IPU_CSI_CLK_MODE_CCIR656_INTERLACED;
			/*protocol bt656 use 27Mhz pixel clock */
			csi_param.mclk = 27000000;
		} else if (ifparm.u.bt656.clock_curr == 1) {
			csi_param.clk_mode = IPU_CSI_CLK_MODE_GATED_CLK;
		} else
			csi_param.clk_mode = IPU_CSI_CLK_MODE_GATED_CLK;
	}

	csi_param.pixclk_pol = ifparm.u.bt656.latch_clk_inv;

	csi_param.data_width =
		(ifparm.u.bt656.mode == V4L2_IF_TYPE_BT656_MODE_NOBT_10BIT) ||
		(ifparm.u.bt656.mode == V4L2_IF_TYPE_BT656_MODE_BT_10BIT) ?
		IPU_CSI_DATA_WIDTH_10 : IPU_CSI_DATA_WIDTH_8;

	csi_param.pack_tight = (csi_param.data_width == IPU_CSI_DATA_WIDTH_10) ? 1 : 0;

	csi_param.Vsync_pol = ifparm.u.bt656.nobt_vs_inv;
	csi_param.Hsync_pol = ifparm.u.bt656.nobt_hs_inv;
	csi_param.ext_vsync = ifparm.u.bt656.bt_sync_correct;
	pr_debug("vsync_pol(%d) hsync_pol(%d) ext_vsync(%d)\n", csi_param.Vsync_pol, csi_param.Hsync_pol, csi_param.ext_vsync);

	/* if the capturemode changed, the size bounds will have changed. */
	cam_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vidioc_int_g_fmt_cap(cam->sensor, &cam_fmt);
	pr_debug("   g_fmt_cap returns widthxheight of input as %d x %d\n",
			cam_fmt.fmt.pix.width, cam_fmt.fmt.pix.height);

	switch (cam_fmt.fmt.pix.pixelformat) {
	case V4L2_PIX_FMT_RGB565:
		csi_param.data_fmt = IPU_PIX_FMT_RGB565;
		break;
	case V4L2_PIX_FMT_BGR24:
		csi_param.data_fmt = IPU_PIX_FMT_BGR24;
		break;
	case V4L2_PIX_FMT_RGB24:
		csi_param.data_fmt = IPU_PIX_FMT_RGB24;
		break;
	case V4L2_PIX_FMT_BGR32:
		csi_param.data_fmt = IPU_PIX_FMT_BGR32;
		break;
	case V4L2_PIX_FMT_RGB32:
		csi_param.data_fmt = IPU_PIX_FMT_RGB32;
		break;
	case V4L2_PIX_FMT_YUV422P:
		csi_param.data_fmt = IPU_PIX_FMT_YUV422P;
		break;
	case V4L2_PIX_FMT_UYVY:
		csi_param.data_fmt = IPU_PIX_FMT_UYVY;
		break;
	case V4L2_PIX_FMT_YUYV:
		csi_param.data_fmt = IPU_PIX_FMT_YUYV;
		break;
	case V4L2_PIX_FMT_YUV420:
		csi_param.data_fmt = IPU_PIX_FMT_YUV420P;
		break;
	case V4L2_PIX_FMT_YVU420:
		csi_param.data_fmt = IPU_PIX_FMT_YVU420P;;
		break;
	case V4L2_PIX_FMT_NV12:
		csi_param.data_fmt = IPU_PIX_FMT_NV12;
		break;
	case V4L2_PIX_FMT_SBGGR8:
	default:
		csi_param.data_fmt = IPU_PIX_FMT_GENERIC;
		break;
	}

	cam->crop_bounds.top = cam->crop_bounds.left = 0;
	cam->crop_bounds.width = cam_fmt.fmt.pix.width;
	cam->crop_bounds.height = cam_fmt.fmt.pix.height;

	/*
	 * Set the default current cropped resolution to be the same with
	 * the cropping boundary(except for tvin module).
	 */
	if (cam->device_type != 1) {
		cam->crop_current.width = cam->crop_bounds.width;
		cam->crop_current.height = cam->crop_bounds.height;
	}

	if (init_defrect) {
		/* This also is the max crop size for this device. */
		cam->crop_defrect.top = cam->crop_defrect.left = 0;
		cam->crop_defrect.width = cam_fmt.fmt.pix.width;
		cam->crop_defrect.height = cam_fmt.fmt.pix.height;

		/* At this point, this is also the current image size. */
		cam->crop_current.top = cam->crop_current.left = 0;
		cam->crop_current.width = cam_fmt.fmt.pix.width;
		cam->crop_current.height = cam_fmt.fmt.pix.height;
		pr_debug("On Open: Input to ipu size is %d x %d\n",
			cam_fmt.fmt.pix.width, cam_fmt.fmt.pix.height);
		pr_debug("End of %s: v2f pix widthxheight %d x %d\n", __func__,
			cam->v2f.fmt.pix.width, cam->v2f.fmt.pix.height);
		pr_debug("End of %s: crop_bounds widthxheight %d x %d\n", __func__,
			cam->crop_bounds.width, cam->crop_bounds.height);
		pr_debug("End of %s: crop_defrect widthxheight %d x %d\n", __func__,
			cam->crop_defrect.width, cam->crop_defrect.height);
		pr_debug("End of %s: crop_current widthxheight %d x %d\n", __func__,
			cam->crop_current.width, cam->crop_current.height);
	}
	swidth = cam->crop_current.width;
	sheight = cam->crop_current.height;
	sleft = 0;
	stop = 0;
	cam_fmt.type = V4L2_BUF_TYPE_SENSOR;
	cam_fmt.fmt.spix.swidth = 0;
	vidioc_int_g_fmt_cap(cam->sensor, &cam_fmt);
	if (cam_fmt.fmt.spix.swidth) {
		swidth = cam_fmt.fmt.spix.swidth;
		sheight = cam_fmt.fmt.spix.sheight;
		sleft =  cam_fmt.fmt.spix.left;
		stop =  cam_fmt.fmt.spix.top;
	}
	/* This essentially loses the data at the left and bottom of the image
	 * giving a digital zoom image, if crop_current is less than the full
	 * size of the image. */
	ipu_csi_window_size_crop(cam->ipu,
			swidth, sheight,
			cam->crop_current.width, cam->crop_current.height,
			sleft + cam->crop_current.left, stop + cam->crop_current.top,
			cam->csi);
	ipu_csi_init_interface(cam->ipu, cam->crop_bounds.width,
			       cam->crop_bounds.height,
			       csi_param.data_fmt, csi_param);
}

/*********************************************************************************************************************/
/*!
 * V4L2 - mxc_v4l2_s_param function
 * Allows setting of capturemode and frame rate.
 *
 * @param cam         structure cam_data *
 * @param parm        structure v4l2_streamparm *
 *
 * @return  status    0 success, EINVAL failed
 */
static int mxc_v4l2_s_param(cam_data *cam, struct v4l2_streamparm *parm)
{
	struct v4l2_streamparm currentparm;
	u32 current_fps, parm_fps;
	int err = 0;

#ifdef ARANZ_DEBUG
	//pr_debug("%s\n", __func__);
  printk(KERN_ALERT "mxc_v4l2_s_param.\n");
#endif

	if (parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) 
  {
		pr_err(KERN_ERR "mxc_v4l2_s_param invalid type\n");
		return -EINVAL;
	}

	/* Stop the viewfinder */
	if (cam->overlay_on == true)
  {
		stop_preview(cam);
  }

	currentparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	/* First check that this device can support the changes requested. */
	err = vidioc_int_g_parm(cam->sensor, &currentparm);
	if (err) 
  {
		pr_err("%s: vidioc_int_g_parm returned an error %d\n",
			__func__, err);
		goto exit;
	}

	current_fps = currentparm.parm.capture.timeperframe.denominator / currentparm.parm.capture.timeperframe.numerator;
	parm_fps = parm->parm.capture.timeperframe.denominator / parm->parm.capture.timeperframe.numerator;

#ifdef ARANZ_DEBUG
	printk(KERN_ALERT "   Current capabilities are %x\n", currentparm.parm.capture.capability);
	printk(KERN_ALERT "   Current capturemode is %d  change to %d\n", currentparm.parm.capture.capturemode, parm->parm.capture.capturemode);
	printk(KERN_ALERT "   Current framerate is %d  change to %d\n", current_fps, parm_fps);
#endif

	/* This will change any camera settings needed. */
	err = vidioc_int_s_parm(cam->sensor, parm);
	if (err) 
  {
		pr_err("%s: vidioc_int_s_parm returned an error %d\n", __func__, err);
	}
  else
  {
    /* If resolution changed, need to re-program the CSI */
	  /* Get new values. */
	  setup_ifparm(cam, 0);
  }

exit:
	if (cam->overlay_on == true)
  {
		start_preview(cam);
  }

	return err;
}

/*********************************************************************************************************************/
/*!
 * V4L2 - mxc_v4l2_s_std function
 *
 * Sets the TV standard to be used.
 *
 * @param cam	      structure cam_data *
 * @param parm	      structure v4l2_streamparm *
 *
 * @return  status    0 success, EINVAL failed
 */
static int mxc_v4l2_s_std(cam_data *cam, v4l2_std_id e)
{
	pr_debug("%s: %Lx\n", __func__, e);
	switch (e) {
	case V4L2_STD_PAL:
		pr_debug("   Setting standard to PAL %Lx\n", V4L2_STD_PAL);
		cam->standard.id = V4L2_STD_PAL;
		video_index = TV_PAL;
		break;
	case V4L2_STD_NTSC:
		pr_debug("   Setting standard to NTSC %Lx\n",
				V4L2_STD_NTSC);
		/* Get rid of the white dot line in NTSC signal input */
		cam->standard.id = V4L2_STD_NTSC;
		video_index = TV_NTSC;
		break;
	case V4L2_STD_UNKNOWN:
	case V4L2_STD_ALL:
		/* auto-detect don't report an error */
		cam->standard.id = V4L2_STD_ALL;
		video_index = TV_NOT_LOCKED;
		break;
	default:
		cam->standard.id = V4L2_STD_ALL;
		video_index = TV_NOT_LOCKED;
		pr_err("ERROR: unrecognized std! %Lx (PAL=%Lx, NTSC=%Lx\n",
			e, V4L2_STD_PAL, V4L2_STD_NTSC);
	}

	cam->standard.index = video_index;
	strcpy(cam->standard.name, video_fmts[video_index].name);
	cam->crop_bounds.width = video_fmts[video_index].raw_width;
	cam->crop_bounds.height = video_fmts[video_index].raw_height;
	cam->crop_current.width = video_fmts[video_index].active_width;
	cam->crop_current.height = video_fmts[video_index].active_height;
	cam->crop_current.top = video_fmts[video_index].active_top;
	cam->crop_current.left = video_fmts[video_index].active_left;

	return 0;
}

/*********************************************************************************************************************/
/*!
 * V4L2 - mxc_v4l2_g_std function
 *
 * Gets the TV standard from the TV input device.
 *
 * @param cam	      structure cam_data *
 *
 * @param e	      structure v4l2_streamparm *
 *
 * @return  status    0 success, EINVAL failed
 */
static int mxc_v4l2_g_std(cam_data *cam, v4l2_std_id *e)
{
	struct v4l2_format tv_fmt;

	pr_debug("%s\n", __func__);

	if (cam->device_type == 1) {
		/* Use this function to get what the TV-In device detects the
		 * format to be. pixelformat is used to return the std value
		 * since the interface has no vidioc_g_std.*/
		tv_fmt.type = V4L2_BUF_TYPE_PRIVATE;
		vidioc_int_g_fmt_cap(cam->sensor, &tv_fmt);

		/* If the TV-in automatically detects the standard, then if it
		 * changes, the settings need to change. */
		if (cam->standard_autodetect) {
			if (cam->standard.id != tv_fmt.fmt.pix.pixelformat) {
				pr_debug("MVC: mxc_v4l2_g_std: "
					"Changing standard\n");
				mxc_v4l2_s_std(cam, tv_fmt.fmt.pix.pixelformat);
			}
		}

		*e = tv_fmt.fmt.pix.pixelformat;
	}

	return 0;
}

/*********************************************************************************************************************/
/*!
 * Dequeue one V4L capture buffer
 *
 * @param cam         structure cam_data *
 * @param buf         structure v4l2_buffer *
 *
 * @return  status    0 success, EINVAL invalid frame number,
 *                    ETIME timeout, ERESTARTSYS interrupted by user
 */
static int mxc_v4l_dqueue(cam_data *cam, struct v4l2_buffer *buf)
{
	int retval = 0;
	struct mxc_v4l_frame *frame;
	unsigned long lock_flags;

	pr_debug("%s\n", __func__);

	if (!wait_event_interruptible_timeout(cam->enc_queue,
					      cam->enc_counter != 0,
					      50 * HZ)) {
		pr_err("ERROR: v4l2 capture: mxc_v4l_dqueue timeout "
			"enc_counter %x\n",
		       cam->enc_counter);
		return -ETIME;
	} else if (signal_pending(current)) {
		pr_err("ERROR: v4l2 capture: mxc_v4l_dqueue() "
			"interrupt received\n");
		return -ERESTARTSYS;
	}

	if (down_interruptible(&cam->busy_lock))
		return -EBUSY;

	spin_lock_irqsave(&cam->dqueue_int_lock, lock_flags);
	cam->enc_counter--;

	frame = list_entry(cam->done_q.next, struct mxc_v4l_frame, queue);
	list_del(cam->done_q.next);
	if (frame->buffer.flags & V4L2_BUF_FLAG_DONE) {
		frame->buffer.flags &= ~V4L2_BUF_FLAG_DONE;
	} else if (frame->buffer.flags & V4L2_BUF_FLAG_QUEUED) {
		pr_err("ERROR: v4l2 capture: VIDIOC_DQBUF: "
			"Buffer not filled.\n");
		frame->buffer.flags &= ~V4L2_BUF_FLAG_QUEUED;
		retval = -EINVAL;
	} else if ((frame->buffer.flags & 0x7) == V4L2_BUF_FLAG_MAPPED) {
		pr_err("ERROR: v4l2 capture: VIDIOC_DQBUF: "
			"Buffer not queued.\n");
		retval = -EINVAL;
	}

	cam->frame[frame->index].buffer.field = cam->device_type ?
				V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;

	buf->bytesused = cam->v2f.fmt.pix.sizeimage;
	buf->index = frame->index;
	buf->flags = frame->buffer.flags;
	buf->m = cam->frame[frame->index].buffer.m;
	buf->timestamp = cam->frame[frame->index].buffer.timestamp;
	buf->field = cam->frame[frame->index].buffer.field;
	spin_unlock_irqrestore(&cam->dqueue_int_lock, lock_flags);

	up(&cam->busy_lock);
	return retval;
}

static void power_down_callback(struct work_struct *work)
{
	cam_data *cam = container_of(work, struct _cam_data, power_down_work.work);

	down(&cam->busy_lock);
	if (!cam->open_count) {
		pr_info("%s: ipu%d/csi%d\n", __func__, cam->ipu_id, cam->csi);
		vidioc_int_s_power(cam->sensor, 0);
		cam->power_on = 0;
	}
	up(&cam->busy_lock);
}

/* cam->busy_lock is held */
void power_up_camera(cam_data *cam)
{
	if (cam->power_on) {
		cancel_delayed_work(&cam->power_down_work);
		return;
	}
	vidioc_int_s_power(cam->sensor, 1);
	vidioc_int_init(cam->sensor);
	vidioc_int_dev_init(cam->sensor);
	cam->power_on = 1;
}

/*********************************************************************************************************************/
void power_off_camera(cam_data *cam)
{
	schedule_delayed_work(&cam->power_down_work, (HZ * 2));
}

static void all_off(cam_data *cam)
{
  gpio_set_value(cam->gpio_FLEN, 0);
  gpio_set_value(cam->gpio_CHARGE_EN, 0);
  gpio_set_value(cam->gpio_LED_CUR1, 0);
  gpio_set_value(cam->gpio_LED_CUR2, 0);
  gpio_set_value(cam->gpio_LED1_EN, 0);
  gpio_set_value(cam->gpio_LED2_EN, 0);
  gpio_set_value(cam->gpio_LED_EN, 0);
  gpio_set_value(cam->gpio_LASER1, 0);
  gpio_set_value(cam->gpio_LASER2, 0);
  gpio_set_value(cam->gpio_LASER3, 0);
}

unsigned long csi_in_use;

/*********************************************************************************************************************/
/*!
 * V4L interface - open function
 *
 * @param file         structure file *
 *
 * @return  status    0 success, ENODEV invalid device instance,
 *                    ENODEV timeout, ERESTARTSYS interrupted by user
 */
static int mxc_v4l_open(struct file *file)
{
	struct video_device *dev = video_devdata(file);
	cam_data *cam = video_get_drvdata(dev);
	int err = 0;
	struct sensor_data *sensor;
	int csi_bit;

	if (!cam) {
		pr_err("%s: %s cam_data not found!\n", __func__, dev->name);
		return -EBADF;
	}
	if (!cam->sensor) {
		pr_err("%s: %s no sensor ipu%d/csi%d\n",
			__func__, dev->name, cam->ipu_id, cam->csi);
		return -EAGAIN;
	}
	if (cam->sensor->type != v4l2_int_type_slave) {
		pr_err("%s: %s wrong type ipu%d/csi%d, type=%d/%d\n",
			__func__, dev->name, cam->ipu_id, cam->csi,
			cam->sensor->type, v4l2_int_type_slave);
		return -EAGAIN;
	}

	sensor = cam->sensor->priv;
	if (!sensor) {
		pr_err("%s: Internal error, sensor_data is not found!\n",
		       __func__);
		return -EBADF;
	}
	pr_debug("%s: %s ipu%d/csi%d\n", __func__, dev->name,
		cam->ipu_id, cam->csi);

  all_off(cam);

	down(&cam->busy_lock);

	err = 0;
	if (signal_pending(current))
		goto oops;

	if (cam->open_count++ == 0) {
		struct regmap *gpr;

		csi_bit = (cam->ipu_id << 1) | cam->csi;
		if (test_and_set_bit(csi_bit, &csi_in_use)) {
			pr_err("%s: %s CSI already in use\n", __func__, dev->name);
			err = -EBUSY;
			cam->open_count = 0;
			goto oops;
		}
		cam->csi_in_use = 1;

		gpr = syscon_regmap_lookup_by_compatible("fsl,imx6q-iomuxc-gpr");
		if (!IS_ERR(gpr)) {
			if (of_machine_is_compatible("fsl,imx6q")) {
				if (cam->ipu_id == cam->csi) {
					unsigned shift = 19 + cam->csi;
					unsigned mask = 1 << shift;
					unsigned val = (cam->mipi_camera ? 0 : 1) << shift;

					regmap_update_bits(gpr, IOMUXC_GPR1, mask, val);
				}
			} else if (of_machine_is_compatible("fsl,imx6dl")) {
				unsigned shift = cam->csi * 3;
				unsigned mask = 7 << shift;
				unsigned val = (cam->mipi_camera ? csi_bit : 4) << shift;

				regmap_update_bits(gpr, IOMUXC_GPR13, mask, val);
			}
		} else {
			pr_err("%s: failed to find fsl,imx6q-iomux-gpr regmap\n",
			       __func__);
		}

		wait_event_interruptible(cam->power_queue,
					 cam->low_power == false);

		if (strcmp(mxc_capture_inputs[cam->current_input].name,
			   "CSI MEM") == 0) {
#if defined(CONFIG_MXC_IPU_CSI_ENC) || defined(CONFIG_MXC_IPU_CSI_ENC_MODULE)
			err = csi_enc_select(cam);
#endif
		} else if (strcmp(mxc_capture_inputs[cam->current_input].name,
				  "CSI IC MEM") == 0) {
#if defined(CONFIG_MXC_IPU_PRP_ENC) || defined(CONFIG_MXC_IPU_PRP_ENC_MODULE)
			err = prp_enc_select(cam);
#endif
		}

		cam->enc_counter = 0;
		INIT_LIST_HEAD(&cam->ready_q);
		INIT_LIST_HEAD(&cam->working_q);
		INIT_LIST_HEAD(&cam->done_q);
		setup_ifparm(cam, 1);
		if (!IS_ERR(sensor->sensor_clk))
			clk_prepare_enable(sensor->sensor_clk);
		power_up_camera(cam);
	}

	file->private_data = dev;

oops:
	up(&cam->busy_lock);
	return err;
}

/*********************************************************************************************************************/
/*!
 * V4L interface - close function
 *
 * @param file     struct file *
 *
 * @return         0 success
 */
static int mxc_v4l_close(struct file *file)
{
	struct video_device *dev = video_devdata(file);
	int err = 0;
	cam_data *cam = video_get_drvdata(dev);
	struct sensor_data *sensor;
	pr_debug("%s\n", __func__);

	if (!cam) {
		pr_err("%s: cam_data not found!\n", __func__);
		return -EBADF;
	}

	if (!cam->sensor) {
		pr_err("%s: Internal error, camera is not found!\n",
		       __func__);
		return -EBADF;
	}

	sensor = cam->sensor->priv;
	if (!sensor) {
		pr_err("%s: Internal error, sensor_data is not found!\n",
		       __func__);
		return -EBADF;
	}

  all_off(cam);

	down(&cam->busy_lock);

	/* for the case somebody hit the ctrl C */
	if (cam->overlay_pid == current->pid && cam->overlay_on) {
		err = stop_preview(cam);
		cam->overlay_on = false;
	}

	if (--cam->open_count == 0) {
		if (cam->capture_pid == current->pid) {
			err |= mxc_streamoff(cam);
			wake_up_interruptible(&cam->enc_queue);
		}
		if (!IS_ERR(sensor->sensor_clk))
			clk_disable_unprepare(sensor->sensor_clk);
		wait_event_interruptible(cam->power_queue,
					 cam->low_power == false);
		pr_debug("mxc_v4l_close: release resource\n");

		if (strcmp(mxc_capture_inputs[cam->current_input].name,
			   "CSI MEM") == 0) {
#if defined(CONFIG_MXC_IPU_CSI_ENC) || defined(CONFIG_MXC_IPU_CSI_ENC_MODULE)
			err |= csi_enc_deselect(cam);
#endif
		} else if (strcmp(mxc_capture_inputs[cam->current_input].name,
				  "CSI IC MEM") == 0) {
#if defined(CONFIG_MXC_IPU_PRP_ENC) || defined(CONFIG_MXC_IPU_PRP_ENC_MODULE)
			err |= prp_enc_deselect(cam);
#endif
		}

		mxc_free_frame_buf(cam);
		file->private_data = NULL;

		/* capture off */
		wake_up_interruptible(&cam->enc_queue);
		mxc_free_frames(cam);
		cam->enc_counter++;
		power_off_camera(cam);

		if (cam->csi_in_use) {
			int csi_bit = (cam->ipu_id << 1) | cam->csi;

			clear_bit(csi_bit, &csi_in_use);
			cam->csi_in_use = 0;
		}
	}

	up(&cam->busy_lock);

	return err;
}

#if defined(CONFIG_MXC_IPU_PRP_ENC) || defined(CONFIG_MXC_IPU_CSI_ENC) || \
    defined(CONFIG_MXC_IPU_PRP_ENC_MODULE) || \
    defined(CONFIG_MXC_IPU_CSI_ENC_MODULE)

/*********************************************************************************************************************/
/*
 * V4L interface - read function
 *
 * @param file       struct file *
 * @param read buf   char *
 * @param count      size_t
 * @param ppos       structure loff_t *
 *
 * @return           bytes read
 */
static ssize_t mxc_v4l_read(struct file *file, char *buf, size_t count,
			    loff_t *ppos)
{
	int err = 0;
	u8 *v_address[2];
	struct video_device *dev = video_devdata(file);
	cam_data *cam = video_get_drvdata(dev);

	if (down_interruptible(&cam->busy_lock))
		return -EINTR;

	/* Stop the viewfinder */
	if (cam->overlay_on == true)
		stop_preview(cam);

	v_address[0] = dma_alloc_coherent(0,
				       PAGE_ALIGN(cam->v2f.fmt.pix.sizeimage),
				       &cam->still_buf[0],
				       GFP_DMA | GFP_ATOMIC);

	v_address[1] = dma_alloc_coherent(0,
				       PAGE_ALIGN(cam->v2f.fmt.pix.sizeimage),
				       &cam->still_buf[1],
				       GFP_DMA | GFP_ATOMIC);

	if (!v_address[0] || !v_address[1]) {
		err = -ENOBUFS;
		goto exit0;
	}

	err = prp_still_select(cam);
	if (err != 0) {
		err = -EIO;
		goto exit0;
	}

	cam->still_counter = 0;
	err = cam->csi_start(cam);
	if (err != 0) {
		err = -EIO;
		goto exit1;
	}

	if (!wait_event_interruptible_timeout(cam->still_queue,
					      cam->still_counter != 0,
					      10 * HZ)) {
		pr_err("ERROR: v4l2 capture: mxc_v4l_read timeout counter %x\n",
		       cam->still_counter);
		err = -ETIME;
		goto exit1;
	}
	err = copy_to_user(buf, v_address[1], cam->v2f.fmt.pix.sizeimage);

exit1:
	prp_still_deselect(cam);

exit0:
	if (v_address[0] != 0)
		dma_free_coherent(0, cam->v2f.fmt.pix.sizeimage, v_address[0],
				  cam->still_buf[0]);
	if (v_address[1] != 0)
		dma_free_coherent(0, cam->v2f.fmt.pix.sizeimage, v_address[1],
				  cam->still_buf[1]);

	cam->still_buf[0] = cam->still_buf[1] = 0;

	if (cam->overlay_on == true)
		start_preview(cam);

	up(&cam->busy_lock);
	if (err < 0)
		return err;

	return cam->v2f.fmt.pix.sizeimage - err;
}
#endif

/*********************************************************************************************************************/
/*!
 * V4L interface - ioctl function
 *
 * @param file       struct file*
 *
 * @param ioctlnr    unsigned int
 *
 * @param arg        void*
 *
 * @return           0 success, ENODEV for invalid device instance,
 *                   -1 for other errors.
 */
static long mxc_v4l_do_ioctl(struct file *file, unsigned int ioctlnr, void *arg)
{
	struct video_device *dev = video_devdata(file);
	cam_data *cam = video_get_drvdata(dev);
	int retval = 0;
	unsigned long lock_flags;

	pr_debug("%s: %x ipu%d/csi%d\n", __func__, ioctlnr, cam->ipu_id, cam->csi);
	wait_event_interruptible(cam->power_queue, cam->low_power == false);
	/* make this _really_ smp-safe */
	if (ioctlnr != VIDIOC_DQBUF)
		if (down_interruptible(&cam->busy_lock))
			return -EBUSY;

	switch (ioctlnr) {
	/*!
	 * V4l2 VIDIOC_QUERYCAP ioctl
	 */
	case VIDIOC_QUERYCAP: {
		struct v4l2_capability *cap = arg;
		pr_debug("   case VIDIOC_QUERYCAP\n");
		strcpy(cap->driver, "mxc_v4l2_aranz");
		cap->version = KERNEL_VERSION(0, 1, 11);
		cap->capabilities = V4L2_CAP_VIDEO_CAPTURE |
				    V4L2_CAP_VIDEO_OVERLAY |
				    V4L2_CAP_STREAMING |
				    V4L2_CAP_READWRITE;
		cap->card[0] = '\0';
		cap->bus_info[0] = '\0';
		break;
	}

	/*!
	 * V4l2 VIDIOC_G_FMT ioctl
	 */
	case VIDIOC_G_FMT: {
		struct v4l2_format *gf = arg;
		pr_debug("   case VIDIOC_G_FMT\n");
		retval = mxc_v4l2_g_fmt(cam, gf);
		break;
	}

	/*!
	 * V4l2 VIDIOC_S_FMT ioctl
	 */
	/* XXX: workaround for gstreamer */
	case VIDIOC_TRY_FMT: {
		struct v4l2_format *sf = arg;
		pr_debug("   case VIDIOC_TRY_FMT\n");
		retval = mxc_v4l2_s_fmt(cam, sf, true);
		break;
	}
	case VIDIOC_S_FMT: {
		struct v4l2_format *sf = arg;
		pr_debug("   case VIDIOC_S_FMT\n");
		retval = mxc_v4l2_s_fmt(cam, sf, false);
		break;
	}

	/*!
	 * V4l2 VIDIOC_REQBUFS ioctl
	 */
	case VIDIOC_REQBUFS: 
  {
		struct v4l2_requestbuffers *req = arg;

#ifdef ARANZ_DEBUG
    printk(KERN_ALERT "VIDIOC_REQBUFS: %d.\n", req->count);
#endif

		if (req->count > FRAME_NUM) 
    {
			pr_err("ERROR: v4l2 capture: VIDIOC_REQBUFS: not enough buffers\n");
			req->count = FRAME_NUM;
		}

		if ((req->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)) 
    {
			pr_err("ERROR: v4l2 capture: VIDIOC_REQBUFS: wrong buffer type\n");
			retval = -EINVAL;
			break;
		}

		mxc_streamoff(cam);
		if (req->memory & V4L2_MEMORY_MMAP) 
    {
			mxc_free_frame_buf(cam);
			retval = mxc_allocate_frame_buf(cam, req->count);
		}
		break;
	}

	/*!
	 * V4l2 VIDIOC_QUERYBUF ioctl
	 */
	case VIDIOC_QUERYBUF: 
  {
		struct v4l2_buffer *buf = arg;
		int index = buf->index;

#ifdef ARANZ_DEBUG
    printk(KERN_ALERT "VIDIOC_QUERYBUF: %d.\n", index);
#endif

		if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
			pr_err("ERROR: v4l2 capture: "
			       "VIDIOC_QUERYBUFS: "
			       "wrong buffer type\n");
			retval = -EINVAL;
			break;
		}

		if (buf->memory & V4L2_MEMORY_MMAP) {
			memset(buf, 0, sizeof(buf));
			buf->index = index;
		}

		down(&cam->param_lock);
		if (buf->memory & V4L2_MEMORY_USERPTR) {
			mxc_v4l2_release_bufs(cam);
			retval = mxc_v4l2_prepare_bufs(cam, buf);
		}

		if (buf->memory & V4L2_MEMORY_MMAP)
			retval = mxc_v4l2_buffer_status(cam, buf);
		up(&cam->param_lock);
		break;
	}

	/*!
	 * V4l2 VIDIOC_QBUF ioctl
	 */
	case VIDIOC_QBUF: {
		struct v4l2_buffer *buf = arg;
		int index = buf->index;

#ifdef ARANZ_DEBUG
    printk(KERN_ALERT "VIDIOC_QBUF: %d.\n", index);
#endif

		spin_lock_irqsave(&cam->queue_int_lock, lock_flags);
		if ((cam->frame[index].buffer.flags & 0x7) ==
		    V4L2_BUF_FLAG_MAPPED) {
			cam->frame[index].buffer.flags |=
			    V4L2_BUF_FLAG_QUEUED;
			list_add_tail(&cam->frame[index].queue,
				      &cam->ready_q);
		} else if (cam->frame[index].buffer.
			   flags & V4L2_BUF_FLAG_QUEUED) {
			pr_err("ERROR: v4l2 capture: VIDIOC_QBUF: "
			       "buffer already queued\n");
			retval = -EINVAL;
		} else if (cam->frame[index].buffer.
			   flags & V4L2_BUF_FLAG_DONE) {
			pr_err("ERROR: v4l2 capture: VIDIOC_QBUF: "
			       "overwrite done buffer.\n");
			cam->frame[index].buffer.flags &=
			    ~V4L2_BUF_FLAG_DONE;
			cam->frame[index].buffer.flags |=
			    V4L2_BUF_FLAG_QUEUED;
			retval = -EINVAL;
		}

		buf->flags = cam->frame[index].buffer.flags;
		spin_unlock_irqrestore(&cam->queue_int_lock, lock_flags);
		break;
	}

	/*!
	 * V4l2 VIDIOC_DQBUF ioctl
	 */
	case VIDIOC_DQBUF: 
  {
		struct v4l2_buffer *buf = arg;
		//pr_debug("   case VIDIOC_DQBUF\n");

		if ((cam->enc_counter == 0) &&
			(file->f_flags & O_NONBLOCK)) {
			retval = -EAGAIN;
			break;
		}

		retval = mxc_v4l_dqueue(cam, buf);
#ifdef ARANZ_DEBUG
    printk(KERN_ALERT "VIDIOC_DQBUF: %d.\n", buf->index);
#endif
		break;
	}

	/*!
	 * V4l2 VIDIOC_STREAMON ioctl
	 */
	case VIDIOC_STREAMON: 
  {
		pr_debug("   case VIDIOC_STREAMON\n");
		retval = mxc_streamon(cam);
		break;
	}

	/*!
	 * V4l2 VIDIOC_STREAMOFF ioctl
	 */
	case VIDIOC_STREAMOFF: 
  {
		pr_debug("   case VIDIOC_STREAMOFF\n");
		retval = mxc_streamoff(cam);
		break;
	}

	/*!
	 * V4l2 VIDIOC_G_CTRL ioctl
	 */
	case VIDIOC_G_CTRL: {
		pr_debug("   case VIDIOC_G_CTRL\n");
		retval = mxc_v4l2_g_ctrl(cam, arg);
		break;
	}

	/*!
	 * V4l2 VIDIOC_S_CTRL ioctl
	 */
	case VIDIOC_S_CTRL: {
		pr_debug("   case VIDIOC_S_CTRL\n");
		retval = mxc_v4l2_s_ctrl(cam, arg);
		break;
	}

	/*!
	 * V4l2 VIDIOC_CROPCAP ioctl
	 */
	case VIDIOC_CROPCAP: {
		struct v4l2_cropcap *cap = arg;
		pr_debug("   case VIDIOC_CROPCAP\n");
		if (cap->type != V4L2_BUF_TYPE_VIDEO_CAPTURE &&
		    cap->type != V4L2_BUF_TYPE_VIDEO_OVERLAY) {
			retval = -EINVAL;
			break;
		}
		cap->bounds = cam->crop_bounds;
		cap->defrect = cam->crop_defrect;
		break;
	}

	/*!
	 * V4l2 VIDIOC_G_CROP ioctl
	 */
	case VIDIOC_G_CROP: {
		struct v4l2_crop *crop = arg;
		pr_debug("   case VIDIOC_G_CROP\n");

		if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE &&
		    crop->type != V4L2_BUF_TYPE_VIDEO_OVERLAY) {
			retval = -EINVAL;
			break;
		}
		crop->c = cam->crop_current;
		break;
	}

	/*!
	 * V4l2 VIDIOC_S_CROP ioctl
	 */
	case VIDIOC_S_CROP: {
		struct v4l2_crop *crop = arg;
		struct v4l2_rect *b = &cam->crop_bounds;
		pr_debug("   case VIDIOC_S_CROP\n");

		if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE &&
		    crop->type != V4L2_BUF_TYPE_VIDEO_OVERLAY) {
			retval = -EINVAL;
			break;
		}

		crop->c.top = (crop->c.top < b->top) ? b->top
			      : crop->c.top;
		if (crop->c.top > b->top + b->height)
			crop->c.top = b->top + b->height - 1;
		if (crop->c.height > b->top + b->height - crop->c.top)
			crop->c.height =
				b->top + b->height - crop->c.top;

		crop->c.left = (crop->c.left < b->left) ? b->left
		    : crop->c.left;
		if (crop->c.left > b->left + b->width)
			crop->c.left = b->left + b->width - 1;
		if (crop->c.width > b->left - crop->c.left + b->width)
			crop->c.width =
				b->left - crop->c.left + b->width;

		crop->c.width -= crop->c.width % 8;
		crop->c.left -= crop->c.left % 4;
		cam->crop_current = crop->c;

		pr_debug("   Cropping Input to ipu size %d x %d\n",
				cam->crop_current.width,
				cam->crop_current.height);
		ipu_csi_set_window_size(cam->ipu, cam->crop_current.width,
					cam->crop_current.height,
					cam->csi);
		ipu_csi_set_window_pos(cam->ipu, cam->crop_current.left,
				       cam->crop_current.top,
				       cam->csi);
		break;
	}

	/*!
	 * V4l2 VIDIOC_OVERLAY ioctl
	 */
	case VIDIOC_OVERLAY: {
		int *on = arg;
		pr_debug("   VIDIOC_OVERLAY on=%d\n", *on);
		if (*on) {
			cam->overlay_on = true;
			cam->overlay_pid = current->pid;
			retval = start_preview(cam);
		}
		if (!*on) {
			retval = stop_preview(cam);
			cam->overlay_on = false;
		}
		break;
	}

	/*!
	 * V4l2 VIDIOC_G_FBUF ioctl
	 */
	case VIDIOC_G_FBUF: {
		struct v4l2_framebuffer *fb = arg;
		pr_debug("   case VIDIOC_G_FBUF\n");
		*fb = cam->v4l2_fb;
		fb->capability = V4L2_FBUF_CAP_EXTERNOVERLAY;
		break;
	}

	/*!
	 * V4l2 VIDIOC_S_FBUF ioctl
	 */
	case VIDIOC_S_FBUF: {
		struct v4l2_framebuffer *fb = arg;
		pr_debug("   case VIDIOC_S_FBUF\n");
		cam->v4l2_fb = *fb;
		break;
	}

	case VIDIOC_G_PARM: {
		struct v4l2_streamparm *parm = arg;
		pr_debug("   case VIDIOC_G_PARM\n");
		if (cam->sensor)
			retval = vidioc_int_g_parm(cam->sensor, parm);
		else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			retval = -ENODEV;
		}
		break;
	}

	case VIDIOC_S_PARM:  
  {
		struct v4l2_streamparm *parm = arg;
		pr_debug("   case VIDIOC_S_PARM\n");
		if (cam->sensor)
    {
			retval = mxc_v4l2_s_param(cam, parm);
    }
		else 
    {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			retval = -ENODEV;
		}
		break;
	}

	/* linux v4l2 bug, kernel c0485619 user c0405619 */
	case VIDIOC_ENUMSTD: {
		struct v4l2_standard *e = arg;
		pr_debug("   case VIDIOC_ENUMSTD\n");
		if (e->index > 0) {
			retval = -EINVAL;
			break;
		}
		*e = cam->standard;
		break;
	}

	case VIDIOC_G_STD: {
		v4l2_std_id *e = arg;
		pr_debug("   case VIDIOC_G_STD\n");
		if (cam->sensor)
			retval = mxc_v4l2_g_std(cam, e);
		else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			retval = -ENODEV;
		}
		break;
	}

	case VIDIOC_S_STD: {
		v4l2_std_id *e = arg;
		pr_debug("   case VIDIOC_S_STD\n");
		retval = mxc_v4l2_s_std(cam, *e);

		break;
	}

	case VIDIOC_ENUMOUTPUT: {
		struct v4l2_output *output = arg;
		pr_debug("   case VIDIOC_ENUMOUTPUT\n");
		if (output->index >= MXC_V4L2_CAPTURE_NUM_OUTPUTS) {
			retval = -EINVAL;
			break;
		}
		*output = mxc_capture_outputs[output->index];

		break;
	}
	case VIDIOC_G_OUTPUT: {
		int *p_output_num = arg;
		pr_debug("   case VIDIOC_G_OUTPUT\n");
		*p_output_num = cam->output;
		break;
	}

	case VIDIOC_S_OUTPUT: {
		int *p_output_num = arg;
		pr_debug("   case VIDIOC_S_OUTPUT\n");
		if (*p_output_num >= MXC_V4L2_CAPTURE_NUM_OUTPUTS) {
			retval = -EINVAL;
			break;
		}
		cam->output = *p_output_num;
		break;
	}

	case VIDIOC_ENUMINPUT: {
		struct v4l2_input *input = arg;
		pr_debug("   case VIDIOC_ENUMINPUT\n");
		if (input->index >= MXC_V4L2_CAPTURE_NUM_INPUTS) {
			retval = -EINVAL;
			break;
		}
		*input = mxc_capture_inputs[input->index];
		break;
	}

	case VIDIOC_G_INPUT: {
		int *index = arg;
		pr_debug("   case VIDIOC_G_INPUT\n");
		*index = cam->current_input;
		break;
	}

	case VIDIOC_S_INPUT: {
		int *index = arg;
		pr_debug("   case VIDIOC_S_INPUT\n");
		if (*index >= MXC_V4L2_CAPTURE_NUM_INPUTS) {
			retval = -EINVAL;
			break;
		}

		if (*index == cam->current_input)
			break;

		if ((mxc_capture_inputs[cam->current_input].status &
		    V4L2_IN_ST_NO_POWER) == 0) {
			retval = mxc_streamoff(cam);
			if (retval)
				break;
			mxc_capture_inputs[cam->current_input].status |=
							V4L2_IN_ST_NO_POWER;
		}

		if (strcmp(mxc_capture_inputs[*index].name, "CSI MEM") == 0) {
#if defined(CONFIG_MXC_IPU_CSI_ENC) || defined(CONFIG_MXC_IPU_CSI_ENC_MODULE)
			retval = csi_enc_select(cam);
			if (retval)
				break;
#endif
		} else if (strcmp(mxc_capture_inputs[*index].name,
				  "CSI IC MEM") == 0) {
#if defined(CONFIG_MXC_IPU_PRP_ENC) || defined(CONFIG_MXC_IPU_PRP_ENC_MODULE)
			retval = prp_enc_select(cam);
			if (retval)
				break;
#endif
		}

		mxc_capture_inputs[*index].status &= ~V4L2_IN_ST_NO_POWER;
		cam->current_input = *index;
		break;
	}
	case VIDIOC_ENUM_FMT: {
		struct v4l2_fmtdesc *f = arg;
		if (cam->sensor)
			retval = vidioc_int_enum_fmt_cap(cam->sensor, f);
		else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			retval = -ENODEV;
		}
		break;
	}
	case VIDIOC_ENUM_FRAMESIZES: {
		struct v4l2_frmsizeenum *fsize = arg;
		if (cam->sensor)
			retval = vidioc_int_enum_framesizes(cam->sensor, fsize);
		else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			retval = -ENODEV;
		}
		break;
	}
	case VIDIOC_ENUM_FRAMEINTERVALS: {
		struct v4l2_frmivalenum *fival = arg;
		if (cam->sensor) {
			retval = vidioc_int_enum_frameintervals(cam->sensor,
								fival);
		} else {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			retval = -ENODEV;
		}
		break;
	}
	case VIDIOC_DBG_G_CHIP_IDENT: 
  {
		struct v4l2_dbg_chip_ident *p = arg;
		p->ident = V4L2_IDENT_NONE;
		p->revision = 0;
		if (cam->sensor)
			retval = vidioc_int_g_chip_ident(cam->sensor, (int *)p);
		else 
    {
			pr_err("ERROR: v4l2 capture: slave not found!\n");
			retval = -ENODEV;
		}
		break;
	}

	case VIDIOC_SEND_COMMAND: 
  {
		retval = mxc_v4l2_send_command(cam, arg);
		break;
	}

	case VIDIOC_QUERYCTRL:
  {
		pr_debug(" case VIDIOC_QUERY_CTRL\n");
		retval = mxc_v4l2_query_ctrl(cam, arg);
		break;
  }

  case VIDIOC_LIGHTING_CTRL:
  {
    __u32 *flags = arg;
    bool torch = false;

#ifdef ARANZ_DEBUG
    printk(KERN_ALERT "mxc_v4l_do_ioctl: VIDIOC_LIGHTING_CTRL: %p, 0x%X\n", flags, *flags);
#endif

    if(*flags & 0x80)
    {
      gpio_set_value(cam->gpio_CHARGE_EN,1);
      gpio_set_value(cam->gpio_LED1_EN, 1);
      gpio_set_value(cam->gpio_LED2_EN, 1);
      gpio_set_value(cam->gpio_LED_CUR1, 1);
      gpio_set_value(cam->gpio_LED_CUR2, 1);
      gpio_set_value(cam->gpio_LED_EN, 0);
      gpio_set_value(cam->gpio_FLEN, 1);

      cam->flashTest = true;
      cam->flashTestCount = 0;
      start_epit(cam);
      retval = 0;
      break;
    }

    if(*flags & 0x01)
      gpio_set_value(cam->gpio_LASER1, 1);
    else
      gpio_set_value(cam->gpio_LASER1, 0);

    if(*flags & 0x02)
      gpio_set_value(cam->gpio_LASER2, 1);
    else
      gpio_set_value(cam->gpio_LASER2, 0);

    if(*flags & 0x04)
      gpio_set_value(cam->gpio_LASER3, 1);
    else
      gpio_set_value(cam->gpio_LASER3, 0);

    if(*flags & 0x10)
    {
      gpio_set_value(cam->gpio_LED1_EN, 1);
      torch = true;
    }
    else
    {
      gpio_set_value(cam->gpio_LED1_EN, 0);
    }

    if(*flags & 0x20)
    {
      gpio_set_value(cam->gpio_LED2_EN, 1);
      torch = true;
    }
    else
    {
      gpio_set_value(cam->gpio_LED2_EN, 0);
    }

    if(torch)
    {
      gpio_set_value(cam->gpio_FLEN, 0);
      gpio_set_value(cam->gpio_LED_CUR1, 1);
      gpio_set_value(cam->gpio_LED_CUR2, 1);
      gpio_set_value(cam->gpio_CHARGE_EN,1);
      gpio_set_value(cam->gpio_LED_EN, 1);
    }
    else
    {
      gpio_set_value(cam->gpio_FLEN, 0);
      gpio_set_value(cam->gpio_LED_CUR1, 0);
      gpio_set_value(cam->gpio_LED_CUR2, 0);
      gpio_set_value(cam->gpio_CHARGE_EN,0);
      gpio_set_value(cam->gpio_LED_EN, 0);
    }

    retval = 0;
    break;
  }

  case VIDIOC_IMAGE_SEQ1:
  {
		const char thread_name[32]="v4l2_SEQ1_SetLaserFrameSettings";
    struct v4l2_control c;
    struct v4l2_seq_settings* pSeqSettings = arg;
    
#ifdef ARANZ_DEBUG
    printk(KERN_ALERT "VIDIOC_ARANZ_SEQ1\n");
#endif

    cam->mode = V4L2_MODE_SEQ1;
    cam->stateMachineIndex = 0;
    memcpy(&cam->seq_settings, pSeqSettings, sizeof(struct v4l2_seq_settings));

    //Set the Texture Frame Settings.
    //Set Frame 1 EXPOSURE
    c.id = V4L2_CID_EXPOSURE;
    c.value = cam->seq_settings.textureExposure;
    vidioc_int_s_ctrl(cam->sensor, &c);

    //Set Frame 1 Gain.
    c.id = V4L2_CID_GAIN;
    c.value = cam->seq_settings.textureGain;
    vidioc_int_s_ctrl(cam->sensor, &c);

    gpio_set_value(cam->gpio_FLEN, 0);
    gpio_set_value(cam->gpio_LED_EN, 0);
    gpio_set_value(cam->gpio_CHARGE_EN,0);
    gpio_set_value(cam->gpio_LED1_EN, 1);
    gpio_set_value(cam->gpio_LED2_EN, 1);
    gpio_set_value(cam->gpio_LED_CUR1, 1);
    gpio_set_value(cam->gpio_LED_CUR2, 1);
    gpio_set_value(cam->gpio_LASER1, 0);
    gpio_set_value(cam->gpio_LASER2, 0);
    gpio_set_value(cam->gpio_LASER3, 0);

		cam->seq_counter = 0;
    cam->frame_settings_thread = kthread_create(setLaserFrameSettings, cam, thread_name);
  	if((cam->frame_settings_thread))
    {
      wake_up_process(cam->frame_settings_thread);
    }

    retval = 0;
    break;
  }

  case VIDIOC_IMAGE_SEQ2:
  {
		const char thread_name[32]="v4l2_SEQ2_SetLaserFrameSettings";
    struct v4l2_control c;
    struct v4l2_seq_settings* pSeqSettings = arg;

#ifdef ARANZ_DEBUG
    printk(KERN_ALERT "VIDIOC_ARANZ_SEQ2\n");
#endif

    cam->mode = V4L2_MODE_SEQ2;
    cam->stateMachineIndex = 0;
    memcpy(&cam->seq_settings, pSeqSettings, sizeof(struct v4l2_seq_settings));

    //Set the Texture Frame Settings.
    //Set Frame 1 EXPOSURE
    c.id = V4L2_CID_EXPOSURE;
    c.value = cam->seq_settings.textureExposure;
    vidioc_int_s_ctrl(cam->sensor, &c);

    //Set Frame 1 Gain.
    c.id = V4L2_CID_GAIN;
    c.value = cam->seq_settings.textureGain;
    vidioc_int_s_ctrl(cam->sensor, &c);

    gpio_set_value(cam->gpio_FLEN, 0);
    gpio_set_value(cam->gpio_LED_EN, 0);
    gpio_set_value(cam->gpio_CHARGE_EN,0);
    gpio_set_value(cam->gpio_LED1_EN, 1);
    gpio_set_value(cam->gpio_LED2_EN, 1);
    gpio_set_value(cam->gpio_LED_CUR1, 1);
    gpio_set_value(cam->gpio_LED_CUR2, 1);
    gpio_set_value(cam->gpio_LASER1, 0);
    gpio_set_value(cam->gpio_LASER2, 0);
    gpio_set_value(cam->gpio_LASER3, 0);

		cam->seq_counter = 0;
		cam->frame_settings_thread = kthread_create(setLaserFrameSettings, cam, thread_name);
  	if((cam->frame_settings_thread))
    {
      wake_up_process(cam->frame_settings_thread);
    }

    retval = 0;
    break;
  }

	/* XXX: workaround for gstreamer */
/*	case VIDIOC_TRY_FMT: */
	case VIDIOC_G_TUNER:
	case VIDIOC_S_TUNER:
	case VIDIOC_G_FREQUENCY:
	case VIDIOC_S_FREQUENCY:
	default:
		pr_debug("   case default or not supported\n");
		retval = -EINVAL;
		break;
	}

	if (ioctlnr != VIDIOC_DQBUF)
		up(&cam->busy_lock);
	return retval;
}

/*********************************************************************************************************************/
/*
 * V4L interface - ioctl function
 *
 * @return  None
 */
static long mxc_v4l_ioctl(struct file *file, unsigned int cmd,
			 unsigned long arg)
{
	pr_debug("%s\n", __func__);
	return video_usercopy(file, cmd, arg, mxc_v4l_do_ioctl);
}

/*********************************************************************************************************************/
/*!
 * V4L interface - mmap function
 *
 * @param file        structure file *
 *
 * @param vma         structure vm_area_struct *
 *
 * @return status     0 Success, EINTR busy lock error, ENOBUFS remap_page error
 */
static int mxc_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct video_device *dev = video_devdata(file);
	unsigned long size;
	int res = 0;
	cam_data *cam = video_get_drvdata(dev);

	pr_debug("%s:pgoff=0x%lx, start=0x%lx, end=0x%lx\n", __func__,
		 vma->vm_pgoff, vma->vm_start, vma->vm_end);

	/* make this _really_ smp-safe */
	if (down_interruptible(&cam->busy_lock))
		return -EINTR;

	size = vma->vm_end - vma->vm_start;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start,
			    vma->vm_pgoff, size, vma->vm_page_prot)) {
		pr_err("ERROR: v4l2 capture: mxc_mmap: "
			"remap_pfn_range failed\n");
		res = -ENOBUFS;
		goto mxc_mmap_exit;
	}

	vma->vm_flags &= ~VM_IO;	/* using shared anonymous pages */

mxc_mmap_exit:
	up(&cam->busy_lock);
	return res;
}

/*********************************************************************************************************************/
/*!
 * V4L interface - poll function
 *
 * @param file       structure file *
 *
 * @param wait       structure poll_table_struct *
 *
 * @return  status   POLLIN | POLLRDNORM
 */
static unsigned int mxc_poll(struct file *file, struct poll_table_struct *wait)
{
	struct video_device *dev = video_devdata(file);
	cam_data *cam = video_get_drvdata(dev);
	wait_queue_head_t *queue = NULL;
	int res = POLLIN | POLLRDNORM;

	pr_debug("%s\n", __func__);

	if (down_interruptible(&cam->busy_lock))
		return -EINTR;

	queue = &cam->enc_queue;
	poll_wait(file, queue, wait);

	up(&cam->busy_lock);

	return res;
}

/*********************************************************************************************************************/
/*!
 * This structure defines the functions to be called in this driver.
 */
static struct v4l2_file_operations mxc_v4l_fops = {
	.owner = THIS_MODULE,
	.open = mxc_v4l_open,
	.release = mxc_v4l_close,
	.read = mxc_v4l_read,
	.ioctl = mxc_v4l_ioctl,
	.mmap = mxc_mmap,
	.poll = mxc_poll,
};

static struct video_device mxc_v4l_template = {
	.name = "Mxc Camera",
	.fops = &mxc_v4l_fops,
	.release = video_device_release,
};

/*********************************************************************************************************************/
/*!
 * This function can be used to release any platform data on closing.
 */
static void camera_platform_release(struct device *device)
{
}

/*********************************************************************************************************************/
static void SEQ1_StateMachine(cam_data *cam, struct mxc_v4l_frame *done_frame)
{
  switch(cam->stateMachineIndex)
  {
    case 0:
      cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.
      done_frame->buffer.flags |= V4L2_BUF_FLAG_QUEUED;
      list_add_tail(&done_frame->queue, &cam->ready_q); //Skip these Frames and place the buffer back on the ready Queue.
      start_epit(cam);
			cam->seq_counter++;
      wake_up_interruptible(&cam->seq_queue);
      break;

    case 6:
      cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.
      done_frame->buffer.flags |= V4L2_BUF_FLAG_QUEUED;
      list_add_tail(&done_frame->queue, &cam->ready_q); //Skip these Frames and place the buffer back on the ready Queue.
      break;

    case 7:
      cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.
      done_frame->buffer.flags |= V4L2_BUF_FLAG_DONE;
      list_add_tail(&done_frame->queue, &cam->done_q);

      gpio_set_value(cam->gpio_FLEN, 0);
      gpio_set_value(cam->gpio_LASER3, 1);

      /* Wake up the queue */
      cam->enc_counter++;
      wake_up_interruptible(&cam->enc_queue);
      break;
      
    case 8:
      done_frame->buffer.flags |= V4L2_BUF_FLAG_DONE;
      list_add_tail(&done_frame->queue, &cam->done_q);

      //End of Sequence.
      cam->mode = V4L2_MODE_NORMAL;
      cam->stateMachineIndex = 0;

      gpio_set_value(cam->gpio_FLEN, 0);
      gpio_set_value(cam->gpio_LED_EN, 0);
      gpio_set_value(cam->gpio_CHARGE_EN,0);
      gpio_set_value(cam->gpio_LED1_EN, 0);
      gpio_set_value(cam->gpio_LED2_EN, 0);
      gpio_set_value(cam->gpio_LED_CUR1, 0);
      gpio_set_value(cam->gpio_LED_CUR2, 0);
      gpio_set_value(cam->gpio_LASER1, 0);
      gpio_set_value(cam->gpio_LASER2, 0);
      gpio_set_value(cam->gpio_LASER3, 0);

      /* Wake up the queue */
      cam->enc_counter++;
      wake_up_interruptible(&cam->enc_queue);

      break;

    default:
      done_frame->buffer.flags |= V4L2_BUF_FLAG_QUEUED;
      list_add_tail(&done_frame->queue, &cam->ready_q); //Skip these Frames and place the buffer back on the ready Queue.
      pr_err("ERROR: v4l2 capture: camera_callback: State machine timming error.\n");
      break;
  }
}

/*********************************************************************************************************************/
static void SEQ2_StateMachine(cam_data *cam, struct mxc_v4l_frame *done_frame)
{
  switch(cam->stateMachineIndex)
  {
    case 0:
      stop_epit(cam);
      cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.
      done_frame->buffer.flags |= V4L2_BUF_FLAG_QUEUED;
      list_add_tail(&done_frame->queue, &cam->ready_q); //Skip these Frames and place the buffer back on the ready Queue.
      start_epit(cam);
			cam->seq_counter++;
      wake_up_interruptible(&cam->seq_queue);
      break;

    case 6:
      stop_epit(cam);
      cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.
      done_frame->buffer.flags |= V4L2_BUF_FLAG_QUEUED;
      list_add_tail(&done_frame->queue, &cam->ready_q); //Skip these Frames and place the buffer back on the ready Queue.
			//gpio_set_value(cam->gpio_FLEN, 1); //Temparary
      start_epit(cam);
      break;

    case 12:
      stop_epit(cam);
      cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.  This is the completion of the Texture Frame.
      done_frame->buffer.flags |= V4L2_BUF_FLAG_DONE;
      list_add_tail(&done_frame->queue, &cam->done_q);

      gpio_set_value(cam->gpio_FLEN, 0);
			//gpio_set_value(cam->gpio_LASER3, 1); //Temparary
      start_epit(cam);

      /* Wake up the queue */
      cam->enc_counter++;
      wake_up_interruptible(&cam->enc_queue);
      break;

    case 18:
      stop_epit(cam);
      cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.  This is the completion of the Laser 3 Frame.
      done_frame->buffer.flags |= V4L2_BUF_FLAG_DONE;
      list_add_tail(&done_frame->queue, &cam->done_q);

			//gpio_set_value(cam->gpio_LASER1, 1); //Temparary
      //gpio_set_value(cam->gpio_LASER2, 1); //Temparary
      //gpio_set_value(cam->gpio_LASER3, 1); //Temparary

      start_epit(cam);

      /* Wake up the queue */
      cam->enc_counter++;
      wake_up_interruptible(&cam->enc_queue);
      break;

    case 24:
      stop_epit(cam);
      cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.  This is the completion of the All Laser Frame.
      done_frame->buffer.flags |= V4L2_BUF_FLAG_DONE;
      list_add_tail(&done_frame->queue, &cam->done_q);

      gpio_set_value(cam->gpio_LASER2, 0);
      gpio_set_value(cam->gpio_LASER3, 0);
      start_epit(cam);

      /* Wake up the queue */
      cam->enc_counter++;
      wake_up_interruptible(&cam->enc_queue);
      break;

    case 30:
      stop_epit(cam);
      cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.  This is the completion of the All Laser Frame.
      done_frame->buffer.flags |= V4L2_BUF_FLAG_DONE;
      list_add_tail(&done_frame->queue, &cam->done_q);

      gpio_set_value(cam->gpio_LASER1, 0);
			//gpio_set_value(cam->gpio_LASER2, 1); //Temparary
      start_epit(cam);

      /* Wake up the queue */
      cam->enc_counter++;
      wake_up_interruptible(&cam->enc_queue);
      break;
      
    case 36:
      stop_epit(cam);
      done_frame->buffer.flags |= V4L2_BUF_FLAG_DONE;
      list_add_tail(&done_frame->queue, &cam->done_q);

      //End of Sequence.
      cam->mode = V4L2_MODE_NORMAL;
      cam->stateMachineIndex = 0;

      gpio_set_value(cam->gpio_FLEN, 0);
      gpio_set_value(cam->gpio_LED_EN, 0);
      gpio_set_value(cam->gpio_CHARGE_EN,0);
      gpio_set_value(cam->gpio_LED1_EN, 0);
      gpio_set_value(cam->gpio_LED2_EN, 0);
      gpio_set_value(cam->gpio_LED_CUR1, 0);
      gpio_set_value(cam->gpio_LED_CUR2, 0);
      gpio_set_value(cam->gpio_LASER1, 0);
      gpio_set_value(cam->gpio_LASER2, 0);
      gpio_set_value(cam->gpio_LASER3, 0);

      /* Wake up the queue */
      cam->enc_counter++;
      wake_up_interruptible(&cam->enc_queue);

      break;

    default:
      done_frame->buffer.flags |= V4L2_BUF_FLAG_QUEUED;
      list_add_tail(&done_frame->queue, &cam->ready_q); //Skip these Frames and place the buffer back on the ready Queue.
      pr_err("ERROR: v4l2 capture: camera_callback: State machine timming error.\n");
      break;
  }
}

/*********************************************************************************************************************/
/*!
 * Camera V4l2 callback function.
 *
 * @param mask      u32
 *
 * @param dev       void device structure
 *
 * @return status
 */
static void camera_callback(u32 mask, void *dev)
{
	struct mxc_v4l_frame *done_frame;
	struct mxc_v4l_frame *ready_frame;
	struct timeval cur_time;

	cam_data *cam = (cam_data *) dev;
	if (cam == NULL)
  {
		return;
  }

  imx_v4l2_io3_toggle(cam);

	//pr_debug("%s\n", __func__);

	spin_lock(&cam->queue_int_lock);
	spin_lock(&cam->dqueue_int_lock);
	if (!list_empty(&cam->working_q)) 
  {
		do_gettimeofday(&cur_time);

		done_frame = list_entry(cam->working_q.next, struct mxc_v4l_frame, queue);

    if (done_frame->buffer.index >= FRAME_NUM)
    {
      //pr_err("ERROR: v4l2 capture: camera_callback: Skipping dummy_frame.\n");
			goto next;
    }

		if (done_frame->ipu_buf_num != cam->local_buf_num)
    {
      //pr_err("ERROR: v4l2 capture: camera_callback: PingPong index Mismatch: %d != %d.\n", done_frame->ipu_buf_num, cam->local_buf_num);
			goto next;
    }

		/*
		 * Set the current time to done frame buffer's
		 * timestamp. Users can use this information to judge
		 * the frame's usage.
		 */
		done_frame->buffer.timestamp = cur_time;

		if (done_frame->buffer.flags & V4L2_BUF_FLAG_QUEUED) 
    {
			done_frame->buffer.flags &= ~V4L2_BUF_FLAG_QUEUED;

			/* Added to the done queue */
			list_del(cam->working_q.next);

      if(cam->mode == V4L2_MODE_SEQ1)
      {
        SEQ1_StateMachine(cam, done_frame);
      }
      else if(cam->mode == V4L2_MODE_SEQ2)
      {
        SEQ2_StateMachine(cam, done_frame);
      }
      else
      {
        done_frame->buffer.flags |= V4L2_BUF_FLAG_DONE;
        list_add_tail(&done_frame->queue, &cam->done_q);

        /* Wake up the queue */
        cam->enc_counter++;
        wake_up_interruptible(&cam->enc_queue);
        //pr_err("v4l2 capture: camera_callback: Frame Done: %d, PingPong: %d\n", done_frame->index, done_frame->ipu_buf_num);
      }
		}
    else
    {
			pr_err("ERROR: v4l2 capture: camera_callback: buffer not queued.\n");
    }
	}
  // else
  // {
  //   pr_err("ERROR: v4l2 capture: camera_callback: working_q empty.\n");
  // }

next:
	if (!list_empty(&cam->ready_q)) 
  {
		ready_frame = list_entry(cam->ready_q.next, struct mxc_v4l_frame, queue);
		if (cam->enc_update_eba)
    {
      ready_frame->ipu_buf_num = cam->ping_pong_csi;
			if (cam->enc_update_eba(cam->ipu, ready_frame->buffer.m.offset, &cam->ping_pong_csi) == 0) 
      {
				list_del(cam->ready_q.next);
				list_add_tail(&ready_frame->queue, &cam->working_q);

        //pr_err("v4l2 capture: camera_callback: ENC Frame Queued: %d, PingPong: %d\n", ready_frame->index, ready_frame->ipu_buf_num);
			}
      else
      {
        pr_err("ERROR: v4l2 capture: camera_callback: enc_update_eba failed.\n");
      }
    }
    else
    {
      pr_err("ERROR: v4l2 capture: camera_callback: EBA not ready for the next frame.\n");
    }
	} 
  else 
  {
		if (cam->enc_update_eba)
    {
      cam->dummy_frame.ipu_buf_num = cam->ping_pong_csi;
			if (cam->enc_update_eba(cam->ipu, cam->dummy_frame.buffer.m.offset, &cam->ping_pong_csi) == 0) 
      {
        //pr_err("ERROR: v4l2 capture: camera_callback: queue dummy frame.\n");
        cam->dummy_frame.index = FRAME_NUM;
        cam->dummy_frame.buffer.index = FRAME_NUM;
      }
      else
      {
        pr_err("ERROR: v4l2 capture: camera_callback: enc_update_eba of dummy frame failed.\n");
      }
    }
    else
    {
      pr_err("ERROR: v4l2 capture: camera_callback: EBA not ready for the dummy frame.\n");
    }
	}

	cam->local_buf_num = (cam->local_buf_num == 0) ? 1 : 0;
	spin_unlock(&cam->dqueue_int_lock);
	spin_unlock(&cam->queue_int_lock);

	return;
}

/*********************************************************************************************************************/
/*!
 * initialize cam_data structure
 *
 * @param cam      structure cam_data *
 *
 * @return status  0 Success
 */
static int init_camera_struct(cam_data *cam, struct platform_device *pdev)
{
	const struct of_device_id *of_id =
			of_match_device(mxc_v4l2_dt_ids, &pdev->dev);
	struct device_node *np = pdev->dev.of_node;
	int ipu_id, csi_id, mclk_source, mipi_camera, def_input;
	int ret = 0;
	struct v4l2_device *v4l2_dev;
	static int camera_id;

	pr_debug("%s\n", __func__);

	ret = of_property_read_u32(np, "ipu_id", &ipu_id);
	if (ret) {
		dev_err(&pdev->dev, "ipu_id missing or invalid\n");
		return ret;
	}

	ret = of_property_read_u32(np, "csi_id", &csi_id);
	if (ret) {
		dev_err(&pdev->dev, "csi_id missing or invalid\n");
		return ret;
	}

	ret = of_property_read_u32(np, "mclk_source", &mclk_source);
	if (ret) {
		dev_err(&pdev->dev, "sensor mclk missing or invalid\n");
		return ret;
	}

	ret = of_property_read_u32(np, "mipi_camera", &mipi_camera);
	if (ret)
		mipi_camera = 0;

	ret = of_property_read_u32(np, "default_input", &def_input);
	if (ret || (def_input != 0 && def_input != 1))
		def_input = 0;

	/* Default everything to 0 */
	memset(cam, 0, sizeof(cam_data));

	/* get devtype to distinguish if the cpu is imx5 or imx6
	 * IMX5_V4L2 specify the cpu is imx5
	 * IMX6_V4L2 specify the cpu is imx6q or imx6sdl
	 */
	if (of_id)
		pdev->id_entry = of_id->data;
	cam->devtype = pdev->id_entry->driver_data;

	cam->ipu = ipu_get_soc(ipu_id);
	if (cam->ipu == NULL) {
		pr_err("ERROR: v4l2 capture: failed to get ipu\n");
		return -EINVAL;
	} else if (cam->ipu == ERR_PTR(-ENODEV)) {
		pr_err("ERROR: v4l2 capture: get invalid ipu\n");
		return -ENODEV;
	}

	init_MUTEX(&cam->param_lock);
	init_MUTEX(&cam->busy_lock);
	INIT_DELAYED_WORK(&cam->power_down_work, power_down_callback);

	cam->video_dev = video_device_alloc();
	if (cam->video_dev == NULL)
		return -ENODEV;

	*(cam->video_dev) = mxc_v4l_template;

	video_set_drvdata(cam->video_dev, cam);
	dev_set_drvdata(&pdev->dev, (void *)cam);
	cam->video_dev->minor = -1;

	v4l2_dev = kzalloc(sizeof(*v4l2_dev), GFP_KERNEL);
	if (!v4l2_dev) {
		dev_err(&pdev->dev, "failed to allocate v4l2_dev structure\n");
		video_device_release(cam->video_dev);
		return -ENOMEM;
	}

	if (v4l2_device_register(&pdev->dev, v4l2_dev) < 0) {
		dev_err(&pdev->dev, "register v4l2 device failed\n");
		video_device_release(cam->video_dev);
		kfree(v4l2_dev);
		return -ENODEV;
	}
	cam->video_dev->v4l2_dev = v4l2_dev;

	init_waitqueue_head(&cam->enc_queue);
	init_waitqueue_head(&cam->still_queue);
	init_waitqueue_head(&cam->seq_queue);

	/* setup cropping */
	cam->crop_bounds.left = 0;
	cam->crop_bounds.width = 640;
	cam->crop_bounds.top = 0;
	cam->crop_bounds.height = 480;
	cam->crop_current = cam->crop_defrect = cam->crop_bounds;
	ipu_csi_set_window_size(cam->ipu, cam->crop_current.width,
				cam->crop_current.height, cam->csi);
	ipu_csi_set_window_pos(cam->ipu, cam->crop_current.left,
				cam->crop_current.top, cam->csi);
	cam->streamparm.parm.capture.capturemode = 0;

	cam->standard.index = 0;
	cam->standard.id = V4L2_STD_UNKNOWN;
	cam->standard.frameperiod.denominator = 30;
	cam->standard.frameperiod.numerator = 1;
	cam->standard.framelines = 480;
	cam->standard_autodetect = true;
	cam->streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	cam->streamparm.parm.capture.timeperframe = cam->standard.frameperiod;
	cam->streamparm.parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
	cam->overlay_on = false;
	cam->capture_on = false;
	cam->v4l2_fb.flags = V4L2_FBUF_FLAG_OVERLAY;

	cam->v2f.fmt.pix.sizeimage = 352 * 288 * 3 / 2;
	cam->v2f.fmt.pix.bytesperline = 288 * 3 / 2;
	cam->v2f.fmt.pix.width = 288;
	cam->v2f.fmt.pix.height = 352;
	cam->v2f.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
	cam->win.w.width = 160;
	cam->win.w.height = 160;
	cam->win.w.left = 0;
	cam->win.w.top = 0;

	cam->ipu_id = ipu_id;
	cam->csi = csi_id;
	cam->mipi_camera = mipi_camera;
	cam->mclk_source = mclk_source;
	cam->mclk_on[cam->mclk_source] = false;
	cam->current_input = def_input;

	cam->enc_callback = camera_callback;
	init_waitqueue_head(&cam->power_queue);
	spin_lock_init(&cam->queue_int_lock);
	spin_lock_init(&cam->dqueue_int_lock);

	// cam->dummy_frame.vaddress = dma_alloc_coherent(0, SZ_8M, &cam->dummy_frame.paddress, GFP_DMA | GFP_ATOMIC);
	// if (cam->dummy_frame.vaddress == 0)
  // {
	// 	pr_err("ERROR: v4l2 capture: Allocate dummy frame failed.\n");
  //   cam->dummy_frame.buffer.length = 0;
  // }
  // else
  // {
	//   cam->dummy_frame.buffer.length = SZ_8M;
  // }

	cam->self = kmalloc(sizeof(struct v4l2_int_device), GFP_KERNEL);
	cam->self->module = THIS_MODULE;
	sprintf(cam->self->name, "mxc_v4l2_cap%d", camera_id++);
	cam->self->type = v4l2_int_type_master;
	cam->self->u.master = &mxc_v4l2_master;

  cam->mode = V4L2_MODE_NORMAL;
  cam->stateMachineIndex = 0;

	return 0;
}

/*********************************************************************************************************************/
static ssize_t show_streaming(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct video_device *video_dev = container_of(dev,
						struct video_device, dev);
	cam_data *cam = video_get_drvdata(video_dev);

	if (cam->capture_on)
		return sprintf(buf, "stream on\n");
	else
		return sprintf(buf, "stream off\n");
}
static DEVICE_ATTR(fsl_v4l2_capture_property, S_IRUGO, show_streaming, NULL);

/*********************************************************************************************************************/
static ssize_t show_overlay(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct video_device *video_dev = container_of(dev,
						struct video_device, dev);
	cam_data *cam = video_get_drvdata(video_dev);

	if (cam->overlay_on)
		return sprintf(buf, "overlay on\n");
	else
		return sprintf(buf, "overlay off\n");
}
static DEVICE_ATTR(fsl_v4l2_overlay_property, S_IRUGO, show_overlay, NULL);

/*********************************************************************************************************************/
static ssize_t show_csi(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct video_device *video_dev = container_of(dev,
						struct video_device, dev);
	cam_data *cam = video_get_drvdata(video_dev);

	return sprintf(buf, "ipu%d_csi%d\n", cam->ipu_id, cam->csi);
}
static DEVICE_ATTR(fsl_csi_property, S_IRUGO, show_csi, NULL);

/*****************************************************************************/
static inline void imx_v4l2_epit_irq_disable(cam_data *cam)
{
	u32 val;

	val = readl(cam->epit_base + EPITCR);
	val &= ~EPITCR_OCIEN;
	writel(val, cam->epit_base + EPITCR);
}

/*****************************************************************************/
static inline void imx_v4l2_epit_irq_enable(cam_data *cam)
{
	u32 val;

	val = readl(cam->epit_base + EPITCR);
	val |= EPITCR_OCIEN;
	writel(val, cam->epit_base + EPITCR);
}

/*****************************************************************************/
static inline void imx_v4l2_epit_enable(cam_data *cam)
{
	u32 val;

	val = readl(cam->epit_base + EPITCR);
	val |= EPITCR_EN;
	writel(val, cam->epit_base + EPITCR);
}

/*****************************************************************************/
static inline void imx_v4l2_epit_disable(cam_data *cam)
{
	u32 val;

	val = readl(cam->epit_base + EPITCR);
	val &= ~EPITCR_EN;
	writel(val, cam->epit_base + EPITCR);
}

/*****************************************************************************/
static void imx_v4l2_epit_irq_acknowledge(cam_data *cam)
{
	writel(EPITSR_OCIF, cam->epit_base + EPITSR);
}

/*****************************************************************************/
static irqreturn_t imx_v4l2_epit_timer_interrupt(int irq, void *dev_id)
{
  struct device*	pdev = (struct device*)dev_id;
  cam_data* cam = dev_get_drvdata(pdev);

	imx_v4l2_epit_irq_acknowledge(cam);

  imx_v4l2_io4_toggle(cam);

  if(cam->flashTest)
  {
    cam->flashTestCount++;

    if(cam->flashTestCount > 3)
    {
      stop_epit(cam);
      cam->flashTest = false;
      gpio_set_value(cam->gpio_FLEN, 0);
      gpio_set_value(cam->gpio_LED_EN, 0);
      gpio_set_value(cam->gpio_CHARGE_EN,0);
      gpio_set_value(cam->gpio_LED1_EN, 0);
      gpio_set_value(cam->gpio_LED2_EN, 0);
      gpio_set_value(cam->gpio_LED_CUR1, 0);
      gpio_set_value(cam->gpio_LED_CUR2, 0);
    }
  }

  if(cam->mode == V4L2_MODE_SEQ1)
  {
    switch(cam->stateMachineIndex)
    {
      case 1:
      case 2:
      case 3:
			case 4:
        cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.
        break;

      case 5:
        cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.
        gpio_set_value(cam->gpio_FLEN, 1);
        stop_epit(cam);
        break;

      default:
        break;
    }
  }
  else if(cam->mode == V4L2_MODE_SEQ2)
  {
    switch(cam->stateMachineIndex)
    {
      case 1:
      case 2:
      case 3:
			case 4:

      case 7:
      case 8:
      case 9:
			case 10:

      case 13:
      case 14:
      case 15:
      case 16:

      case 19:
      case 20:
      case 21:
      case 22:

      case 25:
      case 26:
      case 27:
      case 28:

			case 31:
      case 32:
      case 33:
      case 34:
        cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.
        break;

      case 5:
        cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.
        gpio_set_value(cam->gpio_FLEN, 1);
        gpio_set_value(cam->gpio_LASER1, 0);
        gpio_set_value(cam->gpio_LASER2, 0);
        gpio_set_value(cam->gpio_LASER3, 0);
        stop_epit(cam);
        break;

      case 11:
        cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.
        gpio_set_value(cam->gpio_LASER3, 1);
        stop_epit(cam);
        break;

      case 17:
        cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.
        gpio_set_value(cam->gpio_LASER1, 1);
        gpio_set_value(cam->gpio_LASER2, 1);
        stop_epit(cam);
        break;

      case 23:
        cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.
        stop_epit(cam);
        break;

      case 29:
        cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.
        gpio_set_value(cam->gpio_LASER2, 1);
        stop_epit(cam);
        break;

      case 35:
        cam->stateMachineIndex++; //Move the Statemachine Forward.  Expected Event Seen.
        stop_epit(cam);
        break;

      default:
        break;
    }
  }

	return IRQ_HANDLED;
}

/*****************************************************************************/
static irqreturn_t imx_v4l2_href_interrupt(int irq, void *dev_id)
{
  cam_data* cam = NULL;
  struct device*	pdev = (struct device*)dev_id;

  cam = dev_get_drvdata(pdev);

  //imx_v4l2_href_toggle(cam);

  return IRQ_HANDLED;
}

/*****************************************************************************/
static void start_epit(cam_data *cam)
{
  //writel(0x0, cam->epit_base + EPITCR);
  //imx_v4l2_epit_irq_acknowledge(cam);

  //writel(0x000B3095, cam->epit_base + EPITLR); //This gives a count of 0.011111106061 Seconds
  //writel(0x00000000, cam->epit_base + EPITCMPR); //Signal Interupt when counter Reaches zero
	//writel(EPITCR_CLKSRC_REF_HIGH | EPITCR_RLD | EPITCR_ENMOD | EPITCR_DBGEN | EPITCR_WAITEN | EPITCR_STOPEN, cam->epit_base + EPITCR);

  imx_v4l2_epit_irq_acknowledge(cam);
  imx_v4l2_epit_enable(cam);
  imx_v4l2_epit_irq_enable(cam);

  //imx_v4l2_io4_toggle(cam);
}

/*****************************************************************************/
static void stop_epit(cam_data *cam)
{
	imx_v4l2_epit_irq_disable(cam);
  imx_v4l2_epit_disable(cam);
  imx_v4l2_epit_irq_acknowledge(cam);

  //imx_v4l2_io4_toggle(cam);
}

/*****************************************************************************/
static int imx_v4l2_epit_probe(struct platform_device *pdev, cam_data *cam)
{
  int err = -ENODEV;

  cam->epit_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  cam->epit_base = devm_ioremap_resource(&pdev->dev, cam->epit_res);
  if (IS_ERR(cam->epit_base))
  {
    dev_err(&pdev->dev, "unable to get mem ressource\n");
    return PTR_ERR(cam->epit_base);
  }

  cam->epit_irq = platform_get_irq(pdev, 0);
  if (cam->epit_irq < 0)
  {
    dev_err(&pdev->dev, "no interrupt defined: %d\n", cam->epit_irq);
    err = -ENOENT;
    return err;
  }
  err = request_irq(cam->epit_irq, imx_v4l2_epit_timer_interrupt, IRQF_TIMER | IRQF_IRQPOLL, "epit2", &pdev->dev);
  if (err)
  {
    dev_err(&pdev->dev, "can't reserve irq = %d\n", cam->epit_irq);
    return err;
  }

  cam->epit_clk_per = devm_clk_get(&pdev->dev, "per");
  if (IS_ERR(cam->epit_clk_per))
  {
    dev_err(&pdev->dev, "getting per clock failed with %ld\n", PTR_ERR(cam->epit_clk_per));
    return PTR_ERR(cam->epit_clk_per);
  }

  cam->epit_clk_ipg = devm_clk_get(&pdev->dev, "ipg");
  if (IS_ERR(cam->epit_clk_ipg))
  {
    dev_err(&pdev->dev, "getting ipg clock failed with %ld\n", PTR_ERR(cam->epit_clk_ipg));
    return PTR_ERR(cam->epit_clk_ipg);
  }

	/*
	 * Initialise to a known state (all timers off, and timing reset)
	 */

  err = clk_prepare_enable(cam->epit_clk_per);
  if (err)
  {
    return err;
  }

	__raw_writel(0x0, cam->epit_base + EPITCR);
  imx_v4l2_epit_irq_acknowledge(cam);

  writel(0x000B3095, cam->epit_base + EPITLR); //This gives a count of 0.011111106061 Seconds
  writel(0x00000000, cam->epit_base + EPITCMPR); //Signal Interupt when counter __raw_writel(0x0, base + EPITCR);Reaches zero
	writel(EPITCR_CLKSRC_REF_HIGH | EPITCR_RLD | EPITCR_ENMOD | EPITCR_DBGEN | EPITCR_WAITEN | EPITCR_STOPEN, cam->epit_base + EPITCR);
  imx_v4l2_epit_irq_acknowledge(cam);

  return 0;
}

/*****************************************************************************/
static int imx_v4l2_gpio_probe(struct platform_device *pdev, cam_data *cam)
{
  int err = -ENODEV;

  cam->gpio_io3 = of_get_named_gpio(pdev->dev.of_node, "gpio_io3", 0);
  if (!gpio_is_valid(cam->gpio_io3)) 
  {
		dev_warn(&pdev->dev, "no io3 pin available");
		return -EINVAL;
	}
	err = devm_gpio_request_one(&pdev->dev, cam->gpio_io3, GPIOF_OUT_INIT_LOW, "gpio_io3");
	if (err < 0) 
  {
		dev_warn(&pdev->dev, "request of io3 pin failed");
		return err;
	}

  cam->gpio_io4 = of_get_named_gpio(pdev->dev.of_node, "gpio_io4", 0);
  if (!gpio_is_valid(cam->gpio_io4)) 
  {
		dev_warn(&pdev->dev, "no io4 pin available");
		return -EINVAL;
	}
	err = devm_gpio_request_one(&pdev->dev, cam->gpio_io4, GPIOF_OUT_INIT_LOW, "gpio_io4");
	if (err < 0) 
  {
		dev_warn(&pdev->dev, "request of io4 pin failed");
		return err;
	}

  cam->gpio_LED_EN = of_get_named_gpio(pdev->dev.of_node, "gpio_LED_EN", 0);
  if (!gpio_is_valid(cam->gpio_LED_EN)) 
  {
		dev_warn(&pdev->dev, "no LED_EN pin available");
		return -EINVAL;
	}
	err = devm_gpio_request_one(&pdev->dev, cam->gpio_LED_EN, GPIOF_OUT_INIT_LOW, "gpio_LED_EN");
	if (err < 0) 
  {
		dev_warn(&pdev->dev, "request of LED_EN pin failed");
		return err;
	}

  cam->gpio_LASER1 = of_get_named_gpio(pdev->dev.of_node, "gpio_LASER1", 0);
  if (!gpio_is_valid(cam->gpio_LASER1)) 
  {
		dev_warn(&pdev->dev, "no Laser1 pin available");
		return -EINVAL;
	}
	err = devm_gpio_request_one(&pdev->dev, cam->gpio_LASER1, GPIOF_OUT_INIT_LOW, "gpio_LASER1");
	if (err < 0) 
  {
		dev_warn(&pdev->dev, "request of Laser1 pin failed");
		return err;
	}

  cam->gpio_LASER2 = of_get_named_gpio(pdev->dev.of_node, "gpio_LASER2", 0);
  if (!gpio_is_valid(cam->gpio_LASER2)) 
  {
		dev_warn(&pdev->dev, "no Laser2 pin available");
		return -EINVAL;
	}
	err = devm_gpio_request_one(&pdev->dev, cam->gpio_LASER2, GPIOF_OUT_INIT_LOW, "gpio_LASER2");
	if (err < 0) 
  {
		dev_warn(&pdev->dev, "request of Laser2 pin failed");
		return err;
	}

  cam->gpio_LASER3 = of_get_named_gpio(pdev->dev.of_node, "gpio_LASER3", 0);
  if (!gpio_is_valid(cam->gpio_LASER3)) 
  {
		dev_warn(&pdev->dev, "no Laser3 pin available");
		return -EINVAL;
	}
	err = devm_gpio_request_one(&pdev->dev, cam->gpio_LASER3, GPIOF_OUT_INIT_LOW, "gpio_LASER3");
	if (err < 0) 
  {
		dev_warn(&pdev->dev, "request of Laser3 pin failed");
		return err;
	}

  cam->gpio_LED1_EN = of_get_named_gpio(pdev->dev.of_node, "gpio_LED1_EN", 0);
  if (!gpio_is_valid(cam->gpio_LED1_EN)) 
  {
		dev_warn(&pdev->dev, "no LED1_EN pin available");
		return -EINVAL;
	}
	err = devm_gpio_request_one(&pdev->dev, cam->gpio_LED1_EN, GPIOF_OUT_INIT_LOW, "gpio_LED1_EN");
	if (err < 0)
  {
		dev_warn(&pdev->dev, "request of LED1_EN pin failed");
		return err;
	}

  cam->gpio_LED2_EN = of_get_named_gpio(pdev->dev.of_node, "gpio_LED2_EN", 0);
  if (!gpio_is_valid(cam->gpio_LED2_EN)) 
  {
		dev_warn(&pdev->dev, "no LED2_EN pin available");
		return -EINVAL;
	}
	err = devm_gpio_request_one(&pdev->dev, cam->gpio_LED2_EN, GPIOF_OUT_INIT_LOW, "gpio_LED2_EN");
	if (err < 0)
  {
		dev_warn(&pdev->dev, "request of LED2_EN pin failed");
		return err;
	}

  cam->gpio_LED_CUR1 = of_get_named_gpio(pdev->dev.of_node, "gpio_LED_CUR1", 0);
  if (!gpio_is_valid(cam->gpio_LED_CUR1)) 
  {
		dev_warn(&pdev->dev, "no LED_CUR1 pin available");
		return -EINVAL;
	}
	err = devm_gpio_request_one(&pdev->dev, cam->gpio_LED_CUR1, GPIOF_OUT_INIT_LOW, "gpio_LED_CUR1");
	if (err < 0)
  {
		dev_warn(&pdev->dev, "request of LED_CUR1 pin failed");
		return err;
	}

  cam->gpio_LED_CUR2 = of_get_named_gpio(pdev->dev.of_node, "gpio_LED_CUR2", 0);
  if (!gpio_is_valid(cam->gpio_LED_CUR2)) 
  {
		dev_warn(&pdev->dev, "no LED_CUR2 pin available");
		return -EINVAL;
	}
	err = devm_gpio_request_one(&pdev->dev, cam->gpio_LED_CUR2, GPIOF_OUT_INIT_LOW, "gpio_LED_CUR2");
	if (err < 0)
  {
		dev_warn(&pdev->dev, "request of LED_CUR2 pin failed");
		return err;
	}

  cam->gpio_FLEN = of_get_named_gpio(pdev->dev.of_node, "gpio_FLEN", 0);
  if (!gpio_is_valid(cam->gpio_FLEN)) 
  {
		dev_warn(&pdev->dev, "no FLEN pin available");
		return -EINVAL;
	}
	err = devm_gpio_request_one(&pdev->dev, cam->gpio_FLEN, GPIOF_OUT_INIT_LOW, "gpio_FLEN");
	if (err < 0)
  {
		dev_warn(&pdev->dev, "request of FLEN pin failed");
		return err;
	}

  cam->gpio_CHARGE_EN = of_get_named_gpio(pdev->dev.of_node, "gpio_CHARGE_EN", 0);
  if (!gpio_is_valid(cam->gpio_CHARGE_EN)) 
  {
		dev_warn(&pdev->dev, "no CHARGE_EN pin available");
		return -EINVAL;
	}
	err = devm_gpio_request_one(&pdev->dev, cam->gpio_CHARGE_EN, GPIOF_OUT_INIT_LOW, "gpio_CHARGE_EN");
	if (err < 0)
  {
		dev_warn(&pdev->dev, "request of CHARGE_EN pin failed");
		return err;
	}

  return 0;
}

/*********************************************************************************************************************/
/*!
 * This function is called to probe the devices if registered.
 *
 * @param   pdev  the device structure used to give information on which device
 *                to probe
 *
 * @return  The function returns 0 on success and -1 on failure.
 */
static int mxc_v4l2_probe(struct platform_device *pdev)
{
	int err;
  cam_data *cam = NULL;

#ifdef ARANZ_DEBUG
  printk(KERN_ALERT "mxc_v4l2_probe.\n");
#endif

	/* Create cam and initialize it. */
	cam = kmalloc(sizeof(cam_data), GFP_KERNEL);
	if (cam == NULL) 
  {
		pr_err("ERROR: v4l2 capture: failed to register camera\n");
		return -1;
	}

	err = init_camera_struct(cam, pdev);
	if (err)
  {
		return err;
  }
	pdev->dev.release = camera_platform_release;

	/* Set up the v4l2 device and register it*/
	cam->self->priv = cam;
	v4l2_int_device_register(cam->self);

	/* register v4l video device */
	if (video_register_device(cam->video_dev, VFL_TYPE_GRABBER, video_nr) < 0) 
  {
		kfree(cam);
		cam = NULL;
		pr_err("ERROR: v4l2 capture: video_register_device failed\n");
		return -1;
	}

#ifdef ARANZ_DEBUG
	printk(KERN_ALERT "V4L2 device '%s' on IPU%d_CSI%d registered as %s\n",
		cam->video_dev->name,
		cam->ipu_id + 1, cam->csi,
		video_device_node_name(cam->video_dev));
#endif

	if (device_create_file(&cam->video_dev->dev, &dev_attr_fsl_v4l2_capture_property))
  {
		dev_err(&pdev->dev, "Error on creating sysfs file for capture\n");
  }

	if (device_create_file(&cam->video_dev->dev, &dev_attr_fsl_v4l2_overlay_property))
  {
		dev_err(&pdev->dev, "Error on creating sysfs file for overlay\n");
  }

	if (device_create_file(&cam->video_dev->dev, &dev_attr_fsl_csi_property))
  {
		dev_err(&pdev->dev, "Error on creating sysfs file for csi number\n");
  }

  imx_v4l2_epit_probe(pdev, cam);

  imx_v4l2_gpio_probe(pdev, cam);

	return 0;
}

/*********************************************************************************************************************/
/*!
 * This function is called to remove the devices when device unregistered.
 *
 * @param   pdev  the device structure used to give information on which device
 *                to remove
 *
 * @return  The function returns 0 on success and -1 on failure.
 */
static int mxc_v4l2_remove(struct platform_device *pdev)
{
	cam_data *cam = (cam_data *)platform_get_drvdata(pdev);

	if (cam->open_count) 
  {
		pr_err("ERROR: v4l2 capture:camera open -- setting ops to NULL\n");
		return -EBUSY;
	} 
  else 
  {
		struct v4l2_device *v4l2_dev = cam->video_dev->v4l2_dev;
		device_remove_file(&cam->video_dev->dev,
			&dev_attr_fsl_v4l2_capture_property);
		device_remove_file(&cam->video_dev->dev,
			&dev_attr_fsl_v4l2_overlay_property);
		device_remove_file(&cam->video_dev->dev,
			&dev_attr_fsl_csi_property);

		pr_info("V4L2 freeing image input device\n");
		v4l2_int_device_unregister(cam->self);
		video_unregister_device(cam->video_dev);

		if (cam->dummy_frame.vaddress != 0) {
			dma_free_coherent(0, cam->dummy_frame.buffer.length,
					  cam->dummy_frame.vaddress,
					  cam->dummy_frame.paddress);
			cam->dummy_frame.vaddress = 0;
		}

		mxc_free_frame_buf(cam);
		kfree(cam);

		v4l2_device_unregister(v4l2_dev);
		kfree(v4l2_dev);
	}

	pr_info("V4L2 unregistering video\n");
	return 0;
}

/*********************************************************************************************************************/
/*!
 * This function is called to put the sensor in a low power state.
 * Refer to the document driver-model/driver.txt in the kernel source tree
 * for more information.
 *
 * @param   pdev  the device structure used to give information on which I2C
 *                to suspend
 * @param   state the power state the device is entering
 *
 * @return  The function returns 0 on success and -1 on failure.
 */
static int mxc_v4l2_suspend(struct platform_device *pdev, pm_message_t state)
{
	cam_data *cam = platform_get_drvdata(pdev);

	pr_debug("%s\n", __func__);

	if (cam == NULL)
		return -1;

	down(&cam->busy_lock);

	cam->low_power = true;

	if (cam->overlay_on == true)
		stop_preview(cam);
	if ((cam->capture_on == true) && cam->enc_disable)
		cam->enc_disable(cam);

	if (cam->sensor && cam->open_count) {
		if (cam->mclk_on[cam->mclk_source]) {
			ipu_csi_enable_mclk_if(cam->ipu, CSI_MCLK_I2C,
					       cam->mclk_source,
					       false, false);
			cam->mclk_on[cam->mclk_source] = false;
		}
		vidioc_int_s_power(cam->sensor, 0);
	}

	up(&cam->busy_lock);

	return 0;
}

/*********************************************************************************************************************/
/*!
 * This function is called to bring the sensor back from a low power state.
 * Refer to the document driver-model/driver.txt in the kernel source tree
 * for more information.
 *
 * @param   pdev   the device structure
 *
 * @return  The function returns 0 on success and -1 on failure
 */
static int mxc_v4l2_resume(struct platform_device *pdev)
{
	cam_data *cam = platform_get_drvdata(pdev);

	pr_debug("%s\n", __func__);

	if (cam == NULL)
		return -1;

	down(&cam->busy_lock);

	cam->low_power = false;
	wake_up_interruptible(&cam->power_queue);

	if (cam->sensor && cam->open_count) {
		if ((cam->overlay_on == true) || (cam->capture_on == true))
			vidioc_int_s_power(cam->sensor, 1);

		if (!cam->mclk_on[cam->mclk_source]) {
			ipu_csi_enable_mclk_if(cam->ipu, CSI_MCLK_I2C,
					       cam->mclk_source,
					       true, true);
			cam->mclk_on[cam->mclk_source] = true;
		}
	}

	if (cam->overlay_on == true)
		start_preview(cam);
	if (cam->capture_on == true)
		mxc_streamon(cam);

	up(&cam->busy_lock);

	return 0;
}

/*********************************************************************************************************************/
/*!
 * This structure contains pointers to the power management callback functions.
 */
static struct platform_driver mxc_v4l2_driver = {
	.driver = {
		   .name = "mxc_v4l2_capture",
		   .owner = THIS_MODULE,
		   .of_match_table = mxc_v4l2_dt_ids,
		   },
	.id_table = imx_v4l2_devtype,
	.probe = mxc_v4l2_probe,
	.remove = mxc_v4l2_remove,
	.suspend = mxc_v4l2_suspend,
	.resume = mxc_v4l2_resume,
	.shutdown = NULL,
};

/*********************************************************************************************************************/
/*!
 * Initializes the camera driver.
 */
static int mxc_v4l2_master_attach(struct v4l2_int_device *slave)
{
	cam_data *cam = slave->u.slave->master->priv;
	struct v4l2_format cam_fmt;
	int i;
	struct sensor_data *sdata = slave->priv;

#ifdef ARANZ_DEBUG
	//pr_debug("%s:slave.name = %s, master.name = %s\n", __func__, slave->name, slave->u.slave->master->name);
  printk(KERN_ALERT "mxc_v4l2_master_attach slave.name = %s, master.name = %s\n", slave->name, slave->u.slave->master->name);
#endif

	if (slave == NULL) 
  {
		pr_err("ERROR: v4l2 capture: slave parameter not valid.\n");
		return -1;
	}

	if ((sdata->ipu_id != cam->ipu_id) || (sdata->csi != cam->csi) || (sdata->mipi_camera != cam->mipi_camera)) 
  {
		pr_info("%s: ipu(%d:%d)/csi(%d:%d)/mipi(%d:%d) doesn't match\n", __func__, sdata->ipu_id, cam->ipu_id, sdata->csi, cam->csi, sdata->mipi_camera, cam->mipi_camera);
		return -1;
	}

	cam->sensor = slave;

	if (cam->sensor_index < MXC_SENSOR_NUM) 
  {
		cam->all_sensors[cam->sensor_index] = slave;
		cam->sensor_index++;
	} 
  else 
  {
		pr_err("ERROR: v4l2 capture: slave number exceeds the maximum.\n");
		return -1;
	}

	for (i = 0; i < cam->sensor_index; i++) 
  {
		//pr_err("%s: %x\n", __func__, i);
		vidioc_int_dev_exit(cam->all_sensors[i]);
		vidioc_int_s_power(cam->all_sensors[i], 0);
	}

	cam_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vidioc_int_g_fmt_cap(cam->sensor, &cam_fmt);

	/* Used to detect TV in (type 1) vs. camera (type 0)*/
	cam->device_type = cam_fmt.fmt.pix.priv;

	/* Set the input size to the ipu for this device */
	cam->crop_bounds.top = cam->crop_bounds.left = 0;
	cam->crop_bounds.width = cam_fmt.fmt.pix.width;
	cam->crop_bounds.height = cam_fmt.fmt.pix.height;

	/* This also is the max crop size for this device. */
	cam->crop_defrect.top = cam->crop_defrect.left = 0;
	cam->crop_defrect.width = cam_fmt.fmt.pix.width;
	cam->crop_defrect.height = cam_fmt.fmt.pix.height;

	/* At this point, this is also the current image size. */
	cam->crop_current.top = cam->crop_current.left = 0;
	cam->crop_current.width = cam_fmt.fmt.pix.width;
	cam->crop_current.height = cam_fmt.fmt.pix.height;

	pr_debug("End of %s: v2f pix widthxheight %d x %d\n",
		 __func__,
		 cam->v2f.fmt.pix.width, cam->v2f.fmt.pix.height);
	pr_debug("End of %s: crop_bounds widthxheight %d x %d\n",
		 __func__,
		 cam->crop_bounds.width, cam->crop_bounds.height);
	pr_debug("End of %s: crop_defrect widthxheight %d x %d\n",
		 __func__,
		 cam->crop_defrect.width, cam->crop_defrect.height);
	pr_debug("End of %s: crop_current widthxheight %d x %d\n",
		 __func__,
		 cam->crop_current.width, cam->crop_current.height);

	pr_info("%s: ipu%d:/csi%d %s attached %s:%s\n", __func__,
		cam->ipu_id, cam->csi, cam->mipi_camera ? "mipi" : "parallel",
		slave->name, slave->u.slave->master->name);

#ifdef ARANZ_DEBUG
  printk(KERN_ALERT "mxc_v4l2_master_attach: ipu%d:/csi%d %s attached %s:%s\n", 
    cam->ipu_id, cam->csi, cam->mipi_camera ? "mipi" : "parallel",
		slave->name, slave->u.slave->master->name);
#endif

	return 0;
}

/*********************************************************************************************************************/
/*!
 * Disconnects the camera driver.
 */
static void mxc_v4l2_master_detach(struct v4l2_int_device *slave)
{
	unsigned int i;
	cam_data *cam = slave->u.slave->master->priv;

	pr_debug("%s\n", __func__);

	if (cam->sensor_index > 1) {
		for (i = 0; i < cam->sensor_index; i++) {
			if (cam->all_sensors[i] != slave)
				continue;
			/* Move all the sensors behind this
			 * sensor one step forward
			 */
			for (; i <= MXC_SENSOR_NUM - 2; i++)
				cam->all_sensors[i] = cam->all_sensors[i+1];
			break;
		}
		/* Point current sensor to the last one */
		cam->sensor = cam->all_sensors[cam->sensor_index - 2];
	} else
		cam->sensor = NULL;

	cam->sensor_index--;
	vidioc_int_dev_exit(slave);
}

/*********************************************************************************************************************/
DEFINE_MUTEX(camera_common_mutex);

/*********************************************************************************************************************/
void mxc_camera_common_lock(void)
{
	mutex_lock(&camera_common_mutex);
}
EXPORT_SYMBOL(mxc_camera_common_lock);

/*********************************************************************************************************************/
void mxc_camera_common_unlock(void)
{
	mutex_unlock(&camera_common_mutex);
}
EXPORT_SYMBOL(mxc_camera_common_unlock);

/*********************************************************************************************************************/
/*!
 * Entry point for the V4L2
 *
 * @return  Error code indicating success or failure
 */
static __init int camera_init(void)
{
	u8 err = 0;

	pr_debug("%s\n", __func__);

	/* Register the device driver structure. */
	err = platform_driver_register(&mxc_v4l2_driver);
	if (err != 0) 
  {
		pr_err("ERROR: v4l2 capture:camera_init: platform_driver_register failed.\n");
		return err;
	}

	return err;
}

/*********************************************************************************************************************/
/*!
 * Exit and cleanup for the V4L2
 */
static void __exit camera_exit(void)
{
	pr_debug("%s\n", __func__);

	platform_driver_unregister(&mxc_v4l2_driver);
}

module_init(camera_init);
module_exit(camera_exit);

module_param(video_nr, int, 0444);
MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("V4L2 capture driver for Mxc based cameras");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("video");
