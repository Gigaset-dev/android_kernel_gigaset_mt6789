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

#include "../../../misc/mediatek/prize/prize_common_node/prize_common_node.h"

#if defined(CONFIG_PARADE_REPORT_CONTROL)
bool second_tp_report = false;
EXPORT_SYMBOL(second_tp_report);
#endif

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	struct gpio_desc *vldo18_gpio;
	struct gpio_desc *bias2_pos;
	struct gpio_desc *bias2_neg;
	struct gpio_desc *mipi_switch_gpio;
	struct gpio_desc *rst_switch_gpio;

	bool prepared;
	bool enabled;

	int error;
};

unsigned int rawlevel;
struct lcm *g_ctx;

int current_lcd_mode = 0;
EXPORT_SYMBOL(current_lcd_mode);

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

//drv add by wangwei1 for double screen switch 20240129 start
static void second_lcm_panel_init(struct lcm *ctx)
{
	dev_info(ctx->dev, "%s\n", __func__);

	//lcm_dcs_write_seq_static(ctx,0xFE,0x40);
	//lcm_dcs_write_seq_static(ctx,0xA4,0x2f);

	lcm_dcs_write_seq_static(ctx,0xFE,0xA0);
	lcm_dcs_write_seq_static(ctx,0x04,0x81);

	lcm_dcs_write_seq_static(ctx,0x05,0x01);
	lcm_dcs_write_seq_static(ctx,0xEE,0x00);

	lcm_dcs_write_seq_static(ctx,0xFE,0x00);
	lcm_dcs_write_seq_static(ctx,0xC2,0x08);
	lcm_dcs_write_seq_static(ctx,0x35,0x00);
	lcm_dcs_write_seq_static(ctx,0x51,0xFF);

	lcm_dcs_write_seq_static(ctx,0x11);
	msleep(120);
	lcm_dcs_write_seq_static(ctx,0x29);
	msleep(10);

	pr_info("%s-\n", __func__);
}

static void lcm_panel_init(struct lcm *ctx)
{
	dev_info(ctx->dev, "wangwei 1 %s\n", __func__);
	
	lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x00);

	lcm_dcs_write_seq_static(ctx,0xB0,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x01);
	lcm_dcs_write_seq_static(ctx,0xB9,0x04,0x38);
	lcm_dcs_write_seq_static(ctx,0xBD,0x0A,0x50);
	lcm_dcs_write_seq_static(ctx,0xBA,0x00,0x6D,0x00,0x18,0x00,0x18,0x10);
	lcm_dcs_write_seq_static(ctx,0x6F,0x07);
	lcm_dcs_write_seq_static(ctx,0xBA,0x00,0x6D,0x00,0x18,0x03,0x98,0x10);
	lcm_dcs_write_seq_static(ctx,0x6F,0x0E);
	lcm_dcs_write_seq_static(ctx,0xBA,0x00,0x6D,0x00,0x18,0x0A,0x98,0x10);
	lcm_dcs_write_seq_static(ctx,0x6F,0x15);
	lcm_dcs_write_seq_static(ctx,0xBA,0x00,0x4E,0x00,0x18,0x00,0x30,0x19);
	lcm_dcs_write_seq_static(ctx,0xBB,0x00,0x6F,0x00,0x18,0x00,0x18,0x70);
	lcm_dcs_write_seq_static(ctx,0xBE,0x47,0x4A,0x46);
	lcm_dcs_write_seq_static(ctx,0xC1,0x00,0x6F,0x00,0x6F,0x00,0x6F,0x00,0x51,0x00,0x6F);
	lcm_dcs_write_seq_static(ctx,0x6F,0x0C);
	lcm_dcs_write_seq_static(ctx,0xC3,0x00);
	lcm_dcs_write_seq_static(ctx,0xC6,0x44,0x44,0x44,0x44,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88);
	lcm_dcs_write_seq_static(ctx,0x6F,0x20);
	lcm_dcs_write_seq_static(ctx,0xC6,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55);
	lcm_dcs_write_seq_static(ctx,0xC8,0x6D);
	lcm_dcs_write_seq_static(ctx,0x6F,0x2C);
	lcm_dcs_write_seq_static(ctx,0xCB,0x43);
	lcm_dcs_write_seq_static(ctx,0xB2,0x01,0x40);
	lcm_dcs_write_seq_static(ctx,0x6F,0x04);
	lcm_dcs_write_seq_static(ctx,0xB2,0x22);
	lcm_dcs_write_seq_static(ctx,0x6F,0x05);
	lcm_dcs_write_seq_static(ctx,0xB2,0x20,0x20);
	lcm_dcs_write_seq_static(ctx,0x6F,0x11);
	lcm_dcs_write_seq_static(ctx,0xB2,0x05,0x07,0x4B,0x0B);
	lcm_dcs_write_seq_static(ctx,0x6F,0x15);
	lcm_dcs_write_seq_static(ctx,0xB2,0x05);
	lcm_dcs_write_seq_static(ctx,0x6F,0x18);
	lcm_dcs_write_seq_static(ctx,0xB2,0x1A,0x38,0x3F,0xFF);
	lcm_dcs_write_seq_static(ctx,0x6F,0x1E);
	lcm_dcs_write_seq_static(ctx,0xB2,0x80,0x1E,0x00,0x28,0x00,0x3C,0x00,0x1E);
	lcm_dcs_write_seq_static(ctx,0xB3,0x00,0x01,0x01,0x59,0x01,0x59,0x02,0x1A,0x02,0x1A,0x03,0x0A,0x03,0x0A,0x04,0x44,0x04,0x44,0x05,0x2B,0x05,0x2B,0x05,0xEA,0x05,0xEA,0x06,0x84,0x06,0x84);
	lcm_dcs_write_seq_static(ctx,0x6F,0x1E);
	lcm_dcs_write_seq_static(ctx,0xB3,0x06,0x8F,0x06,0x8F,0x09,0x08,0x09,0x08,0x0D,0xBA,0x0D,0xBA,0x0F,0xFF);
	lcm_dcs_write_seq_static(ctx,0x6F,0x2C);
	lcm_dcs_write_seq_static(ctx,0xB3,0x01,0x55,0x08,0xCC,0x08,0xCC,0x0F,0xFF);
	lcm_dcs_write_seq_static(ctx,0xB4,0x09,0xC8,0x09,0xC8,0x09,0x52,0x09,0x52,0x09,0x0E,0x09,0x0E,0x08,0x22,0x08,0x22,0x06,0x48,0x06,0x48,0x04,0x6E,0x04,0x6E,0x02,0x92,0x02,0x92,0x00,0xC6,0x00,0xC6,0x00,0x1E,0x00,0x1E,0x00,0x1E,0x00,0x1E,0x00,0x1E,0x00,0x1E,0x00,0x1E);
	lcm_dcs_write_seq_static(ctx,0x6F,0x2E);
