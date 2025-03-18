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

#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/mediatek/prize/hardware_info/hardware_info.h"
extern struct hardware_info current_lcm_info;
#endif

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
	bool hbm_wait; 

	int error;
};

unsigned int rawlevel;
static struct lcm *g_ctx;

extern int g_doze_en;
extern int g_hbm_stat;

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

static int panel_hbm_set_cmdq(struct drm_panel *panel,
	void *dsi_drv, dcs_write_gce cb, void *handle, bool en)
{
	char bl_tb[] = {0x51, 0x0D, 0xBB};
	char hbm_tb[] = {0x51, 0x0F, 0xFF};

	if (!cb)
		return -1;

	if (en) {
		cb(dsi_drv, handle, hbm_tb, ARRAY_SIZE(hbm_tb));
		g_hbm_stat = true;
		pr_err("%s panel into HBM", __func__);
	} else {
		if (rawlevel <= 0xDBB) {
			bl_tb[1] = (rawlevel>>8)&0xf;
			bl_tb[2] = (rawlevel)&0xff;
			cb(dsi_drv, handle, bl_tb, ARRAY_SIZE(bl_tb));
			g_hbm_stat = false;
			pr_err("%s panel out HBM, rawlevel=%d, bl_tb[1]=0x%x, bl_tb[2]=0x%x\n",
				__func__, rawlevel, bl_tb[1], bl_tb[2]);
		} else {
			pr_err("%s err, rawlevel=%d\n", __func__, rawlevel);
		}
	}

	g_ctx->hbm_en = en;
	g_ctx->hbm_wait = true;

	return 0;
}

static void panel_hbm_get_state(struct drm_panel *panel, bool *state)
{
	*state = g_ctx->hbm_en;

	pr_err("%s hbm_en=%d", __func__, g_ctx->hbm_en);
}

