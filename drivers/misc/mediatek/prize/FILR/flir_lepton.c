/* flir_lepton.c
   Main source file for the FLIR Lepton VoSPI driver
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/spi/spi.h>
#include <linux/spinlock.h>
#include <linux/pm_qos.h>
#include <asm/uaccess.h>
#include <linux/videodev2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
//#include <media/videobuf2-kmalloc.h>

#include <media/videobuf2-vmalloc.h>
#include <media/videobuf2-memops.h>

//drv add by lipengpen 20240220 start
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/spi/spidev.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <media/videobuf2-dma-contig.h>
//drv add by lipengpen 20240220 end 

#include "flir_lepton.h"
#include "lepton_vospi_funcs.h"

#define TAG "flir_lepton"
#define DEBUG_MODE 
#ifdef DEBUG_MODE
#define LOG_INF(format, args...) pr_info(TAG "[%s][%d] " format, __func__, __LINE__, ##args)
#else
#define LOG_INF(format, args...) pr_debug(TAG "[%s] " format, __func__, ##args)
#endif
#define LOG_ERR(format, args...) pr_err(TAG "[%s][%d] " format, __func__, __LINE__, ##args)

enum lepton_model {
	FLIR_LEPTON2	= 2,
	FLIR_LEPTON3	= 3,
};

enum lepton_sync_state {
	SYNC_STATE_INITIAL = 0,
	SYNC_STATE_NORMAL  = 1,
	SYNC_STATE_RESYNC  = 2,
};

struct spare_spi_buffer {
	unsigned	len;
	void		*rx_buf;
	dma_addr_t	rx_dma;
	void		*tx_buf;
	dma_addr_t	tx_dma;
};

struct lepton {
	struct mutex mutex;
	spinlock_t lock;
	int irq_gpio;
	int irq;
	bool started;
	bool dma_done;
	bool telemetry_enabled;
	bool override_frame_validation;
	struct list_head unfilled_bufs; /* waiting to be filled with data */
	struct spare_spi_buffer spare_buf; /* when not using unfilled_bufs */
	struct v4l2_device *v4l2_dev;
	struct video_device *vid_dev;
	struct vb2_queue *q;
	struct spi_device *spi_dev;
	unsigned int vsync_count;
	unsigned int vsync_missed;
	unsigned int discard_count;
	unsigned int total_discard_count;
	unsigned int total_resync_count;
	unsigned int resync_skip_count;
	unsigned int sparebuf_count;
	enum lepton_sync_state sync_state;
	lepton_vospi_info lep_vospi_info;
	struct lepton_buffer *current_lep_buf;
	struct spi_transfer *spi_xfer;
	struct spi_message *spi_msg;
//drv add by lipengpeng 20240316 start 
	//struct pm_qos_request pm_qos_req;
	struct dev_pm_qos_request pm_qos_req;
//drv add by lipengpeng 20240316 end 
};

struct lepton_buffer {
	struct vb2_buffer buf;
	struct list_head list;
};

/*
 * module parameters
 */
 
//drv add by lipengpeng 20240219 start 
struct device *filr_dev = NULL;
//static int lep_irq_gpio;
static struct pinctrl *lepton_pinctrl;
static struct pinctrl_state *lepton_default;
static struct pinctrl_state *lepton_irq_active;

static struct pinctrl_state *lepton_reset_active;
static struct pinctrl_state *lepton_reset_suspend;

//static struct pinctrl_state *lepton_miso_spi_active;

static struct pinctrl_state *lepton_pdn_high_active;
static struct pinctrl_state *lepton_pdn_low_suspend;

static struct pinctrl_state *lepton_vdd_high_active;
static struct pinctrl_state *lepton_vdd_low_suspend;
static struct pinctrl_state *lepton_vddio_high_active;
static struct pinctrl_state *lepton_vddio_low_suspend;
static struct pinctrl_state *lepton_vddc_high_active;
static struct pinctrl_state *lepton_vddc_low_suspend;

static struct pinctrl_state *lepton_miso_pull_active;
static struct pinctrl_state *lepton_miso_pull_suspend;

static struct pinctrl_state *lepton_mosi_pull_active;
static struct pinctrl_state *lepton_mosi_pull_suspend;
static struct pinctrl_state *lepton_spi_set;

//drv add by lipengpeng 20240219 end  

static int lepton_model = 2; /* default to lepton 3.x */  //2.5 lepton_model=2 ?   3.5 lepton_model=3?
module_param(lepton_model, int, S_IRUGO);
MODULE_PARM_DESC(lepton_model, "lepton model: 2 or 3");

static int telemetry = 1; /* default to telemetry off */
module_param(telemetry, int, S_IRUGO);
MODULE_PARM_DESC(telemetry, "whether lepton has been configured (via i2c) to send a telemetry line");

/*
 * interface to userspace apps: V4L2 video device
 */

static int lepton_querycap(struct file *file, void *priv,
			struct v4l2_capability *cap)
{
			pr_debug("lepton_querycap\n");
	strlcpy(cap->driver, LEPTON_MODULE_NAME, sizeof(cap->driver));
	strlcpy(cap->card, "FLIR Lepton", sizeof(cap->driver));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s", LEPTON_MODULE_NAME);
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_READWRITE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int lepton_enum_input(struct file *file, void *priv,
			 struct v4l2_input *inp)
{
	if (inp->index > 0)
		return -EINVAL;
LOG_INF("lepton_enum_input\n");
	inp->type = V4L2_INPUT_TYPE_CAMERA;
	snprintf(inp->name, sizeof(inp->name), LEPTON_MODULE_NAME);
	inp->capabilities = 0;
	return 0;
}

static int lepton_s_input(struct file *file, void *priv, unsigned int i)
{
	// only 1 input
	if (i != 0)
		return -EINVAL;
	return 0;
}
static int lepton_g_input(struct file *file, void *priv, unsigned int *i)
{
	// only 1 input
	*i = 0;
	return 0;
}
static int lepton_querystd(struct file *file, void *fh, v4l2_std_id *std)
{
	// nothing to say about the video standard
	return -ENODATA;
}
static int lepton_s_std(struct file *file, void *fh, v4l2_std_id std)
{
	return -ENODATA;
}
static int lepton_g_std(struct file *file, void *fh, v4l2_std_id *std)
{
	return -ENODATA;
}
static int lepton_enum_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_fmtdesc *f)
{
		LOG_INF("lepton_enum_fmt_vid_cap\n");
	if (f->index != 0)
		return -EINVAL;
	f->pixelformat = V4L2_PIX_FMT_Y16;
	return 0;
}
static int lepton_set_fmt_fields(struct lepton *lep, struct v4l2_format *f)
{
	struct v4l2_pix_format *pix = NULL;

	pix = &f->fmt.pix;
	LOG_INF("lepton_set_fmt_fields\n");
	pix->width = LEPTON_SUBFRAME_LINE_WORD_COUNT;
	pix->height = lep->lep_vospi_info.subframe_params.line_count;
	pix->pixelformat = V4L2_PIX_FMT_Y16;  // 16-bit grayscale
	pix->colorspace = V4L2_COLORSPACE_RAW;
	pix->bytesperline = LEPTON_SUBFRAME_LINE_BYTE_WIDTH;
	pix->sizeimage = lep->lep_vospi_info.subframe_params.subframe_data_byte_size;
	return 0;
}
static int lepton_s_parm(struct file *file, void *priv,
			     struct v4l2_streamparm *parm)
{
	if (parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	LOG_INF("lepton_s_parm\n");
	parm->parm.capture.timeperframe.numerator = 1;
	parm->parm.capture.timeperframe.denominator = 30;
	parm->parm.capture.readbuffers  = 1;

	return 0;
}
static int lepton_g_parm(struct file *file, void *priv,
			     struct v4l2_streamparm *parm)
{
	if (parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	LOG_INF("lepton_g_parm\n");
	parm->parm.capture.timeperframe.numerator = 1;
	parm->parm.capture.timeperframe.denominator = 30;
	parm->parm.capture.readbuffers  = 1;
	return 0;
}
static int lepton_g_fmt_vid_cap(struct file *file, void *priv,
			     struct v4l2_format *f)
{
	struct lepton *lep = NULL;
	LOG_INF("lepton_g_fmt_vid_cap\n");
	lep = video_drvdata(file);

