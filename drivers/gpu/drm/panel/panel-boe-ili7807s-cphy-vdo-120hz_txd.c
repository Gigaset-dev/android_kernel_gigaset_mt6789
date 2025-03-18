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

extern void ili_tp_reset(void);
extern void ili_tp_rst_low(void);
extern void ili_tp_rst_high(void);
/* DRV added by chenjiaxi, add tp gesture function, start */
extern bool ilitek_is_gesture_wakeup_enabled(void);
/* DRV added by chenjiaxi, add tp gesture function, end */

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */
/****************TPS65132***********/
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
//#include "lcm_i2c.h"

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
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ili_tp_rst_high();
	msleep(10);

lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x01);
lcm_dcs_write_seq_static(ctx,0x00,0x62);
lcm_dcs_write_seq_static(ctx,0x01,0x11);
lcm_dcs_write_seq_static(ctx,0x02,0x00);
lcm_dcs_write_seq_static(ctx,0x03,0x00);
lcm_dcs_write_seq_static(ctx,0x04,0x00);
lcm_dcs_write_seq_static(ctx,0x05,0x00);
lcm_dcs_write_seq_static(ctx,0x06,0x00);
lcm_dcs_write_seq_static(ctx,0x07,0x00);
lcm_dcs_write_seq_static(ctx,0x08,0xA9);
lcm_dcs_write_seq_static(ctx,0x09,0x0A);
lcm_dcs_write_seq_static(ctx,0x0A,0x30);
lcm_dcs_write_seq_static(ctx,0x0B,0x00);
lcm_dcs_write_seq_static(ctx,0x0C,0x01);    //60_CLW_r
lcm_dcs_write_seq_static(ctx,0x0E,0x03);    //60_CLW_f

lcm_dcs_write_seq_static(ctx,0x31,0x30);    //GOUTR01      MUXB
lcm_dcs_write_seq_static(ctx,0x32,0x2F);    //GOUTR02      MUXG
lcm_dcs_write_seq_static(ctx,0x33,0x2E);    //GOUTR03      MUXR
lcm_dcs_write_seq_static(ctx,0x34,0x07);    //GOUTR04      DUMMY
lcm_dcs_write_seq_static(ctx,0x35,0x11);    //GOUTR05      CLK4
lcm_dcs_write_seq_static(ctx,0x36,0x10);    //GOUTR06      CLK3
lcm_dcs_write_seq_static(ctx,0x37,0x13);    //GOUTR07      CLK2
lcm_dcs_write_seq_static(ctx,0x38,0x12);    //GOUTR08      CLK1
lcm_dcs_write_seq_static(ctx,0x39,0x07);    //GOUTR09      DUMMY
lcm_dcs_write_seq_static(ctx,0x3A,0x40);    //GOUTR10      VGH_G
lcm_dcs_write_seq_static(ctx,0x3B,0x40);    //GOUTR11      VGH_G
lcm_dcs_write_seq_static(ctx,0x3C,0x01);    //GOUTR12      CN
lcm_dcs_write_seq_static(ctx,0x3D,0x01);    //GOUTR13      CN
lcm_dcs_write_seq_static(ctx,0x3E,0x07);    //GOUTR14      DUMMY
lcm_dcs_write_seq_static(ctx,0x3F,0x25);    //GOUTR15      EN_TOUCH
lcm_dcs_write_seq_static(ctx,0x40,0x07);    //GOUTR16      DUMMY
lcm_dcs_write_seq_static(ctx,0x41,0x00);    //GOUTR17      CNB
lcm_dcs_write_seq_static(ctx,0x42,0x28);    //GOUTR18      VGL_G
lcm_dcs_write_seq_static(ctx,0x43,0x28);    //GOUTR19      VGL_G
lcm_dcs_write_seq_static(ctx,0x44,0x2C);    //GOUTR20      RESET
lcm_dcs_write_seq_static(ctx,0x45,0x09);    //GOUTR21      STVL2
lcm_dcs_write_seq_static(ctx,0x46,0x08);    //GOUTR22      STVL1
lcm_dcs_write_seq_static(ctx,0x47,0x41);    //GOUTR23      CTSW_VCOM
lcm_dcs_write_seq_static(ctx,0x48,0x41);    //GOUTR24      CTSW_VCOM

lcm_dcs_write_seq_static(ctx,0x49,0x30);    //GOUTL01      MUXB
lcm_dcs_write_seq_static(ctx,0x4A,0x2F);    //GOUTL02      MUXG
lcm_dcs_write_seq_static(ctx,0x4B,0x2E);    //GOUTL03      MUXR
lcm_dcs_write_seq_static(ctx,0x4C,0x07);    //GOUTL04      DUMMY
lcm_dcs_write_seq_static(ctx,0x4D,0x11);    //GOUTL05      CLK4
lcm_dcs_write_seq_static(ctx,0x4E,0x10);    //GOUTL06      CLK3
lcm_dcs_write_seq_static(ctx,0x4F,0x13);    //GOUTL07      CLK2
lcm_dcs_write_seq_static(ctx,0x50,0x12);    //GOUTL08      CLK1
lcm_dcs_write_seq_static(ctx,0x51,0x07);    //GOUTL09      DUMMY
lcm_dcs_write_seq_static(ctx,0x52,0x40);    //GOUTL10      VGH_G
lcm_dcs_write_seq_static(ctx,0x53,0x40);    //GOUTL11      VGH_G
lcm_dcs_write_seq_static(ctx,0x54,0x01);    //GOUTL12      CN
lcm_dcs_write_seq_static(ctx,0x55,0x01);    //GOUTL13      CN
lcm_dcs_write_seq_static(ctx,0x56,0x07);    //GOUTL14      DUMMY
lcm_dcs_write_seq_static(ctx,0x57,0x25);    //GOUTL15      EN_TOUCH
lcm_dcs_write_seq_static(ctx,0x58,0x07);    //GOUTL16      DUMMY
lcm_dcs_write_seq_static(ctx,0x59,0x00);    //GOUTL17      CNB
lcm_dcs_write_seq_static(ctx,0x5A,0x28);    //GOUTL18      VGL_G
lcm_dcs_write_seq_static(ctx,0x5B,0x28);    //GOUTL19      VGL_G
lcm_dcs_write_seq_static(ctx,0x5C,0x2C);    //GOUTL20      RESET
lcm_dcs_write_seq_static(ctx,0x5D,0x09);    //GOUTL21      STVL2
lcm_dcs_write_seq_static(ctx,0x5E,0x08);    //GOUTL22      STVL1
lcm_dcs_write_seq_static(ctx,0x5F,0x41);    //GOUTL23      CTSW_VCOM
lcm_dcs_write_seq_static(ctx,0x60,0x41);    //GOUTL24      CTSW_VCOM