// pps end
	lcm_dcs_write_seq_static(ctx,0xB4,0x0D,0x0A,0x0D,0x0A,0x0C,0x6C,0x0C,0x6C,0x0C,0x14,0x0C,0x14,0x0A,0xD8,0x0A,0xD8,0x08,0x60,0x08,0x60,0x05,0xE8,0x05,0xE8,0x03,0x6E,0x03,0x6E,0x01,0x08,0x01,0x08,0x00,0x28,0x00,0x28,0x00,0x28,0x00,0x28,0x00,0x28,0x00,0x28,0x00,0x28);
	lcm_dcs_write_seq_static(ctx,0x6F,0x5C);
	lcm_dcs_write_seq_static(ctx,0xB4,0x13,0x90,0x13,0x90,0x12,0xA2,0x12,0xA2,0x12,0x1E,0x12,0x1E,0x10,0x44,0x10,0x44,0x0C,0x8E,0x0C,0x8E,0x08,0xDA,0x08,0xDA,0x05,0x26,0x05,0x26,0x01,0x8C,0x01,0x8C,0x00,0x3C,0x00,0x3C,0x00,0x3C,0x00,0x3C,0x00,0x3C,0x00,0x3C,0x00,0x3C);
	lcm_dcs_write_seq_static(ctx,0x6F,0x8A);
	lcm_dcs_write_seq_static(ctx,0xB4,0x09,0xCC,0x09,0xCC,0x09,0x54,0x09,0x54,0x09,0x12,0x09,0x12,0x08,0x24,0x08,0x24,0x06,0x4A,0x06,0x4A,0x04,0x6E,0x04,0x6E,0x02,0x94,0x02,0x94,0x00,0xC6,0x00,0xC6,0x00,0x1E,0x00,0x1E,0x00,0x1E,0x00,0x1E,0x00,0x1E,0x00,0x1E,0x00,0x1E);
	lcm_dcs_write_seq_static(ctx,0xB4,0X0A,0X80,0x09,0xD8,0x09,0x90,0x09,0x90,0x09,0x18,0x09,0x18,0x08,0x28,0x08,0x28,0x06,0x48,0x06,0x48,0x04,0x68,0x04,0x68,0x02,0x88,0x02,0x88,0x00,0xC0,0x00,0xC0);