static void lcm_panel_init(struct lcm *ctx)
{
	pr_err("%s+\n", __func__);
	
lcm_dcs_write_seq_static(ctx,0xFE,0xD2);// switch to D2 page  

lcm_dcs_write_seq_static(ctx,0x50,0x11);// pps000{0x7:3]{0x3:0) =(cfg) DSC_VERSION_MAJOR{0x3:0], DSC_VERSION_MINOR{0x3:0)
lcm_dcs_write_seq_static(ctx,0x51,0xab);// pps003{0x7:4]{0x3:0)=(cfg) bpc{0x3:0], linebuf_depth{0x3:0) = {bits_per_component{0x3:0], LINE_BUFFER_BPC{0x3:0)
lcm_dcs_write_seq_static(ctx,0x52,0x30);// pps004{0x5]{0x4]{0x3]{0x2]{0x1:0] =(cfg) BP, not_useYuvInput(enc_convert_RGB2YCoCg), simple422, VBR, bpp{0x9:8) = {BLOCK_PRED_ENABLE, ~USE_YUV_INPUT, SIMPLE_422, VBR_ENABLE, bits_per_pixel{0x9:8)
lcm_dcs_write_seq_static(ctx,0x53,0x09);// pps006{0x7:0)=(cfg) pic_height{0x15:8)
lcm_dcs_write_seq_static(ctx,0x54,0x60);// pps007{0x7:0)=(cfg) pic_height{0x7:0)
lcm_dcs_write_seq_static(ctx,0x55,0x04);// pps008{0x7:0)=(cfg) pic_width{0x15:8)
lcm_dcs_write_seq_static(ctx,0x56,0x38);// pps009{0x7:0)=(cfg) pic_width{0x7:0] }
lcm_dcs_write_seq_static(ctx,0x58,0x00);// pps010{0x7:0)=(cfg) slice_height{0x15:8) = SLICE_HEIGHT{0x15:8)
lcm_dcs_write_seq_static(ctx,0x59,0x28);// pps011{0x7:0)=(cfg) slice_height{0x7:0)  = SLICE_HEIGHT{0x7:0) 
lcm_dcs_write_seq_static(ctx,0x5a,0x04);// pps012{0x7:0)=(cfg) slice_width{0x15:8)  = SLICE_WIDTH{0x15:8)
lcm_dcs_write_seq_static(ctx,0x5b,0x38);// pps013{0x7:0)=(cfg) slice_width{0x7:0)   = SLICE_WIDTH{0x7:0) 
lcm_dcs_write_seq_static(ctx,0x5c,0x01);// pps016{0x1:0)=(cfg) initial_xmit_delay{0x9:8) = {INITIAL_DELAY{0x9:8)
lcm_dcs_write_seq_static(ctx,0x5d,0x9a);// pps017{0x7:0)=(cfg) initial_xmit_delay{0x7:0) = {INITIAL_DELAY{0x7:0)
lcm_dcs_write_seq_static(ctx,0x5e,0x19);// pps021{0x5:0)=(dyn) initial_scale_value{0x5:0)
lcm_dcs_write_seq_static(ctx,0x5f,0x04);// pps022{0x7:0)=(dyn) scale_increment_interval{0x15:8)
lcm_dcs_write_seq_static(ctx,0x60,0xce);// pps023{0x7:0)=(dyn) scale_increment_interval{0x7:0)
lcm_dcs_write_seq_static(ctx,0x61,0x00);// pps024{0x3:0)=(dyn) scale_decrement_interval{0x11:8)
lcm_dcs_write_seq_static(ctx,0x62,0x15);// pps025{0x7:0)=(dyn) scale_decrement_interval{0x7:0)
lcm_dcs_write_seq_static(ctx,0x63,0x0c);// pps027{0x4:0)=(dyn) first_line_bpg_offset{0x4:0) = {first_line_bpg_ofs{0x4:0)
lcm_dcs_write_seq_static(ctx,0x64,0x02);// pps028{0x7:0)=(dyn) nfl_bpg_offset{0x15:8)
lcm_dcs_write_seq_static(ctx,0x65,0x77);// pps029{0x7:0)=(dyn) nfl_bpg_offset{0x7:0)
lcm_dcs_write_seq_static(ctx,0x66,0x01);// pps030{0x7:0)=(dyn) slice_bpg_offset{0x15:8)
lcm_dcs_write_seq_static(ctx,0x67,0x8f);// pps031{0x7:0)=(dyn) slice_bpg_offset{0x7:0)
lcm_dcs_write_seq_static(ctx,0x68,0x16);// pps032{0x7:0)=(cfg) initial_offset{0x15:8) = {INITIAL_FULLNESS_OFFSET{0x15:8)
lcm_dcs_write_seq_static(ctx,0x69,0x00);// pps033{0x7:0)=(cfg) initial_offset{0x7:0)  = {INITIAL_FULLNESS_OFFSET {0x7:0)
lcm_dcs_write_seq_static(ctx,0x6a,0x10);// pps034{0x7:0)=(dyn) final_offset{0x15:8)
lcm_dcs_write_seq_static(ctx,0x6b,0xec);// pps035{0x7:0)=(dyn) final_offset{0x7:0)
lcm_dcs_write_seq_static(ctx,0x6c,0x07);// pps036{0x4:0)=(cfg) flatness_min_qp{0x4:0) = {FLATNESS_MIN_QP{0x4:0)
lcm_dcs_write_seq_static(ctx,0x6d,0x10);// pps037{0x4:0)=(cfg) flatness_max_qp{0x4:0) = {FLATNESS_MAX_QP{0x4:0)
lcm_dcs_write_seq_static(ctx,0x6e,0x20);// pps038{0x7:0)=(cfg) rc_model_size{0x15:8) = {RC_MODEL_SIZE{0x15:8)
lcm_dcs_write_seq_static(ctx,0x6f,0x00);// pps039{0x7:0)=(cfg) rc_model_size{0x7:0)  = {RC_MODEL_SIZE{0x7:0)
lcm_dcs_write_seq_static(ctx,0x70,0x06);// pps040{0x3:0)=(cfg) rc_edge_factor{0x3:0) = {RC_EDGE_FACTOR{0x3:0)
lcm_dcs_write_seq_static(ctx,0x71,0x0f);// pps041{0x4:0)=(cfg) rc_quant_incr_limit0{0x4:0) = {RC_QUANT_INCR_LIMIT0{0x4:0)
lcm_dcs_write_seq_static(ctx,0x72,0x0f);// pps042{0x4:0)=(cfg) rc_quant_incr_limit1{0x4:0) = {RC_QUANT_INCR_LIMIT1{0x4:0)
lcm_dcs_write_seq_static(ctx,0x73,0x33);// pps043{0x7:4]{0x3:0)=(cfg) rc_tgt_offset_hi{0x3:0], rc_tgt_offset_lo{0x3:0) = {RC_TGT_OFFSET_HI{0x3:0], RC_TGT_OFFSET_LO{0x3:0)
lcm_dcs_write_seq_static(ctx,0x74,0x0e);// pps044{0x7:0]<<6=(cfg) rc_buf_thresh{0x0] {0x13:0) = RC_BUF_THRESH{0x0] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x75,0x1c);// pps045{0x7:0]<<6=(cfg) rc_buf_thresh{0x1] {0x13:0) = RC_BUF_THRESH{0x1] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x76,0x2a);// pps046{0x7:0]<<6=(cfg) rc_buf_thresh{0x2] {0x13:0) = RC_BUF_THRESH{0x2] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x77,0x38);// pps047{0x7:0]<<6=(cfg) rc_buf_thresh{0x3] {0x13:0) = RC_BUF_THRESH{0x3] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x78,0x46);// pps048{0x7:0]<<6=(cfg) rc_buf_thresh{0x4] {0x13:0) = RC_BUF_THRESH{0x4] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x79,0x54);// pps049{0x7:0]<<6=(cfg) rc_buf_thresh{0x5] {0x13:0) = RC_BUF_THRESH{0x5] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x7a,0x62);// pps050{0x7:0]<<6=(cfg) rc_buf_thresh{0x6] {0x13:0) = RC_BUF_THRESH{0x6] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x7b,0x69);// pps051{0x7:0]<<6=(cfg) rc_buf_thresh{0x7] {0x13:0) = RC_BUF_THRESH{0x7] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x7c,0x70);// pps052{0x7:0]<<6=(cfg) rc_buf_thresh{0x8] {0x13:0) = RC_BUF_THRESH{0x8] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x7d,0x77);// pps053{0x7:0]<<6=(cfg) rc_buf_thresh{0x9] {0x13:0) = RC_BUF_THRESH{0x9] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x7e,0x79);// pps054{0x7:0]<<6=(cfg) rc_buf_thresh{0x10]{0x13:0) = RC_BUF_THRESH{0x10]{0x13:0]
lcm_dcs_write_seq_static(ctx,0x7f,0x7b);// pps055{0x7:0]<<6=(cfg) rc_buf_thresh{0x11]{0x13:0) = RC_BUF_THRESH{0x11]{0x13:0]
lcm_dcs_write_seq_static(ctx,0x80,0x7d);// pps056{0x7:0]<<6=(cfg) rc_buf_thresh{0x12]{0x13:0) = RC_BUF_THRESH{0x12]{0x13:0]
lcm_dcs_write_seq_static(ctx,0x81,0x7e);// pps057{0x7:0]<<6=(cfg) rc_buf_thresh{0x13]{0x13:0) = RC_BUF_THRESH{0x13]{0x13:0]
lcm_dcs_write_seq_static(ctx,0x82,0x01);// pps058{0x7:3]{0x2:0)=(cfg) range_min_qp_0{0x4:0] , range_max_qp_0{0x4:2]      } = {RC_MINQP{0x0]{0x4:0], RC_MAXQP{0x0]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x83,0xc2);// pps059{0x7:6]{0x5:0)=(cfg) range_max_qp_0{0x1:0] , range_bpg_offset_0{0x5:0]  } = {RC_MAXQP{0x0]{0x1:0], RC_OFFSET{0x0]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x84,0x22);// pps060{0x7:3]{0x2:0)=(cfg) range_min_qp_1{0x4:0] , range_max_qp_1{0x4:2]      } = {RC_MINQP{0x1]{0x4:0], RC_MAXQP{0x1]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x85,0x00);// pps061{0x7:6]{0x5:0)=(cfg) range_max_qp_1{0x1:0] , range_bpg_offset_1{0x5:0]  } = {RC_MAXQP{0x1]{0x1:0], RC_OFFSET{0x1]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x86,0x2a);// pps062{0x7:3]{0x2:0)=(cfg) range_min_qp_2{0x4:0] , range_max_qp_2{0x4:2]      } = {RC_MINQP{0x2]{0x4:0], RC_MAXQP{0x2]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x87,0x40);// pps063{0x7:6]{0x5:0)=(cfg) range_max_qp_2{0x1:0] , range_bpg_offset_2{0x5:0]  } = {RC_MAXQP{0x2]{0x1:0], RC_OFFSET{0x2]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x88,0x32);// pps064{0x7:3]{0x2:0)=(cfg) range_min_qp_3{0x4:0] , range_max_qp_3{0x4:2]      } = {RC_MINQP{0x3]{0x4:0], RC_MAXQP{0x3]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x89,0xbe);// pps065{0x7:6]{0x5:0)=(cfg) range_max_qp_3{0x1:0] , range_bpg_offset_3{0x5:0]  } = {RC_MAXQP{0x3]{0x1:0], RC_OFFSET{0x3]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x8a,0x3a);// pps066{0x7:3]{0x2:0)=(cfg) range_min_qp_4{0x4:0] , range_max_qp_4{0x4:2]      } = {RC_MINQP{0x4]{0x4:0], RC_MAXQP{0x4]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x8b,0xfc);// pps067{0x7:6]{0x5:0)=(cfg) range_max_qp_4{0x1:0] , range_bpg_offset_4{0x5:0]  } = {RC_MAXQP{0x4]{0x1:0], RC_OFFSET{0x4]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x8c,0x3a);// pps068{0x7:3]{0x2:0)=(cfg) range_min_qp_5{0x4:0] , range_max_qp_5{0x4:2]      } = {RC_MINQP{0x5]{0x4:0], RC_MAXQP{0x5]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x8d,0xfa);// pps069{0x7:6]{0x5:0)=(cfg) range_max_qp_5{0x1:0] , range_bpg_offset_5{0x5:0]  } = {RC_MAXQP{0x5]{0x1:0], RC_OFFSET{0x5]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x8e,0x3a);// pps070{0x7:3]{0x2:0)=(cfg) range_min_qp_6{0x4:0] , range_max_qp_6{0x4:2]      } = {RC_MINQP{0x6]{0x4:0], RC_MAXQP{0x6]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x8f,0xf8);// pps071{0x7:6]{0x5:0)=(cfg) range_max_qp_6{0x1:0] , range_bpg_offset_6{0x5:0]  } = {RC_MAXQP{0x6]{0x1:0], RC_OFFSET{0x6]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x90,0x3b);// pps072{0x7:3]{0x2:0)=(cfg) range_min_qp_7{0x4:0] , range_max_qp_7{0x4:2]      } = {RC_MINQP{0x7]{0x4:0], RC_MAXQP{0x7]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x91,0x38);// pps073{0x7:6]{0x5:0)=(cfg) range_max_qp_7{0x1:0] , range_bpg_offset_7{0x5:0]  } = {RC_MAXQP{0x7]{0x1:0], RC_OFFSET{0x7]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x92,0x3b);// pps074{0x7:3]{0x2:0)=(cfg) range_min_qp_8{0x4:0] , range_max_qp_8{0x4:2]      } = {RC_MINQP{0x8]{0x4:0], RC_MAXQP{0x8]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x93,0x78);// pps075{0x7:6]{0x5:0)=(cfg) range_max_qp_8{0x1:0] , range_bpg_offset_8{0x5:0]  } = {RC_MAXQP{0x8]{0x1:0], RC_OFFSET{0x8]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x94,0x3b);// pps076{0x7:3]{0x2:0)=(cfg) range_min_qp_9{0x4:0] , range_max_qp_9{0x4:2]      } = {RC_MINQP{0x9]{0x4:0], RC_MAXQP{0x9]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x95,0x76);// pps077{0x7:6]{0x5:0)=(cfg) range_max_qp_9{0x1:0] , range_bpg_offset_9{0x5:0]  } = {RC_MAXQP{0x9]{0x1:0], RC_OFFSET{0x9]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x96,0x4b);// pps078{0x7:3]{0x2:0)=(cfg) range_min_qp_10{0x4:0], range_max_qp_10{0x4:2]     } = {RC_MINQP{0x10]{0x4:0], RC_MAXQP{0x10]{0x4:2]  }
lcm_dcs_write_seq_static(ctx,0x97,0xb6);// pps079{0x7:6]{0x5:0)=(cfg) range_max_qp_10{0x1:0], range_bpg_offset_10{0x5:0] } = {RC_MAXQP{0x10]{0x1:0], RC_OFFSET{0x10]{0x5:0] }
lcm_dcs_write_seq_static(ctx,0x98,0x4b);// pps080{0x7:3]{0x2:0)=(cfg) range_min_qp_11{0x4:0], range_max_qp_11{0x4:2]     } = {RC_MINQP{0x11]{0x4:0], RC_MAXQP{0x11]{0x4:2]  }
lcm_dcs_write_seq_static(ctx,0x99,0xf6);// pps081{0x7:6]{0x5:0)=(cfg) range_max_qp_11{0x1:0], range_bpg_offset_11{0x5:0] } = {RC_MAXQP{0x11]{0x1:0], RC_OFFSET{0x11]{0x5:0] }
lcm_dcs_write_seq_static(ctx,0x9a,0x4c);// pps082{0x7:3]{0x2:0)=(cfg) range_min_qp_12{0x4:0], range_max_qp_12{0x4:2]     } = {RC_MINQP{0x12]{0x4:0], RC_MAXQP{0x12]{0x4:2]  }
lcm_dcs_write_seq_static(ctx,0x9b,0x34);// pps083{0x7:6]{0x5:0)=(cfg) range_max_qp_12{0x1:0], range_bpg_offset_12{0x5:0] } = {RC_MAXQP{0x12]{0x1:0], RC_OFFSET{0x12]{0x5:0] }
lcm_dcs_write_seq_static(ctx,0x9c,0x5c);// pps084{0x7:3]{0x2:0)=(cfg) range_min_qp_13{0x4:0], range_max_qp_13{0x4:2]     } = {RC_MINQP{0x13]{0x4:0], RC_MAXQP{0x13]{0x4:2]  }
lcm_dcs_write_seq_static(ctx,0x9d,0x74);// pps085{0x7:6]{0x5:0)=(cfg) range_max_qp_13{0x1:0], range_bpg_offset_13{0x5:0] } = {RC_MAXQP{0x13]{0x1:0], RC_OFFSET{0x13]{0x5:0] }
lcm_dcs_write_seq_static(ctx,0x9e,0x8c);// pps086{0x7:3]{0x2:0)=(cfg) range_min_qp_14{0x4:0], range_max_qp_14{0x4:2]     } = {RC_MINQP{0x14]{0x4:0], RC_MAXQP{0x14]{0x4:2]  }
lcm_dcs_write_seq_static(ctx,0x9f,0xf4);// pps087{0x7:6]{0x5:0)=(cfg) range_max_qp_14{0x1:0], range_bpg_offset_14{0x5:0] } = {RC_MAXQP{0x14]{0x1:0], RC_OFFSET{0x14]{0x5:0] }
lcm_dcs_write_seq_static(ctx,0xa2,0x05);// pps014{0x7:0)=(dyn) chunk_size{0x15:8)
lcm_dcs_write_seq_static(ctx,0xa3,0x46);// pps015{0x7:0)=(dyn) chunk_size{0x7:0)
lcm_dcs_write_seq_static(ctx,0xa4,0x00);// pps088{0x1]{0x0]  =(cfg) native_420, native_422} = {NATIVE_420, NATIVE_422}
lcm_dcs_write_seq_static(ctx,0xa5,0x00);// pps089{0x4:0)=(dyn) second_line_bpg_offset{0x4:0)
lcm_dcs_write_seq_static(ctx,0xa6,0x00);// pps090{0x7:0)=(dyn) nsl_bpg_offset{0x15:8)
lcm_dcs_write_seq_static(ctx,0xa7,0x00);// pps091{0x7:0)=(dyn) nsl_bpg_offset{0x7:0)
lcm_dcs_write_seq_static(ctx,0xa9,0x00);// pps092{0x7:0)=(dyn) second_line_offset_adj{0x15:8)
lcm_dcs_write_seq_static(ctx,0xaa,0x00);// pps093{0x7:0)=(dyn) second_line_offset_adj{0x7:0)
lcm_dcs_write_seq_static(ctx,0xa0,0xa0);// pps005{0x7:0)=(cfg) bpp{0x7:0) = {bits_per_pixel{0x7:0)

lcm_dcs_write_seq_static(ctx,0x4F,0x08);// 0x4FD2=0x08, use ENG PPS
// pps end

lcm_dcs_write_seq_static(ctx,0xFE,0xD6);//1080
lcm_dcs_write_seq_static(ctx,0x06,0x11);
lcm_dcs_write_seq_static(ctx,0x03,0x01);
lcm_dcs_write_seq_static(ctx,0x04,0xB4);
lcm_dcs_write_seq_static(ctx,0x05,0x68);

//IC WRITE
lcm_dcs_write_seq_static(ctx,0xFE,0x40);
lcm_dcs_write_seq_static(ctx,0xB2,0x00);
lcm_dcs_write_seq_static(ctx,0xB3,0x00);

lcm_dcs_write_seq_static(ctx,0xFE,0x26);
lcm_dcs_write_seq_static(ctx,0xA4,0x2B);   // ELVSS -2.4V
lcm_dcs_write_seq_static(ctx,0xA5,0x23);   // HRF ELVSS -3.2V
lcm_dcs_write_seq_static(ctx,0x20,0x01);   // 1pusle ?1st Group AVDD=7.8V
lcm_dcs_write_seq_static(ctx,0x22,0x19);   // 11? pusle??? 2nd Group FD ON
lcm_dcs_write_seq_static(ctx,0x23,0x03);   // 2 group enable

//ESD recover code start
lcm_dcs_write_seq_static(ctx,0xFE,0xD4); //Page D4
lcm_dcs_write_seq_static(ctx,0x40,0x03); // lvd/mipi error flag enable
lcm_dcs_write_seq_static(ctx,0xFE,0xFD); //Page FD
lcm_dcs_write_seq_static(ctx,0x80,0x06); //error report to 0A00 enable
lcm_dcs_write_seq_static(ctx,0x83,0x00); //command 1 OFF
lcm_dcs_write_seq_static(ctx,0xEE,0x00); // LSIF Path CS off
lcm_dcs_write_seq_static(ctx,0xEF,0x00); // LSIF Path CS off
lcm_dcs_write_seq_static(ctx,0xFE,0xD0); //Page D0
lcm_dcs_write_seq_static(ctx,0x7E,0x80); // DB Mode PAD off
lcm_dcs_write_seq_static(ctx,0xFE,0xD2); //Page D0
lcm_dcs_write_seq_static(ctx,0x23,0x56); // CPHY EN PAD off
lcm_dcs_write_seq_static(ctx,0xFE,0xA0); //Page A0
lcm_dcs_write_seq_static(ctx,0x06,0x32); //lvd function VCI/VDDI/AVDD
lcm_dcs_write_seq_static(ctx,0xFE,0xA1); //Page A1
lcm_dcs_write_seq_static(ctx,0xB3,0x3F);   
lcm_dcs_write_seq_static(ctx,0xFE,0xA1);   
lcm_dcs_write_seq_static(ctx,0xC3,0xC3);   
lcm_dcs_write_seq_static(ctx,0xC4,0x9C); 
lcm_dcs_write_seq_static(ctx,0xC5,0x1E); 
lcm_dcs_write_seq_static(ctx,0xC6,0x23); 
//ESD recover code end

lcm_dcs_write_seq_static(ctx,0xFE,0xD0);
lcm_dcs_write_seq_static(ctx,0x2A,0x14); //DC2DC prd1(5:0)
lcm_dcs_write_seq_static(ctx,0x2B,0x14); //DC2DC prd2(5:0)
lcm_dcs_write_seq_static(ctx,0x2D,0x14); //DC2DC prd3(5:0)
lcm_dcs_write_seq_static(ctx,0x2F,0x14); //DC2DC prd4(5:0)
lcm_dcs_write_seq_static(ctx,0x30,0x14); //DC2DC prd5(5:0)
lcm_dcs_write_seq_static(ctx,0x31,0x14); //DC2DC prd6(5:0)

// init code
lcm_dcs_write_seq_static(ctx,0xFE,0x40);
lcm_dcs_write_seq_static(ctx,0xBD,0x00);

lcm_dcs_write_seq_static(ctx,0xFE,0xA1);
lcm_dcs_write_seq_static(ctx,0xCD,0x6B); //mipi_lane0,1_lprx_pluse_dlyREGS.WRITE(0,0x15,0x3:0]
lcm_dcs_write_seq_static(ctx,0xCE,0xBB); //mipi_lane2,3_lprx_pluse_dlyREGS.WRITE(0,0x15,0x3:0]
lcm_dcs_write_seq_static(ctx,0xFE,0xD1);
lcm_dcs_write_seq_static(ctx,0xB4,0x01); //mrosc_meas
lcm_dcs_write_seq_static(ctx,0xFE,0x38);
lcm_dcs_write_seq_static(ctx,0x17,0x0F);
lcm_dcs_write_seq_static(ctx,0x18,0x0F);
lcm_dcs_write_seq_static(ctx,0xFE,0x00);
lcm_dcs_write_seq_static(ctx,0xfa,0x01);
lcm_dcs_write_seq_static(ctx,0xC2,0x08);
lcm_dcs_write_seq_static(ctx,0x35,0x00);
lcm_dcs_write_seq_static(ctx,0x51,0x00,0x00);

lcm_dcs_write_seq_static(ctx,0xFE,0x00);

lcm_dcs_write_seq_static(ctx,0x11);
msleep(100);
lcm_dcs_write_seq_static(ctx,0x29);
msleep(50);

	pr_err("%s-\n", __func__);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	// pr_err("%s\n", __func__);
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

	pr_err("%s+\n", __func__);
	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(80);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(50);
	
	// enter deep standby mode
	// lcm_dcs_write_seq_static(ctx, 0x4f, 0x01);
	// msleep(120);
	
	// lcd reset L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	msleep(5);
	
	//lcd vci L
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	msleep(5);
	
	// lcd dvdd L
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);
	// msleep(5);

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
	
	ctx->error = 0;
	ctx->prepared = false;

	g_doze_en = false;
	g_hbm_stat = false;
	g_ctx->hbm_en = false;

	pr_err("%s-\n", __func__);
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_err("%s+\n", __func__);
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
	// msleep(5);
	
	// lcd dvdd H
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);
	msleep(3);
	
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
	msleep(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(40);
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

	pr_err("%s-\n", __func__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	// pr_err("%s+\n", __func__);
	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;
	// pr_err("%s-\n", __func__);

	return 0;
}

#define HFP (100)
#define HSA (8)
#define HBP (92)
#define HACT (1080)
#define VFP (72)
#define VSA (4)
#define VBP (8)
#define VACT (2400)

static const struct drm_display_mode switch_mode_120hz = {
	.clock		= ((HACT+HFP+HSA+HBP)*(VACT+VFP+VSA+VBP)*(120)/1000),
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
	.clock		= ((HACT+HFP+HSA+HBP)*(VACT+VFP+VSA+VBP)*(60)/1000),
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
	.data_rate = 550*2,
	.pll_clk = 550,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0b,
		.count = 1,
		.para_list[0] = 0x00,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
		.para_list[1] = 0xdc, //aod
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0xfa,
		.count = 1,
		.para_list[0] = 0x01,
	},
	.physical_width_um = 69523,
	.physical_height_um = 154496,
	.lp_perline_en = 1,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 0,
		.rgb_swap = 0,
		.dsc_cfg = 2088,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 160,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 40,
		.slice_width = 1080,
		.chunk_size = 1350,
		.xmit_delay = 410,
		.dec_delay = 688,
		.scale_value = 25,
		.increment_interval = 1230,
		.decrement_interval = 21,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 631,
		.slice_bpg_offset = 399,
		.initial_offset = 5632,
		.final_offset = 4332,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
};

