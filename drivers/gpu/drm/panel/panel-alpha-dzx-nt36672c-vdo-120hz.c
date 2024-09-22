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

#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"

#include "../../../misc/mediatek/gate_ic/gate_i2c.h"

//#include "lcm_i2c.h"

#define HFP_SUPPORT 1

#if HFP_SUPPORT
static int current_fps = 60;
#endif
static char bl_tb0[] = { 0x51, 0xff };

//TO DO: You have to do that remove macro BYPASSI2C and solve build error
//otherwise voltage will be unstable


struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	bool prepared;
	bool enabled;

	int error;
};

#define lcm_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define lcm_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
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
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
#if 1
lcm_dcs_write_seq_static(ctx, 0XFF, 0X10);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X35, 0X00);//TEÊä³ö
lcm_dcs_write_seq_static(ctx, 0XB0, 0X00);
lcm_dcs_write_seq_static(ctx, 0XC0, 0X03);
lcm_dcs_write_seq_static(ctx, 0XB0, 0X00);
lcm_dcs_write_seq_static(ctx, 0XC1, 0X89,0X28,0X00,0X08,0X00,0XAA,0X02,0X0E,0X00,0X2B,0X00,0X07,0X0D,0XB7,0X0C,0XB7);
lcm_dcs_write_seq_static(ctx, 0XC2, 0X1B,0XA0);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X20);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X01, 0X66);
lcm_dcs_write_seq_static(ctx, 0X06, 0X5B);
lcm_dcs_write_seq_static(ctx, 0X07, 0X27);
lcm_dcs_write_seq_static(ctx, 0X1B, 0X01);
lcm_dcs_write_seq_static(ctx, 0X30, 0XFF); //LV detector disable
lcm_dcs_write_seq_static(ctx, 0X5C, 0X90);
lcm_dcs_write_seq_static(ctx, 0X5E, 0XB0);
lcm_dcs_write_seq_static(ctx, 0X69, 0XD0);
lcm_dcs_write_seq_static(ctx, 0X95, 0XE0);
lcm_dcs_write_seq_static(ctx, 0X96, 0XE0);
lcm_dcs_write_seq_static(ctx, 0XF2, 0X65);
lcm_dcs_write_seq_static(ctx, 0XF3, 0X54);
lcm_dcs_write_seq_static(ctx, 0XF4, 0X65);
lcm_dcs_write_seq_static(ctx, 0XF5, 0X54);
lcm_dcs_write_seq_static(ctx, 0XF6, 0X65);
lcm_dcs_write_seq_static(ctx, 0XF7, 0X54);
lcm_dcs_write_seq_static(ctx, 0XF8, 0X65);
lcm_dcs_write_seq_static(ctx, 0XF9, 0X54);

