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

#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/mediatek/prize/hardware_info/hardware_info.h"
extern struct hardware_info current_lcm_info;
#endif

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
lcm_dcs_write_seq_static(ctx,0xFF,0x20),
lcm_dcs_write_seq_static(ctx,0xFB,0x01),
lcm_dcs_write_seq_static(ctx,0x01,0x66),
lcm_dcs_write_seq_static(ctx,0x07,0x28),
lcm_dcs_write_seq_static(ctx,0x1F,0x02),
lcm_dcs_write_seq_static(ctx,0x20,0x03),
lcm_dcs_write_seq_static(ctx,0x29,0x15),
lcm_dcs_write_seq_static(ctx,0x2E,0x36),
lcm_dcs_write_seq_static(ctx,0x2F,0x83),
lcm_dcs_write_seq_static(ctx,0x3E,0x00),
lcm_dcs_write_seq_static(ctx,0x43,0x11),
lcm_dcs_write_seq_static(ctx,0x44,0x53),
lcm_dcs_write_seq_static(ctx,0x4F,0x01),
lcm_dcs_write_seq_static(ctx,0x5C,0x90),
lcm_dcs_write_seq_static(ctx,0x5E,0xAA),
lcm_dcs_write_seq_static(ctx,0x69,0xD0),
lcm_dcs_write_seq_static(ctx,0x88,0xC0),
lcm_dcs_write_seq_static(ctx,0x89,0x12),
lcm_dcs_write_seq_static(ctx,0x8A,0x12),
lcm_dcs_write_seq_static(ctx,0x95,0xD1),
lcm_dcs_write_seq_static(ctx,0x96,0xD1),
lcm_dcs_write_seq_static(ctx,0xB0,0x00,0x90,0x00,0x9A,0x00,0xAA,0x00,0xB7,0x00,0xC7,0x00,0xD3,0x00,0xE0,0x00,0xEB),
lcm_dcs_write_seq_static(ctx,0xB1,0x00,0xF5,0x01,0x1C,0x01,0x39,0x01,0x70,0x01,0x98,0x01,0xDC,0x02,0x15,0x02,0x17),
lcm_dcs_write_seq_static(ctx,0xB2,0x02,0x4E,0x02,0x8E,0x02,0xBB,0x02,0xED,0x03,0x12,0x03,0x3E,0x03,0x4C,0x03,0x5B),
lcm_dcs_write_seq_static(ctx,0xB3,0x03,0x6C,0x03,0x80,0x03,0x97,0x03,0xAE,0x03,0xD0,0x03,0xD9),
lcm_dcs_write_seq_static(ctx,0xB4,0x00,0x72,0x00,0x7C,0x00,0x8F,0x00,0xA1,0x00,0xB0,0x00,0xBF,0x00,0xCC,0x00,0xD9),
lcm_dcs_write_seq_static(ctx,0xB5,0x00,0xE5,0x01,0x0E,0x01,0x30,0x01,0x65,0x01,0x90,0x01,0xD4,0x02,0x0D,0x02,0x0F),
lcm_dcs_write_seq_static(ctx,0xB6,0x02,0x47,0x02,0x88,0x02,0xB4,0x02,0xE9,0x03,0x0F,0x03,0x3C,0x03,0x4B,0x03,0x5B),
lcm_dcs_write_seq_static(ctx,0xB7,0x03,0x6B,0x03,0x7F,0x03,0x96,0x03,0xAD,0x03,0xD0,0x03,0xD9),
lcm_dcs_write_seq_static(ctx,0xB8,0x00,0x00,0x00,0x17,0x00,0x3F,0x00,0x5B,0x00,0x76,0x00,0x8B,0x00,0x9F,0x00,0xB0),
lcm_dcs_write_seq_static(ctx,0xB9,0x00,0xC1,0x00,0xF5,0x01,0x1E,0x01,0x59,0x01,0x88,0x01,0xD0,0x02,0x0A,0x02,0x0C),
lcm_dcs_write_seq_static(ctx,0xBA,0x02,0x45,0x02,0x86,0x02,0xB2,0x02,0xE7,0x03,0x0F,0x03,0x3B,0x03,0x4C,0x03,0x61),
lcm_dcs_write_seq_static(ctx,0xBB,0x03,0x69,0x03,0x7D,0x03,0x95,0x03,0xB8,0x03,0xCD,0x03,0xD9),
lcm_dcs_write_seq_static(ctx,0xF2,0x64),
lcm_dcs_write_seq_static(ctx,0xF4,0x64),
lcm_dcs_write_seq_static(ctx,0xF6,0x64),
lcm_dcs_write_seq_static(ctx,0xF8,0x64),

lcm_dcs_write_seq_static(ctx,0x30,0X10),

lcm_dcs_write_seq_static(ctx,0xFF,0x21),
lcm_dcs_write_seq_static(ctx,0xFB,0x01),

lcm_dcs_write_seq_static(ctx,0xB0,0x00,0x90,0x00,0x9A,0x00,0xAA,0x00,0xB7,0x00,0xC7,0x00,0xD3,0x00,0xE0,0x00,0xEB),
lcm_dcs_write_seq_static(ctx,0xB1,0x00,0xF5,0x01,0x1C,0x01,0x3B,0x01,0x70,0x01,0x98,0x01,0xDC,0x02,0x15,0x02,0x17),
lcm_dcs_write_seq_static(ctx,0xB2,0x02,0x4E,0x02,0x8E,0x02,0xB9,0x02,0xED,0x03,0x12,0x03,0x3E,0x03,0x4C,0x03,0x5B),
lcm_dcs_write_seq_static(ctx,0xB3,0x03,0x6C,0x03,0x80,0x03,0x97,0x03,0xAE,0x03,0xD0,0x03,0xD9),
lcm_dcs_write_seq_static(ctx,0xB4,0x00,0x72,0x00,0x7C,0x00,0x8F,0x00,0xA1,0x00,0xB0,0x00,0xBF,0x00,0xCC,0x00,0xD9),
lcm_dcs_write_seq_static(ctx,0xB5,0x00,0xE5,0x01,0x0E,0x01,0x30,0x01,0x65,0x01,0x90,0x01,0xD4,0x02,0x0D,0x02,0x0F),
lcm_dcs_write_seq_static(ctx,0xB6,0x02,0x47,0x02,0x88,0x02,0xB4,0x02,0xE9,0x03,0x0F,0x03,0x3C,0x03,0x4B,0x03,0x5B),
lcm_dcs_write_seq_static(ctx,0xB7,0x03,0x6B,0x03,0x7F,0x03,0x96,0x03,0xAD,0x03,0xD0,0x03,0xD9),
lcm_dcs_write_seq_static(ctx,0xB8,0x00,0x00,0x00,0x17,0x00,0x3F,0x00,0x5B,0x00,0x76,0x00,0x8B,0x00,0x9F,0x00,0xB0),
lcm_dcs_write_seq_static(ctx,0xB9,0x00,0xC1,0x00,0xF5,0x01,0x1E,0x01,0x59,0x01,0x88,0x01,0xD0,0x02,0x0A,0x02,0x0C),
lcm_dcs_write_seq_static(ctx,0xBA,0x02,0x45,0x02,0x86,0x02,0xB2,0x02,0xE7,0x03,0x0F,0x03,0x3B,0x03,0x4C,0x03,0x61),
lcm_dcs_write_seq_static(ctx,0xBB,0x03,0x69,0x03,0x7D,0x03,0x95,0x03,0xB8,0x03,0xCD,0x03,0xD9),

lcm_dcs_write_seq_static(ctx,0xFF,0x24),
lcm_dcs_write_seq_static(ctx,0xFB,0x01),

lcm_dcs_write_seq_static(ctx,0x00,0x20),
lcm_dcs_write_seq_static(ctx,0x01,0x00),
lcm_dcs_write_seq_static(ctx,0x02,0x24),
lcm_dcs_write_seq_static(ctx,0x03,0x24),
lcm_dcs_write_seq_static(ctx,0x04,0x00),
lcm_dcs_write_seq_static(ctx,0x05,0x00),
lcm_dcs_write_seq_static(ctx,0x06,0x17),
lcm_dcs_write_seq_static(ctx,0x07,0x15),
lcm_dcs_write_seq_static(ctx,0x08,0x13),
lcm_dcs_write_seq_static(ctx,0x09,0x17),
lcm_dcs_write_seq_static(ctx,0x0A,0x15),
lcm_dcs_write_seq_static(ctx,0x0B,0x13),
lcm_dcs_write_seq_static(ctx,0x0C,0x23),
lcm_dcs_write_seq_static(ctx,0x0D,0x2F),
lcm_dcs_write_seq_static(ctx,0x0E,0x2E),
lcm_dcs_write_seq_static(ctx,0x0F,0x2D),
lcm_dcs_write_seq_static(ctx,0x10,0x2C),
lcm_dcs_write_seq_static(ctx,0x11,0x0F),
lcm_dcs_write_seq_static(ctx,0x12,0x24),
lcm_dcs_write_seq_static(ctx,0x13,0x24),
lcm_dcs_write_seq_static(ctx,0x14,0x0B),
lcm_dcs_write_seq_static(ctx,0x15,0x0C),
lcm_dcs_write_seq_static(ctx,0x16,0x1C),
lcm_dcs_write_seq_static(ctx,0x17,0x01),

lcm_dcs_write_seq_static(ctx,0x18,0x20),
lcm_dcs_write_seq_static(ctx,0x19,0x00),
lcm_dcs_write_seq_static(ctx,0x1A,0x24),
lcm_dcs_write_seq_static(ctx,0x1B,0x24),
lcm_dcs_write_seq_static(ctx,0x1C,0x00),
lcm_dcs_write_seq_static(ctx,0x1D,0x00),
lcm_dcs_write_seq_static(ctx,0x1E,0x17),
lcm_dcs_write_seq_static(ctx,0x1F,0x15),
lcm_dcs_write_seq_static(ctx,0x20,0x13),
lcm_dcs_write_seq_static(ctx,0x21,0x17),
lcm_dcs_write_seq_static(ctx,0x22,0x15),
lcm_dcs_write_seq_static(ctx,0x23,0x13),
lcm_dcs_write_seq_static(ctx,0x24,0x23),
lcm_dcs_write_seq_static(ctx,0x25,0x2F),
lcm_dcs_write_seq_static(ctx,0x26,0x2E),
lcm_dcs_write_seq_static(ctx,0x27,0x2D),
lcm_dcs_write_seq_static(ctx,0x28,0x2C),
lcm_dcs_write_seq_static(ctx,0x29,0x0F),
lcm_dcs_write_seq_static(ctx,0x2A,0x24),
lcm_dcs_write_seq_static(ctx,0x2B,0x24),
lcm_dcs_write_seq_static(ctx,0x2D,0x0B),
lcm_dcs_write_seq_static(ctx,0x2F,0x0C),
lcm_dcs_write_seq_static(ctx,0x30,0x1C),
lcm_dcs_write_seq_static(ctx,0x31,0x01),

lcm_dcs_write_seq_static(ctx,0x32,0x44),
lcm_dcs_write_seq_static(ctx,0x33,0x02),
lcm_dcs_write_seq_static(ctx,0x34,0x00),
lcm_dcs_write_seq_static(ctx,0x35,0x01),
lcm_dcs_write_seq_static(ctx,0x36,0x33),
lcm_dcs_write_seq_static(ctx,0x37,0x01),
lcm_dcs_write_seq_static(ctx,0x38,0x10),
lcm_dcs_write_seq_static(ctx,0x3B,0x04),
lcm_dcs_write_seq_static(ctx,0x4E,0x32),
lcm_dcs_write_seq_static(ctx,0x4F,0x32),
lcm_dcs_write_seq_static(ctx,0x53,0x32),
lcm_dcs_write_seq_static(ctx,0x7A,0x83),
lcm_dcs_write_seq_static(ctx,0x7B,0x8D),
lcm_dcs_write_seq_static(ctx,0x7D,0x04),
lcm_dcs_write_seq_static(ctx,0x80,0x04),
lcm_dcs_write_seq_static(ctx,0x81,0x04),
lcm_dcs_write_seq_static(ctx,0x82,0x13),
lcm_dcs_write_seq_static(ctx,0x84,0x31),
lcm_dcs_write_seq_static(ctx,0x85,0x00),
lcm_dcs_write_seq_static(ctx,0x86,0x00),
lcm_dcs_write_seq_static(ctx,0x87,0x00),
lcm_dcs_write_seq_static(ctx,0x90,0x13),
lcm_dcs_write_seq_static(ctx,0x92,0x31),
lcm_dcs_write_seq_static(ctx,0x93,0x00),
lcm_dcs_write_seq_static(ctx,0x94,0x00),
lcm_dcs_write_seq_static(ctx,0x95,0x00),
lcm_dcs_write_seq_static(ctx,0x9C,0xF4),
lcm_dcs_write_seq_static(ctx,0x9D,0x01),
lcm_dcs_write_seq_static(ctx,0xA0,0x0D),
lcm_dcs_write_seq_static(ctx,0xA2,0x0D),
lcm_dcs_write_seq_static(ctx,0xA3,0x03),
lcm_dcs_write_seq_static(ctx,0xA4,0x04),
lcm_dcs_write_seq_static(ctx,0xA5,0x04),
lcm_dcs_write_seq_static(ctx,0xC4,0x80),
lcm_dcs_write_seq_static(ctx,0xC6,0xC0),
lcm_dcs_write_seq_static(ctx,0xC9,0x00),
lcm_dcs_write_seq_static(ctx,0xD9,0x80),
lcm_dcs_write_seq_static(ctx,0xE9,0x03),

