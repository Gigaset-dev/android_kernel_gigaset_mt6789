// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

//prize add by lvyuanchuan for lcd hardware info 20220331 start
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/mediatek/prize/hardware_info/hardware_info.h"
extern struct hardware_info current_lcm_info;
#endif
//prize add by lvyuanchuan for lcd hardware info 20220331 end

extern bool panel_fod_is_enabled;
extern bool drv_doze_state;

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	struct gpio_desc *vldo18_gpio;
	bool prepared;
	bool enabled;
	bool hbm_en;

	int error;
};

unsigned int rawlevel;
struct lcm *g_ctx;

#define lcm_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret,
			 cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static void lcm_panel_init(struct lcm *ctx)
{
dev_info(ctx->dev, "%s\n", __func__);
	
lcm_dcs_write_seq_static(ctx,0xFF,0xAA,0x55,0xA5,0x80);
lcm_dcs_write_seq_static(ctx,0x6F,0x20);
lcm_dcs_write_seq_static(ctx,0xF7,0x32);
//lcm diming????
lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x00);
lcm_dcs_write_seq_static(ctx,0xB2,0x19);
lcm_dcs_write_seq_static(ctx,0x6F,0x05);
lcm_dcs_write_seq_static(ctx,0xB2,0x0A,0x0A);

lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x04);
lcm_dcs_write_seq_static(ctx,0xD0,0x00);
lcm_dcs_write_seq_static(ctx,0x35);
lcm_dcs_write_seq_static(ctx,0x53,0x28);
lcm_dcs_write_seq_static(ctx,0x51,0x0D,0xBB,0x0D,0xBB);
lcm_dcs_write_seq_static(ctx,0x2A,0x00,0x00,0x04,0xC7);
lcm_dcs_write_seq_static(ctx,0x2B,0x00,0x00,0x0A,0x67);
lcm_dcs_write_seq_static(ctx,0x82,0xAC);
// ##compression method selection, slice high 12 (2-dec)
lcm_dcs_write_seq_static(ctx,0x03,0x01);
lcm_dcs_write_seq_static(ctx,0x90,0x01);//0x90,0x11);
lcm_dcs_write_seq_static(ctx,0x91,0x89,0x28,0x00,0x0C,0xC2,0x00,0x02,0x32,0x01,0x31,0x00,0x08,0x08,0xBB,0x07,0x7B,0x10,0xF0);
lcm_dcs_write_seq_static(ctx,0x2C);
lcm_dcs_write_seq_static(ctx,0x5F,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0xAA,0x55,0xA5,0x80);    //no OTP
lcm_dcs_write_seq_static(ctx,0x6F,0x1D);
lcm_dcs_write_seq_static(ctx,0xF2,0x05);
lcm_dcs_write_seq_static(ctx,0x88,0x01);
lcm_dcs_write_seq_static(ctx,0x6F,0x01);
lcm_dcs_write_seq_static(ctx,0x88,0x02,0x65,0x09,0x86);   // ##FPR X/Y Location
lcm_dcs_write_seq_static(ctx,0x2F,0x00);//120hz 01 90hz 02 60hz 00
lcm_dcs_write_seq_static(ctx,0x11);
msleep(120);
lcm_dcs_write_seq_static(ctx,0x29);
msleep(50);
pr_info("%s-\n", __func__);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(200);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(120);
	
	// enter deep standby mode
	// lcm_dcs_write_seq_static(ctx, 0x4f, 0x01);
	// msleep(120);
	
	// lcd reset L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(10);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	
	//lcd vci L
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	msleep(10);
	
	// lcd dvdd L
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);
	msleep(5);	