//IC WRITE
	lcm_dcs_write_seq_static(ctx,0x6F,0x20);
	lcm_dcs_write_seq_static(ctx,0xB4,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24);
	lcm_dcs_write_seq_static(ctx,0x6F,0x2E);
	lcm_dcs_write_seq_static(ctx,0xB4,0X0E,0X00,0x0D,0x20,0x0C,0xC0,0x0C,0xC0,0x0C,0x20,0x0C,0x20,0x0A,0xE0,0x0A,0xE0,0x08,0x60,0x08,0x60,0x05,0xE0,0x05,0xE0,0x03,0x60,0x03,0x60,0x01,0x00,0x01,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x4E);
	lcm_dcs_write_seq_static(ctx,0xB4,0x00,0x30,0x00,0x30,0x00,0x30,0x00,0x30,0x00,0x30,0x00,0x30,0x00,0x30);
	lcm_dcs_write_seq_static(ctx,0x6F,0x5C);
	lcm_dcs_write_seq_static(ctx,0xB4,0X15,0X00,0x13,0xB0,0x13,0x20,0x13,0x20,0x12,0x30,0x12,0x30,0x10,0x50,0x10,0x50,0x0C,0x90,0x0C,0x90,0x08,0xD0,0x08,0xD0,0x05,0x10,0x05,0x10,0x01,0x80);
	lcm_dcs_write_seq_static(ctx,0x6F,0x7C);
	lcm_dcs_write_seq_static(ctx,0xB4,0x01,0x80,0x00,0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,0x48);
	lcm_dcs_write_seq_static(ctx,0x6F,0x8A);
	lcm_dcs_write_seq_static(ctx,0xB4,0X0A,0X80,0x09,0xD8,0x09,0x90,0x09,0x90,0x09,0x18,0x09,0x18,0x08,0x28,0x08,0x28,0x06,0x48,0x06,0x48,0x04,0x68,0x04,0x68,0x02,0x88,0x02,0x88,0x00,0xC0,0x00,0xC0);
	lcm_dcs_write_seq_static(ctx,0x6F,0xAA);
	lcm_dcs_write_seq_static(ctx,0xB4,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24);
	lcm_dcs_write_seq_static(ctx,0x6F,0xB8);
	lcm_dcs_write_seq_static(ctx,0xB4,0x09,0x90,0x00,0x30,0x00,0x30,0x00,0x30,0x00,0x30);
	lcm_dcs_write_seq_static(ctx,0x6F,0x02);
	lcm_dcs_write_seq_static(ctx,0xC0,0x08);
	lcm_dcs_write_seq_static(ctx,0x6F,0x03);
	lcm_dcs_write_seq_static(ctx,0xC0,0x02,0x13);
	lcm_dcs_write_seq_static(ctx,0x6F,0x0B);
	lcm_dcs_write_seq_static(ctx,0xC0,0xC8,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x06);
	lcm_dcs_write_seq_static(ctx,0xB5,0x7F);
	lcm_dcs_write_seq_static(ctx,0x6F,0x07);
	lcm_dcs_write_seq_static(ctx,0xB5,0x4B,0x22,0x4F);
	lcm_dcs_write_seq_static(ctx,0x6F,0x0C);
	lcm_dcs_write_seq_static(ctx,0xB5,0x50,0x27,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x11);
	lcm_dcs_write_seq_static(ctx,0xB5,0x22,0x22,0x22,0x22,0x22);
	lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x00);
	lcm_dcs_write_seq_static(ctx,0xB5,0x80);
	lcm_dcs_write_seq_static(ctx,0x6F,0x2C);
	lcm_dcs_write_seq_static(ctx,0xB5,0x03);
	lcm_dcs_write_seq_static(ctx,0x6F,0x2D);
	lcm_dcs_write_seq_static(ctx,0xB5,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x13);
	lcm_dcs_write_seq_static(ctx,0x6F,0x44);
	lcm_dcs_write_seq_static(ctx,0xB5,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x13);
	lcm_dcs_write_seq_static(ctx,0x6F,0x07);
	lcm_dcs_write_seq_static(ctx,0xCA,0x09,0x06);
	lcm_dcs_write_seq_static(ctx,0x6F,0x09);
	lcm_dcs_write_seq_static(ctx,0xCA,0x36,0x11);
	lcm_dcs_write_seq_static(ctx,0x6F,0x1B);
	lcm_dcs_write_seq_static(ctx,0xBA,0x19);