lcm_dcs_write_seq_static(ctx,0x61,0x30);    //GOUTR01      MUXB
lcm_dcs_write_seq_static(ctx,0x62,0x2F);    //GOUTR02      MUXG
lcm_dcs_write_seq_static(ctx,0x63,0x2E);    //GOUTR03      MUXR
lcm_dcs_write_seq_static(ctx,0x64,0x07);    //GOUTR04      DUMMY
lcm_dcs_write_seq_static(ctx,0x65,0x11);    //GOUTR05      CLK4
lcm_dcs_write_seq_static(ctx,0x66,0x10);    //GOUTR06      CLK3
lcm_dcs_write_seq_static(ctx,0x67,0x13);    //GOUTR07      CLK2
lcm_dcs_write_seq_static(ctx,0x68,0x12);    //GOUTR08      CLK1
lcm_dcs_write_seq_static(ctx,0x69,0x07);    //GOUTR09      DUMMY
lcm_dcs_write_seq_static(ctx,0x6A,0x40);    //GOUTR10      VGH_G
lcm_dcs_write_seq_static(ctx,0x6B,0x40);    //GOUTR11      VGH_G
lcm_dcs_write_seq_static(ctx,0x6C,0x00);    //GOUTR12      CN
lcm_dcs_write_seq_static(ctx,0x6D,0x00);    //GOUTR13      CN
lcm_dcs_write_seq_static(ctx,0x6E,0x07);    //GOUTR14      DUMMY
lcm_dcs_write_seq_static(ctx,0x6F,0x25);    //GOUTR15      EN_TOUCH
lcm_dcs_write_seq_static(ctx,0x70,0x07);    //GOUTR16      DUMMY
lcm_dcs_write_seq_static(ctx,0x71,0x01);    //GOUTR17      CNB
lcm_dcs_write_seq_static(ctx,0x72,0x28);    //GOUTR18      VGL_G
lcm_dcs_write_seq_static(ctx,0x73,0x28);    //GOUTR19      VGL_G
lcm_dcs_write_seq_static(ctx,0x74,0x2C);    //GOUTR20      RESET
lcm_dcs_write_seq_static(ctx,0x75,0x09);    //GOUTR21      STVL2
lcm_dcs_write_seq_static(ctx,0x76,0x08);    //GOUTR22      STVL1
lcm_dcs_write_seq_static(ctx,0x77,0x41);    //GOUTR23      CTSW_VCOM
lcm_dcs_write_seq_static(ctx,0x78,0x41);    //GOUTR24      CTSW_VCOM

lcm_dcs_write_seq_static(ctx,0x79,0x30);    //GOUTL01      MUXB
lcm_dcs_write_seq_static(ctx,0x7A,0x2F);    //GOUTL02      MUXG
lcm_dcs_write_seq_static(ctx,0x7B,0x2E);    //GOUTL03      MUXR
lcm_dcs_write_seq_static(ctx,0x7C,0x07);    //GOUTL04      DUMMY
lcm_dcs_write_seq_static(ctx,0x7D,0x11);    //GOUTL05      CLK4
lcm_dcs_write_seq_static(ctx,0x7E,0x10);    //GOUTL06      CLK3
lcm_dcs_write_seq_static(ctx,0x7F,0x13);    //GOUTL07      CLK2
lcm_dcs_write_seq_static(ctx,0x80,0x12);    //GOUTL08      CLK1
lcm_dcs_write_seq_static(ctx,0x81,0x07);    //GOUTL09      DUMMY
lcm_dcs_write_seq_static(ctx,0x82,0x40);    //GOUTL10      VGH_G
lcm_dcs_write_seq_static(ctx,0x83,0x40);    //GOUTL11      VGH_G
lcm_dcs_write_seq_static(ctx,0x84,0x00);    //GOUTL12      CN
lcm_dcs_write_seq_static(ctx,0x85,0x00);    //GOUTL13      CN
lcm_dcs_write_seq_static(ctx,0x86,0x07);    //GOUTL14      DUMMY
lcm_dcs_write_seq_static(ctx,0x87,0x25);    //GOUTL15      EN_TOUCH
lcm_dcs_write_seq_static(ctx,0x88,0x07);    //GOUTL16      DUMMY
lcm_dcs_write_seq_static(ctx,0x89,0x01);    //GOUTL17      CNB
lcm_dcs_write_seq_static(ctx,0x8A,0x28);    //GOUTL18      VGL_G
lcm_dcs_write_seq_static(ctx,0x8B,0x28);    //GOUTL19      VGL_G
lcm_dcs_write_seq_static(ctx,0x8C,0x2C);    //GOUTL20      RESET
lcm_dcs_write_seq_static(ctx,0x8D,0x09);    //GOUTL21      STVL2
lcm_dcs_write_seq_static(ctx,0x8E,0x08);    //GOUTL22      STVL1
lcm_dcs_write_seq_static(ctx,0x8F,0x41);    //GOUTL23      CTSW_VCOM
lcm_dcs_write_seq_static(ctx,0x90,0x41);    //GOUTL24      CTSW_VCOM

lcm_dcs_write_seq_static(ctx,0xA0,0x4C);
lcm_dcs_write_seq_static(ctx,0xA1,0x4A);
lcm_dcs_write_seq_static(ctx,0xA2,0x00);
lcm_dcs_write_seq_static(ctx,0xA3,0x00);
lcm_dcs_write_seq_static(ctx,0xA7,0x10);
lcm_dcs_write_seq_static(ctx,0xAA,0x00);
lcm_dcs_write_seq_static(ctx,0xAB,0x00);
lcm_dcs_write_seq_static(ctx,0xAC,0x00);
lcm_dcs_write_seq_static(ctx,0xAE,0x00);
lcm_dcs_write_seq_static(ctx,0xB0,0x20);
lcm_dcs_write_seq_static(ctx,0xB1,0x00);
lcm_dcs_write_seq_static(ctx,0xB2,0x01);
lcm_dcs_write_seq_static(ctx,0xB3,0x04);
lcm_dcs_write_seq_static(ctx,0xB4,0x05);
lcm_dcs_write_seq_static(ctx,0xB5,0x00);
lcm_dcs_write_seq_static(ctx,0xB6,0x00);
lcm_dcs_write_seq_static(ctx,0xB7,0x00);
lcm_dcs_write_seq_static(ctx,0xB8,0x00);
lcm_dcs_write_seq_static(ctx,0xC0,0x0C);
lcm_dcs_write_seq_static(ctx,0xC1,0x5D);    //60_reftp_r
lcm_dcs_write_seq_static(ctx,0xC2,0x00);    //60_reftp_f
lcm_dcs_write_seq_static(ctx,0xC5,0x2B);
lcm_dcs_write_seq_static(ctx,0xCA,0x01);

