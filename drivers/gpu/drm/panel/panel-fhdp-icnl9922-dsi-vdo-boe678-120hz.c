/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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

//#include "../../../misc/mediatek/gate_ic/gate_i2c.h"
//#endif

//drv add by yubo for hardware_info 20240416 start
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/mediatek/prize/hardware_info/hardware_info.h"
extern struct hardware_info current_lcm_info;
#endif
//drv add by yubo for hardware_info 20240416 end

#include <linux/i2c-dev.h>
#include <linux/i2c.h>

/* prize added by KLJ, prize tp gesture function, 20240418-start */
//extern g_tp_gesture_flag;
/* prize added by KLJ, prize tp gesture function, 20240418-start */

#define HFP_SUPPORT 1

#if HFP_SUPPORT
static int current_fps = 60;
#endif

#define BYPASSI2C

#ifndef BYPASSI2C
/* i2c control start */
#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"
static struct i2c_client *_lcm_i2c_client;
static int _lcm_i2c_probe(struct i2c_client *client,const struct i2c_device_id *id);
static int _lcm_i2c_remove(struct i2c_client *client);

struct _lcm_i2c_dev {
	struct i2c_client *client;
};

static const struct of_device_id _lcm_i2c_of_match[] = {
	{
	    .compatible = "mediatek,I2C_LCD_BIAS",
	},
	{},
};

static const struct i2c_device_id _lcm_i2c_id[] = { { LCM_I2C_ID_NAME, 0 },
						    {} };

static struct i2c_driver _lcm_i2c_driver = {
	.id_table = _lcm_i2c_id,
	.probe = _lcm_i2c_probe,
	.remove = _lcm_i2c_remove,
	/* .detect		   = _lcm_i2c_detect, */
	.driver = {
		.owner = THIS_MODULE,
		.name = LCM_I2C_ID_NAME,
		.of_match_table = _lcm_i2c_of_match,
	},
};

static int _lcm_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	printk("[LCM][I2C] %s\n", __func__);
	printk("[LCM][I2C] NT: info==>name=%s addr=0x%x\n", client->name,client->addr);
	_lcm_i2c_client = client;
	return 0;
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
	printk("[LCM][I2C] %s\n", __func__);
	_lcm_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		printk("ERROR!! _lcm_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_info("[LCM][ERROR] _lcm_i2c write data fail !!\n");

	return ret;
}

// static int __init _lcm_i2c_init(void)
// {
	// printk("[LCM][I2C] %s\n", __func__);
	// i2c_add_driver(&_lcm_i2c_driver);
	// printk("[LCM][I2C] %s success\n", __func__);
	// return 0;
// }

// static void __exit _lcm_i2c_exit(void)
// {
	// printk("[LCM][I2C] %s\n", __func__);
	// i2c_del_driver(&_lcm_i2c_driver);
// }

// module_init(_lcm_i2c_init);
// module_exit(_lcm_i2c_exit);

#endif

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *ldo18_gpio;
	struct gpio_desc *bias_pos, *bias_neg;

	bool prepared;
	bool enabled;

	int error;
};