//drv add by wangwei1 for tp gesture open 20231219 start
/*
	// lcd ldo 1.8v
	ctx->vldo18_gpio = devm_gpiod_get(ctx->dev, "vldo18", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vldo18_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vldo18 %ld\n",
			__func__, PTR_ERR(ctx->vldo18_gpio));
		return PTR_ERR(ctx->vldo18_gpio);
	}
	gpiod_set_value(ctx->vldo18_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vldo18_gpio);
	msleep(5);
*/
//drv add by wangwei1 for tp gesture open 20231219 end
	
	ctx->error = 0;
	ctx->prepared = false;
	panel_fod_is_enabled = false;
	drv_doze_state = false;
	pr_err("%s-\n", __func__);
	g_ctx->hbm_en = false;

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	// lcd ldo 1.8v
	ctx->vldo18_gpio = devm_gpiod_get(ctx->dev, "vldo18", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vldo18_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vldo18 %ld\n",
			__func__, PTR_ERR(ctx->vldo18_gpio));
		return PTR_ERR(ctx->vldo18_gpio);
	}
	gpiod_set_value(ctx->vldo18_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vldo18_gpio);
	msleep(5);
	
	// lcd dvdd H
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);
	msleep(5);
	
	//lcd vci H
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	msleep(10);
	
	// lcd reset H -> L -> L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(10);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(10);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(50);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	// end
	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#define HFP (60)
#define HSA (20)
#define HBP (20)
#define HACT (1224)
#define VFP (60)
#define VSA (40)
#define VBP (20)
#define VACT (2664)

#define PHYSICAL_WIDTH              72320
#define PHYSICAL_HEIGHT             157676
static const struct drm_display_mode switch_mode_120hz = {
	.clock		= 442322,
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP,
};

static const struct drm_display_mode switch_mode_90hz = {
	.clock		= 331741,
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP,
};