lcm_dcs_write_seq_static(ctx,0xFF,0x25),
lcm_dcs_write_seq_static(ctx,0xFB,0x01),
lcm_dcs_write_seq_static(ctx,0x0F,0x1B),
lcm_dcs_write_seq_static(ctx,0x18,0x22),
lcm_dcs_write_seq_static(ctx,0x19,0xE4),
lcm_dcs_write_seq_static(ctx,0x21,0x40),
lcm_dcs_write_seq_static(ctx,0x58,0x0C),
lcm_dcs_write_seq_static(ctx,0x59,0x0A),
lcm_dcs_write_seq_static(ctx,0x5C,0x05),
lcm_dcs_write_seq_static(ctx,0x5F,0x10),
lcm_dcs_write_seq_static(ctx,0x66,0xD8),
lcm_dcs_write_seq_static(ctx,0x67,0x01),
lcm_dcs_write_seq_static(ctx,0x68,0x58),
lcm_dcs_write_seq_static(ctx,0x69,0x10),
lcm_dcs_write_seq_static(ctx,0x6B,0x00),
lcm_dcs_write_seq_static(ctx,0x6C,0x1D),
lcm_dcs_write_seq_static(ctx,0x71,0x1D),
lcm_dcs_write_seq_static(ctx,0x77,0x62),
lcm_dcs_write_seq_static(ctx,0x7E,0x15),
lcm_dcs_write_seq_static(ctx,0x7F,0x00),
lcm_dcs_write_seq_static(ctx,0x84,0x6D),
lcm_dcs_write_seq_static(ctx,0x8D,0x00),
lcm_dcs_write_seq_static(ctx,0xC0,0xD5),
lcm_dcs_write_seq_static(ctx,0xC1,0x11),
lcm_dcs_write_seq_static(ctx,0xC3,0x00),
lcm_dcs_write_seq_static(ctx,0xC4,0x11),
lcm_dcs_write_seq_static(ctx,0xC5,0x11),
lcm_dcs_write_seq_static(ctx,0xC6,0x11),
lcm_dcs_write_seq_static(ctx,0xEF,0x00),
lcm_dcs_write_seq_static(ctx,0xF0,0x00),
lcm_dcs_write_seq_static(ctx,0xF1,0x04),

lcm_dcs_write_seq_static(ctx,0xFF,0x26),
lcm_dcs_write_seq_static(ctx,0xFB,0x01),
lcm_dcs_write_seq_static(ctx,0x00,0x10),
lcm_dcs_write_seq_static(ctx,0x01,0xF3),
lcm_dcs_write_seq_static(ctx,0x02,0x32),
lcm_dcs_write_seq_static(ctx,0x03,0x00),
lcm_dcs_write_seq_static(ctx,0x04,0xF3),
lcm_dcs_write_seq_static(ctx,0x05,0x08),
lcm_dcs_write_seq_static(ctx,0x06,0x10),
lcm_dcs_write_seq_static(ctx,0x07,0xFD),
lcm_dcs_write_seq_static(ctx,0x08,0x10),
lcm_dcs_write_seq_static(ctx,0x09,0x10),
lcm_dcs_write_seq_static(ctx,0x0A,0x00),
lcm_dcs_write_seq_static(ctx,0x0B,0x01),
lcm_dcs_write_seq_static(ctx,0x0C,0x01),
lcm_dcs_write_seq_static(ctx,0x0D,0x00),
lcm_dcs_write_seq_static(ctx,0x0E,0x00),
lcm_dcs_write_seq_static(ctx,0x0F,0x00),
lcm_dcs_write_seq_static(ctx,0x10,0x00),
lcm_dcs_write_seq_static(ctx,0x11,0x00),
lcm_dcs_write_seq_static(ctx,0x12,0x00),
lcm_dcs_write_seq_static(ctx,0x13,0x00),
lcm_dcs_write_seq_static(ctx,0x14,0x06),
lcm_dcs_write_seq_static(ctx,0x15,0x01),
lcm_dcs_write_seq_static(ctx,0x16,0x00),
lcm_dcs_write_seq_static(ctx,0x17,0x00),
lcm_dcs_write_seq_static(ctx,0x18,0x00),
lcm_dcs_write_seq_static(ctx,0x19,0x00),
lcm_dcs_write_seq_static(ctx,0x1A,0x00),
lcm_dcs_write_seq_static(ctx,0x1B,0x00),
lcm_dcs_write_seq_static(ctx,0x1C,0x00),
lcm_dcs_write_seq_static(ctx,0x1D,0x00),
lcm_dcs_write_seq_static(ctx,0x1E,0x00),
lcm_dcs_write_seq_static(ctx,0x1F,0x00),
lcm_dcs_write_seq_static(ctx,0x20,0x00),
lcm_dcs_write_seq_static(ctx,0x21,0x00),
lcm_dcs_write_seq_static(ctx,0x22,0x00),
lcm_dcs_write_seq_static(ctx,0x23,0x00),
lcm_dcs_write_seq_static(ctx,0x24,0x00),
lcm_dcs_write_seq_static(ctx,0x25,0x00),
lcm_dcs_write_seq_static(ctx,0x26,0x00),
lcm_dcs_write_seq_static(ctx,0x27,0x00),
lcm_dcs_write_seq_static(ctx,0x28,0x00),
lcm_dcs_write_seq_static(ctx,0x29,0x00),
lcm_dcs_write_seq_static(ctx,0x2A,0x00),
lcm_dcs_write_seq_static(ctx,0x2B,0x00),
lcm_dcs_write_seq_static(ctx,0x2C,0x00),
lcm_dcs_write_seq_static(ctx,0x2D,0x00),
lcm_dcs_write_seq_static(ctx,0x2E,0x00),
lcm_dcs_write_seq_static(ctx,0x2F,0x00),
lcm_dcs_write_seq_static(ctx,0x30,0x00),
lcm_dcs_write_seq_static(ctx,0x31,0x00),
lcm_dcs_write_seq_static(ctx,0x32,0x00),
lcm_dcs_write_seq_static(ctx,0x33,0x00),
lcm_dcs_write_seq_static(ctx,0x34,0x00),
lcm_dcs_write_seq_static(ctx,0x35,0x00),
lcm_dcs_write_seq_static(ctx,0x36,0x00),
lcm_dcs_write_seq_static(ctx,0x37,0x00),
lcm_dcs_write_seq_static(ctx,0x38,0x00),
lcm_dcs_write_seq_static(ctx,0x39,0x00),
lcm_dcs_write_seq_static(ctx,0x3A,0x00),
lcm_dcs_write_seq_static(ctx,0x3B,0x00),
lcm_dcs_write_seq_static(ctx,0x3C,0x00),
lcm_dcs_write_seq_static(ctx,0x3D,0x00),
lcm_dcs_write_seq_static(ctx,0x3E,0x00),
lcm_dcs_write_seq_static(ctx,0x3F,0x00),
lcm_dcs_write_seq_static(ctx,0x40,0x00),
lcm_dcs_write_seq_static(ctx,0x41,0x00),
lcm_dcs_write_seq_static(ctx,0x42,0x00),
lcm_dcs_write_seq_static(ctx,0x43,0x00),
lcm_dcs_write_seq_static(ctx,0x44,0x00),
lcm_dcs_write_seq_static(ctx,0x45,0x00),
lcm_dcs_write_seq_static(ctx,0x46,0x00),
lcm_dcs_write_seq_static(ctx,0x47,0x00),
lcm_dcs_write_seq_static(ctx,0x48,0x00),
lcm_dcs_write_seq_static(ctx,0x49,0x00),
lcm_dcs_write_seq_static(ctx,0x4A,0x00),
lcm_dcs_write_seq_static(ctx,0x4B,0x00),
lcm_dcs_write_seq_static(ctx,0x4C,0x00),
lcm_dcs_write_seq_static(ctx,0x4D,0x00),
lcm_dcs_write_seq_static(ctx,0x4E,0x00),
lcm_dcs_write_seq_static(ctx,0x4F,0x00),
lcm_dcs_write_seq_static(ctx,0x50,0x00),
lcm_dcs_write_seq_static(ctx,0x51,0x00),
lcm_dcs_write_seq_static(ctx,0x52,0x00),
lcm_dcs_write_seq_static(ctx,0x53,0x00),
lcm_dcs_write_seq_static(ctx,0x54,0x00),
lcm_dcs_write_seq_static(ctx,0x55,0x00),
lcm_dcs_write_seq_static(ctx,0x56,0x00),
lcm_dcs_write_seq_static(ctx,0x57,0x00),
lcm_dcs_write_seq_static(ctx,0x58,0x00),
lcm_dcs_write_seq_static(ctx,0x59,0x00),
lcm_dcs_write_seq_static(ctx,0x5A,0x00),
lcm_dcs_write_seq_static(ctx,0x5B,0x00),
lcm_dcs_write_seq_static(ctx,0x5C,0x00),
lcm_dcs_write_seq_static(ctx,0x5D,0x00),
lcm_dcs_write_seq_static(ctx,0x5E,0x00),
lcm_dcs_write_seq_static(ctx,0x5F,0x00),
lcm_dcs_write_seq_static(ctx,0x60,0x00),
lcm_dcs_write_seq_static(ctx,0x61,0x00),
lcm_dcs_write_seq_static(ctx,0x62,0x00),
lcm_dcs_write_seq_static(ctx,0x63,0x00),
lcm_dcs_write_seq_static(ctx,0x64,0x00),
lcm_dcs_write_seq_static(ctx,0x65,0x00),
lcm_dcs_write_seq_static(ctx,0x66,0x00),
lcm_dcs_write_seq_static(ctx,0x67,0x00),
lcm_dcs_write_seq_static(ctx,0x68,0x00),
lcm_dcs_write_seq_static(ctx,0x69,0x00),
lcm_dcs_write_seq_static(ctx,0x6A,0x00),
lcm_dcs_write_seq_static(ctx,0x6B,0x00),
lcm_dcs_write_seq_static(ctx,0x6C,0x00),
lcm_dcs_write_seq_static(ctx,0x6D,0x00),
lcm_dcs_write_seq_static(ctx,0x6E,0x00),
lcm_dcs_write_seq_static(ctx,0x6F,0x00),
lcm_dcs_write_seq_static(ctx,0x70,0x00),
lcm_dcs_write_seq_static(ctx,0x71,0x00),
lcm_dcs_write_seq_static(ctx,0x72,0x00),
lcm_dcs_write_seq_static(ctx,0x73,0x00),
lcm_dcs_write_seq_static(ctx,0x74,0xAF),
lcm_dcs_write_seq_static(ctx,0x75,0x00),
lcm_dcs_write_seq_static(ctx,0x76,0x00),
lcm_dcs_write_seq_static(ctx,0x77,0x00),
lcm_dcs_write_seq_static(ctx,0x78,0x00),
lcm_dcs_write_seq_static(ctx,0x79,0x00),
lcm_dcs_write_seq_static(ctx,0x7A,0x00),
lcm_dcs_write_seq_static(ctx,0x7B,0x00),
lcm_dcs_write_seq_static(ctx,0x7C,0x00),
lcm_dcs_write_seq_static(ctx,0x7D,0x00),
lcm_dcs_write_seq_static(ctx,0x7E,0x00),
lcm_dcs_write_seq_static(ctx,0x7F,0x00),
lcm_dcs_write_seq_static(ctx,0x80,0x05),
lcm_dcs_write_seq_static(ctx,0x81,0x0D),
lcm_dcs_write_seq_static(ctx,0x82,0x00),
lcm_dcs_write_seq_static(ctx,0x83,0x03),
lcm_dcs_write_seq_static(ctx,0x84,0x06),
lcm_dcs_write_seq_static(ctx,0x85,0x01),
lcm_dcs_write_seq_static(ctx,0x86,0x06),
lcm_dcs_write_seq_static(ctx,0x87,0x01),
lcm_dcs_write_seq_static(ctx,0x88,0x01),
lcm_dcs_write_seq_static(ctx,0x89,0x00),
lcm_dcs_write_seq_static(ctx,0x8A,0x1A),
lcm_dcs_write_seq_static(ctx,0x8B,0x11),
lcm_dcs_write_seq_static(ctx,0x8C,0x24),
lcm_dcs_write_seq_static(ctx,0x8D,0x33),
lcm_dcs_write_seq_static(ctx,0x8E,0x42),
lcm_dcs_write_seq_static(ctx,0x8F,0x11),
lcm_dcs_write_seq_static(ctx,0x90,0x11),
lcm_dcs_write_seq_static(ctx,0x91,0x11),
lcm_dcs_write_seq_static(ctx,0x92,0x00),
lcm_dcs_write_seq_static(ctx,0x93,0x00),
lcm_dcs_write_seq_static(ctx,0x94,0x00),
lcm_dcs_write_seq_static(ctx,0x95,0x55),
lcm_dcs_write_seq_static(ctx,0x96,0x00),
lcm_dcs_write_seq_static(ctx,0x97,0x00),
lcm_dcs_write_seq_static(ctx,0x98,0x71),
lcm_dcs_write_seq_static(ctx,0x99,0x20),
lcm_dcs_write_seq_static(ctx,0x9A,0x80),
lcm_dcs_write_seq_static(ctx,0x9B,0x04),
lcm_dcs_write_seq_static(ctx,0x9C,0x00),
lcm_dcs_write_seq_static(ctx,0x9D,0x00),
lcm_dcs_write_seq_static(ctx,0x9E,0x00),
lcm_dcs_write_seq_static(ctx,0x9F,0x00),
lcm_dcs_write_seq_static(ctx,0xA0,0x00),
lcm_dcs_write_seq_static(ctx,0xA1,0x00),
lcm_dcs_write_seq_static(ctx,0xA2,0x00),
lcm_dcs_write_seq_static(ctx,0xA3,0x00),
lcm_dcs_write_seq_static(ctx,0xA4,0x00),
lcm_dcs_write_seq_static(ctx,0xA5,0x00),
lcm_dcs_write_seq_static(ctx,0xA6,0x00),
lcm_dcs_write_seq_static(ctx,0xA7,0x00),
lcm_dcs_write_seq_static(ctx,0xA8,0x00),
lcm_dcs_write_seq_static(ctx,0xA9,0x55),
lcm_dcs_write_seq_static(ctx,0xAA,0xAA),
lcm_dcs_write_seq_static(ctx,0xAB,0x55),
lcm_dcs_write_seq_static(ctx,0xAC,0xAA),
lcm_dcs_write_seq_static(ctx,0xAD,0x00),
lcm_dcs_write_seq_static(ctx,0xAE,0x00),
lcm_dcs_write_seq_static(ctx,0xAF,0x00),
lcm_dcs_write_seq_static(ctx,0xB0,0x00),
lcm_dcs_write_seq_static(ctx,0xB1,0x00),
lcm_dcs_write_seq_static(ctx,0xB2,0x00),
lcm_dcs_write_seq_static(ctx,0xB3,0x00),
lcm_dcs_write_seq_static(ctx,0xB4,0x15),
lcm_dcs_write_seq_static(ctx,0xB5,0xE1),
lcm_dcs_write_seq_static(ctx,0xB6,0xE1),
lcm_dcs_write_seq_static(ctx,0xB7,0x0A),
lcm_dcs_write_seq_static(ctx,0xB8,0x3B),
lcm_dcs_write_seq_static(ctx,0xB9,0x3B),
lcm_dcs_write_seq_static(ctx,0xBA,0x0F),
lcm_dcs_write_seq_static(ctx,0xBB,0x00),
lcm_dcs_write_seq_static(ctx,0xBC,0x00),
lcm_dcs_write_seq_static(ctx,0xBD,0x00),
lcm_dcs_write_seq_static(ctx,0xBE,0x00),
lcm_dcs_write_seq_static(ctx,0xBF,0x66),
lcm_dcs_write_seq_static(ctx,0xC0,0x66),
lcm_dcs_write_seq_static(ctx,0xC1,0x00),
lcm_dcs_write_seq_static(ctx,0xC2,0x00),
lcm_dcs_write_seq_static(ctx,0xC3,0x00),
lcm_dcs_write_seq_static(ctx,0xC4,0x00),
lcm_dcs_write_seq_static(ctx,0xC5,0x00),
lcm_dcs_write_seq_static(ctx,0xC6,0x00),
lcm_dcs_write_seq_static(ctx,0xC7,0x00),
lcm_dcs_write_seq_static(ctx,0xC8,0x00),
lcm_dcs_write_seq_static(ctx,0xC9,0x00),
lcm_dcs_write_seq_static(ctx,0xCA,0x00),
lcm_dcs_write_seq_static(ctx,0xCB,0x00),
lcm_dcs_write_seq_static(ctx,0xCC,0x08),
lcm_dcs_write_seq_static(ctx,0xCD,0x00),
lcm_dcs_write_seq_static(ctx,0xCE,0x00),
lcm_dcs_write_seq_static(ctx,0xCF,0x00),
lcm_dcs_write_seq_static(ctx,0xD0,0x00),
lcm_dcs_write_seq_static(ctx,0xD1,0x00),
lcm_dcs_write_seq_static(ctx,0xD2,0x00),
lcm_dcs_write_seq_static(ctx,0xD3,0x00),
lcm_dcs_write_seq_static(ctx,0xD4,0x00),
lcm_dcs_write_seq_static(ctx,0xD5,0x00),
lcm_dcs_write_seq_static(ctx,0xD6,0x00),
lcm_dcs_write_seq_static(ctx,0xD7,0x00),
lcm_dcs_write_seq_static(ctx,0xD8,0x00),
lcm_dcs_write_seq_static(ctx,0xD9,0x00),
lcm_dcs_write_seq_static(ctx,0xDA,0x00),
lcm_dcs_write_seq_static(ctx,0xDB,0x00),
lcm_dcs_write_seq_static(ctx,0xDC,0x00),
lcm_dcs_write_seq_static(ctx,0xDD,0x00),
lcm_dcs_write_seq_static(ctx,0xDE,0x00),
lcm_dcs_write_seq_static(ctx,0xDF,0x00),
lcm_dcs_write_seq_static(ctx,0xE0,0x00),
lcm_dcs_write_seq_static(ctx,0xE1,0x00),
lcm_dcs_write_seq_static(ctx,0xE2,0x00),
lcm_dcs_write_seq_static(ctx,0xE3,0x00),
lcm_dcs_write_seq_static(ctx,0xE4,0x00),
lcm_dcs_write_seq_static(ctx,0xE5,0x00),
lcm_dcs_write_seq_static(ctx,0xE6,0x00),
lcm_dcs_write_seq_static(ctx,0xE7,0x00),
lcm_dcs_write_seq_static(ctx,0xE8,0x00),
lcm_dcs_write_seq_static(ctx,0xE9,0x00),
lcm_dcs_write_seq_static(ctx,0xEA,0x00),
lcm_dcs_write_seq_static(ctx,0xEB,0x00),
lcm_dcs_write_seq_static(ctx,0xEC,0x00),
lcm_dcs_write_seq_static(ctx,0xED,0x00),
lcm_dcs_write_seq_static(ctx,0xEE,0x00),
lcm_dcs_write_seq_static(ctx,0xEF,0x00),
lcm_dcs_write_seq_static(ctx,0xF0,0x00),
lcm_dcs_write_seq_static(ctx,0xF1,0x00),
lcm_dcs_write_seq_static(ctx,0xF2,0x00),
lcm_dcs_write_seq_static(ctx,0xF3,0x00),
lcm_dcs_write_seq_static(ctx,0xF4,0x00),
lcm_dcs_write_seq_static(ctx,0xF5,0x00),
lcm_dcs_write_seq_static(ctx,0xF6,0x00),
lcm_dcs_write_seq_static(ctx,0xF7,0x00),
lcm_dcs_write_seq_static(ctx,0xF8,0x00),
lcm_dcs_write_seq_static(ctx,0xF9,0x00),
lcm_dcs_write_seq_static(ctx,0xFA,0x00),