lcm_dcs_write_seq_static(ctx, 0xFF,0x20);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
//R(+));
lcm_dcs_write_seq_static(ctx, 0xB0 ,0x00 ,0x00 ,0x00 ,0x2E ,0x00 ,0x5D ,0x00 ,0x81 ,0x00 ,0x9D ,0x00 ,0xB6 ,0x00 ,0xCC ,0x00 ,0xDF);
lcm_dcs_write_seq_static(ctx, 0xB1 ,0x00 ,0xF0 ,0x01 ,0x29 ,0x01 ,0x50 ,0x01 ,0x8F ,0x01 ,0xBB ,0x02 ,0x04 ,0x02 ,0x39 ,0x02 ,0x3B);
lcm_dcs_write_seq_static(ctx, 0xB2 ,0x02 ,0x70 ,0x02 ,0xAB ,0x02 ,0xD3 ,0x03 ,0x03 ,0x03 ,0x26 ,0x03 ,0x4E ,0x03 ,0x5D ,0x03 ,0x6B);
lcm_dcs_write_seq_static(ctx, 0xB3 ,0x03 ,0x7E ,0x03 ,0x95 ,0x03 ,0xB0 ,0x03 ,0xC7 ,0x03 ,0xD7 ,0x03 ,0xD9 ,0x00 ,0x00);
//G(+));
lcm_dcs_write_seq_static(ctx, 0xB4 ,0x00 ,0x00 ,0x00 ,0x2E ,0x00 ,0x5D ,0x00 ,0x81 ,0x00 ,0x9D ,0x00 ,0xB6 ,0x00 ,0xCC ,0x00 ,0xDF);
lcm_dcs_write_seq_static(ctx, 0xB5 ,0x00 ,0xF0 ,0x01 ,0x29 ,0x01 ,0x50 ,0x01 ,0x8F ,0x01 ,0xBB ,0x02 ,0x04 ,0x02 ,0x39 ,0x02 ,0x3B);
lcm_dcs_write_seq_static(ctx, 0xB6 ,0x02 ,0x70 ,0x02 ,0xAB ,0x02 ,0xD3 ,0x03 ,0x03 ,0x03 ,0x26 ,0x03 ,0x4E ,0x03 ,0x5D ,0x03 ,0x6B);
lcm_dcs_write_seq_static(ctx, 0xB7 ,0x03 ,0x7E ,0x03 ,0x95 ,0x03 ,0xB0 ,0x03 ,0xC7 ,0x03 ,0xD7 ,0x03 ,0xD9 ,0x00 ,0x00);
//B(+));
lcm_dcs_write_seq_static(ctx, 0xB8 ,0x00 ,0x00 ,0x00 ,0x2E ,0x00 ,0x5D ,0x00 ,0x81 ,0x00 ,0x9D ,0x00 ,0xB6 ,0x00 ,0xCC ,0x00 ,0xDF);
lcm_dcs_write_seq_static(ctx, 0xB9 ,0x00 ,0xF0 ,0x01 ,0x29 ,0x01 ,0x50 ,0x01 ,0x8F ,0x01 ,0xBB ,0x02 ,0x04 ,0x02 ,0x39 ,0x02 ,0x3B);
lcm_dcs_write_seq_static(ctx, 0xBA ,0x02 ,0x70 ,0x02 ,0xAB ,0x02 ,0xD3 ,0x03 ,0x03 ,0x03 ,0x26 ,0x03 ,0x4E ,0x03 ,0x5D ,0x03 ,0x6B);
lcm_dcs_write_seq_static(ctx, 0xBB ,0x03 ,0x7E ,0x03 ,0x95 ,0x03 ,0xB0 ,0x03 ,0xC7 ,0x03 ,0xD7 ,0x03 ,0xD9 ,0x00 ,0x00);
//R+C1);
lcm_dcs_write_seq_static(ctx, 0xC6 ,0x00);
lcm_dcs_write_seq_static(ctx, 0xC7 ,0x00);
lcm_dcs_write_seq_static(ctx, 0xC8 ,0x00);
lcm_dcs_write_seq_static(ctx, 0xC9 ,0x00);
lcm_dcs_write_seq_static(ctx, 0xCA ,0x00);
//R-C1);
lcm_dcs_write_seq_static(ctx, 0xCB ,0x00);
lcm_dcs_write_seq_static(ctx, 0xCC ,0x00);
lcm_dcs_write_seq_static(ctx, 0xCD ,0x00);
lcm_dcs_write_seq_static(ctx, 0xCE ,0x00);
lcm_dcs_write_seq_static(ctx, 0xCF ,0x00);
//G+C1);
lcm_dcs_write_seq_static(ctx, 0xD0 ,0x00);
lcm_dcs_write_seq_static(ctx, 0xD1 ,0x00);
lcm_dcs_write_seq_static(ctx, 0xD2 ,0x00);
lcm_dcs_write_seq_static(ctx, 0xD3 ,0x00);
lcm_dcs_write_seq_static(ctx, 0xD4 ,0x00);
//G-C1);
lcm_dcs_write_seq_static(ctx, 0xD5 ,0x00);
lcm_dcs_write_seq_static(ctx, 0xD6 ,0x00);
lcm_dcs_write_seq_static(ctx, 0xD7 ,0x00);
lcm_dcs_write_seq_static(ctx, 0xD8 ,0x00);
lcm_dcs_write_seq_static(ctx, 0xD9 ,0x00);
//B+C1);
lcm_dcs_write_seq_static(ctx, 0xDA ,0x00);
lcm_dcs_write_seq_static(ctx, 0xDB ,0x00);
lcm_dcs_write_seq_static(ctx, 0xDC ,0x00);
lcm_dcs_write_seq_static(ctx, 0xDD ,0x00);
lcm_dcs_write_seq_static(ctx, 0xDE ,0x00);
//B-C1);
lcm_dcs_write_seq_static(ctx, 0xDF ,0x00);
lcm_dcs_write_seq_static(ctx, 0xE0 ,0x00);
lcm_dcs_write_seq_static(ctx, 0xE1 ,0x00);
lcm_dcs_write_seq_static(ctx, 0xE2 ,0x00);
lcm_dcs_write_seq_static(ctx, 0xE3 ,0x00);
//R+C1);
lcm_dcs_write_seq_static(ctx, 0xE4 ,0x00);
//R-C1);
lcm_dcs_write_seq_static(ctx, 0xE5 ,0x00);
//G+C1);
lcm_dcs_write_seq_static(ctx, 0xE6 ,0x00);
//G-C1);
lcm_dcs_write_seq_static(ctx, 0xE7 ,0x00);
//B+C1);
lcm_dcs_write_seq_static(ctx, 0xE8 ,0x00);
//B-C1);
lcm_dcs_write_seq_static(ctx, 0xE9 ,0x00);
//CMD2_Page1
lcm_dcs_write_seq_static(ctx, 0xFF,0x21);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
//R(-));
lcm_dcs_write_seq_static(ctx, 0xB0 ,0x00 ,0x00 ,0x00 ,0x2E ,0x00 ,0x5D ,0x00 ,0x81 ,0x00 ,0x9D ,0x00 ,0xB6 ,0x00 ,0xCC ,0x00 ,0xDF);
lcm_dcs_write_seq_static(ctx, 0xB1 ,0x00 ,0xF0 ,0x01 ,0x29 ,0x01 ,0x50 ,0x01 ,0x8F ,0x01 ,0xBB ,0x02 ,0x04 ,0x02 ,0x39 ,0x02 ,0x3B);
lcm_dcs_write_seq_static(ctx, 0xB2 ,0x02 ,0x70 ,0x02 ,0xAB ,0x02 ,0xD3 ,0x03 ,0x03 ,0x03 ,0x26 ,0x03 ,0x4E ,0x03 ,0x5D ,0x03 ,0x6B);
lcm_dcs_write_seq_static(ctx, 0xB3 ,0x03 ,0x7E ,0x03 ,0x95 ,0x03 ,0xB0 ,0x03 ,0xC7 ,0x03 ,0xD7 ,0x03 ,0xD9 ,0x00 ,0x00);
//G(-));
lcm_dcs_write_seq_static(ctx, 0xB4 ,0x00 ,0x00 ,0x00 ,0x2E ,0x00 ,0x5D ,0x00 ,0x81 ,0x00 ,0x9D ,0x00 ,0xB6 ,0x00 ,0xCC ,0x00 ,0xDF);
lcm_dcs_write_seq_static(ctx, 0xB5 ,0x00 ,0xF0 ,0x01 ,0x29 ,0x01 ,0x50 ,0x01 ,0x8F ,0x01 ,0xBB ,0x02 ,0x04 ,0x02 ,0x39 ,0x02 ,0x3B);
lcm_dcs_write_seq_static(ctx, 0xB6 ,0x02 ,0x70 ,0x02 ,0xAB ,0x02 ,0xD3 ,0x03 ,0x03 ,0x03 ,0x26 ,0x03 ,0x4E ,0x03 ,0x5D ,0x03 ,0x6B);
lcm_dcs_write_seq_static(ctx, 0xB7 ,0x03 ,0x7E ,0x03 ,0x95 ,0x03 ,0xB0 ,0x03 ,0xC7 ,0x03 ,0xD7 ,0x03 ,0xD9 ,0x00 ,0x00);
//B(-));
lcm_dcs_write_seq_static(ctx, 0xB8 ,0x00 ,0x00 ,0x00 ,0x2E ,0x00 ,0x5D ,0x00 ,0x81 ,0x00 ,0x9D ,0x00 ,0xB6 ,0x00 ,0xCC ,0x00 ,0xDF);
lcm_dcs_write_seq_static(ctx, 0xB9 ,0x00 ,0xF0 ,0x01 ,0x29 ,0x01 ,0x50 ,0x01 ,0x8F ,0x01 ,0xBB ,0x02 ,0x04 ,0x02 ,0x39 ,0x02 ,0x3B);
lcm_dcs_write_seq_static(ctx, 0xBA ,0x02 ,0x70 ,0x02 ,0xAB ,0x02 ,0xD3 ,0x03 ,0x03 ,0x03 ,0x26 ,0x03 ,0x4E ,0x03 ,0x5D ,0x03 ,0x6B);
lcm_dcs_write_seq_static(ctx, 0xBB ,0x03 ,0x7E ,0x03 ,0x95 ,0x03 ,0xB0 ,0x03 ,0xC7 ,0x03 ,0xD7 ,0x03 ,0xD9 ,0x00 ,0x00);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X21);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X24);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X00, 0X11);
lcm_dcs_write_seq_static(ctx, 0X07, 0X18);
lcm_dcs_write_seq_static(ctx, 0X08, 0X16);
lcm_dcs_write_seq_static(ctx, 0X09, 0X14);
lcm_dcs_write_seq_static(ctx, 0X0A, 0X17);
lcm_dcs_write_seq_static(ctx, 0X0B, 0X15);
lcm_dcs_write_seq_static(ctx, 0X0C, 0X13);
lcm_dcs_write_seq_static(ctx, 0X0D, 0X0F);
lcm_dcs_write_seq_static(ctx, 0X0E, 0X2C);
lcm_dcs_write_seq_static(ctx, 0X0F, 0X32);
lcm_dcs_write_seq_static(ctx, 0X10, 0X30);
lcm_dcs_write_seq_static(ctx, 0X11, 0X2E);
lcm_dcs_write_seq_static(ctx, 0X12, 0X01);
lcm_dcs_write_seq_static(ctx, 0X13, 0X10);
lcm_dcs_write_seq_static(ctx, 0X14, 0X10);
lcm_dcs_write_seq_static(ctx, 0X15, 0X10);
lcm_dcs_write_seq_static(ctx, 0X16, 0X10);
lcm_dcs_write_seq_static(ctx, 0X17, 0X10);
lcm_dcs_write_seq_static(ctx, 0X18, 0X11);
lcm_dcs_write_seq_static(ctx, 0X1F, 0X18);
lcm_dcs_write_seq_static(ctx, 0X20, 0X16);
lcm_dcs_write_seq_static(ctx, 0X21, 0X14);
lcm_dcs_write_seq_static(ctx, 0X22, 0X17);
lcm_dcs_write_seq_static(ctx, 0X23, 0X15);
lcm_dcs_write_seq_static(ctx, 0X24, 0X13);
lcm_dcs_write_seq_static(ctx, 0X25, 0X0F);
lcm_dcs_write_seq_static(ctx, 0X26, 0X2D);
lcm_dcs_write_seq_static(ctx, 0X27, 0X33);
lcm_dcs_write_seq_static(ctx, 0X28, 0X31);
lcm_dcs_write_seq_static(ctx, 0X29, 0X2F);
lcm_dcs_write_seq_static(ctx, 0X2A, 0X01);
lcm_dcs_write_seq_static(ctx, 0X2B, 0X10);
lcm_dcs_write_seq_static(ctx, 0X2D, 0X10);
lcm_dcs_write_seq_static(ctx, 0X2F, 0X10);
lcm_dcs_write_seq_static(ctx, 0X30, 0X10);
lcm_dcs_write_seq_static(ctx, 0X31, 0X10);
lcm_dcs_write_seq_static(ctx, 0X32, 0X05);
lcm_dcs_write_seq_static(ctx, 0X35, 0X01);
lcm_dcs_write_seq_static(ctx, 0X36, 0X35);
lcm_dcs_write_seq_static(ctx, 0X4D, 0X03);
lcm_dcs_write_seq_static(ctx, 0X4E, 0X30);
lcm_dcs_write_seq_static(ctx, 0X4F, 0X30);
lcm_dcs_write_seq_static(ctx, 0X53, 0X30);
lcm_dcs_write_seq_static(ctx, 0X7A, 0X83);
lcm_dcs_write_seq_static(ctx, 0X7B, 0X0D);
lcm_dcs_write_seq_static(ctx, 0X7C, 0X00);
lcm_dcs_write_seq_static(ctx, 0X7D, 0X03);
lcm_dcs_write_seq_static(ctx, 0X80, 0X03);
lcm_dcs_write_seq_static(ctx, 0X81, 0X03);
lcm_dcs_write_seq_static(ctx, 0X82, 0X13);
lcm_dcs_write_seq_static(ctx, 0X84, 0X31);
lcm_dcs_write_seq_static(ctx, 0X85, 0X00);
lcm_dcs_write_seq_static(ctx, 0X86, 0X00);
lcm_dcs_write_seq_static(ctx, 0X87, 0X00);
lcm_dcs_write_seq_static(ctx, 0X90, 0X13);
lcm_dcs_write_seq_static(ctx, 0X92, 0X31);
lcm_dcs_write_seq_static(ctx, 0X93, 0X00);
lcm_dcs_write_seq_static(ctx, 0X94, 0X00);
lcm_dcs_write_seq_static(ctx, 0X95, 0X00);
lcm_dcs_write_seq_static(ctx, 0X9C, 0XF4);
lcm_dcs_write_seq_static(ctx, 0X9D, 0X01);
lcm_dcs_write_seq_static(ctx, 0XA0, 0X0D);
lcm_dcs_write_seq_static(ctx, 0XA2, 0X0D);
lcm_dcs_write_seq_static(ctx, 0XA3, 0X03);
lcm_dcs_write_seq_static(ctx, 0XA4, 0X03);
lcm_dcs_write_seq_static(ctx, 0XA5, 0X03);
lcm_dcs_write_seq_static(ctx, 0XC4, 0X80);
lcm_dcs_write_seq_static(ctx, 0XC6, 0XC0);
lcm_dcs_write_seq_static(ctx, 0XC9, 0X00);
lcm_dcs_write_seq_static(ctx, 0XD9, 0X80);
lcm_dcs_write_seq_static(ctx, 0XE9, 0X03);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X25);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X0F, 0X13);
lcm_dcs_write_seq_static(ctx, 0X18, 0X20);//120hz
lcm_dcs_write_seq_static(ctx, 0X19, 0XE4);