#define lcm_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define lcm_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

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
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
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
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif
/*
	lcm_dcs_write_seq_static(ctx,0XB9,0XFF,0X83,0X99);
	lcm_dcs_write_seq_static(ctx,0XBA,0X63,0X23);
	lcm_dcs_write_seq_static(ctx,0XB1,0X00,0X04,0X71,0X91,0X01,0X32,0X33,0X11,0X11,0X4D,0X57,0X06);
	lcm_dcs_write_seq_static(ctx,0XB2,0X00,0X80,0X80,0XCC,0X05,0X07,0X5A,0X11,0X00,0X00,0X10);
	lcm_dcs_write_seq_static(ctx,0XB4,0X00,0XFF,0X02,0XA7,0X02,0XA7,0X02,0XA7,0X02,0X00,0X03,0X05,0X00,0X2D,0X03,0X0E,0X0A,0X21,0X03,0X02,0X00,0X0B,0XA5,0X87,0X02,0XA7,0X02,0XA7,0X02,0XA7,0X02,0X00,0X03,0X05,0X00,0X2D,0X03,0X0E,0X0A,0X02,0X00,0X0B,0XA5,0X01);
	lcm_dcs_write_seq_static(ctx,0XD3,0X00,0X0C,0X03,0X03,0X00,0X00,0X14,0X04,0X32,0X10,0X09,0X00,0X09,0X32,0X10,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X11,0X00,0X02,0X02,0X03,0X00,0X00,0X00,0X0A,0X40);
	lcm_dcs_write_seq_static(ctx,0XD5,0X18,0X18,0X18,0X18,0X03,0X02,0X01,0X00,0X18,0X18,0X18,0X18,0X18,0X18,0X19,0X19,0X21,0X20,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X2F,0X2F,0X30,0X30,0X31,0X31);
	lcm_dcs_write_seq_static(ctx,0XD6,0X18,0X18,0X18,0X18,0X00,0X01,0X02,0X03,0X18,0X18,0X40,0X40,0X19,0X19,0X18,0X18,0X20,0X21,0X40,0X40,0X18,0X18,0X18,0X18,0X18,0X18,0X2F,0X2F,0X30,0X30,0X31,0X31);
	lcm_dcs_write_seq_static(ctx,0XD8,0XAF,0XAA,0XEA,0XAA,0XAF,0XAA,0XEA,0XAA,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00);
	lcm_dcs_write_seq_static(ctx,0XBD,0X01);
	lcm_dcs_write_seq_static(ctx,0XD8,0XFF,0XEF,0XEA,0XBF,0XFF,0XEF,0XEA,0XBF,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00);
	lcm_dcs_write_seq_static(ctx,0XBD,0X02);
	lcm_dcs_write_seq_static(ctx,0XD8,0XFF,0XEF,0XEA,0XBF,0XFF,0XEF,0XEA,0XBF);
	lcm_dcs_write_seq_static(ctx,0XBD,0X00);
	lcm_dcs_write_seq_static(ctx,0XE0,0X01,0X20,0X2B,0X26,0X53,0X5D,0X6D,0X6A,0X72,0X7C,0X84,0X8A,0X8F,0X99,0XA1,0XA5,0XAA,0XB5,0XBB,0XC1,0XB6,0XC4,0XC7,0X66,0X61,0X6B,0X73,0X01,0X11,0X21,0X20,0X53,0X5D,0X6D,0X6A,0X72,0X7C,0X84,0X8A,0X8F,0X99,0XA1,0XA5,0XAA,0XB3,0XB2,0XC1,0XB6,0XC4,0XC7,0X66,0X61,0X6B,0X73);
	lcm_dcs_write_seq_static(ctx,0XB6,0X81,0X81);
	lcm_dcs_write_seq_static(ctx,0XD2,0X66);
	lcm_dcs_write_seq_static(ctx,0XCC,0X08);
*/
static void lcm_panel_init(struct lcm *ctx)
{
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
	//gpiod_set_value(ctx->reset_gpio, 0);
	//udelay(15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(20);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(20);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(150);//ili9882q at least 10ms
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	
	//PASSWORD
	lcm_dcs_write_seq_static(ctx,0xF0,0x99,0x22,0x0C);
	lcm_dcs_write_seq_static(ctx,0x70,0xC1,0x12,0x00,0x05,0x00,0x5A,0x00,0x6F,0x00,0x77,0x00,0x31,0x02,0x00,0x20);
	lcm_dcs_write_seq_static(ctx,0x71,0x11,0x00,0x00,0x89,0x30,0x80,0x09,0x9C,0x04,0x38,0x00,0x14,0x02,0x1C,0x02,0x1C,0x02,0x00,0x02,0x25,0x00,0x20,0x01,0xD5,0x00,0x07,0x00,0x0D,0x05,0x7A,0x05,0x16);
	lcm_dcs_write_seq_static(ctx,0x72,0x18,0x00,0x10,0xF0,0x03,0x0C,0x20,0x00,0x06,0x0B,0x0B,0x33,0x0E,0x1C,0x2A,0x38,0x46,0x54,0x62,0x69,0x70,0x77,0x79,0x7B,0x7D,0x7E,0x01,0x02,0x01,0x00,0x09,0x40);
	lcm_dcs_write_seq_static(ctx,0x73,0x09,0xBE,0x19,0xFC,0x19,0xFA,0x19,0xF8,0x1A,0x38,0x1A,0x78,0x1A,0xB6,0x2A,0xF6,0x2B,0x34,0x2B,0x74,0x3B,0x74,0x6B,0xF4,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xC7,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x10,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x10,0x34,0x00,0x01,0xFF,0xFF,0x00,0xF0,0xF0,0x00);
	lcm_dcs_write_seq_static(ctx,0x80,0xFF,0xF7,0xEB,0xE2,0xDA,0xD3,0xCD,0xC8,0xC3,0xB3,0xA7,0x9D,0x95,0x8D,0x87,0x7C,0x72,0x6A,0x62,0x62,0x5A,0x52,0x49,0x40,0x3B,0x35,0x2E,0x26,0x1C,0x12,0x0F,0x0D);
	lcm_dcs_write_seq_static(ctx,0x81,0xFF,0xF7,0xEB,0xE2,0xDA,0xD3,0xCD,0xC8,0xC3,0xB3,0xA7,0x9D,0x95,0x8D,0x87,0x7C,0x72,0x6A,0x62,0x62,0x5A,0x52,0x49,0x40,0x3B,0x35,0x2E,0x26,0x1C,0x12,0x0F,0x0D);
	lcm_dcs_write_seq_static(ctx,0x82,0xFF,0xF7,0xEB,0xE2,0xDA,0xD3,0xCD,0xC8,0xC3,0xB3,0xA7,0x9D,0x95,0x8D,0x87,0x7C,0x72,0x6A,0x62,0x62,0x5A,0x52,0x49,0x40,0x3B,0x35,0x2E,0x26,0x1C,0x12,0x0F,0x0D);
	lcm_dcs_write_seq_static(ctx,0x83,0x09,0x0B,0x09,0x07,0x05,0x03,0x02,0x0B,0x09,0x07,0x05,0x03,0x02,0x0B,0x09,0x07,0x05,0x03,0x02,0x12,0x0E,0x0A,0x06,0x02,0x00,0x12,0x0E,0x0A,0x06,0x02,0x00,0x12);
	lcm_dcs_write_seq_static(ctx,0x84,0x0E,0x0A,0x06,0x02,0x00,0x2F,0xBD,0xFF,0x7B,0xFD,0x0B,0xDA,0xDB,0xFE,0x32,0xFB,0xDF,0xF7,0xBF,0xD0,0xBD,0xAD,0xBF,0xE3,0x2F,0xBD,0xFF,0x7B,0xFD,0x0B,0xDA,0xDB);
	lcm_dcs_write_seq_static(ctx,0x85,0xFE,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xB2,0x0D,0x06,0x05,0x04,0xF2,0x22,0x03,0x00,0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x61,0x5B,0x00,0x00,0x00,0x00,0x00,0x00,0x55,0x55,0x05,0x05,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xB3,0x31,0x0B,0x01,0x0B,0x81,0x61,0x00,0x00,0x5B,0x00,0x00,0x00,0x00,0x00,0x02,0xFF,0xBC,0x22,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF);
	lcm_dcs_write_seq_static(ctx,0xB4,0x30,0x04,0x01,0x05,0x81,0x02,0x00,0x00,0x47,0x00,0x00,0x00,0x00,0x00,0x02,0xFF,0xBC,0x22,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF);
	lcm_dcs_write_seq_static(ctx,0xB5,0x00,0x0B,0x06,0x0D,0x10,0x26,0x34,0x91,0xA2,0x33,0x44,0x00,0x26,0x00,0xBF,0x3C,0x02,0x08,0x20,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x5C,0x35);
	lcm_dcs_write_seq_static(ctx,0xB6,0x1E,0x1D,0x1C,0x82,0x0D,0x0C,0x0F,0x0E,0x82,0x3A,0x3A,0x3A,0x3A,0x82,0xC0,0x82,0x00,0x00,0x00,0x28,0x05,0x04,0x01,0x01,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x3C,0x00);
	lcm_dcs_write_seq_static(ctx,0xB7,0x1E,0x1D,0x1C,0x82,0x0D,0x0C,0x0F,0x0E,0x82,0x3A,0x3A,0x3A,0x3A,0x82,0xC0,0x82,0x00,0x00,0x00,0x28,0x05,0x04,0x01,0x01,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x3C,0x00);
	lcm_dcs_write_seq_static(ctx,0xB8,0x03,0x01,0x01,0x82,0x00,0x80,0x00,0x00,0x00,0x00,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x9D,0x00,0x00,0x00,0x12,0x1D,0x39,0x44,0x5E,0x54,0x61,0x6B,0x79,0x83,0x92,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xB9,0x12,0x33,0x21,0x12,0x33,0x21,0x12,0x33,0x21,0x12,0x33,0x21,0x12,0x33,0x21,0x12,0x33,0x21,0x12,0x33,0x21,0x12,0x33,0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02);
	lcm_dcs_write_seq_static(ctx,0xBA,0x01,0xFE,0xFF,0xBF,0xEE,0xFF,0xFF,0xFE,0xFF,0xBF,0xEE,0xFF,0xFF,0xFE,0x00,0x80,0x2E,0x00,0x00,0xFE,0x00,0x80,0x2E,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xBB,0x01,0x02,0x03,0x0A,0x04,0x13,0x14,0x52,0x16,0x5C,0x00,0x15,0x16,0x00);
	lcm_dcs_write_seq_static(ctx,0xBC,0x00,0x00,0x00,0x00,0x04,0x00,0xFF,0xF8,0x0B,0x11,0x50,0x5E,0x55,0x99,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xBD,0xA1,0xA2,0x52,0x2E,0x00,0x8F,0x0D,0x09,0xC1,0x04,0x01,0xAE,0x14);
	lcm_dcs_write_seq_static(ctx,0xBE,0x28,0x1E,0x0B,0xAA,0x43,0x35,0x33,0x32,0x1E,0x00,0x00,0x3A);
	lcm_dcs_write_seq_static(ctx,0xC0,0x40,0x93,0xFF,0xFF,0xFF,0x3F,0xFF,0x00,0xFF,0x00,0xCC,0x04,0x12,0x35,0x67,0x89,0xA0,0xFF,0xFF,0xF0,0x0B,0xEB);
	lcm_dcs_write_seq_static(ctx,0xC1,0x00,0x00,0x20,0x26,0x26,0x04,0x08,0x10,0x04,0x9C,0x19,0x22,0x50,0x01,0x11,0x07,0x63,0x08,0xA0,0x00,0x93);
	lcm_dcs_write_seq_static(ctx,0xC2,0x00);
	lcm_dcs_write_seq_static(ctx,0xC3,0x00,0x00,0x00,0x00,0x00,0x00,0x15,0x28,0x23,0x16,0x16,0x16,0x00,0xFF,0x40,0x40,0x2A,0x2A,0x2A,0x2A,0x2A,0x2A,0x2A,0x2A,0x00,0x00,0x18,0x00,0x00,0x10,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xC4,0x0C,0x93,0xA8,0x28,0x00,0x3C,0x02,0x00,0x00,0x0A,0x26,0x48,0x91,0xB3,0x75,0x00,0xF0,0xEF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xC5,0x02,0x4F,0xF0,0xA8,0x64,0x04,0x02,0x02,0x19,0x02,0x10,0x4F,0x05,0x06,0x00,0x20,0x0D,0x0A,0x06,0x12,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xC6,0x65,0x08,0x18,0x48,0x48,0x20,0x3F,0x03,0x16,0x16,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xC8,0x06,0x09);
	lcm_dcs_write_seq_static(ctx,0xC9,0x62,0x62,0x5C,0x5C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xD0,0x0C,0x23,0x18,0xFF,0xFF,0x00,0x80,0x0C,0xFF,0x0F,0x40);
	lcm_dcs_write_seq_static(ctx,0x9A,0x11,0x7A,0x00,0x00,0xFF,0x00,0x0A,0x00,0x17,0x00,0x22);
	lcm_dcs_write_seq_static(ctx,0x99,0x91,0xB5,0x00,0x3F,0x00,0x7A,0x00,0x30,0x22,0x01);
	lcm_dcs_write_seq_static(ctx,0xE0,0x0C,0x00,0xB0,0x10,0x00,0x15,0x7C);
	lcm_dcs_write_seq_static(ctx,0xF0,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x35,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x51,0x0F,0xF0);
	lcm_dcs_write_seq_static(ctx,0x53,0x2C,0x00);
	// SLEEP OUT + DISPLAY ON

	lcm_dcs_write_seq_static(ctx,0x11,0x00);
	mdelay(120);
	lcm_dcs_write_seq_static(ctx,0x29,0x00);
	mdelay(10);
	lcm_dcs_write_seq_static(ctx,0xAC,0x05 ,0x00);

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

#if defined(TPS65132_BIAS_SUPPORT)
extern int tps65132_write_byte(unsigned char cmd, unsigned char writeData);
#endif
static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	if (!ctx->prepared)
		return 0;
	lcm_dcs_write_seq_static(ctx, 0xac, 0x0a, 0x00);

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(10);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(120);

	//modified by KLJ For bias datasheet power off to 5.2v 20240401 start
#if defined(TPS65132_BIAS_SUPPORT)
	tps65132_write_byte(0x0, 0x0E);
	tps65132_write_byte(0x1, 0x0E);
#endif
	msleep(10);
	//modified by KLJ For bias datasheet power off to 5.2v 20240401 end

	ctx->error = 0;
	ctx->prepared = false;
/* prize added by KLJ, prize tp gesture function, 20240418-start */
	//if (g_tp_gesture_flag != 1) {
	/*ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);*/


		pr_info("%s gesture wakeup is disable\n", __func__);
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	mdelay(10);//modified by KLJ For FAE suggestion delay 10ms;

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

   // }else{
	//	pr_info("%s gesture wakeup is enable\n", __func__);
	//}
/* prize added by KLJ, prize tp gesture function, 20240418-start */

	//modified by KLJ For FAE suggestion  after suspend lcm rst keep high start
	//gpiod_set_value(ctx->reset_gpio, 1);
	//devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	//modified by KLJ For FAE suggestion  after suspend lcm rst keep high end

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

//prize-add gpio ldo1.8v-pengzhipeng-20220514-start
	ctx->ldo18_gpio = devm_gpiod_get(ctx->dev, "ldo18", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->ldo18_gpio)) {
		dev_info(ctx->dev, "cannot get ldo18-gpios %ld\n",
			PTR_ERR(ctx->ldo18_gpio));
		return PTR_ERR(ctx->ldo18_gpio);
	}
	gpiod_set_value(ctx->ldo18_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->ldo18_gpio);
	udelay(1000);