lcm_dcs_write_seq_static(ctx,0xFF,0x27),
lcm_dcs_write_seq_static(ctx,0xFB,0x01),
lcm_dcs_write_seq_static(ctx,0x00,0x94),
lcm_dcs_write_seq_static(ctx,0x01,0x68),
lcm_dcs_write_seq_static(ctx,0x02,0x38),
lcm_dcs_write_seq_static(ctx,0x03,0x00),
lcm_dcs_write_seq_static(ctx,0x04,0x00),
lcm_dcs_write_seq_static(ctx,0x05,0x00),
lcm_dcs_write_seq_static(ctx,0x06,0x00),
lcm_dcs_write_seq_static(ctx,0x07,0x01),
lcm_dcs_write_seq_static(ctx,0x08,0x00),
lcm_dcs_write_seq_static(ctx,0x09,0x01),
lcm_dcs_write_seq_static(ctx,0x0A,0x00),
lcm_dcs_write_seq_static(ctx,0x0B,0x00),
lcm_dcs_write_seq_static(ctx,0x0C,0x00),
lcm_dcs_write_seq_static(ctx,0x0D,0x00),
lcm_dcs_write_seq_static(ctx,0x0E,0x00),
lcm_dcs_write_seq_static(ctx,0x0F,0x00),
lcm_dcs_write_seq_static(ctx,0x10,0x00),
lcm_dcs_write_seq_static(ctx,0x11,0x00),
lcm_dcs_write_seq_static(ctx,0x12,0x00),
lcm_dcs_write_seq_static(ctx,0x13,0x00),
lcm_dcs_write_seq_static(ctx,0x14,0x27),
lcm_dcs_write_seq_static(ctx,0x15,0x01),
lcm_dcs_write_seq_static(ctx,0x16,0x01),
lcm_dcs_write_seq_static(ctx,0x17,0x27),
lcm_dcs_write_seq_static(ctx,0x18,0x01),
lcm_dcs_write_seq_static(ctx,0x19,0x27),
lcm_dcs_write_seq_static(ctx,0x1A,0x00),
lcm_dcs_write_seq_static(ctx,0x1B,0x00),
lcm_dcs_write_seq_static(ctx,0x1C,0x00),
lcm_dcs_write_seq_static(ctx,0x1D,0x00),
lcm_dcs_write_seq_static(ctx,0x1E,0x00),
lcm_dcs_write_seq_static(ctx,0x1F,0x00),
lcm_dcs_write_seq_static(ctx,0x20,0x81),
lcm_dcs_write_seq_static(ctx,0x21,0x6F),
lcm_dcs_write_seq_static(ctx,0x22,0x00),
lcm_dcs_write_seq_static(ctx,0x23,0x00),
lcm_dcs_write_seq_static(ctx,0x24,0x46),
lcm_dcs_write_seq_static(ctx,0x25,0x81),
lcm_dcs_write_seq_static(ctx,0x26,0x9A),
lcm_dcs_write_seq_static(ctx,0x27,0x00),
lcm_dcs_write_seq_static(ctx,0x28,0x4F),
lcm_dcs_write_seq_static(ctx,0x29,0x00),
lcm_dcs_write_seq_static(ctx,0x2A,0x00),
lcm_dcs_write_seq_static(ctx,0x2B,0x00),
lcm_dcs_write_seq_static(ctx,0x2C,0x00),
lcm_dcs_write_seq_static(ctx,0x2D,0x00),
lcm_dcs_write_seq_static(ctx,0x2E,0x00),
lcm_dcs_write_seq_static(ctx,0x2F,0x00),
lcm_dcs_write_seq_static(ctx,0x30,0x00),
lcm_dcs_write_seq_static(ctx,0x31,0x00),
lcm_dcs_write_seq_static(ctx,0x32,0x00),
lcm_dcs_write_seq_static(ctx,0x33,0x00),
lcm_dcs_write_seq_static(ctx,0x34,0x00),
lcm_dcs_write_seq_static(ctx,0x35,0x00),
lcm_dcs_write_seq_static(ctx,0x36,0x00),
lcm_dcs_write_seq_static(ctx,0x37,0x00),
lcm_dcs_write_seq_static(ctx,0x38,0x00),
lcm_dcs_write_seq_static(ctx,0x39,0x00),
lcm_dcs_write_seq_static(ctx,0x3A,0x00),
lcm_dcs_write_seq_static(ctx,0x3B,0x00),
lcm_dcs_write_seq_static(ctx,0x3C,0x00),
lcm_dcs_write_seq_static(ctx,0x3D,0x00),
lcm_dcs_write_seq_static(ctx,0x3E,0x00),
lcm_dcs_write_seq_static(ctx,0x3F,0x10),
lcm_dcs_write_seq_static(ctx,0x40,0x24),
lcm_dcs_write_seq_static(ctx,0x41,0x30),
lcm_dcs_write_seq_static(ctx,0x42,0x00),
lcm_dcs_write_seq_static(ctx,0x43,0x00),
lcm_dcs_write_seq_static(ctx,0x44,0x00),
lcm_dcs_write_seq_static(ctx,0x45,0xC0),
lcm_dcs_write_seq_static(ctx,0x46,0x04),
lcm_dcs_write_seq_static(ctx,0x47,0x64),
lcm_dcs_write_seq_static(ctx,0x48,0x19),
lcm_dcs_write_seq_static(ctx,0x49,0x64),
lcm_dcs_write_seq_static(ctx,0x4A,0x19),
lcm_dcs_write_seq_static(ctx,0x4B,0x05),
lcm_dcs_write_seq_static(ctx,0x4C,0x10),
lcm_dcs_write_seq_static(ctx,0x4D,0x00),
lcm_dcs_write_seq_static(ctx,0x4E,0x00),
lcm_dcs_write_seq_static(ctx,0x4F,0x00),
lcm_dcs_write_seq_static(ctx,0x50,0x00),
lcm_dcs_write_seq_static(ctx,0x51,0x30),
lcm_dcs_write_seq_static(ctx,0x52,0x00),
lcm_dcs_write_seq_static(ctx,0x53,0x00),
lcm_dcs_write_seq_static(ctx,0x54,0x00),
lcm_dcs_write_seq_static(ctx,0x55,0x00),
lcm_dcs_write_seq_static(ctx,0x56,0x00),
lcm_dcs_write_seq_static(ctx,0x57,0x00),
lcm_dcs_write_seq_static(ctx,0x58,0x00),
lcm_dcs_write_seq_static(ctx,0x59,0x00),
lcm_dcs_write_seq_static(ctx,0x5A,0x00),
lcm_dcs_write_seq_static(ctx,0x5B,0x00),
lcm_dcs_write_seq_static(ctx,0x5C,0x00),
lcm_dcs_write_seq_static(ctx,0x5D,0x00),
lcm_dcs_write_seq_static(ctx,0x5E,0x00),
lcm_dcs_write_seq_static(ctx,0x5F,0x00),
lcm_dcs_write_seq_static(ctx,0x60,0x00),
lcm_dcs_write_seq_static(ctx,0x61,0x00),
lcm_dcs_write_seq_static(ctx,0x62,0x00),
lcm_dcs_write_seq_static(ctx,0x63,0x00),
lcm_dcs_write_seq_static(ctx,0x64,0x00),
lcm_dcs_write_seq_static(ctx,0x65,0x00),
lcm_dcs_write_seq_static(ctx,0x66,0x00),
lcm_dcs_write_seq_static(ctx,0x67,0x00),
lcm_dcs_write_seq_static(ctx,0x68,0x00),
lcm_dcs_write_seq_static(ctx,0x69,0x00),
lcm_dcs_write_seq_static(ctx,0x6A,0x00),
lcm_dcs_write_seq_static(ctx,0x6B,0x00),
lcm_dcs_write_seq_static(ctx,0x6C,0x00),
lcm_dcs_write_seq_static(ctx,0x6D,0x00),
lcm_dcs_write_seq_static(ctx,0x6E,0x23),
lcm_dcs_write_seq_static(ctx,0x6F,0x01),
lcm_dcs_write_seq_static(ctx,0x70,0x00),
lcm_dcs_write_seq_static(ctx,0x71,0x00),
lcm_dcs_write_seq_static(ctx,0x72,0x00),
lcm_dcs_write_seq_static(ctx,0x73,0x21),
lcm_dcs_write_seq_static(ctx,0x74,0x03),
lcm_dcs_write_seq_static(ctx,0x75,0x00),
lcm_dcs_write_seq_static(ctx,0x76,0x00),
lcm_dcs_write_seq_static(ctx,0x77,0x00),
lcm_dcs_write_seq_static(ctx,0x78,0x00),
lcm_dcs_write_seq_static(ctx,0x79,0x00),
lcm_dcs_write_seq_static(ctx,0x7A,0x00),
lcm_dcs_write_seq_static(ctx,0x7B,0x00),
lcm_dcs_write_seq_static(ctx,0x7C,0x00),
lcm_dcs_write_seq_static(ctx,0x7D,0x09),
lcm_dcs_write_seq_static(ctx,0x7E,0x6B),
lcm_dcs_write_seq_static(ctx,0x7F,0x83),
lcm_dcs_write_seq_static(ctx,0x80,0x23),
lcm_dcs_write_seq_static(ctx,0x81,0x00),
lcm_dcs_write_seq_static(ctx,0x82,0x09),
lcm_dcs_write_seq_static(ctx,0x83,0x6B),
lcm_dcs_write_seq_static(ctx,0x84,0x00),
lcm_dcs_write_seq_static(ctx,0x85,0x00),
lcm_dcs_write_seq_static(ctx,0x86,0x00),
lcm_dcs_write_seq_static(ctx,0x87,0x00),
lcm_dcs_write_seq_static(ctx,0x88,0x03),
lcm_dcs_write_seq_static(ctx,0x89,0x01),
lcm_dcs_write_seq_static(ctx,0x8A,0x00),
lcm_dcs_write_seq_static(ctx,0x8B,0x00),
lcm_dcs_write_seq_static(ctx,0x8C,0x00),
lcm_dcs_write_seq_static(ctx,0x8D,0x00),
lcm_dcs_write_seq_static(ctx,0x8E,0x00),
lcm_dcs_write_seq_static(ctx,0x8F,0x00),
lcm_dcs_write_seq_static(ctx,0x90,0x00),
lcm_dcs_write_seq_static(ctx,0x91,0x00),
lcm_dcs_write_seq_static(ctx,0x92,0x00),
lcm_dcs_write_seq_static(ctx,0x93,0x00),
lcm_dcs_write_seq_static(ctx,0x94,0x00),
lcm_dcs_write_seq_static(ctx,0x95,0x00),
lcm_dcs_write_seq_static(ctx,0x96,0x00),
lcm_dcs_write_seq_static(ctx,0x97,0x00),
lcm_dcs_write_seq_static(ctx,0x98,0x00),
lcm_dcs_write_seq_static(ctx,0x99,0x00),
lcm_dcs_write_seq_static(ctx,0x9A,0x00),
lcm_dcs_write_seq_static(ctx,0x9B,0x00),
lcm_dcs_write_seq_static(ctx,0x9C,0x00),
lcm_dcs_write_seq_static(ctx,0x9D,0x00),
lcm_dcs_write_seq_static(ctx,0x9E,0x00),
lcm_dcs_write_seq_static(ctx,0x9F,0x00),
lcm_dcs_write_seq_static(ctx,0xA0,0x00),
lcm_dcs_write_seq_static(ctx,0xA1,0x00),
lcm_dcs_write_seq_static(ctx,0xA2,0x00),
lcm_dcs_write_seq_static(ctx,0xA3,0x00),
lcm_dcs_write_seq_static(ctx,0xA4,0x00),
lcm_dcs_write_seq_static(ctx,0xA5,0x00),
lcm_dcs_write_seq_static(ctx,0xA6,0x00),
lcm_dcs_write_seq_static(ctx,0xA7,0x00),
lcm_dcs_write_seq_static(ctx,0xA8,0x00),
lcm_dcs_write_seq_static(ctx,0xA9,0x00),
lcm_dcs_write_seq_static(ctx,0xAA,0x00),
lcm_dcs_write_seq_static(ctx,0xAB,0x00),
lcm_dcs_write_seq_static(ctx,0xAC,0x00),
lcm_dcs_write_seq_static(ctx,0xAD,0x00),
lcm_dcs_write_seq_static(ctx,0xAE,0x00),
lcm_dcs_write_seq_static(ctx,0xAF,0x00),
lcm_dcs_write_seq_static(ctx,0xB0,0x00),
lcm_dcs_write_seq_static(ctx,0xB1,0x00),
lcm_dcs_write_seq_static(ctx,0xB2,0x00),
lcm_dcs_write_seq_static(ctx,0xB3,0x03),
lcm_dcs_write_seq_static(ctx,0xB4,0x00),
lcm_dcs_write_seq_static(ctx,0xB5,0x00),
lcm_dcs_write_seq_static(ctx,0xB6,0x00),
lcm_dcs_write_seq_static(ctx,0xB7,0x00),
lcm_dcs_write_seq_static(ctx,0xB8,0x00),
lcm_dcs_write_seq_static(ctx,0xB9,0x00),
lcm_dcs_write_seq_static(ctx,0xBA,0x00),
lcm_dcs_write_seq_static(ctx,0xBB,0x00),
lcm_dcs_write_seq_static(ctx,0xBC,0x00),
lcm_dcs_write_seq_static(ctx,0xBD,0x00),
lcm_dcs_write_seq_static(ctx,0xBE,0x00),
lcm_dcs_write_seq_static(ctx,0xBF,0x00),
lcm_dcs_write_seq_static(ctx,0xC0,0x00),
lcm_dcs_write_seq_static(ctx,0xC1,0x00),
lcm_dcs_write_seq_static(ctx,0xC2,0x00),
lcm_dcs_write_seq_static(ctx,0xC3,0x00),
lcm_dcs_write_seq_static(ctx,0xC4,0x00),
lcm_dcs_write_seq_static(ctx,0xC5,0x00),
lcm_dcs_write_seq_static(ctx,0xC6,0x00),
lcm_dcs_write_seq_static(ctx,0xC7,0x00),
lcm_dcs_write_seq_static(ctx,0xC8,0x00),
lcm_dcs_write_seq_static(ctx,0xC9,0x00),
lcm_dcs_write_seq_static(ctx,0xCA,0x00),
lcm_dcs_write_seq_static(ctx,0xCB,0x00),
lcm_dcs_write_seq_static(ctx,0xCC,0x00),
lcm_dcs_write_seq_static(ctx,0xCD,0x3C),
lcm_dcs_write_seq_static(ctx,0xCE,0x3C),
lcm_dcs_write_seq_static(ctx,0xCF,0x3C),
lcm_dcs_write_seq_static(ctx,0xD0,0x3C),
lcm_dcs_write_seq_static(ctx,0xD1,0x01),
lcm_dcs_write_seq_static(ctx,0xD2,0x01),
lcm_dcs_write_seq_static(ctx,0xD3,0x01),
lcm_dcs_write_seq_static(ctx,0xD4,0x40),
lcm_dcs_write_seq_static(ctx,0xD5,0x00),
lcm_dcs_write_seq_static(ctx,0xD6,0x00),
lcm_dcs_write_seq_static(ctx,0xD7,0x08),
lcm_dcs_write_seq_static(ctx,0xD8,0x10),
lcm_dcs_write_seq_static(ctx,0xD9,0x00),
lcm_dcs_write_seq_static(ctx,0xDA,0x00),
lcm_dcs_write_seq_static(ctx,0xDB,0x05),
lcm_dcs_write_seq_static(ctx,0xDC,0x1E),
lcm_dcs_write_seq_static(ctx,0xDD,0x04),
lcm_dcs_write_seq_static(ctx,0xDE,0x02),
lcm_dcs_write_seq_static(ctx,0xDF,0x00),
lcm_dcs_write_seq_static(ctx,0xE0,0x00),
lcm_dcs_write_seq_static(ctx,0xE1,0x1F),
lcm_dcs_write_seq_static(ctx,0xE2,0x1F),
lcm_dcs_write_seq_static(ctx,0xE3,0x01),
lcm_dcs_write_seq_static(ctx,0xE4,0xEA),
lcm_dcs_write_seq_static(ctx,0xE5,0x02),
lcm_dcs_write_seq_static(ctx,0xE6,0xDF),
lcm_dcs_write_seq_static(ctx,0xE7,0x00),
lcm_dcs_write_seq_static(ctx,0xE8,0x7B),
lcm_dcs_write_seq_static(ctx,0xE9,0x02),
lcm_dcs_write_seq_static(ctx,0xEA,0x23),
lcm_dcs_write_seq_static(ctx,0xEB,0x03),
lcm_dcs_write_seq_static(ctx,0xEC,0x34),
lcm_dcs_write_seq_static(ctx,0xED,0x00),
lcm_dcs_write_seq_static(ctx,0xEE,0x7B),
lcm_dcs_write_seq_static(ctx,0xEF,0x01),
lcm_dcs_write_seq_static(ctx,0xF0,0x11),
lcm_dcs_write_seq_static(ctx,0xF1,0x00),
lcm_dcs_write_seq_static(ctx,0xF2,0x01),
lcm_dcs_write_seq_static(ctx,0xF3,0x00),
lcm_dcs_write_seq_static(ctx,0xF4,0x01),
lcm_dcs_write_seq_static(ctx,0xF5,0x00),
lcm_dcs_write_seq_static(ctx,0xF6,0x01),
lcm_dcs_write_seq_static(ctx,0xF7,0x00),
lcm_dcs_write_seq_static(ctx,0xF8,0x00),
lcm_dcs_write_seq_static(ctx,0xF9,0x00),
lcm_dcs_write_seq_static(ctx,0xFA,0x00),