	lepton_set_fmt_fields(lep, f);
	return 0;
}
static int lepton_try_fmt_vid_cap(struct file *file, void *priv,
			       struct v4l2_format *f)
{
	struct lepton *lep = NULL;
	LOG_INF("lepton_try_fmt_vid_cap\n");
	lep = video_drvdata(file);
	// we only ever have one format, so always send it back.
	lepton_set_fmt_fields(lep, f);
	return 0;
}
static int lepton_s_fmt_vid_cap(struct file *file, void *priv,
			     struct v4l2_format *f)
{
	struct lepton *lep = NULL;
	LOG_INF("lepton_s_fmt_vid_cap\n");
	lep = video_drvdata(file);
	// we only ever have one format, so always send it back.
	lepton_set_fmt_fields(lep, f);
	return 0;
}
static int lepton_enum_frameintervals(struct file *file, void *priv,
				   struct v4l2_frmivalenum *f)
{
	LOG_INF("lepton_enum_frameintervals\n");
	if (f->index != 0)
		return -EINVAL;
	f->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	// always runs at 30Hz
	f->discrete.numerator = 1;
	f->discrete.denominator = 30;
	return 0;
}
static int lepton_enum_framesizes(struct file *file, void *priv,
			       struct v4l2_frmsizeenum *f)
{
	struct lepton *lep = NULL;
LOG_INF("lepton_enum_framesizes\n");
	lep = video_drvdata(file);
	if (f->index != 0)
		return -EINVAL;
	f->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	f->discrete.width = LEPTON_SUBFRAME_LINE_WORD_COUNT;
	f->discrete.height = lep->lep_vospi_info.subframe_params.line_count;
	return 0;
}

static struct v4l2_file_operations lepton_fops = {
	.owner =    THIS_MODULE,
	.open =     v4l2_fh_open,
	.release =  vb2_fop_release,
	.read =     vb2_fop_read,    
	.poll =     vb2_fop_poll,
	.mmap =     vb2_fop_mmap,
	.unlocked_ioctl = video_ioctl2,
};

static const struct v4l2_ioctl_ops lepton_ioctl_ops = {
	.vidioc_s_parm			= lepton_s_parm,
	.vidioc_g_parm			= lepton_g_parm,
	.vidioc_querycap	= lepton_querycap,
	.vidioc_enum_input	= lepton_enum_input,
	.vidioc_g_input		= lepton_g_input,
	.vidioc_s_input		= lepton_s_input,

	.vidioc_querystd	= lepton_querystd,
	.vidioc_g_std		= lepton_g_std,
	.vidioc_s_std		= lepton_s_std,

	.vidioc_enum_fmt_vid_cap = lepton_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap	= lepton_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap	= lepton_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap	= lepton_s_fmt_vid_cap,
	.vidioc_enum_frameintervals	= lepton_enum_frameintervals,
	.vidioc_enum_framesizes		= lepton_enum_framesizes,

	.vidioc_reqbufs		= vb2_ioctl_reqbufs,
	.vidioc_create_bufs	= vb2_ioctl_create_bufs,
	.vidioc_prepare_buf	= vb2_ioctl_prepare_buf,
	.vidioc_querybuf	= vb2_ioctl_querybuf,
	.vidioc_qbuf		= vb2_ioctl_qbuf,
	.vidioc_dqbuf		= vb2_ioctl_dqbuf,
	.vidioc_expbuf		= vb2_ioctl_expbuf,

	.vidioc_streamon	= vb2_ioctl_streamon,
	.vidioc_streamoff	= vb2_ioctl_streamoff,

	.vidioc_log_status	= v4l2_ctrl_log_status,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static struct video_device lepton_videodev_template = {
	.name		= LEPTON_MODULE_NAME,
	.fops		= &lepton_fops,
	.ioctl_ops	= &lepton_ioctl_ops,
	.minor		= -1,
	.release	= video_device_release,
};

/*
 * video buffer queue management
 */

int lepton_queue_setup(struct vb2_queue *vq,
			   unsigned int *nbuffers, unsigned int *nplanes,
			   unsigned int sizes[], struct device *alloc_ctxs[])
{
	struct lepton *lep = NULL;
	unsigned int size = -1;
    LOG_INF("lepton_queue_setup\n");
	lep = vb2_get_drv_priv(vq);
	size = lep->lep_vospi_info.subframe_params.subframe_data_byte_size;
	if (*nplanes)
	{
		// only allow 1 plane per buffer, and verify size is large enough
		if ((*nplanes != 1) || (sizes[0] < size))
			return -EINVAL;
		size = sizes[0];
	}
	if (vq->num_buffers + *nbuffers < 2)
		*nbuffers = 2 - vq->num_buffers;
	*nplanes = 1;
	sizes[0] = size;
	pr_debug("get %d buffers, each holding %d bytes.\n", *nbuffers, sizes[0]);
	return 0;
}

int lepton_buf_prepare(struct vb2_buffer *vb)
{
	struct lepton *lep = NULL;
	LOG_INF("lepton_buf_prepare\n");
	lep = vb2_get_drv_priv(vb->vb2_queue);
	if (vb2_plane_size(vb, 0) < lep->lep_vospi_info.subframe_params.subframe_data_byte_size)
	{
		pr_debug("%s: data will not fit into buf size %ld\n", __func__, vb2_plane_size(vb, 0));
		return -EINVAL;
	}

	/* amount of data that will be filled in this buffer,
	 * which will get passed to userspace client in buffer descriptor */
	vb2_set_plane_payload(vb, 0, lep->lep_vospi_info.subframe_params.subframe_data_byte_size);
	return 0;
}

void lepton_buf_queue(struct vb2_buffer *vb)
{
	struct lepton_buffer *buf = container_of(vb, struct lepton_buffer, buf);
	struct lepton *lep = vb2_get_drv_priv(vb->vb2_queue);
	unsigned long flags;
	LOG_INF("lepton_buf_queue\n");

	spin_lock_irqsave(&lep->lock, flags);
	list_add_tail(&buf->list, &lep->unfilled_bufs);
	spin_unlock_irqrestore(&lep->lock, flags);
}

int lepton_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct lepton *lep = vb2_get_drv_priv(vq);
	unsigned long flags;

	/* request low latency from power management while streaming */
//drv add by lipengpeng 20240220 start 
	//lep->pm_qos_req.type = PM_QOS_REQ_AFFINE_IRQ;
	//lep->pm_qos_req.irq = lep->irq;
//drv add by lipengpeng 20240220 end 
	//drv add by lipengpeng 20240220 start 
	//dev_pm_qos_add_request(&lep->pm_qos_req, PM_QOS_CPU_DMA_LATENCY, 100);  
	dev_pm_qos_add_request(filr_dev,&lep->pm_qos_req, DEV_PM_QOS_RESUME_LATENCY, 100);  
	//drv add by lipengpeng 20240220 end  
    LOG_INF("lepton_start_streaming\n");
	spin_lock_irqsave(&lep->lock, flags);
	lep->started = 1;
	spin_unlock_irqrestore(&lep->lock, flags);
	return 0;
}

void lepton_stop_streaming(struct vb2_queue *vq)
{
	struct lepton *lep = vb2_get_drv_priv(vq);
	struct lepton_buffer *lep_buf = NULL;
	struct list_head *pos, *q;
	unsigned long flags;

	/* remove request to power management for low latency */
//drv add by lipengpeng 20240220 start 	
	dev_pm_qos_remove_request(&lep->pm_qos_req);
//drv add by lipengpeng 20240220 end  
    LOG_INF("lepton_stop_streaming\n");
	spin_lock_irqsave(&lep->lock, flags);
	list_for_each_safe(pos, q, &lep->unfilled_bufs) {
		lep_buf = list_entry(pos, struct lepton_buffer, list);
		vb2_buffer_done(&lep_buf->buf, VB2_BUF_STATE_ERROR);
		list_del(&lep_buf->list);
	}

	lep->started = 0;
	spin_unlock_irqrestore(&lep->lock, flags);
	vb2_wait_for_all_buffers(lep->q);
}

static const struct vb2_ops lepton_video_qops = {
	.queue_setup     = lepton_queue_setup,
	.buf_prepare     = lepton_buf_prepare,
	.buf_queue       = lepton_buf_queue,
	.start_streaming = lepton_start_streaming,
	.stop_streaming	 = lepton_stop_streaming,
	.wait_prepare    = vb2_ops_wait_prepare,
	.wait_finish     = vb2_ops_wait_finish,
};

/*
 * tables to find matching device-tree entries
 *
 * loaded device-tree will be searched for matching "compatible" strings,
 * and driver probe function will be called on any matched nodes
 */

static const struct spi_device_id lepton_id_table[] = {
	{
		.name		= "lepton2",
		.driver_data	= (kernel_ulong_t)FLIR_LEPTON2,
	},
	{
		.name		= "lepton3",
		.driver_data	= (kernel_ulong_t)FLIR_LEPTON3,
	},
	{ }
};
MODULE_DEVICE_TABLE(spi, lepton_id_table);

static const struct of_device_id lepton_of_match[] = {
	{
		.compatible	= "flir,lepton2",
		.data		= (void *)FLIR_LEPTON2,
	},
	{
		.compatible	= "flir,lepton3",
		.data		= (void *)FLIR_LEPTON3,
	},
	{ }
};
MODULE_DEVICE_TABLE(of, lepton_of_match);

/*
 * toplevel module setup and teardown
 */

static void lepton_spi_done_callback(void *context)
{
	struct lepton *lep = (struct lepton *)context;
	unsigned long flags;
	unsigned char *subframe_data = NULL;
	struct lepton_buffer *lep_buf = NULL;
	bool subframe_is_good = false, subframe_is_duplicate = false, reuse_buffer = false;


	/* current_lep_buf will be NULL if spare buffer was in use;
	 * non-NULL if instead data was transferred to a V4L-allocated buffer
	 * for consumption by userspace
	 */
	lep_buf = lep->current_lep_buf;

	/* analyze data to decide if data is synced up into proper subframes yet,
	 * so that data can be sent to userspace when V4L buffers are available
	 */

	subframe_data = lep->spi_xfer->rx_buf;
	/* check line counter in last line *not* counting telemetry, 
	   to avoid complication of offset varying by lepton type 
	   and telemetry setting */
	subframe_is_good = subframe_data[LEPTON_SUBFRAME_LINE_BYTE_WIDTH*59+1] == 59;
	if (subframe_is_good) {
		/* reset counter of consecutive discarded subframes */
		lep->discard_count = 0;
	}
	else {
		lep->total_discard_count++;
		lep->discard_count++;
	}

	/* look for non-zero subframe counter to see if this is new data
	   or just a duplicate to be thrown away.  NOTE: only for lepton 3 */
    LOG_INF("lepton_spi_done_callback111 lepton_model=%d lep->override_frame_validation=%d subframe_is_good=%d\n",lepton_model,lep->override_frame_validation,subframe_is_good);	   

	if (lepton_model == 2) {
		subframe_is_duplicate = 0;
	} else {
		subframe_is_duplicate = (subframe_data[LEPTON_SUBFRAME_LINE_BYTE_WIDTH*20] & 0x70) == 0;
	}

	/* can force all subframes to be accepted, even if discard data */
	if (lep->override_frame_validation) {
		subframe_is_good = 1;
		subframe_is_duplicate = 0;
		lep->discard_count = 0;
	}

	/* find buffer for next subframe, 
	 * unless this one was bad (invalid or a duplicate) so buffer can be reused 
	 */
	LOG_INF("lepton_spi_done_callback222 lep_buf=%p subframe_is_duplicate=%d subframe_is_good=%d\n",lep_buf,subframe_is_duplicate,subframe_is_good);	 
	reuse_buffer = lep_buf != NULL && (!subframe_is_good || subframe_is_duplicate);

	spin_lock_irqsave(&lep->lock, flags);
        if (!reuse_buffer) {
		if (!list_empty(&lep->unfilled_bufs)) {
			lep->current_lep_buf = list_first_entry(&lep->unfilled_bufs, struct lepton_buffer, list);

			list_del(&lep->current_lep_buf->list);
		} else {
			lep->current_lep_buf = NULL;
			lep->sparebuf_count++;
		}
	}

	/* now that next buffer is selected, ready for VSYNC handler to run again */
	lep->dma_done = true;
	spin_unlock_irqrestore(&lep->lock, flags);

	/* if validation check passed, 
	 * V4L buffers need to be dispatched back to userspace
	 */
	if (lep_buf && subframe_is_good && !subframe_is_duplicate) {
		vb2_buffer_done(&lep_buf->buf, VB2_BUF_STATE_DONE); //缓冲区将会填充满需要传递到用户空间的帧数据
	}

}

/* kick off a SPI transfer in interrupt context */
static void lepton_start_transfer(struct lepton *lep, void *rx_buf, dma_addr_t rx_dma, size_t rx_len)
{
	unsigned long flags;
//drv modified by wangmd 20240719 start
	int ret=-1;
	
	unsigned char *subframe_data = NULL;
	struct lepton_buffer *lep_buf = NULL;
	bool subframe_is_good = false, subframe_is_duplicate = false, reuse_buffer = false;
//drv modified by wangmd 20240719 end
	/* SPI message consists of one or more transfers,
	 * in this case only one */
	LOG_INF("lepton_start_transfer111\n");
	spin_lock_irqsave(&lep->lock, flags);
	spi_message_init(lep->spi_msg);
	lep->spi_msg->complete = lepton_spi_done_callback;
	lep->spi_msg->context = (void *)lep;
	lep->spi_msg->is_dma_mapped = 0;

	/* always use same zero-filled buffer for TX -- outgoing data is always zeros */
	lep->spi_xfer->tx_buf = lep->spare_buf.tx_buf;
	lep->spi_xfer->tx_dma = 0;

	lep->spi_xfer->rx_buf = rx_buf;
	lep->spi_xfer->rx_dma = 0;
	lep->spi_xfer->len = rx_len;
	memset(rx_buf, 0, rx_len);
	LOG_INF("lepton_start_transfer222 lep->spi_dev->max_speed_hz=%d\n",lep->spi_xfer->speed_hz);	
//drv add by lipengpeng 20240220 start
	//lep->spi_xfer->delay_usecs = 0;  //cs delay???   
	lep->spi_xfer->cs_change_delay.value = 0;  //delay ?  word_delay ?
	lep->spi_xfer->cs_change_delay.unit = SPI_DELAY_UNIT_NSECS;
//drv add by lipengpeng 20240220 end 
	lep->spi_xfer->speed_hz = lep->spi_dev->max_speed_hz;//lep->spi_dev->max_speed_hz;
	LOG_INF("lepton_start_transfer333 lep->spi_dev->max_speed_hz=%d\n",lep->spi_dev->max_speed_hz);
	/* assign this one transfer to message and send it to controller */
#if 1
	LOG_INF("lepton_start_transfer SPI xfer %u bytes tx %p/%pad rx %p/%pad\n", lep->spi_xfer->len,lep->spi_xfer->tx_buf, &lep->spi_xfer->tx_dma, lep->spi_xfer->rx_buf, &lep->spi_xfer->rx_dma);
#endif
	spi_message_add_tail(lep->spi_xfer, lep->spi_msg);
	spin_unlock_irqrestore(&lep->lock, flags);
//drv add by lipengpeng 20240219 start 
//drv modified by wangmd 20240719 start
	ret = spi_sync(lep->spi_dev, lep->spi_msg);
//drv modified by wangmd 20240719 end
	//spi_sync(lep->spi_dev, lep->spi_msg);
//drv add by lipengpeng 20240219 end 
//drv modified by wangmd 20240719 start

	/* current_lep_buf will be NULL if spare buffer was in use;
	 * non-NULL if instead data was transferred to a V4L-allocated buffer
	 * for consumption by userspace
	 */
	lep_buf = lep->current_lep_buf;

	/* analyze data to decide if data is synced up into proper subframes yet,
	 * so that data can be sent to userspace when V4L buffers are available
	 */

	subframe_data = lep->spi_xfer->rx_buf;
	/* check line counter in last line *not* counting telemetry, 
	   to avoid complication of offset varying by lepton type 
	   and telemetry setting */
	subframe_is_good = subframe_data[LEPTON_SUBFRAME_LINE_BYTE_WIDTH*59+1] == 59;
	if (subframe_is_good) {
		/* reset counter of consecutive discarded subframes */
		lep->discard_count = 0;
	}
	else {
		lep->total_discard_count++;
		lep->discard_count++;
	}

	/* look for non-zero subframe counter to see if this is new data
	   or just a duplicate to be thrown away.  NOTE: only for lepton 3 */
    LOG_INF("lepton_spi_done_callback111 lepton_model=%d lep->override_frame_validation=%d subframe_is_good=%d\n",lepton_model,lep->override_frame_validation,subframe_is_good);	   

	if (lepton_model == 2) {
		subframe_is_duplicate = 0;
	} else {
		subframe_is_duplicate = (subframe_data[LEPTON_SUBFRAME_LINE_BYTE_WIDTH*20] & 0x70) == 0;
	}

	/* can force all subframes to be accepted, even if discard data */
	if (lep->override_frame_validation) {
		subframe_is_good = 1;
		subframe_is_duplicate = 0;
		lep->discard_count = 0;
	}

	/* find buffer for next subframe, 
	 * unless this one was bad (invalid or a duplicate) so buffer can be reused 
	 */
	LOG_INF("lepton_spi_done_callback222 lep_buf=%p subframe_is_duplicate=%d subframe_is_good=%d\n",lep_buf,subframe_is_duplicate,subframe_is_good);	 
	reuse_buffer = lep_buf != NULL && (!subframe_is_good || subframe_is_duplicate);

	spin_lock_irqsave(&lep->lock, flags);
        if (!reuse_buffer) {
		if (!list_empty(&lep->unfilled_bufs)) {
			lep->current_lep_buf = list_first_entry(&lep->unfilled_bufs, struct lepton_buffer, list);

			list_del(&lep->current_lep_buf->list);
		} else {
			lep->current_lep_buf = NULL;
			lep->sparebuf_count++;
		}
	}

	/* now that next buffer is selected, ready for VSYNC handler to run again */
	lep->dma_done = true;
	spin_unlock_irqrestore(&lep->lock, flags);

	/* if validation check passed, 
	 * V4L buffers need to be dispatched back to userspace
	 */
	if (lep_buf && subframe_is_good && !subframe_is_duplicate) {
		vb2_buffer_done(&lep_buf->buf, VB2_BUF_STATE_DONE); //缓冲区将会填充满需要传递到用户空间的帧数据
	}
//drv modified by wangmd 20240719 end
}

/* 
 * Manage lepton sync state
 *
 * If currently in resync state,
 * - bump counter for skips during resync
 * - if counter for DMA skips during resync passes threshold
 *   - enter normal resync state
 *   - reset counter for DMA skips during resync
 *   - reset counter for consecutive discards
 *
 * Otherwise not in resync state,
 * - if counter for consecutive discards passes threshold
 *   - enter resync state
 */
static int lepton_update_sync_state(struct lepton *lep)
{
	if (lep->sync_state == SYNC_STATE_RESYNC) {
		lep->resync_skip_count++;
		if (lep->resync_skip_count > MAX_RESYNC_SKIP_COUNT)
		{
			lep->sync_state = SYNC_STATE_NORMAL;
			lep->discard_count = 0;
			lep->resync_skip_count = 0;
		}
	} else {
		if (lep->discard_count > MAX_CONSEC_DISCARD_COUNT) {
			lep->sync_state = SYNC_STATE_RESYNC;
			lep->total_resync_count++;
		}
	}

	return lep->sync_state;
}

static irqreturn_t lepton_vsync_handler(int irq, void *data)
{
	struct spi_device *spi = (struct spi_device *)data;
	struct device *dev = NULL;
	struct lepton *lep = NULL;
	struct lepton_buffer *lep_buf = NULL;
	unsigned long flags;
	unsigned long *vaddr = NULL;
//drv add by lipengpeng 20240220 start 
	//dma_addr_t dma_addr;
	dma_addr_t dma_addr=0;
//drv add by lipengpeng 20240220 end 
	unsigned rx_len;

	dev = &spi->dev;
	lep = dev_get_drvdata(dev);

#if 1
	//if (LOG_INF_ratelimit()) {
		LOG_INF("lepton_vsync_handler VSYNC %d", lep->vsync_count);
		// LOG_INF(KERN_INFO "spi=%p dev=%p lep=%p\n", spi, dev, lep);
	//}
#endif

	spin_lock_irqsave(&lep->lock, flags);
	lep->vsync_count++;

	/* Do not kick off another DMA if the previous has not
	 * finished (which is a SERIOUS problem, since missing subframes
	 * can knock lepton into a bad state that requires hardware reset 
	 */

	if (lep->dma_done == false) {
		lep->vsync_missed++;
		spin_unlock_irqrestore(&lep->lock, flags);
		return IRQ_HANDLED;
	}

	/* skip DMA while lepton is resyncing */
	if (lepton_update_sync_state(lep) == SYNC_STATE_RESYNC) {
		spin_unlock_irqrestore(&lep->lock, flags);
		return IRQ_HANDLED;
	}

	lep_buf = lep->current_lep_buf;
	lep->dma_done = false; /* reset flag for upcoming spi transfer */
	spin_unlock_irqrestore(&lep->lock, flags);

	/* driver provides a spare buffer for two purposes:
	 * - achieving sync of video frames
	 * - place to stash SPI data when no V4L buffers are available
	 *
	 * Achieving sync:
	 *   When video streaming starts up, there can be some extra lines
	 * of data before the real start of frame (marked by line counter=0).
	 * In that case driver will "catch up" by using the spare buffer,
	 * which has one extra line worth of space. Reading out full frame
	 * plus a line will gradually drain out the extra data until a frame 
	 * starts with line 0 at the beginning. On that first synced frame,
	 * the extra transferred line will be a "discard packet" which can 
	 * just be ignored.
	 *
	 * Fallback when no V4L buffers are available:
	 *   After the extra lines have been cleared out, frame-sized
	 * buffers allocated by V4L layer can be used to receive data
	 * that will then be passed to userspace. However, it is up
	 * to userspace to allocate V4L buffers and to keep returning
	 * them to the driver. The lepton is most stable if data
	 * is *always* clocked out in a timely manner, so the spare buffer
	 * is used to clock out data when no V4L buffer is available.
	 */
	if (lep_buf) {
		/* kick off spi read to V4L buffer */
		vaddr = vb2_plane_vaddr(&lep_buf->buf, 0);
		rx_len = lep->lep_vospi_info.subframe_params.subframe_data_byte_size;
		lepton_start_transfer(lep, vaddr, dma_addr, rx_len);
	}
	else {
		/* kick off spi read to spare buffer, which has an extra line */
		rx_len = lep->spare_buf.len;
		lepton_start_transfer(lep, lep->spare_buf.rx_buf, lep->spare_buf.rx_dma, rx_len);
	}

	return IRQ_HANDLED;
}


//drv add by lipengpeng 20240413 start 
void filr_power_enable(int status)
{
	if(status==1)
	{
	  LOG_INF("filr power on start");	
	  pinctrl_select_state(lepton_pinctrl, lepton_vdd_high_active);  //enable vdd3.0v
	  mdelay(5);
	  pinctrl_select_state(lepton_pinctrl, lepton_vddio_high_active);  //enable vddio2.8v
	  mdelay(5);
	  pinctrl_select_state(lepton_pinctrl, lepton_vddc_high_active);  //enable vddc1.2v
	  mdelay(5);
	  pinctrl_select_state(lepton_pinctrl, lepton_pdn_high_active);  //pdn set high
	  mdelay(5);
	  pinctrl_select_state(lepton_pinctrl, lepton_reset_suspend);  //reset low
	  mdelay(5);
	  pinctrl_select_state(lepton_pinctrl, lepton_reset_active);  //reset high
	  mdelay(5);
	  LOG_INF("filr power on end");  
	}else{
		
	  pinctrl_select_state(lepton_pinctrl, lepton_vdd_low_suspend);  //disable vdd3.0v
	  mdelay(5);
	  pinctrl_select_state(lepton_pinctrl, lepton_vddio_low_suspend);  //disable vddio2.8v
	  mdelay(5);
	  pinctrl_select_state(lepton_pinctrl, lepton_vddc_low_suspend);  //disable vddc1.2v
	  mdelay(5);
	  pinctrl_select_state(lepton_pinctrl, lepton_pdn_low_suspend);  //pdn set low
	  mdelay(5);
	  pinctrl_select_state(lepton_pinctrl, lepton_reset_suspend);  //reset low
	  
	}
}
EXPORT_SYMBOL(filr_power_enable);

//drv add by lipengpeng 20240413 end 
static ssize_t subframe_stats_show(struct device *device,
			       struct device_attribute *attr, char *buf)
{
	struct lepton *lep = dev_get_drvdata(device);
	return sprintf(buf, "vsync=%u missed=%u discard=%u sparebuf=%u resync=%u\n", 
		lep->vsync_count, lep->vsync_missed, lep->total_discard_count, lep->sparebuf_count, lep->total_resync_count);
}

static ssize_t subframe_validation_show(struct device *device,
			       struct device_attribute *attr, char *buf)
{
	struct lepton *lep = dev_get_drvdata(device);
	/* when override is 0, validation is on */
	return sprintf(buf, "%s\n", lep->override_frame_validation ? "off":"on");
}

static ssize_t subframe_validation_store(struct device *device,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct lepton *lep = dev_get_drvdata(device);
//2.8V=gpio24  3.0V=gpio150  1.2V=gpio30  irq/vsync=gpio34 reset=gpio28  pdn=gpio23
	switch (buf[0])
	{
		/* 0 = disable validation */
		case '0': 
		      lep->override_frame_validation = 1; 			  
			  break;
		/* 1 = enable validation */
		case '1': 
		      lep->override_frame_validation = 0;
			  break;
	}	
	
	return strlen(buf);
}

//drv add by lipengpeng 20240312 start 
static int power_status=0;
static ssize_t filr_power_show(struct device *device,
			       struct device_attribute *attr, char *buf)
{
	//struct lepton *lep = dev_get_drvdata(device);
	return sprintf(buf, "%d\n", power_status);
}

static ssize_t filr_power_store(struct device *device,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	LOG_INF("filr_power_store =%d\n",buf[0]);
//2.8V=gpio24  3.0V=gpio150  1.2V=gpio30  irq/vsync=gpio34 reset=gpio28  pdn=gpio23
	switch (buf[0])
	{
		case '0': 
		      power_status = 0; 
//drv add by lipengpeng 20240305 start power off start
			  pinctrl_select_state(lepton_pinctrl, lepton_vdd_low_suspend);  //disable vdd3.0v
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_vddio_low_suspend);  //disable vddio2.8v
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_vddc_low_suspend);  //disable vddc1.2v
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_pdn_low_suspend);  //pdn set low
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_reset_suspend);  //reset low
//drv add by lipengpeng 20240305 end 			  
			  break;
		case '1': 
		      power_status = 1;