//prize-add gpio ldo1.8v-pengzhipeng-20220514-end
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	udelay(2000);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	udelay(2000);

#ifndef BYPASSI2C
	_lcm_i2c_write_bytes(0x0, 0x12);
	_lcm_i2c_write_bytes(0x1, 0x12);
#endif

#if defined(TPS65132_BIAS_SUPPORT)
	tps65132_write_byte(0x0, 0x12);
	tps65132_write_byte(0x1, 0x12);
#endif

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
/*
	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 5;
	params->dsi.vertical_frontporch = 9;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 20; //50--40
	params->dsi.horizontal_backporch = 20;
	params->dsi.horizontal_frontporch = 80;//  60-->75
*/
/*vdo time config*/
#define FRAME_WIDTH                 1080
#define FRAME_HEIGHT                2460

#define HAC (1080)
#define VAC (2460)
/*120HZ D:1000*/
#define HFP (16)
#define HSA (4)
#define HBP (8)
#define VFP_60 (2572)
#define VFP_90 (884)
#define VFP_120 (32)
#define VSA (4)
#define VBP (32)
#define DATA_RATE					852

#define PHYSICAL_WIDTH              69227 //70200
#define PHYSICAL_HEIGHT             157684 //152100

/* DSC RELATED */
#define DSC_ENABLE                  1
#define DSC_VER                     17
#define DSC_SLICE_MODE              1
#define DSC_RGB_SWAP                0
#define DSC_DSC_CFG                 34
#define DSC_RCT_ON                  1
#define DSC_BIT_PER_CHANNEL         8
#define DSC_DSC_LINE_BUF_DEPTH      9
#define DSC_BP_ENABLE               1
#define DSC_BIT_PER_PIXEL           128