lcm_dcs_write_seq_static(ctx,0xFF,0x2A),
lcm_dcs_write_seq_static(ctx,0xFB,0x01),
lcm_dcs_write_seq_static(ctx,0x00,0x91),
lcm_dcs_write_seq_static(ctx,0x01,0x00),
lcm_dcs_write_seq_static(ctx,0x02,0x00),
lcm_dcs_write_seq_static(ctx,0x03,0x20),
lcm_dcs_write_seq_static(ctx,0x04,0x4C),
lcm_dcs_write_seq_static(ctx,0x05,0x00),
lcm_dcs_write_seq_static(ctx,0x06,0x08),
lcm_dcs_write_seq_static(ctx,0x07,0x50),
lcm_dcs_write_seq_static(ctx,0x08,0x0E),
lcm_dcs_write_seq_static(ctx,0x09,0x00),
lcm_dcs_write_seq_static(ctx,0x0A,0x70),
lcm_dcs_write_seq_static(ctx,0x0B,0x01),
lcm_dcs_write_seq_static(ctx,0x0C,0x04),
lcm_dcs_write_seq_static(ctx,0x0D,0x40),
lcm_dcs_write_seq_static(ctx,0x0E,0x01),
lcm_dcs_write_seq_static(ctx,0x0F,0x01),
lcm_dcs_write_seq_static(ctx,0x10,0x64),
lcm_dcs_write_seq_static(ctx,0x11,0xE1),
lcm_dcs_write_seq_static(ctx,0x12,0x00),
lcm_dcs_write_seq_static(ctx,0x13,0x00),
lcm_dcs_write_seq_static(ctx,0x14,0x00),
lcm_dcs_write_seq_static(ctx,0x15,0x0F),
lcm_dcs_write_seq_static(ctx,0x16,0x2F),
lcm_dcs_write_seq_static(ctx,0x17,0x64),
lcm_dcs_write_seq_static(ctx,0x18,0x00),
lcm_dcs_write_seq_static(ctx,0x19,0x0F),
lcm_dcs_write_seq_static(ctx,0x1A,0x03),
lcm_dcs_write_seq_static(ctx,0x1B,0x23),
lcm_dcs_write_seq_static(ctx,0x1C,0x00),
lcm_dcs_write_seq_static(ctx,0x1D,0x20),
lcm_dcs_write_seq_static(ctx,0x1E,0x3F),
lcm_dcs_write_seq_static(ctx,0x1F,0x3F),
lcm_dcs_write_seq_static(ctx,0x20,0x3F),
lcm_dcs_write_seq_static(ctx,0x21,0x07),
lcm_dcs_write_seq_static(ctx,0x22,0xDD),
lcm_dcs_write_seq_static(ctx,0x23,0x24),
lcm_dcs_write_seq_static(ctx,0x24,0x07),
lcm_dcs_write_seq_static(ctx,0x25,0xD7),
lcm_dcs_write_seq_static(ctx,0x26,0x38),
lcm_dcs_write_seq_static(ctx,0x27,0x80),
lcm_dcs_write_seq_static(ctx,0x28,0xFD),
lcm_dcs_write_seq_static(ctx,0x29,0x0A),
lcm_dcs_write_seq_static(ctx,0x2A,0xFF),
lcm_dcs_write_seq_static(ctx,0x2B,0x00),
lcm_dcs_write_seq_static(ctx,0x2C,0x00),
lcm_dcs_write_seq_static(ctx,0x2D,0x0B),
lcm_dcs_write_seq_static(ctx,0x2E,0x00),
lcm_dcs_write_seq_static(ctx,0x2F,0x01),
lcm_dcs_write_seq_static(ctx,0x30,0x85),
lcm_dcs_write_seq_static(ctx,0x31,0x40),
lcm_dcs_write_seq_static(ctx,0x32,0x32),
lcm_dcs_write_seq_static(ctx,0x33,0x2C),
lcm_dcs_write_seq_static(ctx,0x34,0xFF),
lcm_dcs_write_seq_static(ctx,0x35,0x3C),
lcm_dcs_write_seq_static(ctx,0x36,0x00),
lcm_dcs_write_seq_static(ctx,0x37,0xF9),
lcm_dcs_write_seq_static(ctx,0x38,0x41),
lcm_dcs_write_seq_static(ctx,0x39,0xFB),
lcm_dcs_write_seq_static(ctx,0x3A,0x45),
lcm_dcs_write_seq_static(ctx,0x3B,0x80),
lcm_dcs_write_seq_static(ctx,0x3C,0x00),
lcm_dcs_write_seq_static(ctx,0x3D,0x00),
lcm_dcs_write_seq_static(ctx,0x3E,0x00),
lcm_dcs_write_seq_static(ctx,0x3F,0x00),
lcm_dcs_write_seq_static(ctx,0x40,0x00),
lcm_dcs_write_seq_static(ctx,0x41,0x00),
lcm_dcs_write_seq_static(ctx,0x42,0x00),
lcm_dcs_write_seq_static(ctx,0x43,0x00),
lcm_dcs_write_seq_static(ctx,0x44,0x38),
lcm_dcs_write_seq_static(ctx,0x45,0x06),
lcm_dcs_write_seq_static(ctx,0x46,0x40),
lcm_dcs_write_seq_static(ctx,0x47,0x02),
lcm_dcs_write_seq_static(ctx,0x48,0x01),
lcm_dcs_write_seq_static(ctx,0x49,0x64),
lcm_dcs_write_seq_static(ctx,0x4A,0x58),
lcm_dcs_write_seq_static(ctx,0x4B,0x00),
lcm_dcs_write_seq_static(ctx,0x4C,0x00),
lcm_dcs_write_seq_static(ctx,0x4D,0x00),
lcm_dcs_write_seq_static(ctx,0x4E,0x0F),
lcm_dcs_write_seq_static(ctx,0x4F,0x2F),
lcm_dcs_write_seq_static(ctx,0x50,0x64),
lcm_dcs_write_seq_static(ctx,0x51,0x00),
lcm_dcs_write_seq_static(ctx,0x52,0x0F),
lcm_dcs_write_seq_static(ctx,0x53,0x03),
lcm_dcs_write_seq_static(ctx,0x54,0x23),
lcm_dcs_write_seq_static(ctx,0x55,0x00),
lcm_dcs_write_seq_static(ctx,0x56,0x10),
lcm_dcs_write_seq_static(ctx,0x57,0x53),
lcm_dcs_write_seq_static(ctx,0x58,0x53),
lcm_dcs_write_seq_static(ctx,0x59,0x53),
lcm_dcs_write_seq_static(ctx,0x5A,0x07),
lcm_dcs_write_seq_static(ctx,0x5B,0xDD),
lcm_dcs_write_seq_static(ctx,0x5C,0x1E),
lcm_dcs_write_seq_static(ctx,0x5D,0x07),
lcm_dcs_write_seq_static(ctx,0x5E,0xE5),
lcm_dcs_write_seq_static(ctx,0x5F,0x2F),
lcm_dcs_write_seq_static(ctx,0x60,0x00),
lcm_dcs_write_seq_static(ctx,0x61,0x08),
lcm_dcs_write_seq_static(ctx,0x62,0x00),
lcm_dcs_write_seq_static(ctx,0x63,0x38),
lcm_dcs_write_seq_static(ctx,0x64,0x00),
lcm_dcs_write_seq_static(ctx,0x65,0x00),
lcm_dcs_write_seq_static(ctx,0x66,0x00),
lcm_dcs_write_seq_static(ctx,0x67,0x01),
lcm_dcs_write_seq_static(ctx,0x68,0x20),
lcm_dcs_write_seq_static(ctx,0x69,0x10),
lcm_dcs_write_seq_static(ctx,0x6A,0x00),
lcm_dcs_write_seq_static(ctx,0x6B,0x02),
lcm_dcs_write_seq_static(ctx,0x6C,0x36),
lcm_dcs_write_seq_static(ctx,0x6D,0x00),
lcm_dcs_write_seq_static(ctx,0x6E,0x00),
lcm_dcs_write_seq_static(ctx,0x6F,0x36),
lcm_dcs_write_seq_static(ctx,0x70,0x00),
lcm_dcs_write_seq_static(ctx,0x71,0x01),
lcm_dcs_write_seq_static(ctx,0x72,0xA0),
lcm_dcs_write_seq_static(ctx,0x73,0x00),
lcm_dcs_write_seq_static(ctx,0x74,0x00),
lcm_dcs_write_seq_static(ctx,0x75,0x00),
lcm_dcs_write_seq_static(ctx,0x76,0x00),
lcm_dcs_write_seq_static(ctx,0x77,0x00),
lcm_dcs_write_seq_static(ctx,0x78,0x00),
lcm_dcs_write_seq_static(ctx,0x79,0x24),
lcm_dcs_write_seq_static(ctx,0x7A,0x09),
lcm_dcs_write_seq_static(ctx,0x7B,0x40),
lcm_dcs_write_seq_static(ctx,0x7C,0x02),
lcm_dcs_write_seq_static(ctx,0x7D,0x00),
lcm_dcs_write_seq_static(ctx,0x7E,0x64),
lcm_dcs_write_seq_static(ctx,0x7F,0xF0),
lcm_dcs_write_seq_static(ctx,0x80,0x00),
lcm_dcs_write_seq_static(ctx,0x81,0x00),
lcm_dcs_write_seq_static(ctx,0x82,0x00),
lcm_dcs_write_seq_static(ctx,0x83,0x0F),
lcm_dcs_write_seq_static(ctx,0x84,0x2F),
lcm_dcs_write_seq_static(ctx,0x85,0x64),
lcm_dcs_write_seq_static(ctx,0x86,0x00),
lcm_dcs_write_seq_static(ctx,0x87,0x0F),
lcm_dcs_write_seq_static(ctx,0x88,0x03),
lcm_dcs_write_seq_static(ctx,0x89,0x23),
lcm_dcs_write_seq_static(ctx,0x8A,0x00),
lcm_dcs_write_seq_static(ctx,0x8B,0x10),
lcm_dcs_write_seq_static(ctx,0x8C,0x7E),
lcm_dcs_write_seq_static(ctx,0x8D,0x7E),
lcm_dcs_write_seq_static(ctx,0x8E,0x7E),
lcm_dcs_write_seq_static(ctx,0x8F,0x07),
lcm_dcs_write_seq_static(ctx,0x90,0xDD),
lcm_dcs_write_seq_static(ctx,0x91,0x18),
lcm_dcs_write_seq_static(ctx,0x92,0x07),
lcm_dcs_write_seq_static(ctx,0x93,0xFB),
lcm_dcs_write_seq_static(ctx,0x94,0x26),
lcm_dcs_write_seq_static(ctx,0x95,0x00),
lcm_dcs_write_seq_static(ctx,0x96,0x08),
lcm_dcs_write_seq_static(ctx,0x97,0x00),
lcm_dcs_write_seq_static(ctx,0x98,0x38),
lcm_dcs_write_seq_static(ctx,0x99,0x00),
lcm_dcs_write_seq_static(ctx,0x9A,0x00),
lcm_dcs_write_seq_static(ctx,0x9B,0x00),
lcm_dcs_write_seq_static(ctx,0x9C,0x01),
lcm_dcs_write_seq_static(ctx,0x9D,0x20),
lcm_dcs_write_seq_static(ctx,0x9E,0x10),
lcm_dcs_write_seq_static(ctx,0x9F,0x00),
lcm_dcs_write_seq_static(ctx,0xA0,0x02),
lcm_dcs_write_seq_static(ctx,0xA1,0x00),
lcm_dcs_write_seq_static(ctx,0xA2,0x36),
lcm_dcs_write_seq_static(ctx,0xA3,0x00),
lcm_dcs_write_seq_static(ctx,0xA4,0x00),
lcm_dcs_write_seq_static(ctx,0xA5,0x36),
lcm_dcs_write_seq_static(ctx,0xA6,0x00),
lcm_dcs_write_seq_static(ctx,0xA7,0x01),
lcm_dcs_write_seq_static(ctx,0xA8,0x00),
lcm_dcs_write_seq_static(ctx,0xA9,0xC0),
lcm_dcs_write_seq_static(ctx,0xAA,0x00),
lcm_dcs_write_seq_static(ctx,0xAB,0x00),
lcm_dcs_write_seq_static(ctx,0xAC,0x00),
lcm_dcs_write_seq_static(ctx,0xAD,0x00),
lcm_dcs_write_seq_static(ctx,0xAE,0x00),
lcm_dcs_write_seq_static(ctx,0xAF,0x00),
lcm_dcs_write_seq_static(ctx,0xB0,0x12),
lcm_dcs_write_seq_static(ctx,0xB1,0x27),
lcm_dcs_write_seq_static(ctx,0xB2,0x90),
lcm_dcs_write_seq_static(ctx,0xB3,0x04),
lcm_dcs_write_seq_static(ctx,0xB4,0x00),
lcm_dcs_write_seq_static(ctx,0xB5,0x64),
lcm_dcs_write_seq_static(ctx,0xB6,0x3A),
lcm_dcs_write_seq_static(ctx,0xB7,0x00),
lcm_dcs_write_seq_static(ctx,0xB8,0x00),
lcm_dcs_write_seq_static(ctx,0xB9,0x00),
lcm_dcs_write_seq_static(ctx,0xBA,0x07),
lcm_dcs_write_seq_static(ctx,0xBB,0xEB),
lcm_dcs_write_seq_static(ctx,0xBC,0x64),
lcm_dcs_write_seq_static(ctx,0xBD,0x00),
lcm_dcs_write_seq_static(ctx,0xBE,0x07),
lcm_dcs_write_seq_static(ctx,0xBF,0x68),
lcm_dcs_write_seq_static(ctx,0xC0,0x20),
lcm_dcs_write_seq_static(ctx,0xC1,0x00),
lcm_dcs_write_seq_static(ctx,0xC2,0x20),
lcm_dcs_write_seq_static(ctx,0xC3,0xFD),
lcm_dcs_write_seq_static(ctx,0xC4,0xFD),
lcm_dcs_write_seq_static(ctx,0xC5,0xFD),
lcm_dcs_write_seq_static(ctx,0xC6,0x07),
lcm_dcs_write_seq_static(ctx,0xC7,0xDD),
lcm_dcs_write_seq_static(ctx,0xC8,0x12),
lcm_dcs_write_seq_static(ctx,0xC9,0x07),
lcm_dcs_write_seq_static(ctx,0xCA,0xD7),
lcm_dcs_write_seq_static(ctx,0xCB,0x1C),
lcm_dcs_write_seq_static(ctx,0xCC,0x00),
lcm_dcs_write_seq_static(ctx,0xCD,0x08),
lcm_dcs_write_seq_static(ctx,0xCE,0x00),
lcm_dcs_write_seq_static(ctx,0xCF,0x38),
lcm_dcs_write_seq_static(ctx,0xD0,0x00),
lcm_dcs_write_seq_static(ctx,0xD1,0x00),
lcm_dcs_write_seq_static(ctx,0xD2,0x00),
lcm_dcs_write_seq_static(ctx,0xD3,0x01),
lcm_dcs_write_seq_static(ctx,0xD4,0x20),
lcm_dcs_write_seq_static(ctx,0xD5,0x10),
lcm_dcs_write_seq_static(ctx,0xD6,0x00),
lcm_dcs_write_seq_static(ctx,0xD7,0x02),
lcm_dcs_write_seq_static(ctx,0xD8,0x36),
lcm_dcs_write_seq_static(ctx,0xD9,0x00),
lcm_dcs_write_seq_static(ctx,0xDA,0x00),
lcm_dcs_write_seq_static(ctx,0xDB,0x36),
lcm_dcs_write_seq_static(ctx,0xDC,0x00),
lcm_dcs_write_seq_static(ctx,0xDD,0x01),
lcm_dcs_write_seq_static(ctx,0xDE,0xE0),
lcm_dcs_write_seq_static(ctx,0xDF,0x00),
lcm_dcs_write_seq_static(ctx,0xE0,0x00),
lcm_dcs_write_seq_static(ctx,0xE1,0x00),
lcm_dcs_write_seq_static(ctx,0xE2,0x00),
lcm_dcs_write_seq_static(ctx,0xE3,0x00),
lcm_dcs_write_seq_static(ctx,0xE4,0x00),
lcm_dcs_write_seq_static(ctx,0xE5,0x00),
lcm_dcs_write_seq_static(ctx,0xE6,0x00),
lcm_dcs_write_seq_static(ctx,0xE7,0xC0),
lcm_dcs_write_seq_static(ctx,0xE8,0x00),
lcm_dcs_write_seq_static(ctx,0xE9,0x00),
lcm_dcs_write_seq_static(ctx,0xEA,0x00),
lcm_dcs_write_seq_static(ctx,0xEB,0x03),
lcm_dcs_write_seq_static(ctx,0xEC,0x20),
lcm_dcs_write_seq_static(ctx,0xED,0x00),
lcm_dcs_write_seq_static(ctx,0xEE,0x00),
lcm_dcs_write_seq_static(ctx,0xEF,0x00),
lcm_dcs_write_seq_static(ctx,0xF0,0x00),
lcm_dcs_write_seq_static(ctx,0xF1,0x00),
lcm_dcs_write_seq_static(ctx,0xF2,0x00),
lcm_dcs_write_seq_static(ctx,0xF3,0x00),
lcm_dcs_write_seq_static(ctx,0xF4,0x00),
lcm_dcs_write_seq_static(ctx,0xF5,0x00),
lcm_dcs_write_seq_static(ctx,0xF6,0x00),
lcm_dcs_write_seq_static(ctx,0xF7,0x00),
lcm_dcs_write_seq_static(ctx,0xF8,0x00),
lcm_dcs_write_seq_static(ctx,0xF9,0x00),
lcm_dcs_write_seq_static(ctx,0xFA,0x00),