//----------actnum & blanknum
	lcm_dcs_write_seq_static(ctx,0x6F,0x1C);
	lcm_dcs_write_seq_static(ctx,0xBA,0x01,0x01,0x01,0x01,0x01,0x03,0x0B,0x59,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x2C);
	lcm_dcs_write_seq_static(ctx,0xBA,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x3C);
	lcm_dcs_write_seq_static(ctx,0xBA,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x4C);
	lcm_dcs_write_seq_static(ctx,0xBA,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x5C);
	lcm_dcs_write_seq_static(ctx,0xBA,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01);
	lcm_dcs_write_seq_static(ctx,0x6F,0x6C);
	lcm_dcs_write_seq_static(ctx,0xBA,0x01,0x03,0x04,0x0B,0x77,0x02,0x08,0x59,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x7C);
	lcm_dcs_write_seq_static(ctx,0xBA,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01);
	lcm_dcs_write_seq_static(ctx,0x6F,0x8C);
	lcm_dcs_write_seq_static(ctx,0xBA,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
//----------skip_seq1~4_CYC
	lcm_dcs_write_seq_static(ctx,0x6F,0x9C);
	lcm_dcs_write_seq_static(ctx,0xBA,0x11,0x11,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0xA4);
	lcm_dcs_write_seq_static(ctx,0xBA,0x94,0xD3,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0xA8);
	lcm_dcs_write_seq_static(ctx,0xBA,0x22,0x22,0x42,0x44,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0xB0);
	lcm_dcs_write_seq_static(ctx,0xBA,0x00,0x00,0x10,0x11,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x08);
	lcm_dcs_write_seq_static(ctx,0xBB,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x1C);
	lcm_dcs_write_seq_static(ctx,0xBB,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xC0,0x44);
	lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x1A);
	lcm_dcs_write_seq_static(ctx,0xC2,0x23);

	lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x01);
	lcm_dcs_write_seq_static(ctx,0xB0,0x3C,0x3C);
	lcm_dcs_write_seq_static(ctx,0xB1,0x32,0x32);
	lcm_dcs_write_seq_static(ctx,0xB2,0x55,0x01,0x55,0x01);
	lcm_dcs_write_seq_static(ctx,0xB3,0x44,0x44,0x44,0x44,0x44,0x44);
	lcm_dcs_write_seq_static(ctx,0x6F,0x08);
	lcm_dcs_write_seq_static(ctx,0xB4,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xB5,0x00,0xFA,0x00,0xFA,0x00,0xE6,0x00,0xE6);
	lcm_dcs_write_seq_static(ctx,0x6F,0x08);
	lcm_dcs_write_seq_static(ctx,0xB5,0x00,0xFA,0x00,0xFA,0x00,0xE6,0x00,0xE6);
	lcm_dcs_write_seq_static(ctx,0xB6,0x01,0x2C,0x01,0x2C,0x00,0xA0,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xB7,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23);
	lcm_dcs_write_seq_static(ctx,0x6F,0x11);
	lcm_dcs_write_seq_static(ctx,0xB7,0x23,0x23,0x23,0x23);
	lcm_dcs_write_seq_static(ctx,0xB8,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55);
	lcm_dcs_write_seq_static(ctx,0x6F,0x0E);
	lcm_dcs_write_seq_static(ctx,0xB8,0x55,0x55,0x55,0x55);
	lcm_dcs_write_seq_static(ctx,0x6F,0x07);
	lcm_dcs_write_seq_static(ctx,0xB9,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E);
	lcm_dcs_write_seq_static(ctx,0x6F,0x1D);
	lcm_dcs_write_seq_static(ctx,0xB9,0x1E);
	lcm_dcs_write_seq_static(ctx,0x6F,0x21);
	lcm_dcs_write_seq_static(ctx,0xB9,0x1E);
	lcm_dcs_write_seq_static(ctx,0x6F,0x15);
	lcm_dcs_write_seq_static(ctx,0xB9,0x1B,0x1B,0x1B,0x1B,0x1B,0x1B,0x14);
	lcm_dcs_write_seq_static(ctx,0x6F,0x1F);
	lcm_dcs_write_seq_static(ctx,0xB9,0x1B);
	lcm_dcs_write_seq_static(ctx,0x6F,0x23);
	lcm_dcs_write_seq_static(ctx,0xB9,0x1B);
	lcm_dcs_write_seq_static(ctx,0xBA,0x15,0x15,0x12);
	lcm_dcs_write_seq_static(ctx,0x6F,0x5D);
	lcm_dcs_write_seq_static(ctx,0xC0,0x03);
	lcm_dcs_write_seq_static(ctx,0x6F,0x01);
	lcm_dcs_write_seq_static(ctx,0xC0,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x40,0x40,0x40,0x40,0x40,0x40,0x4D);
	lcm_dcs_write_seq_static(ctx,0x6F,0x18);
	lcm_dcs_write_seq_static(ctx,0xC0,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x40,0x40,0x40,0x40,0x40,0x40,0x4D);
	lcm_dcs_write_seq_static(ctx,0xCC,0x00);
	lcm_dcs_write_seq_static(ctx,0xCD,0x71);
	lcm_dcs_write_seq_static(ctx,0x6F,0x12);
	lcm_dcs_write_seq_static(ctx,0xD8,0x40,0x50,0x30);
	lcm_dcs_write_seq_static(ctx,0x6F,0x18);
	lcm_dcs_write_seq_static(ctx,0xD8,0x20);
	lcm_dcs_write_seq_static(ctx,0x6F,0x1D);
	lcm_dcs_write_seq_static(ctx,0xD8,0x20);
	lcm_dcs_write_seq_static(ctx,0x6F,0x1E);
	lcm_dcs_write_seq_static(ctx,0xD8,0x30);
	lcm_dcs_write_seq_static(ctx,0x6F,0x21);
	lcm_dcs_write_seq_static(ctx,0xD8,0x60,0x60,0x60);
	lcm_dcs_write_seq_static(ctx,0x6F,0x24);
	lcm_dcs_write_seq_static(ctx,0xD8,0x50,0x50);
	lcm_dcs_write_seq_static(ctx,0x6F,0x0F);
	lcm_dcs_write_seq_static(ctx,0xB7,0xF0);
	lcm_dcs_write_seq_static(ctx,0x6F,0x07);
	lcm_dcs_write_seq_static(ctx,0xE3,0x28);
	lcm_dcs_write_seq_static(ctx,0x6F,0x02);
	lcm_dcs_write_seq_static(ctx,0xE9,0x08);
	lcm_dcs_write_seq_static(ctx,0xBE,0xF2,0x67,0x02);
	lcm_dcs_write_seq_static(ctx,0xBF,0x08,0x01,0x05,0x91);
	lcm_dcs_write_seq_static(ctx,0x6F,0x03);
	lcm_dcs_write_seq_static(ctx,0xD8,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x09);
	lcm_dcs_write_seq_static(ctx,0xD8,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x0F);
	lcm_dcs_write_seq_static(ctx,0xD8,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x15);
	lcm_dcs_write_seq_static(ctx,0xD8,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x03);
	lcm_dcs_write_seq_static(ctx,0xD3,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xD2,0x00,0x00,0x00,0x00,0x15);
	lcm_dcs_write_seq_static(ctx,0x6F,0x05);
	lcm_dcs_write_seq_static(ctx,0xD2,0x01,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x0E);
	lcm_dcs_write_seq_static(ctx,0xD2,0x02,0x00,0x10,0x05,0x40);
	lcm_dcs_write_seq_static(ctx,0xE4,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x33);
	lcm_dcs_write_seq_static(ctx,0x6F,0x0A);
	lcm_dcs_write_seq_static(ctx,0xE4,0x80,0x00,0x00,0x10,0x00,0x53);
	lcm_dcs_write_seq_static(ctx,0x6F,0x23);
	lcm_dcs_write_seq_static(ctx,0xD9,0x8B);
	lcm_dcs_write_seq_static(ctx,0x6F,0x04);
	lcm_dcs_write_seq_static(ctx,0xD9,0x03,0x00,0x00,0x00,0xFF);
	lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x02);
	lcm_dcs_write_seq_static(ctx,0xBC,0x11);
	lcm_dcs_write_seq_static(ctx,0xBD,0x96,0x00,0x69,0x00,0x00,0x96,0x00,0x69,0xBB,0x44,0x44,0xBB,0xEE,0x11,0x11,0xEE);
	lcm_dcs_write_seq_static(ctx,0xC1,0x02);
	lcm_dcs_write_seq_static(ctx,0xC2,0x19,0x00,0x91,0x00);


	lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x01);
	lcm_dcs_write_seq_static(ctx,0x6F,0x05);
	lcm_dcs_write_seq_static(ctx,0xC5,0x15,0x15,0x15);
	lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x08);
	lcm_dcs_write_seq_static(ctx,0x6F,0x36);
	lcm_dcs_write_seq_static(ctx,0xB8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x07);
	lcm_dcs_write_seq_static(ctx,0xB3,0x00,0x00);

	lcm_dcs_write_seq_static(ctx,0xFF,0xAA,0x55,0xA5,0x81);
	lcm_dcs_write_seq_static(ctx,0x6F,0x0E);
	lcm_dcs_write_seq_static(ctx,0xF5,0x2B);
	lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x02);
	lcm_dcs_write_seq_static(ctx,0xB5,0x55);

	lcm_dcs_write_seq_static(ctx,0xFF,0xAA,0x55,0xA5,0x83);
	lcm_dcs_write_seq_static(ctx,0x6F,0x12);
	lcm_dcs_write_seq_static(ctx,0xFE,0x41);

	lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x01);
	lcm_dcs_write_seq_static(ctx,0x6F,0x08);
	lcm_dcs_write_seq_static(ctx,0xB4,0x00,0x00,0x00,0x00);

	lcm_dcs_write_seq_static(ctx,0xFF,0xAA,0x55,0xA5,0x80);
	lcm_dcs_write_seq_static(ctx,0x6F,0x19);
	lcm_dcs_write_seq_static(ctx,0xF2,0x00);

	lcm_dcs_write_seq_static(ctx,0xFF,0xAA,0x55,0xA5,0x81);
	lcm_dcs_write_seq_static(ctx,0x6F,0x02);
	lcm_dcs_write_seq_static(ctx,0xF9,0x04);

	lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x03);
	lcm_dcs_write_seq_static(ctx,0xB5,0x05);
	lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x01);
	lcm_dcs_write_seq_static(ctx,0x6F,0x05);
	lcm_dcs_write_seq_static(ctx,0xC5,0x15,0x15,0x15,0xDD);

	lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x0B);
	lcm_dcs_write_seq_static(ctx,0xC2,0x11);
	lcm_dcs_write_seq_static(ctx,0xB0,0x00);

	lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x20);
	lcm_dcs_write_seq_static(ctx,0xC6,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55);

	lcm_dcs_write_seq_static(ctx,0xFF,0xAA,0x55,0xA5,0x81);
	lcm_dcs_write_seq_static(ctx,0x6F,0x19);
	lcm_dcs_write_seq_static(ctx,0xFB,0x00);

	lcm_dcs_write_seq_static(ctx,0xFF,0xAA,0x55,0xA5,0x80);
	lcm_dcs_write_seq_static(ctx,0x6F,0x1A);
	lcm_dcs_write_seq_static(ctx,0xF4,0x55);

	lcm_dcs_write_seq_static(ctx,0xFF,0xAA,0x55,0xA5,0x83);
	lcm_dcs_write_seq_static(ctx,0x6F,0x12);
	lcm_dcs_write_seq_static(ctx,0xFE,0x41);

	lcm_dcs_write_seq_static(ctx,0xFF,0xAA,0x55,0xA5,0x80);
	lcm_dcs_write_seq_static(ctx,0x6F,0x18);
	lcm_dcs_write_seq_static(ctx,0xF4,0x30);
	lcm_dcs_write_seq_static(ctx,0xFF,0xAA,0x55,0xA5,0x80);
	lcm_dcs_write_seq_static(ctx,0x6F,0x31);
	lcm_dcs_write_seq_static(ctx,0xF8,0x01,0x2B);
	lcm_dcs_write_seq_static(ctx,0xFF,0xAA,0x55,0xA5,0x80);
	lcm_dcs_write_seq_static(ctx,0x6F,0x15);
	lcm_dcs_write_seq_static(ctx,0xF8,0x01,0x8E);
	lcm_dcs_write_seq_static(ctx,0xFF,0xAA,0x55,0xA5,0x80);
	lcm_dcs_write_seq_static(ctx,0x6F,0x0A);
	lcm_dcs_write_seq_static(ctx,0xF6,0x60,0x60,0x60,0x50,0x60);
	lcm_dcs_write_seq_static(ctx,0xFF,0xAA,0x55,0xA5,0x81);
	lcm_dcs_write_seq_static(ctx,0x6F,0x1E);
	lcm_dcs_write_seq_static(ctx,0xFB,0x0F);

	lcm_dcs_write_seq_static(ctx,0xFF,0xAA,0x55,0xA5,0x81);
	lcm_dcs_write_seq_static(ctx,0x6F,0x05);
	lcm_dcs_write_seq_static(ctx,0xFE,0x34);
	lcm_dcs_write_seq_static(ctx,0x17,0x10);