lcm_dcs_write_seq_static(ctx,0xD1,0x00);
lcm_dcs_write_seq_static(ctx,0xD2,0x10);
lcm_dcs_write_seq_static(ctx,0xD3,0x41);
lcm_dcs_write_seq_static(ctx,0xD4,0x89);
lcm_dcs_write_seq_static(ctx,0xD5,0x06);
lcm_dcs_write_seq_static(ctx,0xD6,0x49);
lcm_dcs_write_seq_static(ctx,0xD7,0x40);
lcm_dcs_write_seq_static(ctx,0xD8,0x09);
lcm_dcs_write_seq_static(ctx,0xD9,0x96);
lcm_dcs_write_seq_static(ctx,0xDA,0xAA);
lcm_dcs_write_seq_static(ctx,0xDB,0xAA);
lcm_dcs_write_seq_static(ctx,0xDC,0x8A);
lcm_dcs_write_seq_static(ctx,0xDD,0xA8);
lcm_dcs_write_seq_static(ctx,0xDE,0x05);
lcm_dcs_write_seq_static(ctx,0xDF,0x42);
lcm_dcs_write_seq_static(ctx,0xE0,0x1E);
lcm_dcs_write_seq_static(ctx,0xE1,0x68);
lcm_dcs_write_seq_static(ctx,0xE2,0x07);
lcm_dcs_write_seq_static(ctx,0xE3,0x11);		
lcm_dcs_write_seq_static(ctx,0xE4,0x42);		
lcm_dcs_write_seq_static(ctx,0xE5,0x4F);		
lcm_dcs_write_seq_static(ctx,0xE6,0x22);
lcm_dcs_write_seq_static(ctx,0xE7,0x0C);
lcm_dcs_write_seq_static(ctx,0xE8,0x00);
lcm_dcs_write_seq_static(ctx,0xE9,0x00);
lcm_dcs_write_seq_static(ctx,0xEA,0x00);
lcm_dcs_write_seq_static(ctx,0xEB,0x00);
lcm_dcs_write_seq_static(ctx,0xEC,0x80);
lcm_dcs_write_seq_static(ctx,0xED,0x55);
lcm_dcs_write_seq_static(ctx,0xEE,0x00);
lcm_dcs_write_seq_static(ctx,0xEF,0x32);
lcm_dcs_write_seq_static(ctx,0xF0,0x00);
lcm_dcs_write_seq_static(ctx,0xF1,0xC0);
lcm_dcs_write_seq_static(ctx,0xF4,0x54);

lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x11);
lcm_dcs_write_seq_static(ctx,0x00,0x01);    //120_CLW_r  
lcm_dcs_write_seq_static(ctx,0x01,0x03);    //120_CLW_f
lcm_dcs_write_seq_static(ctx,0x18,0x2B);    //120_reftp_r 
lcm_dcs_write_seq_static(ctx,0x19,0x00);    //120_reftp_f

lcm_dcs_write_seq_static(ctx,0x38,0x01);    //90_CLW_r 
lcm_dcs_write_seq_static(ctx,0x39,0x03);    //90_CLW_f
lcm_dcs_write_seq_static(ctx,0x50,0x35);    //90_reftp_r 
lcm_dcs_write_seq_static(ctx,0x51,0x00);    //90_reftp_f

lcm_dcs_write_seq_static(ctx,0x70,0x01);    //144_CLW_r  
lcm_dcs_write_seq_static(ctx,0x71,0x01);    //144_CLW_f
lcm_dcs_write_seq_static(ctx,0x88,0x14);    //144_reftp_r 
lcm_dcs_write_seq_static(ctx,0x89,0x00);    //144_reftp_f

lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x02);
lcm_dcs_write_seq_static(ctx,0x1B,0x00);    //00:120_240 ,01:90_270 ,02:60_120 ,03:144_144 ,
lcm_dcs_write_seq_static(ctx,0x24,0x16);    //TE

lcm_dcs_write_seq_static(ctx,0x40,0x0C);    //60_T8_DE
lcm_dcs_write_seq_static(ctx,0x41,0x00);    //60_T7P_DE
lcm_dcs_write_seq_static(ctx,0x42,0x09);    //60_T9_DE
lcm_dcs_write_seq_static(ctx,0x43,0x2E);    //60_T7_DE
lcm_dcs_write_seq_static(ctx,0x53,0x09);    //60_SDT
lcm_dcs_write_seq_static(ctx,0x46,0x21);    //DUMMY CKH
lcm_dcs_write_seq_static(ctx,0x47,0x03);    //CKH CONNECT
lcm_dcs_write_seq_static(ctx,0x4F,0x01);    //CKH_3to1_set_opt

lcm_dcs_write_seq_static(ctx,0x76,0x1F);    //save power SRC bias through rate
lcm_dcs_write_seq_static(ctx,0x80,0x25);    //save power SRC bias current
lcm_dcs_write_seq_static(ctx,0x06,0x69);    //60_Int BIST RTN=6.594us
lcm_dcs_write_seq_static(ctx,0x08,0x00);    //60_Int BIST RTN[10:8]
lcm_dcs_write_seq_static(ctx,0x0E,0x28);    //60_Int BIST VBP
lcm_dcs_write_seq_static(ctx,0x0F,0x28);    //60_Int BIST VFP

lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x12);
lcm_dcs_write_seq_static(ctx,0x10,0x05);    //120_T8_DE
lcm_dcs_write_seq_static(ctx,0x11,0x00);    //120_T7P_DE
lcm_dcs_write_seq_static(ctx,0x12,0x06);    //120_T9_DE
lcm_dcs_write_seq_static(ctx,0x13,0x15);    //120_T7_DE
lcm_dcs_write_seq_static(ctx,0x16,0x06);    //120_SDT
lcm_dcs_write_seq_static(ctx,0x1A,0x1F);    //save power SRC bias through rate
lcm_dcs_write_seq_static(ctx,0x1B,0x25);    //save power SRC bias current
lcm_dcs_write_seq_static(ctx,0xC0,0x34);    //120_Int BIST RTN=3.281us
lcm_dcs_write_seq_static(ctx,0xC1,0x00);    //120_Int BIST RTN[10:8]
lcm_dcs_write_seq_static(ctx,0xC2,0x28);    //120_Int BIST VBP
lcm_dcs_write_seq_static(ctx,0xC3,0x28);    //120_Int BIST VFP

lcm_dcs_write_seq_static(ctx,0x48,0x05);    //90_T8_DE
lcm_dcs_write_seq_static(ctx,0x49,0x00);    //90_T7P_DE
lcm_dcs_write_seq_static(ctx,0x4A,0x06);    //90_T9_DE
lcm_dcs_write_seq_static(ctx,0x4B,0x1C);    //90_T7_DE
lcm_dcs_write_seq_static(ctx,0x4E,0x06);    //90_SDT
lcm_dcs_write_seq_static(ctx,0x52,0x1F);    //save power SRC bias through rate
lcm_dcs_write_seq_static(ctx,0x53,0x25);    //save power SRC bias current
lcm_dcs_write_seq_static(ctx,0xC8,0x46);    //90_Int BIST RTN=4.406us
lcm_dcs_write_seq_static(ctx,0xC9,0x00);    //90_Int BIST RTN[10:8]
lcm_dcs_write_seq_static(ctx,0xCA,0x28);    //90_Int BIST VBP
lcm_dcs_write_seq_static(ctx,0xCB,0x28);    //90_Int BIST VFP