lcm_dcs_write_seq_static(ctx,0xFF,0x2B),
lcm_dcs_write_seq_static(ctx,0xFB,0x01),
lcm_dcs_write_seq_static(ctx,0x00,0x00),
lcm_dcs_write_seq_static(ctx,0x01,0x00),
lcm_dcs_write_seq_static(ctx,0x02,0x00),
lcm_dcs_write_seq_static(ctx,0x03,0x00),
lcm_dcs_write_seq_static(ctx,0x04,0x00),
lcm_dcs_write_seq_static(ctx,0x05,0x00),
lcm_dcs_write_seq_static(ctx,0x06,0x00),
lcm_dcs_write_seq_static(ctx,0x07,0x00),
lcm_dcs_write_seq_static(ctx,0x08,0x00),
lcm_dcs_write_seq_static(ctx,0x09,0x00),
lcm_dcs_write_seq_static(ctx,0x0A,0x00),
lcm_dcs_write_seq_static(ctx,0x0B,0x00),
lcm_dcs_write_seq_static(ctx,0x0C,0x00),
lcm_dcs_write_seq_static(ctx,0x0D,0x00),
lcm_dcs_write_seq_static(ctx,0x0E,0x00),
lcm_dcs_write_seq_static(ctx,0x0F,0x00),
lcm_dcs_write_seq_static(ctx,0x10,0x00),
lcm_dcs_write_seq_static(ctx,0x11,0x00),
lcm_dcs_write_seq_static(ctx,0x12,0x00),
lcm_dcs_write_seq_static(ctx,0x13,0x00),
lcm_dcs_write_seq_static(ctx,0x14,0x00),
lcm_dcs_write_seq_static(ctx,0x15,0x00),
lcm_dcs_write_seq_static(ctx,0x16,0x00),
lcm_dcs_write_seq_static(ctx,0x17,0x00),
lcm_dcs_write_seq_static(ctx,0x18,0x00),
lcm_dcs_write_seq_static(ctx,0x19,0x00),
lcm_dcs_write_seq_static(ctx,0x1A,0x00),
lcm_dcs_write_seq_static(ctx,0x1B,0x00),
lcm_dcs_write_seq_static(ctx,0x1C,0x00),
lcm_dcs_write_seq_static(ctx,0x1D,0x00),
lcm_dcs_write_seq_static(ctx,0x1E,0x00),
lcm_dcs_write_seq_static(ctx,0x1F,0x00),
lcm_dcs_write_seq_static(ctx,0x20,0x00),
lcm_dcs_write_seq_static(ctx,0x21,0x00),
lcm_dcs_write_seq_static(ctx,0x22,0x00),
lcm_dcs_write_seq_static(ctx,0x23,0x00),
lcm_dcs_write_seq_static(ctx,0x24,0x00),
lcm_dcs_write_seq_static(ctx,0x25,0x00),
lcm_dcs_write_seq_static(ctx,0x26,0x00),
lcm_dcs_write_seq_static(ctx,0x27,0x00),
lcm_dcs_write_seq_static(ctx,0x28,0x00),
lcm_dcs_write_seq_static(ctx,0x29,0x00),
lcm_dcs_write_seq_static(ctx,0x2A,0x00),
lcm_dcs_write_seq_static(ctx,0x2B,0x00),
lcm_dcs_write_seq_static(ctx,0x2C,0x00),
lcm_dcs_write_seq_static(ctx,0x2D,0x00),
lcm_dcs_write_seq_static(ctx,0x2E,0x00),
lcm_dcs_write_seq_static(ctx,0x2F,0x00),
lcm_dcs_write_seq_static(ctx,0x30,0x00),
lcm_dcs_write_seq_static(ctx,0x31,0x00),
lcm_dcs_write_seq_static(ctx,0x32,0x00),
lcm_dcs_write_seq_static(ctx,0x33,0x00),
lcm_dcs_write_seq_static(ctx,0x34,0x00),
lcm_dcs_write_seq_static(ctx,0x35,0x00),
lcm_dcs_write_seq_static(ctx,0x36,0x20),
lcm_dcs_write_seq_static(ctx,0x37,0x00),
lcm_dcs_write_seq_static(ctx,0x38,0x00),
lcm_dcs_write_seq_static(ctx,0x39,0x00),
lcm_dcs_write_seq_static(ctx,0x3A,0x00),
lcm_dcs_write_seq_static(ctx,0x3B,0x00),
lcm_dcs_write_seq_static(ctx,0x3C,0x00),
lcm_dcs_write_seq_static(ctx,0x3D,0x00),
lcm_dcs_write_seq_static(ctx,0x3E,0x00),
lcm_dcs_write_seq_static(ctx,0x3F,0x00),
lcm_dcs_write_seq_static(ctx,0x40,0x00),
lcm_dcs_write_seq_static(ctx,0x41,0x00),
lcm_dcs_write_seq_static(ctx,0x42,0x00),
lcm_dcs_write_seq_static(ctx,0x43,0x00),
lcm_dcs_write_seq_static(ctx,0x44,0x00),
lcm_dcs_write_seq_static(ctx,0x45,0x00),
lcm_dcs_write_seq_static(ctx,0x46,0x00),
lcm_dcs_write_seq_static(ctx,0x47,0x00),
lcm_dcs_write_seq_static(ctx,0x48,0x00),
lcm_dcs_write_seq_static(ctx,0x49,0x00),
lcm_dcs_write_seq_static(ctx,0x4A,0x00),
lcm_dcs_write_seq_static(ctx,0x4B,0x00),
lcm_dcs_write_seq_static(ctx,0x4C,0x00),
lcm_dcs_write_seq_static(ctx,0x4D,0x00),
lcm_dcs_write_seq_static(ctx,0x4E,0x00),
lcm_dcs_write_seq_static(ctx,0x4F,0x00),
lcm_dcs_write_seq_static(ctx,0x50,0x00),
lcm_dcs_write_seq_static(ctx,0x51,0x00),
lcm_dcs_write_seq_static(ctx,0x52,0x00),
lcm_dcs_write_seq_static(ctx,0x53,0x00),
lcm_dcs_write_seq_static(ctx,0x54,0x00),
lcm_dcs_write_seq_static(ctx,0x55,0x00),
lcm_dcs_write_seq_static(ctx,0x56,0x00),
lcm_dcs_write_seq_static(ctx,0x57,0x00),
lcm_dcs_write_seq_static(ctx,0x58,0x00),
lcm_dcs_write_seq_static(ctx,0x59,0x00),
lcm_dcs_write_seq_static(ctx,0x5A,0x00),
lcm_dcs_write_seq_static(ctx,0x5B,0x00),
lcm_dcs_write_seq_static(ctx,0x5C,0x00),
lcm_dcs_write_seq_static(ctx,0x5D,0x00),
lcm_dcs_write_seq_static(ctx,0x5E,0x00),
lcm_dcs_write_seq_static(ctx,0x5F,0x00),
lcm_dcs_write_seq_static(ctx,0x60,0x00),
lcm_dcs_write_seq_static(ctx,0x61,0x00),
lcm_dcs_write_seq_static(ctx,0x62,0x00),
lcm_dcs_write_seq_static(ctx,0x63,0x00),
lcm_dcs_write_seq_static(ctx,0x64,0x00),
lcm_dcs_write_seq_static(ctx,0x65,0x00),
lcm_dcs_write_seq_static(ctx,0x66,0x00),
lcm_dcs_write_seq_static(ctx,0x67,0x00),
lcm_dcs_write_seq_static(ctx,0x68,0x00),
lcm_dcs_write_seq_static(ctx,0x69,0x00),
lcm_dcs_write_seq_static(ctx,0x6A,0x00),
lcm_dcs_write_seq_static(ctx,0x6B,0x00),
lcm_dcs_write_seq_static(ctx,0x6C,0x00),
lcm_dcs_write_seq_static(ctx,0x6D,0x00),
lcm_dcs_write_seq_static(ctx,0x6E,0x00),
lcm_dcs_write_seq_static(ctx,0x6F,0x00),
lcm_dcs_write_seq_static(ctx,0x70,0x00),
lcm_dcs_write_seq_static(ctx,0x71,0x00),
lcm_dcs_write_seq_static(ctx,0x72,0x00),
lcm_dcs_write_seq_static(ctx,0x73,0x00),
lcm_dcs_write_seq_static(ctx,0x74,0x00),
lcm_dcs_write_seq_static(ctx,0x75,0x00),
lcm_dcs_write_seq_static(ctx,0x76,0x00),
lcm_dcs_write_seq_static(ctx,0x77,0x00),
lcm_dcs_write_seq_static(ctx,0x78,0x00),
lcm_dcs_write_seq_static(ctx,0x79,0x00),
lcm_dcs_write_seq_static(ctx,0x7A,0x00),
lcm_dcs_write_seq_static(ctx,0x7B,0x00),
lcm_dcs_write_seq_static(ctx,0x7C,0x00),
lcm_dcs_write_seq_static(ctx,0x7D,0x00),
lcm_dcs_write_seq_static(ctx,0x7E,0x00),
lcm_dcs_write_seq_static(ctx,0x7F,0x00),
lcm_dcs_write_seq_static(ctx,0x80,0x00),
lcm_dcs_write_seq_static(ctx,0x81,0x00),
lcm_dcs_write_seq_static(ctx,0x82,0x00),
lcm_dcs_write_seq_static(ctx,0x83,0x00),
lcm_dcs_write_seq_static(ctx,0x84,0x00),
lcm_dcs_write_seq_static(ctx,0x85,0x00),
lcm_dcs_write_seq_static(ctx,0x86,0x00),
lcm_dcs_write_seq_static(ctx,0x87,0x00),
lcm_dcs_write_seq_static(ctx,0x88,0x00),
lcm_dcs_write_seq_static(ctx,0x89,0x00),
lcm_dcs_write_seq_static(ctx,0x8A,0x00),
lcm_dcs_write_seq_static(ctx,0x8B,0x00),
lcm_dcs_write_seq_static(ctx,0x8C,0x00),
lcm_dcs_write_seq_static(ctx,0x8D,0x00),
lcm_dcs_write_seq_static(ctx,0x8E,0x00),
lcm_dcs_write_seq_static(ctx,0x8F,0x00),
lcm_dcs_write_seq_static(ctx,0x90,0x00),
lcm_dcs_write_seq_static(ctx,0x91,0x00),
lcm_dcs_write_seq_static(ctx,0x92,0x00),
lcm_dcs_write_seq_static(ctx,0x93,0x00),
lcm_dcs_write_seq_static(ctx,0x94,0x00),
lcm_dcs_write_seq_static(ctx,0x95,0x00),
lcm_dcs_write_seq_static(ctx,0x96,0x00),
lcm_dcs_write_seq_static(ctx,0x97,0x00),
lcm_dcs_write_seq_static(ctx,0x98,0x00),
lcm_dcs_write_seq_static(ctx,0x99,0x00),
lcm_dcs_write_seq_static(ctx,0x9A,0x00),
lcm_dcs_write_seq_static(ctx,0x9B,0x00),
lcm_dcs_write_seq_static(ctx,0x9C,0x00),
lcm_dcs_write_seq_static(ctx,0x9D,0x00),
lcm_dcs_write_seq_static(ctx,0x9E,0x00),
lcm_dcs_write_seq_static(ctx,0x9F,0x00),
lcm_dcs_write_seq_static(ctx,0xA0,0x00),
lcm_dcs_write_seq_static(ctx,0xA1,0x00),
lcm_dcs_write_seq_static(ctx,0xA2,0x00),
lcm_dcs_write_seq_static(ctx,0xA3,0x00),
lcm_dcs_write_seq_static(ctx,0xA4,0x00),
lcm_dcs_write_seq_static(ctx,0xA5,0x00),
lcm_dcs_write_seq_static(ctx,0xA6,0x00),
lcm_dcs_write_seq_static(ctx,0xA7,0x00),
lcm_dcs_write_seq_static(ctx,0xA8,0x00),
lcm_dcs_write_seq_static(ctx,0xA9,0x00),
lcm_dcs_write_seq_static(ctx,0xAA,0x00),
lcm_dcs_write_seq_static(ctx,0xAB,0x00),
lcm_dcs_write_seq_static(ctx,0xAC,0x00),
lcm_dcs_write_seq_static(ctx,0xAD,0x00),
lcm_dcs_write_seq_static(ctx,0xAE,0x00),
lcm_dcs_write_seq_static(ctx,0xAF,0xF0),
lcm_dcs_write_seq_static(ctx,0xB0,0x00),
lcm_dcs_write_seq_static(ctx,0xB1,0x00),
lcm_dcs_write_seq_static(ctx,0xB2,0x00),
lcm_dcs_write_seq_static(ctx,0xB3,0x00),
lcm_dcs_write_seq_static(ctx,0xB4,0x00),
lcm_dcs_write_seq_static(ctx,0xB5,0x00),
lcm_dcs_write_seq_static(ctx,0xB6,0x00),
lcm_dcs_write_seq_static(ctx,0xB7,0x08),
lcm_dcs_write_seq_static(ctx,0xB8,0x13),
lcm_dcs_write_seq_static(ctx,0xB9,0x00),
lcm_dcs_write_seq_static(ctx,0xBA,0x00),
lcm_dcs_write_seq_static(ctx,0xBB,0x00),
lcm_dcs_write_seq_static(ctx,0xBC,0x00),
lcm_dcs_write_seq_static(ctx,0xBD,0x00),
lcm_dcs_write_seq_static(ctx,0xBE,0x00),
lcm_dcs_write_seq_static(ctx,0xBF,0x00),
lcm_dcs_write_seq_static(ctx,0xC0,0x03),
lcm_dcs_write_seq_static(ctx,0xC1,0x00),
lcm_dcs_write_seq_static(ctx,0xC2,0x00),
lcm_dcs_write_seq_static(ctx,0xC3,0x00),
lcm_dcs_write_seq_static(ctx,0xC4,0x00),
lcm_dcs_write_seq_static(ctx,0xC5,0x00),
lcm_dcs_write_seq_static(ctx,0xC6,0x00),
lcm_dcs_write_seq_static(ctx,0xC7,0x00),
lcm_dcs_write_seq_static(ctx,0xC8,0x00),
lcm_dcs_write_seq_static(ctx,0xC9,0x00),
lcm_dcs_write_seq_static(ctx,0xCA,0x00),
lcm_dcs_write_seq_static(ctx,0xCB,0x0F),
lcm_dcs_write_seq_static(ctx,0xCC,0x00),
lcm_dcs_write_seq_static(ctx,0xCD,0x00),
lcm_dcs_write_seq_static(ctx,0xCE,0x00),
lcm_dcs_write_seq_static(ctx,0xCF,0x00),
lcm_dcs_write_seq_static(ctx,0xD0,0x00),
lcm_dcs_write_seq_static(ctx,0xD1,0x00),
lcm_dcs_write_seq_static(ctx,0xD2,0x00),
lcm_dcs_write_seq_static(ctx,0xD3,0x00),
lcm_dcs_write_seq_static(ctx,0xD4,0x00),
lcm_dcs_write_seq_static(ctx,0xD5,0x00),
lcm_dcs_write_seq_static(ctx,0xD6,0x00),
lcm_dcs_write_seq_static(ctx,0xD7,0x00),
lcm_dcs_write_seq_static(ctx,0xD8,0x00),
lcm_dcs_write_seq_static(ctx,0xD9,0x00),
lcm_dcs_write_seq_static(ctx,0xDA,0x00),
lcm_dcs_write_seq_static(ctx,0xDB,0x00),
lcm_dcs_write_seq_static(ctx,0xDC,0x00),
lcm_dcs_write_seq_static(ctx,0xDD,0x00),
lcm_dcs_write_seq_static(ctx,0xDE,0x00),
lcm_dcs_write_seq_static(ctx,0xDF,0x00),
lcm_dcs_write_seq_static(ctx,0xE0,0x00),
lcm_dcs_write_seq_static(ctx,0xE1,0x00),
lcm_dcs_write_seq_static(ctx,0xE2,0x00),
lcm_dcs_write_seq_static(ctx,0xE3,0x00),
lcm_dcs_write_seq_static(ctx,0xE4,0x00),
lcm_dcs_write_seq_static(ctx,0xE5,0x00),
lcm_dcs_write_seq_static(ctx,0xE6,0x00),
lcm_dcs_write_seq_static(ctx,0xE7,0x00),
lcm_dcs_write_seq_static(ctx,0xE8,0x00),
lcm_dcs_write_seq_static(ctx,0xE9,0x00),
lcm_dcs_write_seq_static(ctx,0xEA,0x00),
lcm_dcs_write_seq_static(ctx,0xEB,0x00),
lcm_dcs_write_seq_static(ctx,0xEC,0x00),
lcm_dcs_write_seq_static(ctx,0xED,0x00),
lcm_dcs_write_seq_static(ctx,0xEE,0x00),
lcm_dcs_write_seq_static(ctx,0xEF,0x00),
lcm_dcs_write_seq_static(ctx,0xF0,0x00),
lcm_dcs_write_seq_static(ctx,0xF1,0x00),
lcm_dcs_write_seq_static(ctx,0xF2,0x00),
lcm_dcs_write_seq_static(ctx,0xF3,0x00),
lcm_dcs_write_seq_static(ctx,0xF4,0x00),
lcm_dcs_write_seq_static(ctx,0xF5,0x00),
lcm_dcs_write_seq_static(ctx,0xF6,0x00),
lcm_dcs_write_seq_static(ctx,0xF7,0x00),
lcm_dcs_write_seq_static(ctx,0xF8,0x00),
lcm_dcs_write_seq_static(ctx,0xF9,0x00),
lcm_dcs_write_seq_static(ctx,0xFA,0x00),
lcm_dcs_write_seq_static(ctx,0xFF,0x2C),
lcm_dcs_write_seq_static(ctx,0xFB,0x01),
lcm_dcs_write_seq_static(ctx,0x00,0x04),
lcm_dcs_write_seq_static(ctx,0x01,0x04),
lcm_dcs_write_seq_static(ctx,0x02,0x04),
lcm_dcs_write_seq_static(ctx,0x03,0x13),
lcm_dcs_write_seq_static(ctx,0x04,0x13),
lcm_dcs_write_seq_static(ctx,0x05,0x13),
lcm_dcs_write_seq_static(ctx,0x06,0x00),
lcm_dcs_write_seq_static(ctx,0x07,0x00),
lcm_dcs_write_seq_static(ctx,0x08,0x00),
lcm_dcs_write_seq_static(ctx,0x09,0x00),
lcm_dcs_write_seq_static(ctx,0x0A,0x00),
lcm_dcs_write_seq_static(ctx,0x0B,0x00),
lcm_dcs_write_seq_static(ctx,0x0C,0x00),
lcm_dcs_write_seq_static(ctx,0x0D,0x01),
lcm_dcs_write_seq_static(ctx,0x0E,0x11),
lcm_dcs_write_seq_static(ctx,0x0F,0x00),
lcm_dcs_write_seq_static(ctx,0x10,0x01),
lcm_dcs_write_seq_static(ctx,0x11,0x01),
lcm_dcs_write_seq_static(ctx,0x12,0x00),
lcm_dcs_write_seq_static(ctx,0x13,0x01),
lcm_dcs_write_seq_static(ctx,0x14,0x01),
lcm_dcs_write_seq_static(ctx,0x15,0x00),
lcm_dcs_write_seq_static(ctx,0x16,0x01),
lcm_dcs_write_seq_static(ctx,0x17,0x48),
lcm_dcs_write_seq_static(ctx,0x18,0x48),
lcm_dcs_write_seq_static(ctx,0x19,0x48),
lcm_dcs_write_seq_static(ctx,0x1A,0x00),
lcm_dcs_write_seq_static(ctx,0x1B,0x0A),
lcm_dcs_write_seq_static(ctx,0x1C,0x80),
lcm_dcs_write_seq_static(ctx,0x1D,0x80),
lcm_dcs_write_seq_static(ctx,0x1E,0x01),
lcm_dcs_write_seq_static(ctx,0x1F,0x80),
lcm_dcs_write_seq_static(ctx,0x20,0x80),
lcm_dcs_write_seq_static(ctx,0x21,0x00),
lcm_dcs_write_seq_static(ctx,0x22,0x00),
lcm_dcs_write_seq_static(ctx,0x23,0x00),
lcm_dcs_write_seq_static(ctx,0x24,0x00),
lcm_dcs_write_seq_static(ctx,0x25,0x00),
lcm_dcs_write_seq_static(ctx,0x26,0x00),
lcm_dcs_write_seq_static(ctx,0x27,0x00),
lcm_dcs_write_seq_static(ctx,0x28,0x08),
lcm_dcs_write_seq_static(ctx,0x29,0x00),
lcm_dcs_write_seq_static(ctx,0x2A,0x00),
lcm_dcs_write_seq_static(ctx,0x2B,0x00),
lcm_dcs_write_seq_static(ctx,0x2C,0x00),
lcm_dcs_write_seq_static(ctx,0x2D,0xAF),
lcm_dcs_write_seq_static(ctx,0x2E,0x00),
lcm_dcs_write_seq_static(ctx,0x2F,0x10),
lcm_dcs_write_seq_static(ctx,0x30,0xF3),
lcm_dcs_write_seq_static(ctx,0x31,0x32),
lcm_dcs_write_seq_static(ctx,0x32,0x00),
lcm_dcs_write_seq_static(ctx,0x33,0xF3),
lcm_dcs_write_seq_static(ctx,0x34,0x00),
lcm_dcs_write_seq_static(ctx,0x35,0x15),
lcm_dcs_write_seq_static(ctx,0x36,0xFD),
lcm_dcs_write_seq_static(ctx,0x37,0x15),
lcm_dcs_write_seq_static(ctx,0x38,0x10),
lcm_dcs_write_seq_static(ctx,0x39,0x00),
lcm_dcs_write_seq_static(ctx,0x3A,0x02),
lcm_dcs_write_seq_static(ctx,0x3B,0x02),
lcm_dcs_write_seq_static(ctx,0x3C,0x00),
lcm_dcs_write_seq_static(ctx,0x3D,0x00),
lcm_dcs_write_seq_static(ctx,0x3E,0x00),
lcm_dcs_write_seq_static(ctx,0x3F,0x00),
lcm_dcs_write_seq_static(ctx,0x40,0x00),
lcm_dcs_write_seq_static(ctx,0x41,0x00),
lcm_dcs_write_seq_static(ctx,0x42,0x00),
lcm_dcs_write_seq_static(ctx,0x43,0x00),
lcm_dcs_write_seq_static(ctx,0x44,0x00),
lcm_dcs_write_seq_static(ctx,0x45,0x00),
lcm_dcs_write_seq_static(ctx,0x46,0x00),
lcm_dcs_write_seq_static(ctx,0x47,0x00),
lcm_dcs_write_seq_static(ctx,0x48,0x00),
lcm_dcs_write_seq_static(ctx,0x49,0x00),
lcm_dcs_write_seq_static(ctx,0x4A,0x00),
lcm_dcs_write_seq_static(ctx,0x4B,0x20),
lcm_dcs_write_seq_static(ctx,0x4C,0x05),
lcm_dcs_write_seq_static(ctx,0x4D,0x14),
lcm_dcs_write_seq_static(ctx,0x4E,0x04),
lcm_dcs_write_seq_static(ctx,0x4F,0x00),
lcm_dcs_write_seq_static(ctx,0x50,0x00),
lcm_dcs_write_seq_static(ctx,0x51,0x00),
lcm_dcs_write_seq_static(ctx,0x52,0x00),
lcm_dcs_write_seq_static(ctx,0x53,0x03),
lcm_dcs_write_seq_static(ctx,0x54,0x03),
lcm_dcs_write_seq_static(ctx,0x55,0x03),
lcm_dcs_write_seq_static(ctx,0x56,0x20),
lcm_dcs_write_seq_static(ctx,0x57,0x00),
lcm_dcs_write_seq_static(ctx,0x58,0x20),
lcm_dcs_write_seq_static(ctx,0x59,0x20),
lcm_dcs_write_seq_static(ctx,0x5A,0x00),
lcm_dcs_write_seq_static(ctx,0x5B,0x00),
lcm_dcs_write_seq_static(ctx,0x5C,0x00),
lcm_dcs_write_seq_static(ctx,0x5D,0x00),
lcm_dcs_write_seq_static(ctx,0x5E,0x00),
lcm_dcs_write_seq_static(ctx,0x5F,0x00),
lcm_dcs_write_seq_static(ctx,0x60,0x00),
lcm_dcs_write_seq_static(ctx,0x61,0x01),
lcm_dcs_write_seq_static(ctx,0x62,0x11),
lcm_dcs_write_seq_static(ctx,0x63,0x00),
lcm_dcs_write_seq_static(ctx,0x64,0x01),
lcm_dcs_write_seq_static(ctx,0x65,0x01),
lcm_dcs_write_seq_static(ctx,0x66,0x00),
lcm_dcs_write_seq_static(ctx,0x67,0x01),
lcm_dcs_write_seq_static(ctx,0x68,0x01),
lcm_dcs_write_seq_static(ctx,0x69,0x00),
lcm_dcs_write_seq_static(ctx,0x6A,0x01),
lcm_dcs_write_seq_static(ctx,0x6B,0x72),
lcm_dcs_write_seq_static(ctx,0x6C,0x72),
lcm_dcs_write_seq_static(ctx,0x6D,0x72),
lcm_dcs_write_seq_static(ctx,0x6E,0x00),
lcm_dcs_write_seq_static(ctx,0x6F,0x0A),
lcm_dcs_write_seq_static(ctx,0x70,0x80),
lcm_dcs_write_seq_static(ctx,0x71,0x80),
lcm_dcs_write_seq_static(ctx,0x72,0x01),
lcm_dcs_write_seq_static(ctx,0x73,0x80),
lcm_dcs_write_seq_static(ctx,0x74,0x80),
lcm_dcs_write_seq_static(ctx,0x75,0x00),
lcm_dcs_write_seq_static(ctx,0x76,0x00),
lcm_dcs_write_seq_static(ctx,0x77,0x00),
lcm_dcs_write_seq_static(ctx,0x78,0x00),
lcm_dcs_write_seq_static(ctx,0x79,0x00),
lcm_dcs_write_seq_static(ctx,0x7A,0x00),
lcm_dcs_write_seq_static(ctx,0x7B,0x00),
lcm_dcs_write_seq_static(ctx,0x7C,0x08),
lcm_dcs_write_seq_static(ctx,0x7D,0x00),
lcm_dcs_write_seq_static(ctx,0x7E,0x00),
lcm_dcs_write_seq_static(ctx,0x7F,0x00),
lcm_dcs_write_seq_static(ctx,0x80,0xAF),
lcm_dcs_write_seq_static(ctx,0x81,0x10),
lcm_dcs_write_seq_static(ctx,0x82,0xF3),
lcm_dcs_write_seq_static(ctx,0x83,0x32),
lcm_dcs_write_seq_static(ctx,0x84,0x00),
lcm_dcs_write_seq_static(ctx,0x85,0xF3),
lcm_dcs_write_seq_static(ctx,0x86,0x00),
lcm_dcs_write_seq_static(ctx,0x87,0x20),
lcm_dcs_write_seq_static(ctx,0x88,0xFD),
lcm_dcs_write_seq_static(ctx,0x89,0x20),
lcm_dcs_write_seq_static(ctx,0x8A,0x10),
lcm_dcs_write_seq_static(ctx,0x8B,0x00),
lcm_dcs_write_seq_static(ctx,0x8C,0x03),
lcm_dcs_write_seq_static(ctx,0x8D,0x03),
lcm_dcs_write_seq_static(ctx,0x8E,0x00),
lcm_dcs_write_seq_static(ctx,0x8F,0x00),
lcm_dcs_write_seq_static(ctx,0x90,0x00),
lcm_dcs_write_seq_static(ctx,0x91,0x00),
lcm_dcs_write_seq_static(ctx,0x92,0x00),
lcm_dcs_write_seq_static(ctx,0x93,0x00),
lcm_dcs_write_seq_static(ctx,0x94,0x00),
lcm_dcs_write_seq_static(ctx,0x95,0x00),
lcm_dcs_write_seq_static(ctx,0x96,0x00),
lcm_dcs_write_seq_static(ctx,0x97,0x00),
lcm_dcs_write_seq_static(ctx,0x98,0x00),
lcm_dcs_write_seq_static(ctx,0x99,0x00),
lcm_dcs_write_seq_static(ctx,0x9A,0x00),
lcm_dcs_write_seq_static(ctx,0x9B,0x20),
lcm_dcs_write_seq_static(ctx,0x9C,0x05),
lcm_dcs_write_seq_static(ctx,0x9D,0x1D),
lcm_dcs_write_seq_static(ctx,0x9E,0x03),
lcm_dcs_write_seq_static(ctx,0x9F,0x10),
lcm_dcs_write_seq_static(ctx,0xA0,0x10),
lcm_dcs_write_seq_static(ctx,0xA1,0x00),
lcm_dcs_write_seq_static(ctx,0xA2,0x00),
lcm_dcs_write_seq_static(ctx,0xA3,0x00),
lcm_dcs_write_seq_static(ctx,0xA4,0x00),
lcm_dcs_write_seq_static(ctx,0xA5,0x00),
lcm_dcs_write_seq_static(ctx,0xA6,0x00),
lcm_dcs_write_seq_static(ctx,0xA7,0x00),
lcm_dcs_write_seq_static(ctx,0xA8,0x00),
lcm_dcs_write_seq_static(ctx,0xA9,0x04),
lcm_dcs_write_seq_static(ctx,0xAA,0x04),
lcm_dcs_write_seq_static(ctx,0xAB,0x04),
lcm_dcs_write_seq_static(ctx,0xAC,0x1E),
lcm_dcs_write_seq_static(ctx,0xAD,0x1E),
lcm_dcs_write_seq_static(ctx,0xAE,0x1E),
lcm_dcs_write_seq_static(ctx,0xAF,0x00),
lcm_dcs_write_seq_static(ctx,0xB0,0x00),
lcm_dcs_write_seq_static(ctx,0xB1,0x00),
lcm_dcs_write_seq_static(ctx,0xB2,0x00),
lcm_dcs_write_seq_static(ctx,0xB3,0x00),
lcm_dcs_write_seq_static(ctx,0xB4,0x00),
lcm_dcs_write_seq_static(ctx,0xB5,0x00),
lcm_dcs_write_seq_static(ctx,0xB6,0x04),
lcm_dcs_write_seq_static(ctx,0xB7,0x68),
lcm_dcs_write_seq_static(ctx,0xB8,0x00),
lcm_dcs_write_seq_static(ctx,0xB9,0x01),
lcm_dcs_write_seq_static(ctx,0xBA,0x01),
lcm_dcs_write_seq_static(ctx,0xBB,0x00),
lcm_dcs_write_seq_static(ctx,0xBC,0x01),
lcm_dcs_write_seq_static(ctx,0xBD,0x01),
lcm_dcs_write_seq_static(ctx,0xBE,0x00),
lcm_dcs_write_seq_static(ctx,0xBF,0x01),
lcm_dcs_write_seq_static(ctx,0xC0,0x71),
lcm_dcs_write_seq_static(ctx,0xC1,0x71),
lcm_dcs_write_seq_static(ctx,0xC2,0x71),
lcm_dcs_write_seq_static(ctx,0xC3,0x00),
lcm_dcs_write_seq_static(ctx,0xC4,0x0A),
lcm_dcs_write_seq_static(ctx,0xC5,0x80),
lcm_dcs_write_seq_static(ctx,0xC6,0x80),
lcm_dcs_write_seq_static(ctx,0xC7,0x01),
lcm_dcs_write_seq_static(ctx,0xC8,0x80),
lcm_dcs_write_seq_static(ctx,0xC9,0x80),
lcm_dcs_write_seq_static(ctx,0xCA,0x00),
lcm_dcs_write_seq_static(ctx,0xCB,0x00),
lcm_dcs_write_seq_static(ctx,0xCC,0x00),
lcm_dcs_write_seq_static(ctx,0xCD,0x00),
lcm_dcs_write_seq_static(ctx,0xCE,0x00),
lcm_dcs_write_seq_static(ctx,0xCF,0x00),
lcm_dcs_write_seq_static(ctx,0xD0,0x00),
lcm_dcs_write_seq_static(ctx,0xD1,0x08),
lcm_dcs_write_seq_static(ctx,0xD2,0x00),
lcm_dcs_write_seq_static(ctx,0xD3,0x00),
lcm_dcs_write_seq_static(ctx,0xD4,0x00),
lcm_dcs_write_seq_static(ctx,0xD5,0xFA),
lcm_dcs_write_seq_static(ctx,0xD6,0x11),
lcm_dcs_write_seq_static(ctx,0xD7,0x84),
lcm_dcs_write_seq_static(ctx,0xD8,0x32),
lcm_dcs_write_seq_static(ctx,0xD9,0x01),
lcm_dcs_write_seq_static(ctx,0xDA,0x9A),
lcm_dcs_write_seq_static(ctx,0xDB,0x00),
lcm_dcs_write_seq_static(ctx,0xDC,0x29),
lcm_dcs_write_seq_static(ctx,0xDD,0xFD),
lcm_dcs_write_seq_static(ctx,0xDE,0x96),
lcm_dcs_write_seq_static(ctx,0xDF,0x10),
lcm_dcs_write_seq_static(ctx,0xE0,0x00),
lcm_dcs_write_seq_static(ctx,0xE1,0x04),
lcm_dcs_write_seq_static(ctx,0xE2,0x04),
lcm_dcs_write_seq_static(ctx,0xE3,0x00),
lcm_dcs_write_seq_static(ctx,0xE4,0x00),
lcm_dcs_write_seq_static(ctx,0xE5,0x00),
lcm_dcs_write_seq_static(ctx,0xE6,0x00),
lcm_dcs_write_seq_static(ctx,0xE7,0x00),
lcm_dcs_write_seq_static(ctx,0xE8,0x00),
lcm_dcs_write_seq_static(ctx,0xE9,0x00),
lcm_dcs_write_seq_static(ctx,0xEA,0x00),
lcm_dcs_write_seq_static(ctx,0xEB,0x00),
lcm_dcs_write_seq_static(ctx,0xEC,0x00),
lcm_dcs_write_seq_static(ctx,0xED,0x00),
lcm_dcs_write_seq_static(ctx,0xEE,0x00),
lcm_dcs_write_seq_static(ctx,0xEF,0x00),
lcm_dcs_write_seq_static(ctx,0xF0,0x20),
lcm_dcs_write_seq_static(ctx,0xF1,0x05),
lcm_dcs_write_seq_static(ctx,0xF2,0x1B),
lcm_dcs_write_seq_static(ctx,0xF3,0x08),
lcm_dcs_write_seq_static(ctx,0xF4,0x10),
lcm_dcs_write_seq_static(ctx,0xF5,0x00),
lcm_dcs_write_seq_static(ctx,0xF6,0x00),
lcm_dcs_write_seq_static(ctx,0xF7,0x00),
lcm_dcs_write_seq_static(ctx,0xF8,0x00),
lcm_dcs_write_seq_static(ctx,0xF9,0x00),
lcm_dcs_write_seq_static(ctx,0xFA,0x00),