//drv add by lipengpeng 20240305 start power on start
			  pinctrl_select_state(lepton_pinctrl, lepton_vdd_high_active);  //enable vdd3.0v
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_vddio_high_active);  //enable vddio2.8v
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_vddc_high_active);  //enable vddc1.2v
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_pdn_high_active);  //pdn set high
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_reset_suspend);  //reset low
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_reset_active);  //reset high
			  mdelay(5);
//drv add by lipengpeng 20240305 end 
			  break;
	}	
	
	return strlen(buf);
}
//drv add by lipengpeng 20240312 end 

//drv add by lipengpeng 20240415 start 
int filr_status=0;
static ssize_t filr_status_show(struct device *device,
			       struct device_attribute *attr, char *buf)
{
	//struct lepton *lep = dev_get_drvdata(device);
	return sprintf(buf, "%d\n", filr_status);
}
EXPORT_SYMBOL(filr_status);
//drv add by lipengpeng 20240415 end 

static DEVICE_ATTR(subframe_stats, 0444, subframe_stats_show, NULL);
static DEVICE_ATTR(subframe_validation, 0644, subframe_validation_show, subframe_validation_store);
static DEVICE_ATTR(filr_power, 0644, filr_power_show, filr_power_store);
//drv add by lipengpeng 20240415 start 
static DEVICE_ATTR(filr_status, 0644, filr_status_show, NULL);
//drv add by lipengpeng 20240415 end 