static struct mtk_panel_params ext_params_60hz = {
	.data_rate = 550*2,
	.pll_clk = 550,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0b,
		.count = 1,
		.para_list[0] = 0x00,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
		.para_list[1] = 0xdc, //aod
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0xfa,
		.count = 1,
		.para_list[0] = 0x01,
	},
	.physical_width_um = 69523,
	.physical_height_um = 154496,
	.lp_perline_en = 1,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 0,
		.rgb_swap = 0,
		.dsc_cfg = 2088,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 160,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 40,
		.slice_width = 1080,
		.chunk_size = 1350,
		.xmit_delay = 410,
		.dec_delay = 688,
		.scale_value = 25,
		.increment_interval = 1230,
		.decrement_interval = 21,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 631,
		.slice_bpg_offset = 399,
		.initial_offset = 5632,
		.final_offset = 4332,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	char bl_tb[] = {0x51, 0x0D, 0xBB};
	char hbm_tb[] = {0x51, 0x0F, 0xFF};

	if (level > 4095) {
		if (level == 4096) {
			if (!cb)
				return -1;
			pr_err("%s panel into HBM", __func__);
			g_hbm_stat = true;
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
			g_hbm_stat = false;
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
			if ((g_hbm_stat == false) || (level == 0))
				cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
			pr_err("%s level=%d, rawlevel=%d, bl_tb[1]=0x%x, bl_tb[2]=0x%x\n",
				__func__, level, rawlevel, bl_tb[1], bl_tb[2]);
		} else {
			pr_err("%s err, rawlevel=%d\n", __func__, rawlevel);
		}
	}
	
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
	else if (drm_mode_vrefresh(m) == 60)
		ext->params = &ext_params_60hz;
	else
		ret = 1;

	return ret;
}

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);

		lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x06);
		lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	}
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);

		lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
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
	} else if (drm_mode_vrefresh(m) == 120) { /*switch to 120 */
		mode_switch_to_120(panel, stage);
	} else
		ret = 1;

	return ret;
}