lcm_dcs_write_seq_static(ctx, 0X21, 0X40);
lcm_dcs_write_seq_static(ctx, 0X68, 0X58);
lcm_dcs_write_seq_static(ctx, 0X69, 0X10);
lcm_dcs_write_seq_static(ctx, 0X6B, 0X00);
lcm_dcs_write_seq_static(ctx, 0X77, 0X72);
lcm_dcs_write_seq_static(ctx, 0X78, 0X15);
lcm_dcs_write_seq_static(ctx, 0X81, 0X00);
lcm_dcs_write_seq_static(ctx, 0X84, 0X2D);
lcm_dcs_write_seq_static(ctx, 0X86, 0X0C);
lcm_dcs_write_seq_static(ctx, 0X8E, 0X10);
lcm_dcs_write_seq_static(ctx, 0XF1, 0X40);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X26);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X14, 0X06);
lcm_dcs_write_seq_static(ctx, 0X15, 0X01);
lcm_dcs_write_seq_static(ctx, 0X74, 0XAF);
lcm_dcs_write_seq_static(ctx, 0X81, 0X0D);
lcm_dcs_write_seq_static(ctx, 0X83, 0X03);
lcm_dcs_write_seq_static(ctx, 0X84, 0X02);
lcm_dcs_write_seq_static(ctx, 0X85, 0X01);
lcm_dcs_write_seq_static(ctx, 0X86, 0X02);
lcm_dcs_write_seq_static(ctx, 0X87, 0X01);
lcm_dcs_write_seq_static(ctx, 0X88, 0X05);
lcm_dcs_write_seq_static(ctx, 0X8A, 0X1A);
lcm_dcs_write_seq_static(ctx, 0X8B, 0X11);
lcm_dcs_write_seq_static(ctx, 0X8C, 0X24);
lcm_dcs_write_seq_static(ctx, 0X8E, 0X42);
lcm_dcs_write_seq_static(ctx, 0X8F, 0X11);
lcm_dcs_write_seq_static(ctx, 0X90, 0X11);
lcm_dcs_write_seq_static(ctx, 0X91, 0X11);
lcm_dcs_write_seq_static(ctx, 0X9A, 0X80);
lcm_dcs_write_seq_static(ctx, 0X9B, 0X04);
lcm_dcs_write_seq_static(ctx, 0X9C, 0X00);
lcm_dcs_write_seq_static(ctx, 0X9D, 0X00);
lcm_dcs_write_seq_static(ctx, 0X9E, 0X00);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X27);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X01, 0X68);
lcm_dcs_write_seq_static(ctx, 0X20, 0X81);
lcm_dcs_write_seq_static(ctx, 0X21, 0X6F);
lcm_dcs_write_seq_static(ctx, 0X25, 0X81);
lcm_dcs_write_seq_static(ctx, 0X26, 0X97);
lcm_dcs_write_seq_static(ctx, 0X6E, 0X12);
lcm_dcs_write_seq_static(ctx, 0X6F, 0X00);
lcm_dcs_write_seq_static(ctx, 0X70, 0X00);
lcm_dcs_write_seq_static(ctx, 0X71, 0X00);
lcm_dcs_write_seq_static(ctx, 0X72, 0X00);
lcm_dcs_write_seq_static(ctx, 0X73, 0X76);
lcm_dcs_write_seq_static(ctx, 0X74, 0X10);
lcm_dcs_write_seq_static(ctx, 0X75, 0X32);
lcm_dcs_write_seq_static(ctx, 0X76, 0X54);
lcm_dcs_write_seq_static(ctx, 0X77, 0X00);
lcm_dcs_write_seq_static(ctx, 0X7D, 0X09);
lcm_dcs_write_seq_static(ctx, 0X7E, 0X6D);
lcm_dcs_write_seq_static(ctx, 0X80, 0X27);
lcm_dcs_write_seq_static(ctx, 0X82, 0X09);
lcm_dcs_write_seq_static(ctx, 0X83, 0X6D);
lcm_dcs_write_seq_static(ctx, 0X88, 0X01);
lcm_dcs_write_seq_static(ctx, 0X89, 0X01);
lcm_dcs_write_seq_static(ctx, 0XE3, 0X01);
lcm_dcs_write_seq_static(ctx, 0XE4, 0XE9);
lcm_dcs_write_seq_static(ctx, 0XE5, 0X02);
lcm_dcs_write_seq_static(ctx, 0XE6, 0XDE);
lcm_dcs_write_seq_static(ctx, 0XE9, 0X02);
lcm_dcs_write_seq_static(ctx, 0XEA, 0X1E);
lcm_dcs_write_seq_static(ctx, 0XEB, 0X03);
lcm_dcs_write_seq_static(ctx, 0XEC, 0X2D);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X2A);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X00, 0X91);
lcm_dcs_write_seq_static(ctx, 0X03, 0X20);
lcm_dcs_write_seq_static(ctx, 0X07, 0X64);
lcm_dcs_write_seq_static(ctx, 0X0A, 0X70);
lcm_dcs_write_seq_static(ctx, 0X0D, 0X40);
lcm_dcs_write_seq_static(ctx, 0X0E, 0X02);
lcm_dcs_write_seq_static(ctx, 0X11, 0XF0);
lcm_dcs_write_seq_static(ctx, 0X15, 0X0F);
lcm_dcs_write_seq_static(ctx, 0X16, 0X28);
lcm_dcs_write_seq_static(ctx, 0X19, 0X0E);
lcm_dcs_write_seq_static(ctx, 0X1A, 0XFC);
lcm_dcs_write_seq_static(ctx, 0X1B, 0X12);
lcm_dcs_write_seq_static(ctx, 0X1D, 0X36);
lcm_dcs_write_seq_static(ctx, 0X1E, 0X37);
lcm_dcs_write_seq_static(ctx, 0X1F, 0X37);
lcm_dcs_write_seq_static(ctx, 0X20, 0X37);
lcm_dcs_write_seq_static(ctx, 0X28, 0XF3);
lcm_dcs_write_seq_static(ctx, 0X29, 0X1F);
lcm_dcs_write_seq_static(ctx, 0X2A, 0XFF);
lcm_dcs_write_seq_static(ctx, 0X2D, 0X05);
lcm_dcs_write_seq_static(ctx, 0X2F, 0X06);
lcm_dcs_write_seq_static(ctx, 0X31, 0X9F);
lcm_dcs_write_seq_static(ctx, 0X33, 0X19);
lcm_dcs_write_seq_static(ctx, 0X34, 0XF5);
lcm_dcs_write_seq_static(ctx, 0X35, 0X45);
lcm_dcs_write_seq_static(ctx, 0X36, 0XCA);
lcm_dcs_write_seq_static(ctx, 0X37, 0XEE);
lcm_dcs_write_seq_static(ctx, 0X38, 0X4B);
lcm_dcs_write_seq_static(ctx, 0X39, 0XC4);
lcm_dcs_write_seq_static(ctx, 0X46, 0X40);
lcm_dcs_write_seq_static(ctx, 0X47, 0X02);
lcm_dcs_write_seq_static(ctx, 0X4A, 0XF0);
lcm_dcs_write_seq_static(ctx, 0X4E, 0X0F);
lcm_dcs_write_seq_static(ctx, 0X4F, 0X28);
lcm_dcs_write_seq_static(ctx, 0X52, 0X0E);
lcm_dcs_write_seq_static(ctx, 0X53, 0XFC);
lcm_dcs_write_seq_static(ctx, 0X54, 0X12);
lcm_dcs_write_seq_static(ctx, 0X56, 0X36);
lcm_dcs_write_seq_static(ctx, 0X57, 0X4F);
lcm_dcs_write_seq_static(ctx, 0X58, 0X4F);
lcm_dcs_write_seq_static(ctx, 0X59, 0X4F);
lcm_dcs_write_seq_static(ctx, 0X60, 0X80);
lcm_dcs_write_seq_static(ctx, 0X61, 0XE5);
lcm_dcs_write_seq_static(ctx, 0X62, 0X17);
lcm_dcs_write_seq_static(ctx, 0X63, 0XBC);
lcm_dcs_write_seq_static(ctx, 0X65, 0X05);
lcm_dcs_write_seq_static(ctx, 0X66, 0X04);
lcm_dcs_write_seq_static(ctx, 0X67, 0X14);
lcm_dcs_write_seq_static(ctx, 0X68, 0XAB);
lcm_dcs_write_seq_static(ctx, 0X6A, 0XD6);
lcm_dcs_write_seq_static(ctx, 0X6B, 0XE7);
lcm_dcs_write_seq_static(ctx, 0X6C, 0X31);
lcm_dcs_write_seq_static(ctx, 0X6D, 0XFD);
lcm_dcs_write_seq_static(ctx, 0X6E, 0XE2);
lcm_dcs_write_seq_static(ctx, 0X6F, 0X35);
lcm_dcs_write_seq_static(ctx, 0X70, 0XF9);
lcm_dcs_write_seq_static(ctx, 0X71, 0X14);
lcm_dcs_write_seq_static(ctx, 0X7A, 0X09);
lcm_dcs_write_seq_static(ctx, 0X7B, 0X40);
lcm_dcs_write_seq_static(ctx, 0X7F, 0XF0);
lcm_dcs_write_seq_static(ctx, 0X83, 0X0F);
lcm_dcs_write_seq_static(ctx, 0X84, 0X28);
lcm_dcs_write_seq_static(ctx, 0X87, 0X0E);
lcm_dcs_write_seq_static(ctx, 0X88, 0XFC);
lcm_dcs_write_seq_static(ctx, 0X89, 0X12);
lcm_dcs_write_seq_static(ctx, 0X8B, 0X36);
lcm_dcs_write_seq_static(ctx, 0X8C, 0X7F);
lcm_dcs_write_seq_static(ctx, 0X8D, 0X7F);
lcm_dcs_write_seq_static(ctx, 0X8E, 0X7F);
lcm_dcs_write_seq_static(ctx, 0X95, 0X80);
lcm_dcs_write_seq_static(ctx, 0X96, 0XEB);
lcm_dcs_write_seq_static(ctx, 0X97, 0X0B);
lcm_dcs_write_seq_static(ctx, 0X98, 0X1B);
lcm_dcs_write_seq_static(ctx, 0X9A, 0X05);
lcm_dcs_write_seq_static(ctx, 0X9B, 0X02);
lcm_dcs_write_seq_static(ctx, 0X9C, 0X0A);
lcm_dcs_write_seq_static(ctx, 0X9D, 0XC7);
lcm_dcs_write_seq_static(ctx, 0X9F, 0X77);
lcm_dcs_write_seq_static(ctx, 0XA0, 0XED);
lcm_dcs_write_seq_static(ctx, 0XA2, 0X1F);
lcm_dcs_write_seq_static(ctx, 0XA3, 0XFC);
lcm_dcs_write_seq_static(ctx, 0XA4, 0XEA);
lcm_dcs_write_seq_static(ctx, 0XA5, 0X21);
lcm_dcs_write_seq_static(ctx, 0XA6, 0XFA);
lcm_dcs_write_seq_static(ctx, 0XA7, 0X0A);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X2C);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X00, 0X03);
lcm_dcs_write_seq_static(ctx, 0X01, 0X03);
lcm_dcs_write_seq_static(ctx, 0X02, 0X03);
lcm_dcs_write_seq_static(ctx, 0X03, 0X13);
lcm_dcs_write_seq_static(ctx, 0X04, 0X13);
lcm_dcs_write_seq_static(ctx, 0X05, 0X13);
lcm_dcs_write_seq_static(ctx, 0X0D, 0X01);
lcm_dcs_write_seq_static(ctx, 0X0E, 0X4D);
lcm_dcs_write_seq_static(ctx, 0X16, 0X03);
lcm_dcs_write_seq_static(ctx, 0X17, 0X42);
lcm_dcs_write_seq_static(ctx, 0X18, 0X42);
lcm_dcs_write_seq_static(ctx, 0X19, 0X42);
lcm_dcs_write_seq_static(ctx, 0X2D, 0XAF);
lcm_dcs_write_seq_static(ctx, 0X4D, 0X13);
lcm_dcs_write_seq_static(ctx, 0X4E, 0X03);
lcm_dcs_write_seq_static(ctx, 0X4F, 0X0B);
lcm_dcs_write_seq_static(ctx, 0X53, 0X03);
lcm_dcs_write_seq_static(ctx, 0X54, 0X03);
lcm_dcs_write_seq_static(ctx, 0X55, 0X03);
lcm_dcs_write_seq_static(ctx, 0X61, 0X01);
lcm_dcs_write_seq_static(ctx, 0X62, 0X7D);
lcm_dcs_write_seq_static(ctx, 0X6A, 0X03);
lcm_dcs_write_seq_static(ctx, 0X6B, 0X63);
lcm_dcs_write_seq_static(ctx, 0X6C, 0X63);
lcm_dcs_write_seq_static(ctx, 0X6D, 0X63);
lcm_dcs_write_seq_static(ctx, 0X80, 0XAF);
lcm_dcs_write_seq_static(ctx, 0X9D, 0X1E);
lcm_dcs_write_seq_static(ctx, 0X9E, 0X03);
lcm_dcs_write_seq_static(ctx, 0X9F, 0X1A);