static struct attribute *lepton_attributes[] = {
	&dev_attr_subframe_stats.attr,
	&dev_attr_subframe_validation.attr,
	&dev_attr_filr_power.attr,
	&dev_attr_filr_status.attr,
	NULL
};

static const struct attribute_group lepton_attr_group = {
	.attrs = lepton_attributes,
};

//drv add by lipengpen 20240220 start
static int lepton_get_gpio_dts_info(struct device *dev)
{
	int ret = 0;
/*	struct device_node *node = NULL;
	struct platform_device *pdev = NULL;

	LOG_INF("lepton_get_gpio_dts_info %s line = %d\n", __func__, __LINE__);

	node = of_find_compatible_node(NULL, NULL, "mediatek,lepton_filr");
	if (node) {
		pdev = of_find_device_by_node(node);
		if (pdev) {
			lepton_pinctrl = devm_pinctrl_get(&pdev->dev);
			if (IS_ERR(lepton_pinctrl)) {
				ret = PTR_ERR(lepton_pinctrl);
				LOG_INF("failed can't find lepton filr pinctrl\n");
				return ret;
			}
		} else {
			LOG_INF("failed platform device is null\n");
		}
	} else {
		LOG_INF("failed device node is null\n");
	}
*/
	LOG_INF("lepton_get_gpio_dts_info %s line = %d\n", __func__, __LINE__);
	
	/* get pinctrl */
	lepton_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(lepton_pinctrl)) {
		LOG_ERR("Failed to get devm_pinctrl_get ret = %d\n",IS_ERR(lepton_pinctrl));
		ret = PTR_ERR(lepton_pinctrl);
		return ret;
	}	
	
    LOG_INF("of_find_compatible_node  111\n");
	
	lepton_default = pinctrl_lookup_state(lepton_pinctrl, "default");
	if (IS_ERR(lepton_default)) {
		LOG_ERR("Failed to init default\n");
		ret = PTR_ERR(lepton_default);
	}else{
		ret = pinctrl_select_state(lepton_pinctrl, lepton_default);
		LOG_INF("ret:%d\n",ret);			
	}

	lepton_irq_active = pinctrl_lookup_state(lepton_pinctrl, "lepton_irq");
	if (IS_ERR(lepton_irq_active)) {
		LOG_ERR("Failed to init lepton_irq_active\n");
		ret = PTR_ERR(lepton_irq_active);
	}
	
	lepton_reset_active = pinctrl_lookup_state(lepton_pinctrl, "lepton_reset_high");
	if (IS_ERR(lepton_reset_active)) {
		LOG_ERR("Failed to init lepton_reset_active\n");
		ret = PTR_ERR(lepton_reset_active);
	}

	lepton_reset_suspend = pinctrl_lookup_state(lepton_pinctrl, "lepton_reset_low");
	if (IS_ERR(lepton_reset_suspend)) {
		LOG_ERR("Failed to init lepton_reset_suspend\n");
		ret = PTR_ERR(lepton_reset_suspend);
	}
	
	//lepton_miso_spi_active = pinctrl_lookup_state(lepton_pinctrl, "lepton_miso_spi");
	//if (IS_ERR(lepton_miso_spi_active)) {
	//	LOG_ERR("Failed to init lepton_miso_spi_active\n");
	//	ret = PTR_ERR(lepton_miso_spi_active);
	//}
	lepton_pdn_high_active = pinctrl_lookup_state(lepton_pinctrl, "lepton_pdn_high");
	if (IS_ERR(lepton_pdn_high_active)) {
		LOG_ERR("Failed to init lepton_pdn_high_active\n");
		ret = PTR_ERR(lepton_pdn_high_active);
	}
	
	lepton_pdn_low_suspend = pinctrl_lookup_state(lepton_pinctrl, "lepton_pdn_low");
	if (IS_ERR(lepton_pdn_low_suspend)) {
		LOG_ERR("Failed to init lepton_pdn_low_suspend\n");
		ret = PTR_ERR(lepton_pdn_low_suspend);
	}