static const struct drm_display_mode switch_mode_60hz = {
	.clock		= 221161,
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params_120hz = {
	.data_rate = 1000,
	.pll_clk = 500,//484
 	//.dyn_fps = {
 		//.data_rate = 968,
 	//},
	.physical_width_um  = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.lp_perline_en = 1,
	//.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 0,
		.rgb_swap = 0,
		.dsc_cfg = 34,//34
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2664,
		.pic_width = 1224,
		.slice_height = 12,
		.slice_width = 1224,
		.chunk_size = 1224,
		.xmit_delay = 512,
		.dec_delay = 562,
		.scale_value = 32,
		.increment_interval = 305,
		.decrement_interval = 8,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 1915,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
};

static struct mtk_panel_params ext_params_90hz = {
	.data_rate = 1000,
	.pll_clk = 500,//345
 	//.dyn_fps = {
 		//.data_rate = 720,
 	//},
	.physical_width_um  = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.lp_perline_en = 1,
	//.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 0,
		.rgb_swap = 0,
		.dsc_cfg = 34,//34
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2664,
		.pic_width = 1224,
		.slice_height = 12,
		.slice_width = 1224,
		.chunk_size = 1224,
		.xmit_delay = 512,
		.dec_delay = 562,
		.scale_value = 32,
		.increment_interval = 305,
		.decrement_interval = 8,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 1915,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
};

static struct mtk_panel_params ext_params_60hz = {
	.data_rate = 1000,
	.pll_clk = 500,//220
 	//.dyn_fps = {
 		//.data_rate = 482,
 	//},
	.physical_width_um  = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.lp_perline_en = 1,
	//.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 0,
		.rgb_swap = 0,
		.dsc_cfg = 34,//34
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2664,
		.pic_width = 1224,
		.slice_height = 12,
		.slice_width = 1224,
		.chunk_size = 1224,
		.xmit_delay = 512,
		.dec_delay = 562,
		.scale_value = 32,
		.increment_interval = 305,
		.decrement_interval = 8,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 1915,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

int trans_level = 0;
unsigned short led_level_disp_get(char *name)
{
	pr_err("[%s]: name: %s, level : %d",__func__, name, trans_level);
	return trans_level;
}
EXPORT_SYMBOL(led_level_disp_get);

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	char bl_tb[] = {0x51, 0x0D, 0xBB};
	char hbm_tb[] = {0x51, 0x0F, 0xFF};

	if (!g_ctx->prepared)
		return 0;

	if (level > 4095) {
		if (level == 4096) {
			if (!cb)
				return -1;
			pr_err("%s panel into HBM", __func__);
			g_ctx->hbm_en = true;
			cb(dsi, handle, hbm_tb, ARRAY_SIZE(hbm_tb));
		} else if (level == 4097) {
			if (rawlevel <= 0xDBB) {
				bl_tb[1] = (rawlevel>>8)&0xf;
				bl_tb[2] = (rawlevel)&0xff;
			} else {
				pr_err("%s err, rawlevel=%d\n", __func__, rawlevel);
			}
			if (!cb)
				return -1;
			pr_err("%s panel out HBM, rawlevel=%d, bl_tb[1]=0x%x, bl_tb[2]=0x%x\n",
				__func__, rawlevel, bl_tb[1], bl_tb[2]);
			g_ctx->hbm_en = false;
			cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
		}
	} else {
		if (level) {
			rawlevel = level*3515/255;
		} else {
			rawlevel = 0;
		}
		if (rawlevel <= 0xDBB) {
			bl_tb[1] = (rawlevel>>8)&0xf;
			bl_tb[2] = (rawlevel)&0xff;
			if (!cb)
				return -1;
			if ((g_ctx->hbm_en == false) || (level == 0))
			cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
			pr_err("%s level=%d, rawlevel=%d, bl_tb[1]=0x%x, bl_tb[2]=0x%x\n",
				__func__, level, rawlevel, bl_tb[1], bl_tb[2]);
		} else {
			pr_err("%s err, rawlevel=%d\n", __func__, rawlevel);
		}
	}
	
        trans_level = rawlevel;
	return 0;
}


static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);
	if (!m) {
		pr_err("%s:%d invalid display_mode\n", __func__, __LINE__);
		return ret;
	}
	if (drm_mode_vrefresh(m) == 120)
		ext->params = &ext_params_120hz;
	else if (drm_mode_vrefresh(m) == 90)
		ext->params = &ext_params_90hz;
	else if (drm_mode_vrefresh(m) == 60)
		ext->params = &ext_params_60hz;
	else
		ret = 1;

	return ret;
}

#ifdef CONFIG_TOUCHSCREEN_SYNA_REPORT_RATE_SWITCH
extern void syna_fps_report_rate(int data);
#endif
static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);
		lcm_dcs_write_seq_static(ctx, 0x2F, 0x02);
		mdelay(20);
		lcm_dcs_write_seq_static(ctx, 0x2F, 0x01);
#ifdef CONFIG_TOUCHSCREEN_SYNA_REPORT_RATE_SWITCH
		syna_fps_report_rate(120);
#endif
	}
}

static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);
		lcm_dcs_write_seq_static(ctx, 0x2F, 0x01);
		mdelay(20);
		lcm_dcs_write_seq_static(ctx, 0x2F, 0x02);
#ifdef CONFIG_TOUCHSCREEN_SYNA_REPORT_RATE_SWITCH
		syna_fps_report_rate(90);
#endif
	}
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);

		lcm_dcs_write_seq_static(ctx, 0x2F, 0x00);
#ifdef CONFIG_TOUCHSCREEN_SYNA_REPORT_RATE_SWITCH
		syna_fps_report_rate(60);
#endif
	}
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, dst_mode);

	if (cur_mode == dst_mode)
		return ret;
	if (drm_mode_vrefresh(m) == 60) { /*switch to 60 */
		mode_switch_to_60(panel, stage);
	}
	else if (drm_mode_vrefresh(m) == 90) { /*switch to 90 */
		mode_switch_to_90(panel, stage);
	}
	else if (drm_mode_vrefresh(m) == 120) { /*switch to 120 */
		mode_switch_to_120(panel, stage);
	} else
		ret = 1;

	return ret;
}