lcm_dcs_write_seq_static(ctx,0xFF,0x10),
lcm_dcs_write_seq_static(ctx,0xFB,0x01),
lcm_dcs_write_seq_static(ctx,0xB0,0x00),
lcm_dcs_write_seq_static(ctx,0xC0,0x00),
lcm_dcs_write_seq_static(ctx,0xC1,0x89,0x28,0x00,0x08,0x00,0xAA,0x02,0x0E,0x00,0x2B,0x00,0x07,0x0D,0xB7,0x0C,0xB7),
lcm_dcs_write_seq_static(ctx,0xC2,0x1B,0xA0),
lcm_dcs_write_seq_static(ctx,0xFF,0x10),
lcm_dcs_write_seq_static(ctx,0x35,0x00),

lcm_dcs_write_seq_static(ctx, 0x11);
mdelay(120);
lcm_dcs_write_seq_static(ctx, 0x29);
mdelay(50);

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

	// lcm reset
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	// lcm bias
	ctx->bias_neg =
		devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	usleep_range(2000, 2001);

	// lcm bias
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

	// lcd reset H -> L -> L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10000, 10001);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(20);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	// end

	// lcm bias
	ctx->bias_pos =
		devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);
	usleep_range(2000, 2001);
	
	// lcm bias
	ctx->bias_neg =
		devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	usleep_range(2000, 2001);

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