lcm_dcs_write_seq_static(ctx, 0XFF, 0XE0);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X35, 0X82);

lcm_dcs_write_seq_static(ctx, 0XFF, 0XF0);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X1C, 0X01);
lcm_dcs_write_seq_static(ctx, 0X33, 0X01);
lcm_dcs_write_seq_static(ctx, 0X5A, 0X00);

lcm_dcs_write_seq_static(ctx, 0XFF, 0XD0);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X53, 0X22);

lcm_dcs_write_seq_static(ctx, 0X54, 0X02);
lcm_dcs_write_seq_static(ctx, 0XFF, 0XC0);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X9C, 0X11);
lcm_dcs_write_seq_static(ctx, 0X9D, 0X11);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X2B);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0XB7, 0X2B);
lcm_dcs_write_seq_static(ctx, 0XB8, 0X0A);
lcm_dcs_write_seq_static(ctx, 0XC0, 0X01);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X10);
#else
lcm_dcs_write_seq_static(ctx, 0XFF, 0X10);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0XB0, 0X00);
lcm_dcs_write_seq_static(ctx, 0XC0, 0X03);
lcm_dcs_write_seq_static(ctx, 0XB0, 0X00);
lcm_dcs_write_seq_static(ctx, 0XC1,0X89,0X28,0X00,0X08,0X00,0XAA,0X02,0X0E,0X00,0X2B,0X00,0X07,0X0D,0XB7,0X0C,0XB7);
lcm_dcs_write_seq_static(ctx, 0XC2,0X1B,0XA0);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X20);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X01, 0X66);
lcm_dcs_write_seq_static(ctx, 0X06, 0X5B);
lcm_dcs_write_seq_static(ctx, 0X07, 0X27);
lcm_dcs_write_seq_static(ctx, 0X1B, 0X01);