lcm_dcs_write_seq_static(ctx,0x7A,0x05);    //144_T8_DE
lcm_dcs_write_seq_static(ctx,0x7B,0x00);    //144_T7P_DE
lcm_dcs_write_seq_static(ctx,0x7C,0x05);    //144_T9_DE
lcm_dcs_write_seq_static(ctx,0x7D,0x13);    //144_T7_DE
lcm_dcs_write_seq_static(ctx,0x80,0x05);    //144_SDT
lcm_dcs_write_seq_static(ctx,0x84,0x1F);    //save power SRC bias through rate
lcm_dcs_write_seq_static(ctx,0x85,0x25);    //save power SRC bias current
lcm_dcs_write_seq_static(ctx,0xD0,0x2D);    //144_Int BIST RTN=2.788us
lcm_dcs_write_seq_static(ctx,0xD1,0x00);    //144_Int BIST RTN[10:8]
lcm_dcs_write_seq_static(ctx,0xD2,0x10);    //144_Int BIST VBP
lcm_dcs_write_seq_static(ctx,0xD3,0x1E);    //144_Int BIST VFP

lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x04);
lcm_dcs_write_seq_static(ctx,0xBD,0x01);

lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x05);
lcm_dcs_write_seq_static(ctx,0x1B,0x00);    //120_VCOM1 = -0.2V
lcm_dcs_write_seq_static(ctx,0x1C,0x87);    //120_VCOM1 = -0.2V
lcm_dcs_write_seq_static(ctx,0x1D,0x00);    //90_VCOM1 = -0.2V
lcm_dcs_write_seq_static(ctx,0x1E,0x87);    //90_VCOM1 = -0.2V
lcm_dcs_write_seq_static(ctx,0x1F,0x00);    //90_VCOM1 = -0.2V
lcm_dcs_write_seq_static(ctx,0x20,0x87);    //90_VCOM1 = -0.2V
lcm_dcs_write_seq_static(ctx,0x21,0x00);    //144_VCOM1 = -0.2V
lcm_dcs_write_seq_static(ctx,0x22,0x87);    //144_VCOM1 = -0.2V

lcm_dcs_write_seq_static(ctx,0x72,0x7E);    // VGH  = 11V
lcm_dcs_write_seq_static(ctx,0x74,0x56);    // VGL  = -9V
lcm_dcs_write_seq_static(ctx,0x76,0x79);    // VGHO = 10V
lcm_dcs_write_seq_static(ctx,0x7A,0x51);    // VGLO = -8V
lcm_dcs_write_seq_static(ctx,0x7B,0x88);    // GVDDP =  5.1V
lcm_dcs_write_seq_static(ctx,0x7C,0x88);    // GVDDN = -5.1V
lcm_dcs_write_seq_static(ctx,0x46,0x5E);    //PWR_TCON_VGHO_EN
lcm_dcs_write_seq_static(ctx,0x47,0x7E);    //PWR_TCON_VGLO_EN
lcm_dcs_write_seq_static(ctx,0xB5,0x55);    //PWR_D2A_HVREG_VGHO_EN
lcm_dcs_write_seq_static(ctx,0xB7,0x75);    //PWR_D2A_HVREG_VGLO_EN
lcm_dcs_write_seq_static(ctx,0x56,0xFF);
lcm_dcs_write_seq_static(ctx,0x3E,0x50);
lcm_dcs_write_seq_static(ctx,0xC6,0x1B);

lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x06);
lcm_dcs_write_seq_static(ctx,0xC0,0x9C);    //1080x2460
lcm_dcs_write_seq_static(ctx,0xC1,0x19);    //1080x2460
lcm_dcs_write_seq_static(ctx,0xC2,0xF0);    //1080

lcm_dcs_write_seq_static(ctx,0xC3,0x06);    //SS_REG
lcm_dcs_write_seq_static(ctx,0xD6,0x55);
lcm_dcs_write_seq_static(ctx,0xCD,0x68);    //FTE=TSHD,FTE1=TSVD1
lcm_dcs_write_seq_static(ctx,0x13,0x13);    //force otp DDI
lcm_dcs_write_seq_static(ctx,0x12,0xBD);

lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x07);
lcm_dcs_write_seq_static(ctx,0x29,0xCF);    //CF: DSC on, 80: DSC off,
lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x17);    //slice high 8
lcm_dcs_write_seq_static(ctx,0x20,0x00,0x00,0x00,0x00,0x00,0x11,0x00,0x00,0x89,0x30,0x80,0x09,0x9c,0x04,0x38,0x00,0x0a,0x02,0x1c,0x02,0x1c,0x02,0x00,0x02,0x0e,0x00,0x20,0x00,0xed,0x00,0x07,0x00,0x0c,0x0a,0xab,0x0a,0x2c,0x18,0x00,0x10,0xf0,0x03,0x0c,0x20,0x00,0x06,0x0b,0x0b,0x33,0x0e,0x1c,0x2a,0x38,0x46,0x54,0x62,0x69,0x70,0x77,0x79,0x7b,0x7d,0x7e,0x01,0x02,0x01,0x00,0x09,0x40,0x09,0xbe,0x19,0xfc,0x19,0xfa,0x19,0xf8,0x1a,0x38,0x1a,0x78,0x1a,0xb6,0x2a,0xf6,0x2b,0x34,0x2b,0x74,0x3b,0x74,0x6b,0xf4,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);

//Gamma Register									
lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x08);    //
lcm_dcs_write_seq_static(ctx,0xE0,0x00,0x00,0x1D,0x49,0x00,0x88,0xB6,0xD9,0x15,0x12,0x3C,0x7A,0x25,0xAE,0xF8,0x2F,0x2A,0x69,0xA6,0xCF,0x3F,0x01,0x23,0x52,0x3F,0x6A,0x8C,0xBC,0x0F,0xD8,0xD9);
lcm_dcs_write_seq_static(ctx,0xE1,0x00,0x00,0x1D,0x49,0x00,0x88,0xB6,0xD9,0x15,0x12,0x3C,0x7A,0x25,0xAE,0xF8,0x2F,0x2A,0x69,0xA6,0xCF,0x3F,0x01,0x23,0x52,0x3F,0x6A,0x8C,0xBC,0x0F,0xD8,0xD9);

lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x0B);    // AUTOTRIM
lcm_dcs_write_seq_static(ctx,0xC0,0x84);    //120_VDO_DIV_SEL (Frame)
lcm_dcs_write_seq_static(ctx,0xC1,0x10);    //120_VDO_CNT_IDEAL[12:0]
lcm_dcs_write_seq_static(ctx,0xC2,0x03);    //120_UB [4:0]
lcm_dcs_write_seq_static(ctx,0xC3,0x03);    //120_LB [4:0]
lcm_dcs_write_seq_static(ctx,0xC4,0x65);    //120_Keep trim code range MAX[7:0]
lcm_dcs_write_seq_static(ctx,0xC5,0x65);    //120_Keep trim code range MIN[7:0]
lcm_dcs_write_seq_static(ctx,0xD2,0x06);    //120_VDO_LN_DIV_SEL, cnt_line
lcm_dcs_write_seq_static(ctx,0xD3,0x8E);    //120_VDO_CNT_IDEAL[12:0]
lcm_dcs_write_seq_static(ctx,0xD4,0x05);    //120_UB [4:0]
lcm_dcs_write_seq_static(ctx,0xD5,0x05);    //120_LB [4:0]
lcm_dcs_write_seq_static(ctx,0xD6,0xA4);    //120_Keep trim code range MAX[7:0]
lcm_dcs_write_seq_static(ctx,0xD7,0xA4);    //120_Keep trim code range MIN[7:0]