//----------XS[15:0] = 0, XE[15:0] = 1079
	lcm_dcs_write_seq_static(ctx,0x2A,0x00,0x00,0x04,0x37);
	lcm_dcs_write_seq_static(ctx,0x2B,0x00,0x00,0x0A,0x4F);
	lcm_dcs_write_seq_static(ctx,0x2F,0x02);
	lcm_dcs_write_seq_static(ctx,0x5F,0x80);
//----------TE On
	lcm_dcs_write_seq_static(ctx,0x35,0x00);

	lcm_dcs_write_seq_static(ctx,0x51,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x6F,0x04);

// init code
	lcm_dcs_write_seq_static(ctx,0x51,0x0F,0xFF);
	lcm_dcs_write_seq_static(ctx,0x53,0x20);

//	lcm_dcs_write_seq_static(ctx,0x90,0x03,0x03);

	lcm_dcs_write_seq_static(ctx,0x90,0x00,0x00);

	lcm_dcs_write_seq_static(ctx,0x91,0x89,0x28,0x00,0x14,0xC2,0x00,0x02,0x0E,0x01,0xE8,0x00,0x07,0x05,0x0E,0x05,0x16,0x10,0xF0);


	lcm_dcs_write_seq_static(ctx,0xF0,0x55,0xAA,0x52,0x08,0x01);
	lcm_dcs_write_seq_static(ctx,0x6F,0x02);
	lcm_dcs_write_seq_static(ctx,0xC7,0x00,0x00);