#define DSC_SLICE_HEIGHT            20
#define DSC_SLICE_WIDTH             540
#define DSC_CHUNK_SIZE              540
#define DSC_XMIT_DELAY              512
#define DSC_DEC_DELAY               549
#define DSC_SCALE_VALUE             32
#define DSC_INCREMENT_INTERVAL      469
#define DSC_DECREMENT_INTERVAL      7
#define DSC_LINE_BPG_OFFSET         13
#define DSC_NFL_BPG_OFFSET          1402
#define DSC_SLICE_BPG_OFFSET        1302
#define DSC_INITIAL_OFFSET          6144
#define DSC_FINAL_OFFSET            4336
#define DSC_FLATNESS_MINQP          3
#define DSC_FLATNESS_MAXQP          12
#define DSC_RC_MODEL_SIZE           8192
#define DSC_RC_EDGE_FACTOR          6
#define DSC_RC_QUANT_INCR_LIMIT0    11
#define DSC_RC_QUANT_INCR_LIMIT1    11
#define DSC_RC_TGT_OFFSET_HI        3
#define DSC_RC_TGT_OFFSET_LO        3

#define PCLK_IN_KHZ_60HZ \
    ((HAC+HFP+HSA+HBP)*(VAC+VFP_60+VSA+VBP)*(60)/1000)