static int panel_doze_enable(struct drm_panel *panel,
	void *dsi_drv, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_err("%s", __func__);

	/* Enter AOD */
	lcm_dcs_write_seq_static(ctx,0x51,0x0D,0xBB,0x0F,0xFE);
	lcm_dcs_write_seq_static(ctx,0x39);
	lcm_dcs_write_seq_static(ctx,0x2C);

	panel_fod_is_enabled = true;
	drv_doze_state = true;
	return 0;
}

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi_drv, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_err("%s", __func__);
	/* Exit AOD */
	lcm_dcs_write_seq_static(ctx,0x38);
	lcm_dcs_write_seq_static(ctx,0x2C);

	panel_fod_is_enabled = false;
	drv_doze_state = false;
	return 0;
}

static int panel_set_aod_light_mode(void *dsi_drv, dcs_write_gce cb,
	void *handle, unsigned int mode)
{
	pr_err("%s, mode%d", __func__, mode);

	if (mode == 1) {
		/* AOD1 Switch */
		lcm_dcs_write_seq_static(g_ctx,0x51,0x0D,0xBB,0x0F,0xFE);
		lcm_dcs_write_seq_static(g_ctx,0x39);
		lcm_dcs_write_seq_static(g_ctx,0x2C);
	} else if (mode == 2) {
		/* AOD2 Switch */
		lcm_dcs_write_seq_static(g_ctx,0x51,0x0D,0xBB,0x03,0x33);
		lcm_dcs_write_seq_static(g_ctx,0x39);
		lcm_dcs_write_seq_static(g_ctx,0x2C);
	} else if (mode == 3) {
		/* AOD3 Switch */
		lcm_dcs_write_seq_static(g_ctx,0x51,0x0D,0xBB,0x00,0xA4);
		lcm_dcs_write_seq_static(g_ctx,0x39);
		lcm_dcs_write_seq_static(g_ctx,0x2C);
	} else {
		pr_err("%s, invalid mode", __func__);
	}
	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
	.set_aod_light_mode = panel_set_aod_light_mode,
};
#endif

static int lcm_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_1;
	struct drm_display_mode *mode_2;

	mode = drm_mode_duplicate(connector->dev, &switch_mode_60hz);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 switch_mode_60hz.hdisplay, switch_mode_60hz.vdisplay,
			 drm_mode_vrefresh(&switch_mode_60hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	mode_1 = drm_mode_duplicate(connector->dev, &switch_mode_90hz);
	if (!mode_1) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_90hz.hdisplay, switch_mode_90hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_90hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_1);

	mode_2 = drm_mode_duplicate(connector->dev, &switch_mode_120hz);
	if (!mode_2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_120hz.hdisplay, switch_mode_120hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_120hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode_2);
	mode_2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_2);

	connector->display_info.width_mm = 72;
	connector->display_info.height_mm = 158;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