//vdd3.0 vddio2.8v vddc1.2v
	lepton_vdd_high_active = pinctrl_lookup_state(lepton_pinctrl, "lepton_vdd_en");
	if (IS_ERR(lepton_vdd_high_active)) {
		LOG_ERR("Failed to init lepton_vdd_high_active\n");
		ret = PTR_ERR(lepton_vdd_high_active);
	}
	
	lepton_vdd_low_suspend = pinctrl_lookup_state(lepton_pinctrl, "lepton_vdd_disable");
	if (IS_ERR(lepton_vdd_low_suspend)) {
		LOG_ERR("Failed to init lepton_vdd_low_suspend\n");
		ret = PTR_ERR(lepton_vdd_low_suspend);
	}

	lepton_vddio_high_active = pinctrl_lookup_state(lepton_pinctrl, "lepton_vddio_en");
	if (IS_ERR(lepton_vddio_high_active)) {
		LOG_ERR("Failed to init lepton_vddio_high_active\n");
		ret = PTR_ERR(lepton_vddio_high_active);
	}
	
	lepton_vddio_low_suspend = pinctrl_lookup_state(lepton_pinctrl, "lepton_vddio_disable");
	if (IS_ERR(lepton_vddio_low_suspend)) {
		LOG_ERR("Failed to init lepton_vddio_low_suspend\n");
		ret = PTR_ERR(lepton_vddio_low_suspend);
	}

	lepton_vddc_high_active = pinctrl_lookup_state(lepton_pinctrl, "lepton_vddc_en");
	if (IS_ERR(lepton_vddc_high_active)) {
		LOG_ERR("Failed to init lepton_vddc_high_active\n");
		ret = PTR_ERR(lepton_vddc_high_active);
	}
	
	lepton_vddc_low_suspend = pinctrl_lookup_state(lepton_pinctrl, "lepton_vddc_disable");
	if (IS_ERR(lepton_vddc_low_suspend)) {
		LOG_ERR("Failed to init lepton_vddc_low_suspend\n");
		ret = PTR_ERR(lepton_vddc_low_suspend);
	}