#define PCLK_IN_KHZ_90HZ \
    ((HAC+HFP+HSA+HBP)*(VAC+VFP_90+VSA+VBP)*(90)/1000)
#define PCLK_IN_KHZ_120HZ \
    ((HAC+HFP+HSA+HBP)*(VAC+VFP_120+VSA+VBP)*(120)/1000)

static const struct drm_display_mode default_mode = {
	.clock = PCLK_IN_KHZ_60HZ,//PCLK_IN_KHZ,  //339264
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
	.clock = PCLK_IN_KHZ_90HZ,//PCLK2_IN_KHZ,
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
	.clock = PCLK_IN_KHZ_120HZ,//PCLK3_IN_KHZ,
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
static struct mtk_panel_params ext_params_120 = {
	//.vfp_low_power = 2540,//60hz
	.pll_clk = DATA_RATE/2,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.data_rate = DATA_RATE,
	//.is_cphy = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = DATA_RATE,
		.hfp = HFP,
		.vfp = VFP_120,
	},

	//modified by KLJ for LP rate start
	.phy_timcon = {
		.lpx = 8,
	},
	//modified by KLJ for LP rate end
};

static struct mtk_panel_params ext_params_90 = {
	//.vfp_low_power = 2540,//60hz
	.pll_clk = DATA_RATE/2,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.data_rate = DATA_RATE,
	//.is_cphy = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = DATA_RATE,
		.hfp = HFP,
		.vfp = VFP_90,
	},

	//modified by KLJ for LP rate start
	.phy_timcon = {
		.lpx = 8,
	},
	//modified by KLJ for LP rate end
};