lcm_dcs_write_seq_static(ctx, 0X5C, 0X90);
lcm_dcs_write_seq_static(ctx, 0X5E, 0XB0);
lcm_dcs_write_seq_static(ctx, 0X69, 0XD0);
lcm_dcs_write_seq_static(ctx, 0X95, 0XE0);
lcm_dcs_write_seq_static(ctx, 0X96, 0XE0);
lcm_dcs_write_seq_static(ctx, 0XF2, 0X65);
lcm_dcs_write_seq_static(ctx, 0XF3, 0X54);
lcm_dcs_write_seq_static(ctx, 0XF4, 0X65);
lcm_dcs_write_seq_static(ctx, 0XF5, 0X54);
lcm_dcs_write_seq_static(ctx, 0XF6, 0X65);
lcm_dcs_write_seq_static(ctx, 0XF7, 0X54);
lcm_dcs_write_seq_static(ctx, 0XF8, 0X65);
lcm_dcs_write_seq_static(ctx, 0XF9, 0X54);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X21);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X24);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X00, 0X11);
lcm_dcs_write_seq_static(ctx, 0X07, 0X18);
lcm_dcs_write_seq_static(ctx, 0X08, 0X16);
lcm_dcs_write_seq_static(ctx, 0X09, 0X14);
lcm_dcs_write_seq_static(ctx, 0X0A, 0X17);
lcm_dcs_write_seq_static(ctx, 0X0B, 0X15);
lcm_dcs_write_seq_static(ctx, 0X0C, 0X13);
lcm_dcs_write_seq_static(ctx, 0X0D, 0X0F);
lcm_dcs_write_seq_static(ctx, 0X0E, 0X2C);
lcm_dcs_write_seq_static(ctx, 0X0F, 0X32);
lcm_dcs_write_seq_static(ctx, 0X10, 0X30);
lcm_dcs_write_seq_static(ctx, 0X11, 0X2E);
lcm_dcs_write_seq_static(ctx, 0X12, 0X01);
lcm_dcs_write_seq_static(ctx, 0X13, 0X10);
lcm_dcs_write_seq_static(ctx, 0X14, 0X10);
lcm_dcs_write_seq_static(ctx, 0X15, 0X10);
lcm_dcs_write_seq_static(ctx, 0X16, 0X10);
lcm_dcs_write_seq_static(ctx, 0X17, 0X10);
lcm_dcs_write_seq_static(ctx, 0X18, 0X11);
lcm_dcs_write_seq_static(ctx, 0X1F, 0X18);
lcm_dcs_write_seq_static(ctx, 0X20, 0X16);
lcm_dcs_write_seq_static(ctx, 0X21, 0X14);
lcm_dcs_write_seq_static(ctx, 0X22, 0X17);
lcm_dcs_write_seq_static(ctx, 0X23, 0X15);
lcm_dcs_write_seq_static(ctx, 0X24, 0X13);
lcm_dcs_write_seq_static(ctx, 0X25, 0X0F);
lcm_dcs_write_seq_static(ctx, 0X26, 0X2D);
lcm_dcs_write_seq_static(ctx, 0X27, 0X33);
lcm_dcs_write_seq_static(ctx, 0X28, 0X31);
lcm_dcs_write_seq_static(ctx, 0X29, 0X2F);
lcm_dcs_write_seq_static(ctx, 0X2A, 0X01);
lcm_dcs_write_seq_static(ctx, 0X2B, 0X10);
lcm_dcs_write_seq_static(ctx, 0X2D, 0X10);
lcm_dcs_write_seq_static(ctx, 0X2F, 0X10);
lcm_dcs_write_seq_static(ctx, 0X30, 0X10);
lcm_dcs_write_seq_static(ctx, 0X31, 0X10);
lcm_dcs_write_seq_static(ctx, 0X32, 0X05);
lcm_dcs_write_seq_static(ctx, 0X35, 0X01);
lcm_dcs_write_seq_static(ctx, 0X36, 0X35);
lcm_dcs_write_seq_static(ctx, 0X4D, 0X03);
lcm_dcs_write_seq_static(ctx, 0X4E, 0X30);
lcm_dcs_write_seq_static(ctx, 0X4F, 0X30);
lcm_dcs_write_seq_static(ctx, 0X53, 0X30);
lcm_dcs_write_seq_static(ctx, 0X7A, 0X83);
lcm_dcs_write_seq_static(ctx, 0X7B, 0X0D);
lcm_dcs_write_seq_static(ctx, 0X7C, 0X00);
lcm_dcs_write_seq_static(ctx, 0X7D, 0X03);
lcm_dcs_write_seq_static(ctx, 0X80, 0X03);
lcm_dcs_write_seq_static(ctx, 0X81, 0X03);
lcm_dcs_write_seq_static(ctx, 0X82, 0X13);
lcm_dcs_write_seq_static(ctx, 0X84, 0X31);
lcm_dcs_write_seq_static(ctx, 0X85, 0X00);
lcm_dcs_write_seq_static(ctx, 0X86, 0X00);
lcm_dcs_write_seq_static(ctx, 0X87, 0X00);
lcm_dcs_write_seq_static(ctx, 0X90, 0X13);
lcm_dcs_write_seq_static(ctx, 0X92, 0X31);
lcm_dcs_write_seq_static(ctx, 0X93, 0X00);
lcm_dcs_write_seq_static(ctx, 0X94, 0X00);
lcm_dcs_write_seq_static(ctx, 0X95, 0X00);
lcm_dcs_write_seq_static(ctx, 0X9C, 0XF4);
lcm_dcs_write_seq_static(ctx, 0X9D, 0X01);
lcm_dcs_write_seq_static(ctx, 0XA0, 0X0D);
lcm_dcs_write_seq_static(ctx, 0XA2, 0X0D);
lcm_dcs_write_seq_static(ctx, 0XA3, 0X03);
lcm_dcs_write_seq_static(ctx, 0XA4, 0X03);
lcm_dcs_write_seq_static(ctx, 0XA5, 0X03);
lcm_dcs_write_seq_static(ctx, 0XC4, 0X80);
lcm_dcs_write_seq_static(ctx, 0XC6, 0XC0);
lcm_dcs_write_seq_static(ctx, 0XC9, 0X00);
lcm_dcs_write_seq_static(ctx, 0XD9, 0X80);
lcm_dcs_write_seq_static(ctx, 0XE9, 0X03);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X25);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X0F, 0X13);
lcm_dcs_write_seq_static(ctx, 0X18, 0X20);//120hz
lcm_dcs_write_seq_static(ctx, 0X19, 0XE4);