static int panel_doze_enable(struct drm_panel *panel,
	void *dsi_drv, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_err("%s+", __func__);

	lcm_dcs_write_seq_static(ctx, 0x39);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x01, 0x55);
	
	g_doze_en = true;
	// pr_err("%s-\n", __func__);
	return 0;
}

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi_drv, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_err("%s+", __func__);

	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x38);

	g_doze_en = false;
	// pr_err("%s-\n", __func__);
	return 0;
}

static int panel_set_aod_light_mode(void *dsi_drv, dcs_write_gce cb,
	void *handle, unsigned int mode)
{
	pr_err("%s, mode%d", __func__, mode);

	if (mode == 1) {
		/* AOD1 Switch */
		lcm_dcs_write_seq_static(g_ctx,0xFE,0x00);
		lcm_dcs_write_seq_static(g_ctx,0x39);
		lcm_dcs_write_seq_static(g_ctx,0x51,0x0F,0xFE);
	} else if (mode == 2) {
		/* AOD2 Switch */
		lcm_dcs_write_seq_static(g_ctx,0xFE,0x00);
		lcm_dcs_write_seq_static(g_ctx,0x39);
		lcm_dcs_write_seq_static(g_ctx,0x51,0x07,0xFF);
	} else if (mode == 3) {
		/* AOD3 Switch */
		lcm_dcs_write_seq_static(g_ctx,0xFE,0x00);
		lcm_dcs_write_seq_static(g_ctx,0x39);
		lcm_dcs_write_seq_static(g_ctx,0x51,0x01,0x55);
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
	.hbm_set_cmdq = panel_hbm_set_cmdq,
	.hbm_get_state = panel_hbm_get_state,
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

	mode = drm_mode_duplicate(connector->dev, &switch_mode_120hz);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 switch_mode_120hz.hdisplay, switch_mode_120hz.vdisplay,
			 drm_mode_vrefresh(&switch_mode_120hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode);

	mode_1 = drm_mode_duplicate(connector->dev, &switch_mode_60hz);
	if (!mode_1) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_60hz.hdisplay, switch_mode_60hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_60hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_1);

	connector->display_info.width_mm = 64;
	connector->display_info.height_mm = 129;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

// drv added by chenjiaxi, add /sys/devices/platform/oled_node/oled, begin
static ssize_t oled_node_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;
	count = snprintf(buf, PAGE_SIZE, "oled screens:yes\n");  
	return count;
}