// lcm_dcs_write_seq_static(ctx,0x51,0x0D,0Xbb);

	lcm_dcs_write_seq_static(ctx,0x26,0x00);

lcm_dcs_write_seq_static(ctx,0x11);
	msleep(50);
lcm_dcs_write_seq_static(ctx,0x29);
	msleep(70);

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

	pr_info("%s nt37705 wangwei1 \n", __func__);

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(70);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(20);
	
	// enter deep standby mode
	// lcm_dcs_write_seq_static(ctx, 0x4f, 0x01);
	// msleep(120);

	// lcd reset L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(10);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);


	if (current_lcd_mode == 0){
		pr_info("%s nt37705 main display\n", __func__);
#if defined(CONFIG_PARADE_REPORT_CONTROL)
		second_tp_report = false;
#endif
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
	} else if (current_lcd_mode == 1){
		pr_info("%s nt37705 second display\n", __func__);
#if defined(CONFIG_PARADE_REPORT_CONTROL)
		second_tp_report = false;
#endif
		//lcd vci L
		ctx->bias2_neg = devm_gpiod_get_index(ctx->dev, "bias2", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias2_neg)) {
			dev_err(ctx->dev, "%s: cannot get bias2_neg %ld\n",
				__func__, PTR_ERR(ctx->bias2_neg));
			return PTR_ERR(ctx->bias2_neg);
		}
		gpiod_set_value(ctx->bias2_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias2_neg);
		msleep(10);
		
		// lcd dvdd L
		ctx->bias2_pos = devm_gpiod_get_index(ctx->dev, "bias2", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias2_pos)) {
			dev_err(ctx->dev, "%s: cannot get bias2_pos %ld\n",
				__func__, PTR_ERR(ctx->bias2_pos));
			return PTR_ERR(ctx->bias2_pos);
		}
		gpiod_set_value(ctx->bias2_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias2_pos);
		msleep(5);	
	}

	ctx->mipi_switch_gpio = devm_gpiod_get(ctx->dev, "mipi_switch", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->mipi_switch_gpio)) {
		dev_info(ctx->dev, "cannot get switch-gpios %ld\n",
			PTR_ERR(ctx->mipi_switch_gpio));
		return PTR_ERR(ctx->mipi_switch_gpio);
	}

	ctx->rst_switch_gpio = devm_gpiod_get(ctx->dev, "rst_switch", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->rst_switch_gpio)) {
		dev_info(ctx->dev, "cannot get switch-gpios %ld\n",
			PTR_ERR(ctx->rst_switch_gpio));
		return PTR_ERR(ctx->rst_switch_gpio);
	}

	if (current_lcd_mode == 1){
		printk("wangwei drm lcm second display timing setting\n");
		gpiod_set_value(ctx->mipi_switch_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->mipi_switch_gpio);
		gpiod_set_value(ctx->rst_switch_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->rst_switch_gpio);
	}
	else if (current_lcd_mode == 0){
		printk("wangwei drm lcm main display timing setting\n");
		gpiod_set_value(ctx->mipi_switch_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->mipi_switch_gpio);
		gpiod_set_value(ctx->rst_switch_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->rst_switch_gpio);
	}

	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("nt37705 wangwei 2 %s+\n", __func__);
	if (ctx->prepared)
		return 0;

	if (current_lcd_mode == 0){
		pr_info("%s nt37705 main display\n", __func__);
#if defined(CONFIG_PARADE_REPORT_CONTROL)
		second_tp_report = false;
#endif
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
	} else if (current_lcd_mode == 1){
			pr_info("%s nt37705 second display\n", __func__);
			// lcd dvdd L
			ctx->bias2_pos = devm_gpiod_get_index(ctx->dev, "bias2", 0, GPIOD_OUT_HIGH);
			if (IS_ERR(ctx->bias2_pos)) {
				dev_err(ctx->dev, "%s: cannot get bias2_pos %ld\n",
					__func__, PTR_ERR(ctx->bias2_pos));
				return PTR_ERR(ctx->bias2_pos);
			}
			gpiod_set_value(ctx->bias2_pos, 1);
			devm_gpiod_put(ctx->dev, ctx->bias2_pos);
			msleep(10);

			//lcd vci L
			ctx->bias2_neg = devm_gpiod_get_index(ctx->dev, "bias2", 1, GPIOD_OUT_HIGH);
			if (IS_ERR(ctx->bias2_neg)) {
				dev_err(ctx->dev, "%s: cannot get bias2_neg %ld\n",
					__func__, PTR_ERR(ctx->bias2_neg));
				return PTR_ERR(ctx->bias2_neg);
			}
			gpiod_set_value(ctx->bias2_neg, 1);
			devm_gpiod_put(ctx->dev, ctx->bias2_neg);			
			msleep(5);
#if defined(CONFIG_PARADE_REPORT_CONTROL)
			second_tp_report = true;
#endif
	}

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

	if (current_lcd_mode == 1)
		second_lcm_panel_init(ctx);
	else
		lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	pr_info("nt37705 %s-\n", __func__);
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