lcm_dcs_write_seq_static(ctx, 0X21, 0X40);
lcm_dcs_write_seq_static(ctx, 0X68, 0X58);
lcm_dcs_write_seq_static(ctx, 0X69, 0X10);
lcm_dcs_write_seq_static(ctx, 0X6B, 0X00);
lcm_dcs_write_seq_static(ctx, 0X77, 0X72);
lcm_dcs_write_seq_static(ctx, 0X78, 0X15);
lcm_dcs_write_seq_static(ctx, 0X81, 0X00);
lcm_dcs_write_seq_static(ctx, 0X84, 0X2D);
lcm_dcs_write_seq_static(ctx, 0X86, 0X0C);
lcm_dcs_write_seq_static(ctx, 0X8E, 0X10);
lcm_dcs_write_seq_static(ctx, 0XF1, 0X40);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X26);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X14, 0X06);
lcm_dcs_write_seq_static(ctx, 0X15, 0X01);
lcm_dcs_write_seq_static(ctx, 0X74, 0XAF);
lcm_dcs_write_seq_static(ctx, 0X81, 0X0D);
lcm_dcs_write_seq_static(ctx, 0X83, 0X03);
lcm_dcs_write_seq_static(ctx, 0X84, 0X02);
lcm_dcs_write_seq_static(ctx, 0X85, 0X01);
lcm_dcs_write_seq_static(ctx, 0X86, 0X02);
lcm_dcs_write_seq_static(ctx, 0X87, 0X01);
lcm_dcs_write_seq_static(ctx, 0X88, 0X05);
lcm_dcs_write_seq_static(ctx, 0X8A, 0X1A);
lcm_dcs_write_seq_static(ctx, 0X8B, 0X11);
lcm_dcs_write_seq_static(ctx, 0X8C, 0X24);
lcm_dcs_write_seq_static(ctx, 0X8E, 0X42);
lcm_dcs_write_seq_static(ctx, 0X8F, 0X11);
lcm_dcs_write_seq_static(ctx, 0X90, 0X11);
lcm_dcs_write_seq_static(ctx, 0X91, 0X11);
lcm_dcs_write_seq_static(ctx, 0X9A, 0X80);
lcm_dcs_write_seq_static(ctx, 0X9B, 0X04);
lcm_dcs_write_seq_static(ctx, 0X9C, 0X00);
lcm_dcs_write_seq_static(ctx, 0X9D, 0X00);
lcm_dcs_write_seq_static(ctx, 0X9E, 0X00);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X27);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X01, 0X68);
lcm_dcs_write_seq_static(ctx, 0X20, 0X81);
lcm_dcs_write_seq_static(ctx, 0X21, 0X6F);
lcm_dcs_write_seq_static(ctx, 0X25, 0X81);
lcm_dcs_write_seq_static(ctx, 0X26, 0X97);
lcm_dcs_write_seq_static(ctx, 0X6E, 0X12);
lcm_dcs_write_seq_static(ctx, 0X6F, 0X00);
lcm_dcs_write_seq_static(ctx, 0X70, 0X00);
lcm_dcs_write_seq_static(ctx, 0X71, 0X00);
lcm_dcs_write_seq_static(ctx, 0X72, 0X00);
lcm_dcs_write_seq_static(ctx, 0X73, 0X76);
lcm_dcs_write_seq_static(ctx, 0X74, 0X10);
lcm_dcs_write_seq_static(ctx, 0X75, 0X32);
lcm_dcs_write_seq_static(ctx, 0X76, 0X54);
lcm_dcs_write_seq_static(ctx, 0X77, 0X00);
lcm_dcs_write_seq_static(ctx, 0X7D, 0X09);
lcm_dcs_write_seq_static(ctx, 0X7E, 0X6D);
lcm_dcs_write_seq_static(ctx, 0X80, 0X27);
lcm_dcs_write_seq_static(ctx, 0X82, 0X09);
lcm_dcs_write_seq_static(ctx, 0X83, 0X6D);
lcm_dcs_write_seq_static(ctx, 0X88, 0X01);
lcm_dcs_write_seq_static(ctx, 0X89, 0X01);
lcm_dcs_write_seq_static(ctx, 0XE3, 0X01);
lcm_dcs_write_seq_static(ctx, 0XE4, 0XE9);
lcm_dcs_write_seq_static(ctx, 0XE5, 0X02);
lcm_dcs_write_seq_static(ctx, 0XE6, 0XDE);
lcm_dcs_write_seq_static(ctx, 0XE9, 0X02);
lcm_dcs_write_seq_static(ctx, 0XEA, 0X1E);
lcm_dcs_write_seq_static(ctx, 0XEB, 0X03);
lcm_dcs_write_seq_static(ctx, 0XEC, 0X2D);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X2A);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X00, 0X91);
lcm_dcs_write_seq_static(ctx, 0X03, 0X20);
lcm_dcs_write_seq_static(ctx, 0X07, 0X64);
lcm_dcs_write_seq_static(ctx, 0X0A, 0X70);
lcm_dcs_write_seq_static(ctx, 0X0D, 0X40);
lcm_dcs_write_seq_static(ctx, 0X0E, 0X02);
lcm_dcs_write_seq_static(ctx, 0X11, 0XF0);
lcm_dcs_write_seq_static(ctx, 0X15, 0X0F);
lcm_dcs_write_seq_static(ctx, 0X16, 0X28);
lcm_dcs_write_seq_static(ctx, 0X19, 0X0E);
lcm_dcs_write_seq_static(ctx, 0X1A, 0XFC);
lcm_dcs_write_seq_static(ctx, 0X1B, 0X12);
lcm_dcs_write_seq_static(ctx, 0X1D, 0X36);
lcm_dcs_write_seq_static(ctx, 0X1E, 0X37);
lcm_dcs_write_seq_static(ctx, 0X1F, 0X37);
lcm_dcs_write_seq_static(ctx, 0X20, 0X37);
lcm_dcs_write_seq_static(ctx, 0X28, 0XF3);
lcm_dcs_write_seq_static(ctx, 0X29, 0X1F);
lcm_dcs_write_seq_static(ctx, 0X2A, 0XFF);
lcm_dcs_write_seq_static(ctx, 0X2D, 0X05);
lcm_dcs_write_seq_static(ctx, 0X2F, 0X06);
lcm_dcs_write_seq_static(ctx, 0X31, 0X9F);
lcm_dcs_write_seq_static(ctx, 0X33, 0X19);
lcm_dcs_write_seq_static(ctx, 0X34, 0XF5);
lcm_dcs_write_seq_static(ctx, 0X35, 0X45);
lcm_dcs_write_seq_static(ctx, 0X36, 0XCA);
lcm_dcs_write_seq_static(ctx, 0X37, 0XEE);
lcm_dcs_write_seq_static(ctx, 0X38, 0X4B);
lcm_dcs_write_seq_static(ctx, 0X39, 0XC4);
lcm_dcs_write_seq_static(ctx, 0X46, 0X40);
lcm_dcs_write_seq_static(ctx, 0X47, 0X02);
lcm_dcs_write_seq_static(ctx, 0X4A, 0XF0);
lcm_dcs_write_seq_static(ctx, 0X4E, 0X0F);
lcm_dcs_write_seq_static(ctx, 0X4F, 0X28);
lcm_dcs_write_seq_static(ctx, 0X52, 0X0E);
lcm_dcs_write_seq_static(ctx, 0X53, 0XFC);
lcm_dcs_write_seq_static(ctx, 0X54, 0X12);
lcm_dcs_write_seq_static(ctx, 0X56, 0X36);
lcm_dcs_write_seq_static(ctx, 0X57, 0X4F);
lcm_dcs_write_seq_static(ctx, 0X58, 0X4F);
lcm_dcs_write_seq_static(ctx, 0X59, 0X4F);
lcm_dcs_write_seq_static(ctx, 0X60, 0X80);
lcm_dcs_write_seq_static(ctx, 0X61, 0XE5);
lcm_dcs_write_seq_static(ctx, 0X62, 0X17);
lcm_dcs_write_seq_static(ctx, 0X63, 0XBC);
lcm_dcs_write_seq_static(ctx, 0X65, 0X05);
lcm_dcs_write_seq_static(ctx, 0X66, 0X04);
lcm_dcs_write_seq_static(ctx, 0X67, 0X14);
lcm_dcs_write_seq_static(ctx, 0X68, 0XAB);
lcm_dcs_write_seq_static(ctx, 0X6A, 0XD6);
lcm_dcs_write_seq_static(ctx, 0X6B, 0XE7);
lcm_dcs_write_seq_static(ctx, 0X6C, 0X31);
lcm_dcs_write_seq_static(ctx, 0X6D, 0XFD);
lcm_dcs_write_seq_static(ctx, 0X6E, 0XE2);
lcm_dcs_write_seq_static(ctx, 0X6F, 0X35);
lcm_dcs_write_seq_static(ctx, 0X70, 0XF9);
lcm_dcs_write_seq_static(ctx, 0X71, 0X14);
lcm_dcs_write_seq_static(ctx, 0X7A, 0X09);
lcm_dcs_write_seq_static(ctx, 0X7B, 0X40);
lcm_dcs_write_seq_static(ctx, 0X7F, 0XF0);
lcm_dcs_write_seq_static(ctx, 0X83, 0X0F);
lcm_dcs_write_seq_static(ctx, 0X84, 0X28);
lcm_dcs_write_seq_static(ctx, 0X87, 0X0E);
lcm_dcs_write_seq_static(ctx, 0X88, 0XFC);
lcm_dcs_write_seq_static(ctx, 0X89, 0X12);
lcm_dcs_write_seq_static(ctx, 0X8B, 0X36);
lcm_dcs_write_seq_static(ctx, 0X8C, 0X7F);
lcm_dcs_write_seq_static(ctx, 0X8D, 0X7F);
lcm_dcs_write_seq_static(ctx, 0X8E, 0X7F);
lcm_dcs_write_seq_static(ctx, 0X95, 0X80);
lcm_dcs_write_seq_static(ctx, 0X96, 0XEB);
lcm_dcs_write_seq_static(ctx, 0X97, 0X0B);
lcm_dcs_write_seq_static(ctx, 0X98, 0X1B);
lcm_dcs_write_seq_static(ctx, 0X9A, 0X05);
lcm_dcs_write_seq_static(ctx, 0X9B, 0X02);
lcm_dcs_write_seq_static(ctx, 0X9C, 0X0A);
lcm_dcs_write_seq_static(ctx, 0X9D, 0XC7);
lcm_dcs_write_seq_static(ctx, 0X9F, 0X77);
lcm_dcs_write_seq_static(ctx, 0XA0, 0XED);
lcm_dcs_write_seq_static(ctx, 0XA2, 0X1F);
lcm_dcs_write_seq_static(ctx, 0XA3, 0XFC);
lcm_dcs_write_seq_static(ctx, 0XA4, 0XEA);
lcm_dcs_write_seq_static(ctx, 0XA5, 0X21);
lcm_dcs_write_seq_static(ctx, 0XA6, 0XFA);
lcm_dcs_write_seq_static(ctx, 0XA7, 0X0A);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X2C);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X00, 0X03);
lcm_dcs_write_seq_static(ctx, 0X01, 0X03);
lcm_dcs_write_seq_static(ctx, 0X02, 0X03);
lcm_dcs_write_seq_static(ctx, 0X03, 0X13);
lcm_dcs_write_seq_static(ctx, 0X04, 0X13);
lcm_dcs_write_seq_static(ctx, 0X05, 0X13);
lcm_dcs_write_seq_static(ctx, 0X0D, 0X01);
lcm_dcs_write_seq_static(ctx, 0X0E, 0X4D);
lcm_dcs_write_seq_static(ctx, 0X16, 0X03);
lcm_dcs_write_seq_static(ctx, 0X17, 0X42);
lcm_dcs_write_seq_static(ctx, 0X18, 0X42);
lcm_dcs_write_seq_static(ctx, 0X19, 0X42);
lcm_dcs_write_seq_static(ctx, 0X2D, 0XAF);
lcm_dcs_write_seq_static(ctx, 0X4D, 0X13);
lcm_dcs_write_seq_static(ctx, 0X4E, 0X03);
lcm_dcs_write_seq_static(ctx, 0X4F, 0X0B);
lcm_dcs_write_seq_static(ctx, 0X53, 0X03);
lcm_dcs_write_seq_static(ctx, 0X54, 0X03);
lcm_dcs_write_seq_static(ctx, 0X55, 0X03);
lcm_dcs_write_seq_static(ctx, 0X61, 0X01);
lcm_dcs_write_seq_static(ctx, 0X62, 0X7D);
lcm_dcs_write_seq_static(ctx, 0X6A, 0X03);
lcm_dcs_write_seq_static(ctx, 0X6B, 0X63);
lcm_dcs_write_seq_static(ctx, 0X6C, 0X63);
lcm_dcs_write_seq_static(ctx, 0X6D, 0X63);
lcm_dcs_write_seq_static(ctx, 0X80, 0XAF);
lcm_dcs_write_seq_static(ctx, 0X9D, 0X1E);
lcm_dcs_write_seq_static(ctx, 0X9E, 0X03);
lcm_dcs_write_seq_static(ctx, 0X9F, 0X1A);