#define HFP (30)
#define HSA (10)
#define HBP (40)
#define VFP (10)
#define VSA (5)
#define VBP (5)
#define VAC (2408)
#define HAC (1080)

#define PCLK_IN_KHZ \
    ((HAC+HFP+HSA+HBP)*(VAC+VFP+VSA+VBP)*(60)/1000)

static const struct drm_display_mode default_mode = {
	.clock = PCLK_IN_KHZ,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.physical_width_um = 68395,
	.physical_height_um = 152496,

	.pll_clk = 553,
	.data_rate = 553*2,

	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.ssc_enable = 0,

	.dsc_params = {
		.enable = 0,
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
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 1,
		.pll_clk = 553,
		.vfp_lp_dyn = 2400,
		.hfp = 30,
		.vfp = 10,
	},
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0xFF};

	bl_tb0[1] = level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

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

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
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

	connector->display_info.width_mm = 68;
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

#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
	strcpy(current_lcm_info.chip,"nt36672c,vdo,boe,yx");
	strcpy(current_lcm_info.vendor,"Novatek");
	sprintf(current_lcm_info.id,"0x%02x",0x80);
	strcpy(current_lcm_info.more,"2408*1080");
#endif

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
	    .compatible = "nt36672c,vdo,boe,yx",
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-boe-nt36672c-vdo-yx",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("shaohua deng <shaohua.deng@mediatek.com>");
MODULE_DESCRIPTION("JDI NT36672C VDO 120HZ AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");