//main display porch
#define HFP (100)
#define HSA (8)
#define HBP (92)
#define HACT (1080)
#define VFP (72)
#define VSA (4)
#define VBP (8)
#define VACT (2640)

//second display porch
#define HFP_S (28)
#define HSA_S (4)
#define HBP_S (25)
#define HACT_S (800)
#define VFP_S (18)
#define VSA_S (4)
#define VBP_S (18)
#define VACT_S (600)

static const struct drm_display_mode main_display_mode_60hz = {
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

static const struct drm_display_mode second_display_mode_60hz = {
	.clock		= ((HACT_S+HFP_S+HSA_S+HBP_S)*(VACT_S+VFP_S+VSA_S+VBP_S)*(60)/1000),
	.hdisplay	= HACT_S,
	.hsync_start	= HACT_S + HFP_S,
	.hsync_end	= HACT_S + HFP_S + HSA_S,
	.htotal		= HACT_S + HFP_S + HSA_S + HBP_S,
	.vdisplay	= VACT_S,
	.vsync_start	= VACT_S + VFP_S,
	.vsync_end	= VACT_S + VFP_S + VSA_S,
	.vtotal		= VACT_S + VFP_S + VSA_S + VBP_S,
};

#if defined(CONFIG_MTK_PANEL_EXT)

static struct mtk_panel_params ext_params_second_60hz = {
	.data_rate = 220*2,
	.pll_clk = 220,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
};

static struct mtk_panel_params ext_params_60hz = {
	.data_rate = 700*2,
	.pll_clk = 700,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = 69523,
	.physical_height_um = 154496,

	.dsc_params = {
		.enable = 0,
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

extern void prize_common_node_register(char* name,void(*set)(unsigned char on_off));
static int blstate = 1;
static void bl_control_func(unsigned char on)
{

	if(1 == on){
		blstate = 1;
		printk("%s backlight control state = %d\n", __func__, blstate);
	}else if(0 == on){
		blstate = 0;
		printk("%s backlight control state = %d\n", __func__, blstate);
	}

}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	char bl_tb[] = {0x51, 0x0D, 0xBB};
	char hbm_tb[] = {0x51, 0x0F, 0xFF};

	pr_info("nt37705 %s = %d\n", __func__,level);

	if (level > 4095) {
		if (level == 4096) {
			if (!cb)
				return -1;
			pr_err("%s panel into HBM", __func__);
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
			if (blstate == 1) {
				cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
				pr_err("%s level=%d, rawlevel=%d, bl_tb[1]=0x%x, bl_tb[2]=0x%x\n",
					__func__, level, rawlevel, bl_tb[1], bl_tb[2]);
			} else {
				bl_tb[1] = 0x00;
				bl_tb[2] = 0x00;
				cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
				printk("%s level=%d, rawlevel=%d, bl_tb[1]=0x%x, bl_tb[2]=0x%x\n",
					__func__, level, rawlevel, bl_tb[1], bl_tb[2]);
				printk("%s backlight control state = %d\n", __func__, blstate);
			}
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

	printk("wangwei drm lcm drm_mode_vrefresh(m) = %d mode name = %s\n", drm_mode_vrefresh(m), m->name);

	if (strcmp( "800x600", m->name) == 0){
		printk("wangwei mode name = %s\n", m->name);
		current_lcd_mode = 1;
		ext->params = &ext_params_second_60hz;
	}
	else if (strcmp( "1080x2640", m->name) == 0){
		printk("wangwei mode name = %s\n", m->name);
		current_lcd_mode = 0;
		ext->params = &ext_params_60hz;		
	}
	else{
		printk("wangwei 1 mode name = %s\n", m->name);
		ext->params = &ext_params_60hz;
		ret = 1;
	}

	return ret;
}

#if 0
static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);

		lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x02, 0x05);
	}
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);

		lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x02, 0x00);
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
#endif