//mosi miso cs clk	
	lepton_miso_pull_active = pinctrl_lookup_state(lepton_pinctrl, "lepton_miso_pullhigh");
	if (IS_ERR(lepton_miso_pull_active)) {
		LOG_ERR("Failed to init lepton_miso_pullhigh\n");
		ret = PTR_ERR(lepton_miso_pull_active);
	}

	lepton_miso_pull_suspend = pinctrl_lookup_state(lepton_pinctrl, "lepton_miso_pulllow");
	if (IS_ERR(lepton_miso_pull_suspend)) {
		LOG_ERR("Failed to init lepton_miso_pulllow\n");
		ret = PTR_ERR(lepton_miso_pull_suspend);
	}

	lepton_mosi_pull_active = pinctrl_lookup_state(lepton_pinctrl, "lepton_mosi_pull_up");
	if (IS_ERR(lepton_mosi_pull_active)) {
		LOG_ERR("Failed to init lepton_mosi_pull_up\n");
		ret = PTR_ERR(lepton_mosi_pull_active);
	}

	lepton_mosi_pull_suspend = pinctrl_lookup_state(lepton_pinctrl, "lepton_mosi_pull_down");
	if (IS_ERR(lepton_mosi_pull_suspend)) {
		LOG_ERR("Failed to init lepton_mosi_pull_down\n");
		ret = PTR_ERR(lepton_mosi_pull_suspend);
	}
	

	lepton_spi_set = pinctrl_lookup_state(lepton_pinctrl, "lepton_spi_set");
	if (IS_ERR(lepton_spi_set)) {
		LOG_ERR("Failed to init lepton_spi_set\n");
		ret = PTR_ERR(lepton_spi_set);
	}else{
		ret = pinctrl_select_state(lepton_pinctrl, lepton_spi_set);
		LOG_INF("ret:%d\n",ret);			
	}
	
	LOG_INF("of_find_compatible_node  222\n");	
	return ret;
}
//drv add by lipengpen 20240220 end 