lcm_dcs_write_seq_static(ctx, 0XFF, 0XE0);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X35, 0X82);

lcm_dcs_write_seq_static(ctx, 0XFF, 0XF0);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X1C, 0X01);
lcm_dcs_write_seq_static(ctx, 0X33, 0X01);
lcm_dcs_write_seq_static(ctx, 0X5A, 0X00);

lcm_dcs_write_seq_static(ctx, 0XFF, 0XD0);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X53, 0X22);

lcm_dcs_write_seq_static(ctx, 0X54, 0X02);
lcm_dcs_write_seq_static(ctx, 0XFF, 0XC0);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0X9C, 0X11);
lcm_dcs_write_seq_static(ctx, 0X9D, 0X11);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X2B);
lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
lcm_dcs_write_seq_static(ctx, 0XB7, 0X2B);
lcm_dcs_write_seq_static(ctx, 0XB8, 0X0A);
lcm_dcs_write_seq_static(ctx, 0XC0, 0X01);

lcm_dcs_write_seq_static(ctx, 0XFF, 0X10);
#endif

	lcm_dcs_write_seq_static(ctx, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x29);

	//lcm_dcs_write_seq(ctx, bl_tb0[0], bl_tb0[1]);

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

	pr_info("%s\n", __func__);
	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);


	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 1);   //prize hjw
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

		ctx->bias_neg =
			devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);

		usleep_range(2000, 2001);

		ctx->bias_pos =
			devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;
#if 0
	// lcd reset H -> L -> L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10000, 10001);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(20);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	// end
#endif
		ctx->bias_pos =
			devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		usleep_range(2000, 2001);
		ctx->bias_neg =
			devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);


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
	pr_info("%s+\n", __func__);
	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}