lcm_dcs_write_seq_static(ctx,0xC6,0x85);    //90_VDO_DIV_SEL (Frame)
lcm_dcs_write_seq_static(ctx,0xC7,0x6B);    //90_VDO_CNT_IDEAL[12:0]
lcm_dcs_write_seq_static(ctx,0xC8,0x04);    //90_UB [4:0]
lcm_dcs_write_seq_static(ctx,0xC9,0x04);    //90_LB [4:0]
lcm_dcs_write_seq_static(ctx,0xCA,0x87);    //90_Keep trim code range MAX[7:0]
lcm_dcs_write_seq_static(ctx,0xCB,0x87);    //90_Keep trim code range MIN[7:0]
lcm_dcs_write_seq_static(ctx,0xD8,0x44);    //90_VDO_LN_DIV_SEL, cnt_line
lcm_dcs_write_seq_static(ctx,0xD9,0x5E);    //90_VDO_CNT_IDEAL[12:0]
lcm_dcs_write_seq_static(ctx,0xDA,0x03);    //90_UB [4:0]
lcm_dcs_write_seq_static(ctx,0xDB,0x03);    //90_LB [4:0]
lcm_dcs_write_seq_static(ctx,0xDC,0x6D);    //90_Keep trim code range MAX[7:0]
lcm_dcs_write_seq_static(ctx,0xDD,0x6D);    //90_Keep trim code range MIN[7:0]

lcm_dcs_write_seq_static(ctx,0x94,0x88);    //60_VDO_DIV_SEL (Frame)
lcm_dcs_write_seq_static(ctx,0x95,0x1F);    //60_VDO_CNT_IDEAL[12:0]
lcm_dcs_write_seq_static(ctx,0x96,0x06);    //60_UB [4:0]
lcm_dcs_write_seq_static(ctx,0x97,0x06);    //60_LB [4:0]
lcm_dcs_write_seq_static(ctx,0x98,0xCB);    //60_Keep trim code range MAX[7:0]
lcm_dcs_write_seq_static(ctx,0x99,0xCB);    //60_Keep trim code range MIN[7:0]
lcm_dcs_write_seq_static(ctx,0x9A,0x46);    //60_VDO_LN_DIV_SEL, cnt_line
lcm_dcs_write_seq_static(ctx,0x9B,0x8C);    //60_VDO_CNT_IDEAL[12:0]
lcm_dcs_write_seq_static(ctx,0x9C,0x05);    //60_UB [4:0]
lcm_dcs_write_seq_static(ctx,0x9D,0x05);    //60_LB [4:0]
lcm_dcs_write_seq_static(ctx,0x9E,0xA3);    //60_Keep trim code range MAX[7:0]
lcm_dcs_write_seq_static(ctx,0x9F,0xA3);    //60_Keep trim code range MIN[7:0]

lcm_dcs_write_seq_static(ctx,0xCC,0x46);    //144_VDO_DIV_SEL (Frame)
lcm_dcs_write_seq_static(ctx,0xCD,0xEA);    //144_VDO_CNT_IDEAL[12:0]
lcm_dcs_write_seq_static(ctx,0xCE,0x05);    //144_UB [4:0]
lcm_dcs_write_seq_static(ctx,0xCF,0x05);    //144_LB [4:0]
lcm_dcs_write_seq_static(ctx,0xD0,0xAD);    //144_Keep trim code range MAX[7:0]
lcm_dcs_write_seq_static(ctx,0xD1,0xAD);    //144_Keep trim code range MIN[7:0]
lcm_dcs_write_seq_static(ctx,0xDE,0x05);    //144_VDO_LN_DIV_SEL, cnt_line
lcm_dcs_write_seq_static(ctx,0xDF,0xA6);    //144_VDO_CNT_IDEAL[12:0]
lcm_dcs_write_seq_static(ctx,0xE0,0x04);    //144_UB [4:0]
lcm_dcs_write_seq_static(ctx,0xE1,0x04);    //144_LB [4:0]
lcm_dcs_write_seq_static(ctx,0xE2,0x8D);    //144_Keep trim code range MAX[7:0]
lcm_dcs_write_seq_static(ctx,0xE3,0x8D);    //144_Keep trim code range MIN[7:0]

lcm_dcs_write_seq_static(ctx,0xAA,0x12);    //D[4]: Trim_ln_num
lcm_dcs_write_seq_static(ctx,0xAB,0xE0);    //OSC auto trim en

//TP MODULATION
lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x0C);
lcm_dcs_write_seq_static(ctx,0x00,0x3F);    //120
lcm_dcs_write_seq_static(ctx,0x01,0x68);    //120
lcm_dcs_write_seq_static(ctx,0x02,0x3F);    //120
lcm_dcs_write_seq_static(ctx,0x03,0x6A);    //120
lcm_dcs_write_seq_static(ctx,0x04,0x3F);    //120
lcm_dcs_write_seq_static(ctx,0x05,0x69);    //120
lcm_dcs_write_seq_static(ctx,0x06,0x3F);    //120
lcm_dcs_write_seq_static(ctx,0x07,0x6B);    //120
lcm_dcs_write_seq_static(ctx,0x08,0x3F);    //120
lcm_dcs_write_seq_static(ctx,0x09,0x6C);    //120
lcm_dcs_write_seq_static(ctx,0x0A,0x3F);    //120
lcm_dcs_write_seq_static(ctx,0x0B,0x67);    //120

lcm_dcs_write_seq_static(ctx,0x40,0x3F);    //90
lcm_dcs_write_seq_static(ctx,0x41,0x6B);    //90
lcm_dcs_write_seq_static(ctx,0x42,0x3F);    //90
lcm_dcs_write_seq_static(ctx,0x43,0x6C);    //90
lcm_dcs_write_seq_static(ctx,0x44,0x3F);    //90
lcm_dcs_write_seq_static(ctx,0x45,0x69);    //90
lcm_dcs_write_seq_static(ctx,0x46,0x3F);    //90
lcm_dcs_write_seq_static(ctx,0x47,0x68);    //90
lcm_dcs_write_seq_static(ctx,0x48,0x3F);    //90
lcm_dcs_write_seq_static(ctx,0x49,0x6A);    //90
lcm_dcs_write_seq_static(ctx,0x4A,0x3F);    //90
lcm_dcs_write_seq_static(ctx,0x4B,0x67);    //90