//drv add by wangwei1 for poweroff charger fliker 20240113 start
#if IS_ENABLED(CONFIG_DRM_PANEL_CSOT_NT37701A_POWEROFF_CHARGER_TINMING)
void nt37701a_poweroff_charger_supspend(void)
{
	pr_info("%s+\n", __func__);	

	//struct lcm *ctx = panel_to_lcm(panel);

	if (!g_ctx->prepared)
		return;

	lcm_dcs_write_seq_static(g_ctx, 0x28);
	msleep(200);
	lcm_dcs_write_seq_static(g_ctx, 0x10);
	msleep(120);
	
	// enter deep standby mode
	// lcm_dcs_write_seq_static(g_ctx, 0x4f, 0x01);
	// msleep(120);
	
	// lcd reset L
	g_ctx->reset_gpio = devm_gpiod_get(g_ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(g_ctx->reset_gpio, 0);
	msleep(10);
	devm_gpiod_put(g_ctx->dev, g_ctx->reset_gpio);
	
	//lcd vci L
	g_ctx->bias_neg = devm_gpiod_get_index(g_ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(g_ctx->bias_neg)) {
		dev_err(g_ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(g_ctx->bias_neg));
		return;
	}
	gpiod_set_value(g_ctx->bias_neg, 0);
	devm_gpiod_put(g_ctx->dev, g_ctx->bias_neg);
	msleep(10);
	
	// lcd dvdd L
	g_ctx->bias_pos = devm_gpiod_get_index(g_ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(g_ctx->bias_pos)) {
		dev_err(g_ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(g_ctx->bias_pos));
		return;
	}
	gpiod_set_value(g_ctx->bias_pos, 0);
	devm_gpiod_put(g_ctx->dev, g_ctx->bias_pos);
	msleep(5);	


	// lcd ldo 1.8v
	g_ctx->vldo18_gpio = devm_gpiod_get(g_ctx->dev, "vldo18", GPIOD_OUT_HIGH);
	if (IS_ERR(g_ctx->vldo18_gpio)) {
		dev_err(g_ctx->dev, "%s: cannot get vldo18 %ld\n",
			__func__, PTR_ERR(g_ctx->vldo18_gpio));
		return;
	}
	gpiod_set_value(g_ctx->vldo18_gpio, 0);
	devm_gpiod_put(g_ctx->dev, g_ctx->vldo18_gpio);
	msleep(5);

	
	g_ctx->error = 0;
	g_ctx->prepared = false;

	pr_err("%s-\n", __func__);

	return;
}
EXPORT_SYMBOL(nt37701a_poweroff_charger_supspend);
#endif
//drv add by wangwei1 for poweroff charger fliker 20240113 end

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;

	pr_info("%s+\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "cannot get reset-gpios %ld\n",
			 PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->vldo18_gpio = devm_gpiod_get(dev, "vldo18", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vldo18_gpio)) {
		dev_info(dev, "cannot get vldo18-gpios %ld\n",
			 PTR_ERR(ctx->vldo18_gpio));
		return PTR_ERR(ctx->vldo18_gpio);
	}
	devm_gpiod_put(dev, ctx->vldo18_gpio);
	
	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_info(dev, "cannot get bias-gpios 0 %ld\n",
			 PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);
	
	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_info(dev, "cannot get bias-gpios 1 %ld\n",
			 PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);
	
	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params_60hz, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

#endif

	g_ctx = ctx;

#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
    strcpy(current_lcm_info.chip,"NT37701A");
    strcpy(current_lcm_info.vendor,"CSOT");
    sprintf(current_lcm_info.id,"0x%02x",0x02);
    strcpy(current_lcm_info.more,"1224*2664");
#endif

	drv_doze_state = false;
	g_ctx->hbm_en = false;

	pr_info("%s- lcm,nt37701a,cmd\n", __func__);

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif

	return 0;
}

static void lcm_shutdown(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	pr_info("%s+ lcm,nt37701a,cmd\n", __func__);
	// lcd ldo 1.8v
	ctx->vldo18_gpio = devm_gpiod_get(ctx->dev, "vldo18", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vldo18_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vldo18 %ld\n",
			__func__, PTR_ERR(ctx->vldo18_gpio));
		return;
	}
	gpiod_set_value(ctx->vldo18_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vldo18_gpio);

	pr_info("%s- lcm,nt37701a,cmd\n", __func__);
	return;
}

static const struct of_device_id lcm_of_match[] = {
	{
	    .compatible = "csot,nt37701a,cmd",
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.shutdown = lcm_shutdown,
	.driver = {
		.name = "panel-csot-nt37701a-dphy-cmd-120hz",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("MEDIATEK");
MODULE_DESCRIPTION("nt37701a AMOLED CMD LCD Panel Driver");
MODULE_LICENSE("GPL v2");