//#define HFP_60 (20)
//#define HFP_90 (20)
#define HFP (30)  //30
#define HSA (10)
#define HBP (86)
#define VFP_60 (2420)
#define VFP_90 (810)
#define VFP_120 (10)
#define VSA (8)
#define VBP (10)
#define VAC (2408)
#define HAC (1080)

#define PCLK_IN_KHZ \
    ((HAC+HFP+HSA+HBP)*(VAC+VFP_60+VSA+VBP)*(60)/1000) //2500  1160*4942*60/1000=343963
#define PCLK2_IN_KHZ \
    ((HAC+HFP+HSA+HBP)*(VAC+VFP_90+VSA+VBP)*(90)/1000) //853
#define PCLK3_IN_KHZ \
    ((HAC+HFP+HSA+HBP)*(VAC+VFP_120+VSA+VBP)*(120)/1000) //30	
static const struct drm_display_mode default_mode = {
	.clock = 350656,//PCLK_IN_KHZ,  //339264
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,//HFP
	.hsync_end = HAC + HFP + HSA,//HSA
	.htotal = HAC + HFP + HSA + HBP,//HBP
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_60,//VFP
	.vsync_end = VAC + VFP_60 + VSA,//VSA
	.vtotal = VAC + VFP_60 + VSA + VBP,//VBP
};

static const struct drm_display_mode performance_mode_90hz = {
	.clock = 351235,//PCLK2_IN_KHZ,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,//HFP
	.hsync_end = HAC + HFP + HSA,//HSA
	.htotal = HAC + HFP + HSA + HBP,//HBP
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_90,//VFP
	.vsync_end = VAC + VFP_90 + VSA,//VSA
	.vtotal = VAC + VFP_90 + VSA + VBP,//VBP
};

static const struct drm_display_mode performance_mode_120hz = {
	.clock = 352537,//PCLK3_IN_KHZ,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,//HFP
	.hsync_end = HAC + HFP + HSA,//HSA
	.htotal = HAC + HFP + HSA + HBP,//HBP
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_120,//VFP
	.vsync_end = VAC + VFP_120 + VSA,//VSA
	.vtotal = VAC + VFP_120 + VSA + VBP,//VBP
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.physical_width_um = 68395,
	.physical_height_um = 152496,
	.pll_clk = 495,
	//.vfp_low_power = 4180,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.ssc_enable = 0,

	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2408,
		.pic_width = 1080,
		.slice_height = 8,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 170,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 43,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 3255,
		.initial_offset = 6144,
		.final_offset = 7072,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 990,

	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 60,

	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 1,
		.pll_clk = 495,
		.vfp_lp_dyn = 4178,
		.hfp = 76,
		.vfp = 2575,
	},

};

static struct mtk_panel_params ext_params_90hz = {
	.physical_width_um = 68395,
	.physical_height_um = 152496,
	.pll_clk = 495,
	//.vfp_low_power = 2528,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.ssc_enable = 0,

	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2408,
		.pic_width = 1080,
		.slice_height = 8,//20
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 170,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 43,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 3255,
		.initial_offset = 6144,
		.final_offset = 7072,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 990,
	.lfr_enable = 0,
	.lfr_minimum_fps = 60,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,

	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 1,
		.pll_clk = 495,
		.vfp_lp_dyn = 2528,
		.hfp = 76,
		.vfp = 905,
	},

};

static struct mtk_panel_params ext_params_120hz = {
	.physical_width_um = 68395,
	.physical_height_um = 152496,
	.pll_clk = 495,
	//.vfp_low_power = 2528,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.ssc_enable = 0,

	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2408,
		.pic_width = 1080,
		.slice_height = 8,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 170,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 43,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 3255,
		.initial_offset = 6144,
		.final_offset = 7072,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 990,

	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,

	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 1,
		.pll_clk = 495,
		.vfp_lp_dyn = 2528,
		.hfp = 76,
		.vfp = 82,
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

	if (level > 255)
		level = 255;
	pr_info("%s backlight = -%d\n", __func__, level);
	bl_tb0[1] = (u8)level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	return 0;
}

struct drm_display_mode *get_mode_by_id_hfp(struct drm_connector *connector,
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
	struct drm_display_mode *m = get_mode_by_id_hfp(connector, mode);

	if (m == NULL) {
		pr_err("%s:%d invalid display_mode\n", __func__, __LINE__);
		return -1;
	}

	if (drm_mode_vrefresh(m) == 60) {
printk("drm_mode_vrefresh  60hz\n");
		ext->params = &ext_params;
#if HFP_SUPPORT
		current_fps = 60;
#endif
	} else if (drm_mode_vrefresh(m)== 90) {
printk("drm_mode_vrefresh  90hz\n");		
		ext->params = &ext_params_90hz;
#if HFP_SUPPORT
		current_fps = 90;
#endif
	} else if (drm_mode_vrefresh(m) == 120) {
printk("drm_mode_vrefresh  120hz\n");		
		ext->params = &ext_params_120hz;
#if HFP_SUPPORT
		current_fps = 120;
#endif
	} else
		ret = 1;

	return ret;
}
static int mtk_panel_ext_param_get(struct drm_panel *panel,
			struct drm_connector *connector,
			struct mtk_panel_params **ext_para,
			 unsigned int mode)
{
	int ret = 0;

	if (mode == 0)
		*ext_para = &ext_params;
	else if (mode == 1)
		*ext_para = &ext_params_90hz;
	else if (mode == 2)
		*ext_para = &ext_params_120hz;
	else
		ret = 1;

	return ret;
}

static void mode_switch_to_120(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x25);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x18, 0x20);//120hz
	//lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	//lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	//cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

}

static void mode_switch_to_90(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x25);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x18, 0x20);//90hz
	//lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	//lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);

}

static void mode_switch_to_60(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x25);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x18, 0x20);
	//lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	//lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id_hfp(connector, dst_mode);
	if (!m)
		return ret;
	pr_info("%s cur_mode = %d dst_mode %d\n", __func__, cur_mode, dst_mode);

	if (m == NULL) {
		pr_err("%s:%d invalid display_mode\n", __func__, __LINE__);
		return -1;
	}

	if (drm_mode_vrefresh(m) == 60) { /* 60 switch to 120 */
		mode_switch_to_60(panel);
	} else if (drm_mode_vrefresh(m) == 90) { /* 1200 switch to 60 */
		mode_switch_to_90(panel);
	} else if (drm_mode_vrefresh(m) == 120) { /* 1200 switch to 60 */
		mode_switch_to_120(panel);
	} else
		ret = 1;

	return ret;
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

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.mode_switch = mode_switch,
	.ata_check = panel_ata_check,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *	   become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *	  display the first valid frame after starting to receive
	 *	  video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *	   turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *		 to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int lcm_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;
	struct drm_display_mode *mode3;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 default_mode.hdisplay, default_mode.vdisplay,
			 drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	mode2 = drm_mode_duplicate(connector->dev, &performance_mode_90hz);
	if (!mode2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_90hz.hdisplay, performance_mode_90hz.vdisplay,
			 drm_mode_vrefresh(&performance_mode_90hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);

	mode3 = drm_mode_duplicate(connector->dev, &performance_mode_120hz);
	if (!mode3) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_120hz.hdisplay, performance_mode_120hz.vdisplay,
			 drm_mode_vrefresh(&performance_mode_120hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode3);

	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 152;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;

	pr_info("%s+ lcm,nt36672c,vdo,120hz\n", __func__);

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

	pr_info("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | 
			MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET |
			MIPI_DSI_CLOCK_NON_CONTINUOUS;

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
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

#endif

	pr_info("%s- lcm,nt36672c,vdo,120hz\n", __func__);

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif
	pr_info("%s+\n", __func__);
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
	    .compatible = "nt36672c,vdo,120hz",
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-dzx-nt36672c-vdo-120hz",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("shaohua deng <shaohua.deng@mediatek.com>");
MODULE_DESCRIPTION("JDI NT36672C VDO 120HZ AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");