lcm_dcs_write_seq_static(ctx,0x80,0x23);    //60
lcm_dcs_write_seq_static(ctx,0x81,0x6B);    //60
lcm_dcs_write_seq_static(ctx,0x82,0x23);    //60
lcm_dcs_write_seq_static(ctx,0x83,0x6A);    //60
lcm_dcs_write_seq_static(ctx,0x84,0x23);    //60
lcm_dcs_write_seq_static(ctx,0x85,0x69);    //60
lcm_dcs_write_seq_static(ctx,0x86,0x23);    //60
lcm_dcs_write_seq_static(ctx,0x87,0x67);    //60
lcm_dcs_write_seq_static(ctx,0x88,0x23);    //60
lcm_dcs_write_seq_static(ctx,0x89,0x68);    //60
lcm_dcs_write_seq_static(ctx,0x8A,0x24);    //60
lcm_dcs_write_seq_static(ctx,0x8B,0x6C);    //60

lcm_dcs_write_seq_static(ctx,0xC0,0x1E);    //144
lcm_dcs_write_seq_static(ctx,0xC1,0x67);    //144
lcm_dcs_write_seq_static(ctx,0xC2,0x1E);    //144
lcm_dcs_write_seq_static(ctx,0xC3,0x68);    //144
lcm_dcs_write_seq_static(ctx,0xC4,0x1E);    //144
lcm_dcs_write_seq_static(ctx,0xC5,0x6B);    //144
lcm_dcs_write_seq_static(ctx,0xC6,0x1E);    //144
lcm_dcs_write_seq_static(ctx,0xC7,0x6A);    //144
lcm_dcs_write_seq_static(ctx,0xC8,0x1E);    //144
lcm_dcs_write_seq_static(ctx,0xC9,0x6C);    //144
lcm_dcs_write_seq_static(ctx,0xCA,0x1E);    //144
lcm_dcs_write_seq_static(ctx,0xCB,0x69);    //144

lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x0E);
lcm_dcs_write_seq_static(ctx,0x00,0xA3);    //LH MODE
lcm_dcs_write_seq_static(ctx,0x02,0x0F);
lcm_dcs_write_seq_static(ctx,0x04,0x06);    //TSHD_1VP Off & TSVD FREE RUN
lcm_dcs_write_seq_static(ctx,0x05,0x20);    //TP modulation on:20 , off:24
lcm_dcs_write_seq_static(ctx,0x13,0x04);    //LV_TSHD_pos
lcm_dcs_write_seq_static(ctx,0xB0,0x21);    //01 TP1 UNIT
lcm_dcs_write_seq_static(ctx,0xC0,0x12);    //34 TP3 UNIT

lcm_dcs_write_seq_static(ctx,0x20,0x03);    //120_TP term num
lcm_dcs_write_seq_static(ctx,0x21,0x28);    //120_LH_TSVD1_pos
lcm_dcs_write_seq_static(ctx,0x22,0x04);    //120_LH_TSVD1_width
lcm_dcs_write_seq_static(ctx,0x23,0x28);    //120_LH_TSVD2_pos
lcm_dcs_write_seq_static(ctx,0x24,0x84);    //120_LH_TSVD2_width
lcm_dcs_write_seq_static(ctx,0x25,0x11);    //120_TP2_unit0=278.2us
lcm_dcs_write_seq_static(ctx,0x26,0x62);    //120_TP2_unit0=278.2us
lcm_dcs_write_seq_static(ctx,0x27,0x20);    //120_unit_line_num
lcm_dcs_write_seq_static(ctx,0x29,0x67);    //120_unit_line_num
lcm_dcs_write_seq_static(ctx,0x2D,0x59);    //120_RTN=2.813us
lcm_dcs_write_seq_static(ctx,0x30,0x00);    //120_RTN[9:8]
lcm_dcs_write_seq_static(ctx,0x2B,0x05);    //120_TPM_step=6

lcm_dcs_write_seq_static(ctx,0x40,0x07);    //60_TP term num
lcm_dcs_write_seq_static(ctx,0x41,0x14);    //60_LH_TSVD1_pos
lcm_dcs_write_seq_static(ctx,0x42,0x02);    //60_LH_TSVD1_width
lcm_dcs_write_seq_static(ctx,0x43,0x14);    //60_LH_TSVD2_pos
lcm_dcs_write_seq_static(ctx,0x44,0x82);    //60_LH_TSVD2_width
lcm_dcs_write_seq_static(ctx,0x45,0x0A);    //60_TP2_unit0=170.7us
lcm_dcs_write_seq_static(ctx,0x46,0xAA);    //60_TP2_unit0=170.7us
lcm_dcs_write_seq_static(ctx,0x47,0x10);    //60_unit_line_num
lcm_dcs_write_seq_static(ctx,0x49,0x34);    //60_unit_line_num
lcm_dcs_write_seq_static(ctx,0xB1,0x5F);    //60_TP1_period
lcm_dcs_write_seq_static(ctx,0xC8,0x5F);    //60_TP3-2 period
lcm_dcs_write_seq_static(ctx,0xC9,0x5F);    //60_TP3-1 period
lcm_dcs_write_seq_static(ctx,0x4D,0xBD);    //60_RTN=5.938us
lcm_dcs_write_seq_static(ctx,0x50,0x00);    //60_RTN[9:8]
lcm_dcs_write_seq_static(ctx,0x4B,0x05);    //60_TPM_step=6

lcm_dcs_write_seq_static(ctx,0xE0,0x0C);    //60_T8_DE_TP
lcm_dcs_write_seq_static(ctx,0xE1,0x00);    //60_T7P_DE_TP
lcm_dcs_write_seq_static(ctx,0xE2,0x0B);    //60_T9_DE_TP
lcm_dcs_write_seq_static(ctx,0xE3,0x2E);    //60_T7_DE_TP
lcm_dcs_write_seq_static(ctx,0xE5,0x0B);    //60_SDT

lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x1E);
lcm_dcs_write_seq_static(ctx,0xAD,0x00);    //120_tpt_fr_sel
lcm_dcs_write_seq_static(ctx,0xA1,0x1F);    //120_LAT2_OFFSET
lcm_dcs_write_seq_static(ctx,0xBD,0x01);    //90_tpt_fr_sel
lcm_dcs_write_seq_static(ctx,0xB1,0x1F);    //90_LAT2_OFFSET
lcm_dcs_write_seq_static(ctx,0xC9,0x02);    //60_tpt_fr_sel
lcm_dcs_write_seq_static(ctx,0xC0,0x1F);    //60_LAT2_OFFSET
lcm_dcs_write_seq_static(ctx,0xDD,0x03);    //144_tpt_fr_sel
lcm_dcs_write_seq_static(ctx,0xD1,0x1F);    //144_LAT2_OFFSET