static ssize_t oled_node_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}
static DEVICE_ATTR(oled, 0664, oled_node_show, oled_node_store);

static int oled_node_sysfs_add(struct platform_device *pdev)
{
	int err = 0;
    pr_info("%s\n", __func__);
  	err = device_create_file(&pdev->dev, &dev_attr_oled);
	if (err) {
        pr_err("%s device_create_file failed\n", __func__);
        return -ENODEV;
	}
	return 0;
}

static int oled_node_probe(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	oled_node_sysfs_add(pdev);
	return 0;
}

static const struct of_device_id oled_node_of_match[] = {
	{ .compatible = "coosea,oled", },
	{ },
};
MODULE_DEVICE_TABLE(of, oled_node_of_match);

static struct platform_driver oled_node_driver = {
	.probe = oled_node_probe,
	.driver = {
		.name = "oled",
		.of_match_table = oled_node_of_match,
	},
};

static int oled_node_init(void)
{
	pr_info("%s\n",__func__);
	return platform_driver_register(&oled_node_driver);
}
// drv added by chenjiaxi, add /sys/devices/platform/oled_node/oled, end

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
	g_doze_en = false;
	g_hbm_stat = false;
	g_ctx->hbm_en = false;

	pr_info("%s- lcm,rm692e5,cmd,gy2\n", __func__);

	// drv added by chenjiaxi, add /sys/devices/platform/oled_node/oled, begin
	oled_node_init();
	// drv added by chenjiaxi, add /sys/devices/platform/oled_node/oled, end

#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
    strcpy(current_lcm_info.chip,"rm692e5,cmd,gy2");
    strcpy(current_lcm_info.vendor,"Raydium");
    sprintf(current_lcm_info.id,"0x%x",0xd28004);
    strcpy(current_lcm_info.more,"1080*2400");
#endif
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

static const struct of_device_id lcm_of_match[] = {
	{
	    .compatible = "visionox,rm692e5,cmd,gy2",
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-visionox-rm692e5-dphy-cmd-120hz-gy2",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("MEDIATEK");
MODULE_DESCRIPTION("rm692e5 AMOLED CMD LCD Panel Driver");
MODULE_LICENSE("GPL v2");