static int panel_doze_enable(struct drm_panel *panel,
	void *dsi_drv, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_err("%s", __func__);
	/* Enter AOD */
	lcm_dcs_write_seq_static(ctx,0xFE,0x26);
	lcm_dcs_write_seq_static(ctx,0x1D,0x29);
	lcm_dcs_write_seq_static(ctx,0xFE,0x00);
	lcm_dcs_write_seq_static(ctx,0x39);
	lcm_dcs_write_seq_static(ctx,0xFE,0x40);
	lcm_dcs_write_seq_static(ctx,0XB2,0x43);
	/* AOD2 Switch */
	lcm_dcs_write_seq_static(ctx,0xFE,0x00);
	lcm_dcs_write_seq_static(ctx,0x39);
	lcm_dcs_write_seq_static(ctx,0x51,0x01,0x55);
	return 0;
}

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi_drv, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_err("%s", __func__);
	/* Exit AOD */
	lcm_dcs_write_seq_static(ctx,0xFE,0x00);
	lcm_dcs_write_seq_static(ctx,0x38);
	lcm_dcs_write_seq_static(ctx,0xFE,0x40);
	lcm_dcs_write_seq_static(ctx,0xB2,0x4E);
	/* Page00 */
	lcm_dcs_write_seq_static(ctx,0xFE,0x00);
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
	//.mode_switch = mode_switch,
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


	mode = drm_mode_duplicate(connector->dev, &main_display_mode_60hz);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 main_display_mode_60hz.hdisplay, main_display_mode_60hz.vdisplay,
			 drm_mode_vrefresh(&main_display_mode_60hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);


	mode_1 = drm_mode_duplicate(connector->dev, &second_display_mode_60hz);
	if (!mode_1) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			second_display_mode_60hz.hdisplay, second_display_mode_60hz.vdisplay,
			drm_mode_vrefresh(&second_display_mode_60hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
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
	//dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			   //| MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET;

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

	pr_info("%s- lcm,nt37705,cmd\n", __func__);

#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
    strcpy(current_lcm_info.chip,"nt37705,cmd");
    strcpy(current_lcm_info.vendor,"cost");
    sprintf(current_lcm_info.id,"0x%02x",0x00);
    strcpy(current_lcm_info.more,"1080*2640");
#endif

	prize_common_node_register("BLSTATE", &bl_control_func);

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
	    .compatible = "cost,nt37705,cmd",
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-cost-nt37705-dphy-cmd-120hz",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("MEDIATEK");
MODULE_DESCRIPTION("nt37705 AMOLED CMD LCD Panel Driver");
MODULE_LICENSE("GPL v2");