lcm_dcs_write_seq_static(ctx,0x00,0x2D);    //120_TP1_period
lcm_dcs_write_seq_static(ctx,0x08,0x2D);    //120_TP3-2 period
lcm_dcs_write_seq_static(ctx,0x09,0x2D);    //120_TP3-1 period
lcm_dcs_write_seq_static(ctx,0xA4,0x00);    //120_Qsync_T1
lcm_dcs_write_seq_static(ctx,0xA5,0x73);    //120_Qsync_T3
lcm_dcs_write_seq_static(ctx,0xA6,0x73);    //120_Qsync_T4
lcm_dcs_write_seq_static(ctx,0xA7,0x54);    //120_Qsync_T2=278.69us
lcm_dcs_write_seq_static(ctx,0xAA,0x00);    //120_Qsync_T5

lcm_dcs_write_seq_static(ctx,0x0A,0x05);    //120_T8_DE_TP
lcm_dcs_write_seq_static(ctx,0x0B,0x00);    //120_T7P_DE_TP
lcm_dcs_write_seq_static(ctx,0x0C,0x06);    //120_T9_DE_TP
lcm_dcs_write_seq_static(ctx,0x0D,0x15);    //120_T7_DE_TP
lcm_dcs_write_seq_static(ctx,0x0E,0x06);    //120_SDT_TP

lcm_dcs_write_seq_static(ctx,0x20,0x05);    //90_T8_DE_TP
lcm_dcs_write_seq_static(ctx,0x21,0x00);    //90_T7P_DE_TP
lcm_dcs_write_seq_static(ctx,0x22,0x06);    //90_T9_DE_TP
lcm_dcs_write_seq_static(ctx,0x23,0x1C);    //90_T7_DE_TP
lcm_dcs_write_seq_static(ctx,0x24,0x06);    //90_SDT_TP

lcm_dcs_write_seq_static(ctx,0x60,0x0B);    //90_TP term num
lcm_dcs_write_seq_static(ctx,0x61,0x1E);    //90_LH_TSVD1_pos
lcm_dcs_write_seq_static(ctx,0x62,0x03);    //90_LH_TSVD1_width
lcm_dcs_write_seq_static(ctx,0x63,0x1E);    //90_LH_TSVD2_pos
lcm_dcs_write_seq_static(ctx,0x64,0x83);    //90_LH_TSVD2_width
lcm_dcs_write_seq_static(ctx,0x65,0x0B);    //90_TP2_unit0=180.8us
lcm_dcs_write_seq_static(ctx,0x66,0x4B);    //90_TP2_unit0=180.8us
lcm_dcs_write_seq_static(ctx,0x67,0x00);    //90_unit_line_num
lcm_dcs_write_seq_static(ctx,0x69,0xCD);    //90_unit_line_num
lcm_dcs_write_seq_static(ctx,0x16,0x37);    //90_TP1_period
lcm_dcs_write_seq_static(ctx,0x1E,0x37);    //90_TP3-2 period
lcm_dcs_write_seq_static(ctx,0x1F,0x37);    //90_TP3-1 period
lcm_dcs_write_seq_static(ctx,0x6D,0x6D);    //90_RTN=3.438us
lcm_dcs_write_seq_static(ctx,0x70,0x00);    //90_RTN[9:8]
lcm_dcs_write_seq_static(ctx,0x6B,0x05);    //90_TPM_step=6
lcm_dcs_write_seq_static(ctx,0xB4,0x00);    //90_Qsync_T1
lcm_dcs_write_seq_static(ctx,0xB5,0x28);    //90_Qsync_T3
lcm_dcs_write_seq_static(ctx,0xB6,0x28);    //90_Qsync_T4
lcm_dcs_write_seq_static(ctx,0xB7,0x29);    //90_Qsync_T2=183.52us
lcm_dcs_write_seq_static(ctx,0xBA,0x00);    //90_Qsync_T5

lcm_dcs_write_seq_static(ctx,0xC1,0x00);    //60_Qsync_T1
lcm_dcs_write_seq_static(ctx,0xC2,0x33);    //60_Qsync_T3
lcm_dcs_write_seq_static(ctx,0xC3,0x33);    //60_Qsync_T4
lcm_dcs_write_seq_static(ctx,0xC4,0x1A);    //60_Qsync_T2=176.84us
lcm_dcs_write_seq_static(ctx,0xC7,0x00);    //60_Qsync_T5

lcm_dcs_write_seq_static(ctx,0x36,0x05);    //144_T8_DE_TP
lcm_dcs_write_seq_static(ctx,0x37,0x00);    //144_T7P_DE_TP
lcm_dcs_write_seq_static(ctx,0x38,0x05);    //144_T9_DE_TP
lcm_dcs_write_seq_static(ctx,0x39,0x13);    //144_T7_DE_TP
lcm_dcs_write_seq_static(ctx,0x3A,0x05);    //144_SDT_TP

lcm_dcs_write_seq_static(ctx,0x80,0x03);    //144_TP term num
lcm_dcs_write_seq_static(ctx,0x81,0x2F);    //144_LH_TSVD1_pos
lcm_dcs_write_seq_static(ctx,0x82,0x04);    //144_LH_TSVD1_width
lcm_dcs_write_seq_static(ctx,0x83,0x2F);    //144_LH_TSVD2_pos
lcm_dcs_write_seq_static(ctx,0x84,0x84);    //144_LH_TSVD2_width
lcm_dcs_write_seq_static(ctx,0x85,0x08);    //144_TP2_unit0=132.4us
lcm_dcs_write_seq_static(ctx,0x86,0x6F);    //144_TP2_unit0=132.4us
lcm_dcs_write_seq_static(ctx,0x87,0x20);    //144_unit_line_num
lcm_dcs_write_seq_static(ctx,0x89,0x67);    //144_unit_line_num
lcm_dcs_write_seq_static(ctx,0x2C,0x2A);    //144_TP1_period
lcm_dcs_write_seq_static(ctx,0x34,0x2A);    //144_TP3-2 period
lcm_dcs_write_seq_static(ctx,0x35,0x2A);    //144_TP3-1 period
lcm_dcs_write_seq_static(ctx,0x8D,0x52);    //144_RTN=2.543us
lcm_dcs_write_seq_static(ctx,0x90,0x00);    //144_RTN[9:8]
lcm_dcs_write_seq_static(ctx,0x8B,0x05);    //144_TPM_step=6
lcm_dcs_write_seq_static(ctx,0xD4,0x00);    //144_Qsync_T1
lcm_dcs_write_seq_static(ctx,0xD5,0x78);    //144_Qsync_T3
lcm_dcs_write_seq_static(ctx,0xD6,0x78);    //144_Qsync_T4
lcm_dcs_write_seq_static(ctx,0xD7,0x2F);    //144_Qsync_T2=135.66us
lcm_dcs_write_seq_static(ctx,0xDA,0x00);    //144_Qsync_T5