//drv add by lipengpeng 20240323 start 
static ssize_t leptondev_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    return 0;
}

static ssize_t leptondev_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    return count;
}

static long leptondev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int			retval = 0;
	//struct lepton *lep = dev_get_drvdata(filr_dev); //filr_dev = &spi->dev;

	LOG_INF("leptondev_ioctl cmd=%x START_SPI_IOCTL=%x",cmd ,START_SPI_IOCTL);  
   
	switch (cmd) {
      case START_SPI_IOCTL:

	           // enable_irq(lep->irq);
			  pinctrl_select_state(lepton_pinctrl, lepton_vdd_high_active);  //enable vdd3.0v
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_vddio_high_active);  //enable vddio2.8v
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_vddc_high_active);  //enable vddc1.2v
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_pdn_high_active);  //pdn set high
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_reset_suspend);  //reset low
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_reset_active);  //reset high
			  mdelay(5);	   
		break;
	case STOP_SPI_IOCTL:
			//	disable_irq(lep->irq);
			  pinctrl_select_state(lepton_pinctrl, lepton_vdd_low_suspend);  //disable vdd3.0v
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_vddio_low_suspend);  //disable vddio2.8v
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_vddc_low_suspend);  //disable vddc1.2v
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_pdn_low_suspend);  //pdn set low
			  mdelay(5);
			  pinctrl_select_state(lepton_pinctrl, lepton_reset_suspend);  //reset low
		break;
     default:
            retval = (-EINVAL);
            break;
	}

	return retval;
}

static int leptondev_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int leptondev_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations filedev_fops = {
	.owner =	THIS_MODULE,
	.write =	leptondev_write,
	.read =		leptondev_read,
	.unlocked_ioctl = leptondev_ioctl,
	//.compat_ioctl = leptondev_compat_ioctl,
	.open =		leptondev_open,
	.release =	leptondev_release,
	.llseek =	no_llseek,
};

static struct miscdevice lepton_filr25_miscdev = {
        .minor  = MISC_DYNAMIC_MINOR,
        .name   = SPI_FILR_NAME,
        .fops   = &filedev_fops,
}; 
//drv add by lipengpeng 20240323 end 

static int lepton_probe(struct spi_device *spi)
{
	struct lepton *lep = NULL;
	struct device *dev = NULL;
	struct device_node *of_node = NULL;
	struct v4l2_device *v4l2_dev = NULL;
	struct video_device *vid_dev = NULL;
	struct vb2_queue *q = NULL;
	struct spi_transfer *spi_xfer = NULL;
	struct spi_message *spi_msg = NULL;
	int ret;
//drv add by lipengpeng 20240323 start
    int		 status;
//drv add by lipengpeng 20240323 end

//	struct device_node *node;

	dev = &spi->dev;
//drv add by lipengpeng 20240316 start
	filr_dev = &spi->dev;
//drv add by lipengpeng 20240316 ebd 
	of_node = dev->of_node;
	if (!of_node) {
		dev_err(dev, "missing device tree entry");
		return -EINVAL;
	}

#ifndef CONFIG_ARCH_QCOM
	/* set 64-bit DMA mask for allocation of video buffers
	 */
	of_dma_configure(dev, of_node); 
	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));
	if (ret) {
		dev_err(dev, "failed to set DMA mask, err %d", ret);
		return -EINVAL;
	}
#endif

//drv add by lipengpeng 20240219 start 
   lepton_get_gpio_dts_info(dev);
//drv add by lipengpeng 20240219 end     
	/* lepton struct keeps track of both video and spi-related structs,
	 * and will be available in driver callbacks via private data pointers
	 */
	LOG_INF(KERN_INFO LEPTON_MODULE_NAME ": Allocate struct lepton, init mutex\n");
	lep = devm_kzalloc(dev, sizeof(*lep), GFP_KERNEL);
	if (lep == NULL) {
		dev_err(dev, "failed to allocate lepton struct");
		return -ENOMEM;
	}
	mutex_init(&lep->mutex);
	spin_lock_init(&lep->lock);

	/* initialize frame dimensions
	 */

	ret = init_lepton_info(&lep->lep_vospi_info, lepton_model, telemetry);
	if (ret) {
		dev_err(dev, "bad module parameters");
		return -EINVAL;
	}

	/* initialize v4l2_device -- used for tracking relationships among 
	 * video-related hardware managed by the V4L2 subsystem 
	 */

	LOG_INF(KERN_INFO LEPTON_MODULE_NAME ": Allocate struct v4l2_dev, register V4L2 device\n");
	v4l2_dev = devm_kzalloc(dev, sizeof(*v4l2_dev), GFP_KERNEL);
	if (v4l2_dev == NULL) {
		dev_err(dev, "failed to allocate v4l2 struct");
		return -ENOMEM;
	}
	ret = v4l2_device_register(dev, v4l2_dev);
	if (ret) {
		dev_err(dev, "failed to register v4l2");
		return ret;
	}

	/* initialize video_device -- used for managing device file
	 * (e.g. /dev/videoN) owned by parent v4l2_device 
	 */

	vid_dev = video_device_alloc();
	if (vid_dev == NULL) {
		dev_err(dev, "failed to allocate video struct");
		ret = -ENOMEM;
		goto unreg_v4l2_device;
	}

	*vid_dev = lepton_videodev_template;
	vid_dev->v4l2_dev = v4l2_dev;
//drv add by lipengpeng 20240226 start 
    vid_dev->device_caps =V4L2_CAP_VIDEO_CAPTURE |V4L2_CAP_STREAMING |V4L2_CAP_READWRITE;	
//drv add by lipengpeng 20240226 end 	
//drv add by lipengpeng 20240220 start 
//	ret = video_register_device(vid_dev, VFL_TYPE_GRABBER, -1);
	ret = video_register_device(vid_dev, VFL_TYPE_VIDEO, -1);
//drv add by lipengpeng 20240220 end  	
	if (ret) {
		/* now have non-devm (i.e. not automatically released when
		   owning device struct is gone) resources to free */
		dev_err(dev, "failed to video_register_device");   
		goto unreg_v4l2_device;
	}

	/* initialize vb2_queue -- used to manage buffers for
	 * capturing video data 
	 */

	q = devm_kzalloc(dev, sizeof(*q), GFP_KERNEL);
	if (q == NULL) {
		/* now have non-devm (i.e. not automatically released when
		   owning device struct is gone) resources to free */
		dev_err(dev, "failed to allocate queue struct");
		ret = -ENOMEM;
		goto unreg_video_and_v4l_device;
	}

	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