static struct mtk_panel_params ext_params_60 = {
	//.vfp_low_power = 2540,//60hz
	.pll_clk = DATA_RATE/2,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.data_rate = DATA_RATE,
	//.is_cphy = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = DATA_RATE,
		.hfp = HFP,
		.vfp = VFP_60,
	},
	//modified by KLJ for LP rate start
	.phy_timcon = {
		.lpx = 8,
	},
	//modified by KLJ for LP rate end
};

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
		ext->params = &ext_params_60;
#if HFP_SUPPORT
		current_fps = 60;
#endif
	} else if (drm_mode_vrefresh(m)== 90) {
printk("drm_mode_vrefresh  90hz\n");	
		ext->params = &ext_params_90;
#if HFP_SUPPORT
		current_fps = 90;
#endif
	} else if (drm_mode_vrefresh(m) == 120) {
printk("drm_mode_vrefresh  120hz\n");	
		ext->params = &ext_params_120;
#if HFP_SUPPORT
		current_fps = 120;
#endif
	} else
		ret = 1;

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x30, 0x00, 0x00};//modified by KLJ for lcd ata test
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0xDA, data, 3);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	pr_info("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
	    data[1] == id[1] &&
	    data[2] == id[2])
		return 1;

	pr_info("ATA expect data is %x %x %x\n", id[0], id[1], id[2]);

	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	char bl_tb0[] = {0x51, 0xFF};

	bl_tb0[1] = level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static int lcm_get_virtual_heigh(void)
{
	return VAC;
}

static int lcm_get_virtual_width(void)
{
	return HAC;
}


static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.ata_check = panel_ata_check,
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
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

	connector->display_info.width_mm = 69;
	connector->display_info.height_mm = 157;

	return 3;
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
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	pr_info("%s+\n", __func__);

#ifndef BYPASSI2C
	_lcm_i2c_init();
#endif

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
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			 | MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(dev, "%s: cannot get bias-pos 0 %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(dev, "%s: cannot get bias-neg 1 %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
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
	ret = mtk_panel_ext_create(dev, &ext_params_60, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	pr_info("%s-\n", __func__);

//drv add by yubo for hardware_info 20240416 start
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
    strcpy(current_lcm_info.chip,"ICNL9922");
    strcpy(current_lcm_info.vendor,"CHIPONE");
    sprintf(current_lcm_info.id,"0x%02x",0x01);
    strcpy(current_lcm_info.more,"1080*2460");
#endif
//drv add by yubo for hardware_info 20240416 end

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif
	pr_info("%s+\n", __func__);
	//i2c_del_driver(&_lcm_i2c_driver);
	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "hdplus,icnl9922c,120hz", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-fhdp-icnl9922-dsi-vdo-boe678-120hz",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("truly ili9882q VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