lcm_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x00);    //PAGE0
lcm_dcs_write_seq_static(ctx,0x35,0x00);
lcm_dcs_write_seq_static(ctx,0x11);
msleep(130);
lcm_dcs_write_seq_static(ctx,0x29);
msleep(30);

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

	pr_info("%s+\n", __func__);

	if (!ctx->prepared)
		return 0;

	msleep(10);
	lcm_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF); //0x28
	msleep(70);
	lcm_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE); //0x10
	msleep(100);

	// ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	// gpiod_set_value(ctx->reset_gpio, 0);
	// devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	/* DRV modified by chenjiaxi, add tp gesture function, start */
	if (ilitek_is_gesture_wakeup_enabled() == 0) {
		// ili_tp_rst_low();
		// msleep(5);

		ctx->bias_pos =
			devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH); //AVEE
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		msleep(5);

		ctx->bias_neg =
			devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH); //AVDD
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);

		msleep(5);
	}
	/* DRV modified by chenjiaxi, add tp gesture function, end */

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

	/* DRV modified by chenjiaxi, add tp gesture function, start */
	if (ilitek_is_gesture_wakeup_enabled() == 0) {
		ctx->bias_neg =
			devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH); //AVDD
		gpiod_set_value(ctx->bias_neg, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
		
		msleep(5);

		ctx->bias_pos =
			devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH); //AVEE
		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		msleep(5);
	}
	/* DRV modified by chenjiaxi, add tp gesture function, end */

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

#define HFP (18)
#define HSA (4)
#define HBP (18)
#define VFP_60 (2680)
#define VFP_90 (920)
#define VFP_120 (40)
#define VSA (2)
#define VBP (38)
#define VAC (2460)
#define HAC (1080)
static u32 fake_heigh = 2460;
static u32 fake_width = 1080;
static bool need_fake_resolution;

static struct drm_display_mode default_mode = {
	.clock = ((HAC+HFP+HSA+HBP)*(VAC+VFP_60+VSA+VBP)*(60)/1000),
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_60,
	.vsync_end = VAC + VFP_60 + VSA,
	.vtotal = VAC + VFP_60 + VSA + VBP,
};

static struct drm_display_mode performance_mode_90hz = {
	.clock = ((HAC+HFP+HSA+HBP)*(VAC+VFP_90+VSA+VBP)*(90)/1000),
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_90,
	.vsync_end = VAC + VFP_90 + VSA,
	.vtotal = VAC + VFP_90 + VSA + VBP,
};

static struct drm_display_mode performance_mode_120hz = {
	.clock = ((HAC+HFP+HSA+HBP)*(VAC+VFP_120+VSA+VBP)*(120)/1000),
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_120,
	.vsync_end = VAC + VFP_120 + VSA,
	.vtotal = VAC + VFP_120 + VSA + VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = 307,
	.data_rate = 614,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
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
		.pic_height = 2460,
		.pic_width = 1080,
		.slice_height = 10,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 237,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2731,
		.slice_bpg_offset = 2604,
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
	.lfr_enable = 1,
	.lfr_minimum_fps = 60,
	.physical_width_um = 69228,
	.physical_height_um = 157685,
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = 307,
	.data_rate = 614,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
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
		.pic_height = 2460,
		.pic_width = 1080,
		.slice_height = 10,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 237,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2731,
		.slice_bpg_offset = 2604,
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
	.lfr_enable = 1,
	.lfr_minimum_fps = 60,
	.physical_width_um = 69228,
	.physical_height_um = 157685,
};

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = 307,
	.data_rate = 614,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
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
		.pic_height = 2460,
		.pic_width = 1080,
		.slice_height = 10,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 237,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2731,
		.slice_bpg_offset = 2604,
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
	.lfr_enable = 1,
	.lfr_minimum_fps = 60,
	.physical_width_um = 69228,
	.physical_height_um = 157685,
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
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

	if (m == NULL)
		return 1;

	if (drm_mode_vrefresh(m) == 60)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == 90)
		ext->params = &ext_params_90hz;
	else if (drm_mode_vrefresh(m) == 120)
		ext->params = &ext_params_120hz;
	else
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
	.ext_param_set = mtk_panel_ext_param_set,
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

static void change_drm_disp_mode_params(struct drm_display_mode *mode)
{
	int vtotal = mode->vtotal;
	int htotal = mode->htotal;
	int fps = mode->clock * 1000 / vtotal / htotal;

	if (fake_heigh > 0 && fake_heigh < VAC) {
		mode->vsync_start = mode->vsync_start - mode->vdisplay
					+ fake_heigh;
		mode->vsync_end = mode->vsync_end - mode->vdisplay + fake_heigh;
		mode->vtotal = mode->vtotal - mode->vdisplay + fake_heigh;
		mode->vdisplay = fake_heigh;
	}
	if (fake_width > 0 && fake_width < HAC) {
		mode->hsync_start = mode->hsync_start - mode->hdisplay
					+ fake_width;
		mode->hsync_end = mode->hsync_end - mode->hdisplay + fake_width;
		mode->htotal = mode->htotal - mode->hdisplay + fake_width;
		mode->hdisplay = fake_width;
	}
	if (fps > 70)
		fps = 90;
	else
		fps = 60;
	mode->clock = fps * mode->vtotal * mode->htotal / 1000;
	mode->clock += 1;
}

static int lcm_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;
	struct drm_display_mode *mode3;

	if (need_fake_resolution) {
		change_drm_disp_mode_params(&default_mode);
		change_drm_disp_mode_params(&performance_mode_90hz);
	}
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
		dev_info(connector->dev->dev, "failed to add mode2 %ux%ux@%u\n",
			 performance_mode_90hz.hdisplay, performance_mode_90hz.vdisplay,
			 drm_mode_vrefresh(&performance_mode_90hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);
	
	mode3 = drm_mode_duplicate(connector->dev, &performance_mode_120hz);
	if (!mode3) {
		dev_info(connector->dev->dev, "failed to add mode3 %ux%ux@%u\n",
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

static void check_is_need_fake_resolution(struct device *dev)
{
	unsigned int ret = 0;

	ret = of_property_read_u32(dev->of_node, "fake-heigh", &fake_heigh);
	if (ret)
		need_fake_resolution = false;
	ret = of_property_read_u32(dev->of_node, "fake-width", &fake_width);
	if (ret)
		need_fake_resolution = false;
	if (fake_heigh > 0 && fake_heigh < VAC)
		need_fake_resolution = true;
	if (fake_width > 0 && fake_width < HAC)
		need_fake_resolution = true;
}

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
	dsi->lanes = 3;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
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
	check_is_need_fake_resolution(dev);

#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
    strcpy(current_lcm_info.chip,"ili7807s,cphy,vdo,txd");
    strcpy(current_lcm_info.vendor,"tongxingda,boe-12");
    sprintf(current_lcm_info.id,"0x%04x",0x7807);
    strcpy(current_lcm_info.more,"1080*2460");
#endif

	pr_info("%s- boe,ili7807s,cphy,vdo,120hz,txd\n", __func__);

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
	if (ext_ctx == NULL)
		return -1;
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{
	    .compatible = "boe,ili7807s,cphy,vdo,120hz,txd",
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-boe-ili7807s-cphy-vdo-120hz_txd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Elon Hsu <elon.hsu@mediatek.com>");
MODULE_DESCRIPTION("boe ili7807s vdo Panel Driver");
MODULE_LICENSE("GPL v2");