//	q->io_modes = VB2_MMAP | VB2_DMABUF | VB2_READ; //@@@ is DMABUF freebie with vb2 boilerplate?
	q->io_modes = VB2_MMAP;
	q->buf_struct_size = sizeof(struct lepton_buffer);
	q->gfp_flags = 0;
	q->ops = &lepton_video_qops;
	q->mem_ops = &vb2_vmalloc_memops;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->lock = &lep->mutex;
	q->min_buffers_needed = 3;

	ret = vb2_queue_init(q);
	if (ret) {
		/* now have non-devm (i.e. not automatically released when
		   owning device struct is gone) resources to free */
		dev_err(dev, "failed to init queue struct");
		goto unreg_video_and_v4l_device;
	}

	/* initialize spi descriptors and transmit buffer
	 */
	spi_msg = devm_kzalloc(dev, sizeof(*spi_msg), GFP_KERNEL);
	spi_xfer = devm_kzalloc(dev, sizeof(*spi_xfer), GFP_KERNEL);
	if (spi_xfer == NULL || spi_msg == NULL) {
		dev_err(dev, "failed to allocate SPI message/transfer structs");
		ret = -ENOMEM;
		goto unreg_video_and_v4l_device;
	}
	/* spare rx buffer when not using allocated V4L buf is subframe size + 1 line 
	 * so that eventually we will sync up if we start out in middle of a subframe */
	lep->spare_buf.len = lep->lep_vospi_info.subframe_params.subframe_data_byte_size + LEPTON_SUBFRAME_LINE_BYTE_WIDTH;
	lep->spare_buf.rx_buf = kmalloc(lep->spare_buf.len, GFP_KERNEL);
	if (lep->spare_buf.rx_buf == NULL) {
		dev_err(dev, "failed to allocate SPI rx buffer");
		ret = -ENOMEM;
		goto unreg_video_and_v4l_device;
	}

	lep->spare_buf.tx_buf = kmalloc(lep->spare_buf.len, GFP_KERNEL);
	if (lep->spare_buf.tx_buf == NULL) {
		dev_err(dev, "failed to allocate SPI tx buffer");
		ret = -ENOMEM;
		goto unreg_video_and_v4l_device;
	}

	/* set up data pointers to be able to find any of the core structs
	 * when only one is passed into a callback function 
	 */
//drv add by lipengpeng 20240219 start
/*		lepton SPI mode 3: CPHA = 1, CPOL = 1
		reg = <0>;
		spi-max-frequency = <24000000>;
		spi-cpha;
		spi-cpol;
*/
	spi->mode = SPI_MODE_3;
	spi->bits_per_word = 8;
	spi->max_speed_hz = 20000000;
	spi_setup(spi);
//drv add by lipengpeng 20240219 end 
	
	lep->v4l2_dev = v4l2_dev;
	lep->vid_dev = vid_dev;
	lep->q = q;
	lep->spi_dev = spi;
	lep->spi_xfer = spi_xfer;
	lep->spi_msg = spi_msg;
	lep->current_lep_buf = NULL;
//drv add by lipengpeng 20240321 start  default enable validation
    //lep->override_frame_validation=1;  //drv add by lipengpeng 20240801 end
//drv add by lipengpeng 20240321 end  default enable validation
	dev_set_drvdata(dev, lep);
	video_set_drvdata(vid_dev, lep);
	vid_dev->queue = q;
	q->drv_priv = lep;

	lep->dma_done = true;   /* no spi transfer pending yet */

	INIT_LIST_HEAD(&lep->unfilled_bufs);

	/* set up interrupt handler for lepton VSYNC (frame ready signal) 
	 */
	//node = of_find_compatible_node(NULL, NULL, "mediatek,lepton_filr");
	
/*	lep->irq_gpio = of_get_named_gpio(of_node, "lepton,irq-gpio", 0);
	if (lep->irq_gpio < 0)
		LOG_INF("lepton_irq_gpio not available\n");

    if (gpio_is_valid(lep->irq_gpio)) {

		gpio_direction_input(lep->irq_gpio);
		
        lep->irq = gpio_to_irq(lep->irq_gpio);
        if (lep->irq < 0) {
            LOG_INF("failed to lepton_irq_gpio\n");
            return -EINVAL;
        }
    } else {
        LOG_INF("irq gpio not provided\n");
        return -EINVAL;
    }
*/	
	lep->irq = irq_of_parse_and_map(of_node, 0);
	LOG_INF("lepton irq num=%d\n",lep->irq);
	if (lep->irq < 0) {
		dev_err(dev, "failed to map irq");
		goto unreg_video_and_v4l_device;
	}
//drv modified by wangmd 20240719 start
	//ret = devm_request_irq(dev, lep->irq, lepton_vsync_handler, 0, "lepton_irq", spi);
	ret = request_threaded_irq(lep->irq, NULL, lepton_vsync_handler, IRQF_TRIGGER_RISING | IRQF_ONESHOT , "lepton_irq", spi);
//drv modified by wangmd 20240719 end
	if (ret) {
		dev_err(dev, "failed to register irq");
		goto unreg_video_and_v4l_device;
	}
//drv add by lipengpeng 20240227 start
 // spidev_probe(spi);
//drv add by lipengpeng 20240227 end   
//drv add by lipengpeng 20240323 start 
    status = misc_register(&lepton_filr25_miscdev);
	if (status)
	{
			LOG_INF("lepton misc_register failed status=%d",status);
	}
//drv add by lipengpeng 20240323 end 	
	ret = sysfs_create_group(&dev->kobj, &lepton_attr_group);
	if (ret) {
		dev_err(dev, "failed to create sysfs group");
		goto unreg_video_and_v4l_device;
	}
//drv add by lipengpeng 20240319 start
   enable_irq(lep->irq);
//drv add by lipengpeng 20240319 end   

	LOG_INF(KERN_INFO LEPTON_MODULE_NAME ": Probe complete\n");
	return 0;

unreg_video_and_v4l_device:
	video_unregister_device(lep->vid_dev);
unreg_v4l2_device:
	v4l2_device_unregister(lep->v4l2_dev);

	return ret;
}

static int lepton_remove(struct spi_device *spi)
{
	struct lepton *lep = dev_get_drvdata(&spi->dev);

	/* tear down the things that are not "devm" (device-managed) */
	video_unregister_device(lep->vid_dev);
	v4l2_device_unregister(lep->v4l2_dev);
	sysfs_remove_group(&spi->dev.kobj, &lepton_attr_group);
//drv add by lipengpeng 20240321 start 
	disable_irq(lep->irq);
//drv add by lipengpeng 20240321 end  
	return 0;
}

static struct spi_driver lepton_spi_driver = {
	.driver = {
		.name	= LEPTON_MODULE_NAME,
		.of_match_table	= lepton_of_match,
	},
	.id_table	= lepton_id_table,
	.probe		= lepton_probe,
	.remove		= lepton_remove,
};

module_spi_driver(lepton_spi_driver);

MODULE_AUTHOR("Team Lockwood-Childs, VCT Labs, Inc.");
MODULE_DESCRIPTION("VoSPI driver for FLIR lepton 2.x/3.x");
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");

