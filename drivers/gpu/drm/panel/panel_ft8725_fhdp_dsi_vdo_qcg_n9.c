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
//#include "../mediatek/mtk_panel_ext.h"
//#include "../mediatek/mtk_log.h"
//#include "../mediatek/mtk_drm_graphics_base.h"
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif
//prize add by lvyuanchuan for lcd hardware info 20220331 start
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/mediatek/prize/hardware_info/hardware_info.h"
extern struct hardware_info current_lcm_info;
#endif
//prize add by lvyuanchuan for lcd hardware info 20220331 end

#include "../../../misc/mediatek/gate_ic/gate_i2c.h"

#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#define TPS65132_BIAS_SUPPORT
//TO DO: You have to do that remove macro BYPASSI2C and solve build error
//otherwise voltage will be unstable
#define BYPASSI2C

#define HFP_SUPPORT 1

#if HFP_SUPPORT
static int current_fps = 60;
#endif

#ifndef BYPASSI2C
/* i2c control start */
#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"
static struct i2c_client *_lcm_i2c_client;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id);
static int _lcm_i2c_remove(struct i2c_client *client);

/*****************************************************************************
 * Data Structure
 *****************************************************************************/
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

/*****************************************************************************
 * Function
 *****************************************************************************/

#ifdef VENDOR_EDIT
// shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
extern void lcd_queue_load_tp_fw(void);
#endif /*VENDOR_EDIT*/

static int _lcm_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	pr_info("[LCM][I2C] %s\n", __func__);
	pr_info("[LCM][I2C] NT: info==>name=%s addr=0x%x\n", client->name,
		 client->addr);
	_lcm_i2c_client = client;
	return 0;
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
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
		pr_debug("ERROR!! _lcm_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_info("[LCM][ERROR] _lcm_i2c write data fail !!\n");

	return ret;
}

/*
 * module load/unload record keeping
 */
static int __init _lcm_i2c_init(void)
{
	pr_info("[LCM][I2C] %s\n", __func__);
	i2c_add_driver(&_lcm_i2c_driver);
	pr_debug("[LCM][I2C] %s success\n", __func__);
	return 0;
}

static void __exit _lcm_i2c_exit(void)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	i2c_del_driver(&_lcm_i2c_driver);
}

//module_init(_lcm_i2c_init);
//module_exit(_lcm_i2c_exit);
/***********************************/
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

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
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
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n",
		ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
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
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
	//LCD reset
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(10);

	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(10);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(10);

	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(60);

	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xFF,0x87,0x25,0x01);
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xFF,0x87,0x25);

	//lcm_dcs_write_seq_static(ctx,0x00,0x00);
	//lcm_dcs_write_seq_static(ctx,0x2A,0x00,0x00,0x04,0x37);
	//lcm_dcs_write_seq_static(ctx,0x00,0x00);
	//lcm_dcs_write_seq_static(ctx,0x2B,0x00,0x00,0x09,0x5F);

	lcm_dcs_write_seq_static(ctx,0x00,0xA3);
	lcm_dcs_write_seq_static(ctx,0xB3,0x09,0x60,0x00,0x18); //1080x2400

	//==============================================
	//TCON		
	lcm_dcs_write_seq_static(ctx,0x00, 0x80);		
	lcm_dcs_write_seq_static(ctx,0xC0, 0x00 ,0x4B ,0x00 ,0x1E ,0x00 ,0x14);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0x90);		
	lcm_dcs_write_seq_static(ctx,0xC0, 0x00 ,0x4B ,0x00 ,0x1E ,0x00 ,0x14);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0xA0);		
	lcm_dcs_write_seq_static(ctx,0xC0, 0x00 ,0x6E ,0x00 ,0x1E ,0x00 ,0x14);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0xB0);		
	lcm_dcs_write_seq_static(ctx,0xC0, 0x00 ,0xCF ,0x00 ,0x1E ,0x14);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0xC1);		
	lcm_dcs_write_seq_static(ctx,0xC0, 0x00 ,0x94 ,0x00 ,0x81 ,0x00 ,0x63 ,0x00 ,0xC1);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0x70);		
	lcm_dcs_write_seq_static(ctx,0xC0, 0x00 ,0xB1 ,0x00 ,0x1E ,0x00 ,0x14);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0xA3);		
	lcm_dcs_write_seq_static(ctx,0xC1, 0x00, 0x5E, 0x00 ,0x3C ,0x00 ,0x02);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0xB7);		
	lcm_dcs_write_seq_static(ctx,0xC1, 0x00 ,0x4E);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0x7B);		
	lcm_dcs_write_seq_static(ctx,0xCE, 0xFF ,0xFF);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0x80);		
	lcm_dcs_write_seq_static(ctx,0xCE, 0x01, 0x81 ,0xFF ,0xFF ,0x00 ,0xBE ,0x00 ,0xC8 ,0x00 ,0xC4 ,0x00 ,0xC4 ,0x00 ,0x8C ,0x00,0xCC);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0x90);		
	lcm_dcs_write_seq_static(ctx,0xCE, 0x00 ,0xB7 ,0x10 ,0xAE ,0x00 ,0xB7 ,0x80 ,0xFF ,0xFF ,0x00 ,0x05 ,0xDC ,0x10 ,0x1B ,0x0F);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0xA0);		
	lcm_dcs_write_seq_static(ctx,0xCE, 0x00 ,0x00 ,0x00);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0xB0);		
	lcm_dcs_write_seq_static(ctx,0xCE, 0x22 ,0x00 ,0x00);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0xD1);		
	lcm_dcs_write_seq_static(ctx,0xCE, 0x00 ,0x00 ,0x01 ,0x00 ,0x00 ,0x00 ,0x00);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0xE1);		
	lcm_dcs_write_seq_static(ctx,0xCE, 0x0A ,0x02 ,0xFB ,0x02 ,0xFB ,0x02 ,0xFB ,0x00 ,0x00 ,0x00 ,0x00);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0xF1);		
	lcm_dcs_write_seq_static(ctx,0xCE, 0x27 ,0x1D ,0x13 ,0x00 ,0xE1 ,0x01 ,0x03 ,0x01 ,0x48);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0xB0);		
	lcm_dcs_write_seq_static(ctx,0xCF, 0x00 ,0x00 ,0xB4 ,0xB8);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0xB5);		
	lcm_dcs_write_seq_static(ctx,0xCF, 0x05 ,0x05 ,0x64 ,0x68);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0xC0);		
	lcm_dcs_write_seq_static(ctx,0xCF, 0x09 ,0x09 ,0x5B ,0x5F);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0xC5);		
	lcm_dcs_write_seq_static(ctx,0xCF, 0x09 ,0x09 ,0x61 ,0x65);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0x60);		
	lcm_dcs_write_seq_static(ctx,0xCF, 0x00 ,0x00 ,0x85 ,0x89 ,0x05 ,0x05 ,0x4D ,0x51);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0x70);		
	lcm_dcs_write_seq_static(ctx,0xCF, 0x00 ,0x00 ,0xBF ,0xC3 ,0x05 ,0x05 ,0x57 ,0x5B);		
			
	//Qsync Detect		
	lcm_dcs_write_seq_static(ctx,0x00, 0xD1);		
	lcm_dcs_write_seq_static(ctx,0xC1, 0x0B ,0x60 ,0x0F ,0xDC ,0x1B ,0x15 ,0x05 ,0xAF ,0x07 ,0xE5 ,0x0D ,0x81);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0xE1);		
	lcm_dcs_write_seq_static(ctx,0xC1, 0x0F ,0xDC);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0xE4);		
	lcm_dcs_write_seq_static(ctx,0xCF, 0x09 ,0xE2 ,0x09 ,0xE1 ,0x09 ,0xE1 ,0x09 ,0xE1 ,0x09 ,0xE1 ,0x09 ,0xE1);		
			
	//OSC		
	lcm_dcs_write_seq_static(ctx,0x00, 0x80);		
	lcm_dcs_write_seq_static(ctx,0xC1, 0x44 ,0x44);		
			
	lcm_dcs_write_seq_static(ctx,0x00, 0x90);		
	lcm_dcs_write_seq_static(ctx,0xC1, 0x03);		
			
	//Line rate for TP		
	lcm_dcs_write_seq_static(ctx,0x00, 0xF5);		
	lcm_dcs_write_seq_static(ctx,0xCF, 0x00);		
			
	//TP Frame rate		
	lcm_dcs_write_seq_static(ctx,0x00, 0xF6);		
	lcm_dcs_write_seq_static(ctx,0xCF, 0x78);		
			
	//TCON Frame rate		
	lcm_dcs_write_seq_static(ctx,0x00, 0xF1);		
	lcm_dcs_write_seq_static(ctx,0xCF, 0x78);		
			
	//Gram Vesa Line		
	lcm_dcs_write_seq_static(ctx,0x00, 0x85);		
	lcm_dcs_write_seq_static(ctx,0xB4, 0x77);		

	//Source Clk Select
	lcm_dcs_write_seq_static(ctx,0x00, 0x91);
	lcm_dcs_write_seq_static(ctx,0xC4, 0x88);

	//VDD=1.275V LVDSVDD=1.25V VDD_TP=1.2V
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xC5,0x87,0x59);

	lcm_dcs_write_seq_static(ctx,0x00,0x87);
	lcm_dcs_write_seq_static(ctx,0xC5,0x0A,0x0A);

	//VDDI current 20230404
	lcm_dcs_write_seq_static(ctx,0x00, 0x93);
	lcm_dcs_write_seq_static(ctx,0xC1, 0x82);

	//4 power VDD=1.275V LVDSVDD=1.25V
	lcm_dcs_write_seq_static(ctx,0x00,0x9E);
	lcm_dcs_write_seq_static(ctx,0xC5,0x87);

	lcm_dcs_write_seq_static(ctx,0x00,0x88);
	lcm_dcs_write_seq_static(ctx,0xC4,0x08);
			
	//========================================
	//STV1 & STV2 Setting
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xC2,0x83,0x01,0x01,0x86,0x82,0x01,0x01,0x86,0x8D,0x02,0x01,0x86);

	//CKV1-3 setting
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xC2,0x8A,0x07,0x00,0x01,0x87,0x89,0x08,0x00,0x01,0x87,0x88,0x09,0x00,0x01,0x87);

	//CKV4 setting
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xC2,0x87,0x0A,0x00,0x01,0x87);

	//CKV width setting
	lcm_dcs_write_seq_static(ctx,0x00,0xE0);
	lcm_dcs_write_seq_static(ctx,0xC2,0x33,0x33,0x00,0x00);

	//Rst1 Setting
	lcm_dcs_write_seq_static(ctx,0x00,0xE8);
	lcm_dcs_write_seq_static(ctx,0xC2,0x12,0x00,0x0A,0x0A,0x03,0x88,0x00,0x00);

	//GOFF setting
	lcm_dcs_write_seq_static(ctx,0x00,0xD0);
	lcm_dcs_write_seq_static(ctx,0xC3,0x35,0x0A,0x00,0x00,0x35,0x0A,0x00,0x00,0x35,0x0A,0x00,0x00,0x35,0x0A,0x00,0x00);

	lcm_dcs_write_seq_static(ctx,0x00,0xE0);
	lcm_dcs_write_seq_static(ctx,0xC3,0x35,0x0A,0x00,0x00,0x35,0x0A,0x00,0x00,0x35,0x0A,0x00,0x00,0x35,0x0A,0x00,0x00);

	//power off enmode setting
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xCB,0xCD,0xCD,0xCD,0x00,0xCD,0xCC,0x00,0xCD,0xCE,0xFE,0xCD,0x00,0xCC,0xCC,0x00,0x00);

	//power on enmode setting
	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xCB,0x0C,0x00,0x00,0x00,0x0C,0x0C,0x00,0x00,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00);

	//skip & powr on1 enmode setting
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);

	//power off blank enmode setting
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xCB,0x50,0x41,0xA4,0x00);

	//power on blank enmode setting
	lcm_dcs_write_seq_static(ctx,0x00,0xC0);
	lcm_dcs_write_seq_static(ctx,0xCB,0x50,0x41,0xA4,0x00);

	//power on blank enmode setting
	lcm_dcs_write_seq_static(ctx,0x00,0xD5);
	lcm_dcs_write_seq_static(ctx,0xCB,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00);

	lcm_dcs_write_seq_static(ctx,0x00,0xE0);
	lcm_dcs_write_seq_static(ctx,0xCB,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83);

	//panel mapping setting
	//u2d_L
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xCC,0x2C,0x2C,0x2C,0x18,0x17,0x16,0x2C,0x07,0x06,0x09,0x08,0x2C,0x25,0x26,0x2C,0x24);
	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xCC,0x2C,0x22,0x23,0x23,0x04,0x03,0x02,0x26);
	//u2d_R
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xCD,0x2C,0x2C,0x2C,0x18,0x17,0x16,0x2C,0x07,0x06,0x09,0x08,0x2C,0x25,0x26,0x2C,0x24);
	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xCD,0x2C,0x22,0x23,0x23,0x04,0x03,0x02,0x26);

	//d2u_L
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xCC,0x2C,0x2C,0x2C,0x18,0x17,0x16,0x2C,0x08,0x09,0x06,0x07,0x2C,0x25,0x22,0x2C,0x24);
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xCC,0x2C,0x26,0x23,0x23,0x04,0x02,0x03,0x26);
	//d2u_R
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xCD,0x2C,0x2C,0x2C,0x18,0x17,0x16,0x2C,0x08,0x09,0x06,0x07,0x2C,0x25,0x22,0x2C,0x24);
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xCD,0x2C,0x26,0x23,0x23,0x04,0x02,0x03,0x26);
	//==============================================

	//ckh
	lcm_dcs_write_seq_static(ctx,0x00,0x86);//Normal
	lcm_dcs_write_seq_static(ctx,0xC0,0x01,0x01,0x01,0x00,0x12,0x12,0x12,0x03);

	lcm_dcs_write_seq_static(ctx,0x00,0x96);//IDLE
	lcm_dcs_write_seq_static(ctx,0xC0,0x01,0x02,0x01,0x00,0x13,0x13,0x13,0x03);

	lcm_dcs_write_seq_static(ctx,0x00,0xA6);//LPF
	lcm_dcs_write_seq_static(ctx,0xC0,0x01,0x02,0x01,0x00,0x1D,0x1D,0x1D,0x03);

	lcm_dcs_write_seq_static(ctx,0x00,0xA3);//PER_DMY
	lcm_dcs_write_seq_static(ctx,0xCE,0x01,0x01,0x01,0x00,0x12,0x03);

	lcm_dcs_write_seq_static(ctx,0x00,0xB3);//POS_DMY
	lcm_dcs_write_seq_static(ctx,0xCE,0x00,0x01,0x01,0x00,0x12,0x03);

	lcm_dcs_write_seq_static(ctx,0x00,0x76);//FIFO
	lcm_dcs_write_seq_static(ctx,0xC0,0x01,0x02,0x01,0x01,0x31,0x31,0x31,0x05);

	//CKH_dummy			
	lcm_dcs_write_seq_static(ctx,0x00,0x82);			
	lcm_dcs_write_seq_static(ctx,0xa7,0x20,0x00);			
				
	lcm_dcs_write_seq_static(ctx,0x00,0x8d);			
	lcm_dcs_write_seq_static(ctx,0xa7,0x02);			
				
	lcm_dcs_write_seq_static(ctx,0x00,0x8f);			
	lcm_dcs_write_seq_static(ctx,0xa7,0x01);


	//analog setting
	//vgh=11V
	lcm_dcs_write_seq_static(ctx,0x00,0x93);
	lcm_dcs_write_seq_static(ctx,0xC5,0x37);

	lcm_dcs_write_seq_static(ctx,0x00,0x97);
	lcm_dcs_write_seq_static(ctx,0xC5,0x37);

	//vgl=-9V
	lcm_dcs_write_seq_static(ctx,0x00,0x9A);
	lcm_dcs_write_seq_static(ctx,0xC5,0x23);

	lcm_dcs_write_seq_static(ctx,0x00,0x9C);
	lcm_dcs_write_seq_static(ctx,0xC5,0x23);

	//vgho1=10V, vglo1=-8V 
	lcm_dcs_write_seq_static(ctx,0x00,0xB6);
	lcm_dcs_write_seq_static(ctx,0xC5,0x2D,0x2D,0x19,0x19);

	//GVDDP=5.2V, GVDDN=-5.2V
	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xD8,0x2F,0x2F);

	/*

	//VCOM=-0.2V
	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xD9,0x23,0x23,0x23,0x23);
	lcm_dcs_write_seq_static(ctx,0x00,0x06);
	lcm_dcs_write_seq_static(ctx,0xD9,0x23,0x23,0x23);
	  */


	lcm_dcs_write_seq_static(ctx,0x00,0x88);
	lcm_dcs_write_seq_static(ctx,0xC4,0x08);

	//CKH Rotate	
	//0x03：RGBBGR  0x10：RGBRGB
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xA7,0x03);		
					
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xC3,0x00,0x01,0x23,0x45,0x21,0x03,0x45,0x00,0x00,0x00,0x21,0x03,0x45,0x01,0x23,0x45);

	lcm_dcs_write_seq_static(ctx,0x00,0xB1);			
	lcm_dcs_write_seq_static(ctx,0xF5,0x1F);
	//C0CB[7:4]=PONBLANK=1= 2 FRAME
	//C0CB[3:0]=POFBLANK=1= 2 FRAME
	lcm_dcs_write_seq_static(ctx,0x00,0xCB);
	lcm_dcs_write_seq_static(ctx,0xC0,0x01);

	lcm_dcs_write_seq_static(ctx,0x00,0x88);
	lcm_dcs_write_seq_static(ctx,0xC4,0x08);

	lcm_dcs_write_seq_static(ctx,0x00,0x94);
	lcm_dcs_write_seq_static(ctx,0xE9,0x00);

	lcm_dcs_write_seq_static(ctx,0x00,0x9A);
	lcm_dcs_write_seq_static(ctx,0xC4,0x11);

	lcm_dcs_write_seq_static(ctx,0x00,0x95);
	lcm_dcs_write_seq_static(ctx,0xE9,0x10);
	lcm_dcs_write_seq_static(ctx,0x00,0x82);
	lcm_dcs_write_seq_static(ctx,0xF5,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0x93);
	lcm_dcs_write_seq_static(ctx,0xF5,0x00);

	//AC MODE GIP toggle
	lcm_dcs_write_seq_static(ctx,0x00,0x99);			
	lcm_dcs_write_seq_static(ctx,0xCF,0x50);

	lcm_dcs_write_seq_static(ctx,0x00,0x9C);			
	lcm_dcs_write_seq_static(ctx,0xF5,0x00);

	lcm_dcs_write_seq_static(ctx,0x00,0x9E);			
	lcm_dcs_write_seq_static(ctx,0xF5,0x00);

	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xC5,0x10,0x4A,0x01,0x1F,0x4A,0x00);//fw q全驱 8725 其他客戶

	//lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	//lcm_dcs_write_seq_static(ctx,0xC5,0x10,0x4A,0x09,0x1F,0x4A,0x02);//8725 JDI ini

	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xB0,0x00,0x00,0x00,0x00,0x00,0x1D,0x01); //D-phy設置phy的省電

	lcm_dcs_write_seq_static(ctx,0x00,0x93);
	lcm_dcs_write_seq_static(ctx,0xE9,0xAA);

	lcm_dcs_write_seq_static(ctx,0x00,0x95);
	lcm_dcs_write_seq_static(ctx,0xE9,0xB0);

	lcm_dcs_write_seq_static(ctx,0x00,0x9B);
	lcm_dcs_write_seq_static(ctx,0xC4,0x08);

	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xA4,0xC8);


	//mirror_x2=1
	lcm_dcs_write_seq_static(ctx,0x00,0xE8);
	lcm_dcs_write_seq_static(ctx,0xC0,0x40);


	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xE1,0x00,0x01,0x05,0x0B,0x2B,0x18,0x20,0x27,0x32,0x3E,0x3B,0x42,0x48,0x4D,0x67,0x52,0x5B,0x63,0x6A,0xE7,0x72,0x79,0x82,0x8B,0x30,0x95,0x9B,0xA2,0xA9,0x53,0xB3,0xBE,0xCC,0xD5,0x11,0xE0,0xEF,0xF9,0xFF,0x44);
	lcm_dcs_write_seq_static(ctx,0x00,0x30);
	lcm_dcs_write_seq_static(ctx,0xE1,0x00,0x01,0x05,0x0B,0x2B,0x18,0x20,0x27,0x32,0x3E,0x3B,0x42,0x48,0x4D,0x67,0x52,0x5B,0x63,0x6A,0xE7,0x72,0x79,0x82,0x8B,0x30,0x95,0x9B,0xA2,0xA9,0x53,0xB3,0xBE,0xCC,0xD5,0x11,0xE0,0xEF,0xF9,0xFF,0x44);
	lcm_dcs_write_seq_static(ctx,0x00,0x60);
	lcm_dcs_write_seq_static(ctx,0xE1,0x00,0x01,0x05,0x0B,0x2B,0x18,0x20,0x27,0x32,0x3E,0x3B,0x42,0x48,0x4D,0x67,0x52,0x5B,0x63,0x6A,0xE7,0x72,0x79,0x82,0x8B,0x30,0x95,0x9B,0xA2,0xA9,0x53,0xB3,0xBE,0xCC,0xD5,0x11,0xE0,0xEF,0xF9,0xFF,0x44);
	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xE1,0x00,0x01,0x05,0x0B,0x2B,0x18,0x20,0x27,0x32,0x3E,0x3B,0x42,0x48,0x4D,0x67,0x52,0x5B,0x63,0x6A,0xE7,0x72,0x79,0x82,0x8B,0x30,0x95,0x9B,0xA2,0xA9,0x53,0xB3,0xBE,0xCC,0xD5,0x11,0xE0,0xEF,0xF9,0xFF,0x44);
	lcm_dcs_write_seq_static(ctx,0x00,0xC0);
	lcm_dcs_write_seq_static(ctx,0xE1,0x00,0x01,0x05,0x0B,0x2B,0x18,0x20,0x27,0x32,0x3E,0x3B,0x42,0x48,0x4D,0x67,0x52,0x5B,0x63,0x6A,0xE7,0x72,0x79,0x82,0x8B,0x30,0x95,0x9B,0xA2,0xA9,0x53,0xB3,0xBE,0xCC,0xD5,0x11,0xE0,0xEF,0xF9,0xFF,0x44);
	lcm_dcs_write_seq_static(ctx,0x00,0xF0);
	lcm_dcs_write_seq_static(ctx,0xE1,0x00,0x01,0x05,0x0B,0x2B,0x18,0x20,0x27,0x32,0x3E,0x3B,0x42,0x48,0x4D,0x67,0x52);
	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xE2,0x5B,0x63,0x6A,0xE7,0x72,0x79,0x82,0x8B,0x30,0x95,0x9B,0xA2,0xA9,0x53,0xB3,0xBE,0xCC,0xD5,0x11,0xE0,0xEF,0xF9,0xFF,0x44);


	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xE3,0x00,0x04,0x08,0x0F,0x3F,0x1B,0x23,0x29,0x34,0x08,0x3C,0x43,0x49,0x4E,0x40,0x52,0x5B,0x62,0x69,0xC5,0x70,0x77,0x7E,0x87,0x5C,0x90,0x96,0x9C,0xA4,0xDC,0xAC,0xB7,0xC4,0xCD,0xE1,0xD9,0xE9,0xF3,0xFF,0x18);
	lcm_dcs_write_seq_static(ctx,0x00,0x30);
	lcm_dcs_write_seq_static(ctx,0xE3,0x00,0x04,0x08,0x0F,0x3F,0x1B,0x23,0x29,0x34,0x08,0x3C,0x43,0x49,0x4E,0x40,0x52,0x5B,0x62,0x69,0xC5,0x70,0x77,0x7E,0x87,0x5C,0x90,0x96,0x9C,0xA4,0xDC,0xAC,0xB7,0xC4,0xCD,0xE1,0xD9,0xE9,0xF3,0xFF,0x18);
	lcm_dcs_write_seq_static(ctx,0x00,0x60);
	lcm_dcs_write_seq_static(ctx,0xE3,0x00,0x04,0x08,0x0F,0x3F,0x1B,0x23,0x29,0x34,0x08,0x3C,0x43,0x49,0x4E,0x40,0x52,0x5B,0x62,0x69,0xC5,0x70,0x77,0x7E,0x87,0x5C,0x90,0x96,0x9C,0xA4,0xDC,0xAC,0xB7,0xC4,0xCD,0xE1,0xD9,0xE9,0xF3,0xFF,0x18);
	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xE3,0x00,0x04,0x08,0x0F,0x3F,0x1B,0x23,0x29,0x34,0x08,0x3C,0x43,0x49,0x4E,0x40,0x52,0x5B,0x62,0x69,0xC5,0x70,0x77,0x7E,0x87,0x5C,0x90,0x96,0x9C,0xA4,0xDC,0xAC,0xB7,0xC4,0xCD,0xE1,0xD9,0xE9,0xF3,0xFF,0x18);
	lcm_dcs_write_seq_static(ctx,0x00,0xC0);
	lcm_dcs_write_seq_static(ctx,0xE3,0x00,0x04,0x08,0x0F,0x3F,0x1B,0x23,0x29,0x34,0x08,0x3C,0x43,0x49,0x4E,0x40,0x52,0x5B,0x62,0x69,0xC5,0x70,0x77,0x7E,0x87,0x5C,0x90,0x96,0x9C,0xA4,0xDC,0xAC,0xB7,0xC4,0xCD,0xE1,0xD9,0xE9,0xF3,0xFF,0x18);
	lcm_dcs_write_seq_static(ctx,0x00,0xF0);
	lcm_dcs_write_seq_static(ctx,0xE3,0x00,0x04,0x08,0x0F,0x3F,0x1B,0x23,0x29,0x34,0x08,0x3C,0x43,0x49,0x4E,0x40,0x52);
	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xE4,0x5B,0x62,0x69,0xC5,0x70,0x77,0x7E,0x87,0x5C,0x90,0x96,0x9C,0xA4,0xDC,0xAC,0xB7,0xC4,0xCD,0xE1,0xD9,0xE9,0xF3,0xFF,0x18);

	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xCA,0xEB,0xD6,0xC5,0xB8,0xAE,0xA4,0x9D,0x96,0x91,0x8C,0x87,0x83);

	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xCA,0xFD,0xFF,0xEA,0xFC,0xFF,0xCC,0xFA,0xFF,0x66);




	lcm_dcs_write_seq_static(ctx,0x00,0xE0);
	lcm_dcs_write_seq_static(ctx,0xCF,0x34);

	lcm_dcs_write_seq_static(ctx,0x00,0x85);
	lcm_dcs_write_seq_static(ctx,0xA7,0x00);

	//ESD disable reg_21h_rev_disable
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xB3,0x22);
	 
	//HS lock CMD1. B3B0h=0x01->0x00
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xB3,0x00);

	//TP TERM=48
	lcm_dcs_write_seq_static(ctx,0x00,0x82);
	lcm_dcs_write_seq_static(ctx,0xCE,0x2F,0x2F);
	//CKH TOGGLE
	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xA7,0x00);

	//SD CHOP 20230510
	//lcm_dcs_write_seq_static(ctx,0x00,0x81);
	//lcm_dcs_write_seq_static(ctx,0xA4,0x83);

	lcm_dcs_write_seq_static(ctx,0x00,0xFC);
	lcm_dcs_write_seq_static(ctx,0xC0,0x00,0x15);

	//CABC PWM 21.37Khz 11bit
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xCA,0x05,0x05,0x0B);

	lcm_dcs_write_seq_static(ctx,0x00,0x81);
	lcm_dcs_write_seq_static(ctx,0xA4,0x73);

	lcm_dcs_write_seq_static(ctx,0x00,0x87);
	lcm_dcs_write_seq_static(ctx,0xC4,0x08);

	//Slice Height=8
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xB4,0x00,0x08,0x02,0x00,0x00,0xbb,0x00,0x07,0x0d,0xb7,0x0c,0xb7,0x10,0xf0);

	////////////VESA DP90TP180////////////
	////LPF Mode ON
	//lcm_dcs_write_seq_static(ctx,0x00, 0xD2);
	//lcm_dcs_write_seq_static(ctx,0xC0, 0x01);
	////Line rate for TP
	//lcm_dcs_write_seq_static(ctx,0x00, 0xF5);
	//lcm_dcs_write_seq_static(ctx,0xCF, 0x01);

	////TP Frame rate
	//lcm_dcs_write_seq_static(ctx,0x00, 0xF6);
	//lcm_dcs_write_seq_static(ctx,0xCF, 0x5A);

	////TCON Frame rate
	//lcm_dcs_write_seq_static(ctx,0x00, 0xF1);
	//lcm_dcs_write_seq_static(ctx,0xCF, 0x5A);

	////Source Clk Select
	//lcm_dcs_write_seq_static(ctx,0x00, 0x91);
	//lcm_dcs_write_seq_static(ctx,0xC4, 0x08);

	//lcm_dcs_write_seq_static(ctx,0x00,0xA7);
	//lcm_dcs_write_seq_static(ctx,0xCE,0x1D);
	//lcm_dcs_write_seq_static(ctx,0x00,0xB7);
	//lcm_dcs_write_seq_static(ctx,0xCE,0x1D);
	////TP TERM=36
	//lcm_dcs_write_seq_static(ctx,0x00,0x82);
	//lcm_dcs_write_seq_static(ctx,0xCE,0x23,0x23);

	//lcm_dcs_write_seq_static(ctx,0x00,0xFC);
	//lcm_dcs_write_seq_static(ctx,0xC0,0x00,0x20);
	////////////VESA DP90TP180////////////

	////////////FIFO DP60TP120////////////
	////LPF Mode ON
	//lcm_dcs_write_seq_static(ctx,0x1C, 0x02);

	////Line rate for TP
	//lcm_dcs_write_seq_static(ctx,0x00, 0xF5);
	//lcm_dcs_write_seq_static(ctx,0xCF, 0x02);

	////TP Frame rate
	//lcm_dcs_write_seq_static(ctx,0x00, 0xF6);
	//lcm_dcs_write_seq_static(ctx,0xCF, 0x3C);

	////TCON Frame rate
	//lcm_dcs_write_seq_static(ctx,0x00, 0xF1);
	//lcm_dcs_write_seq_static(ctx,0xCF, 0x3C);

	////Source Clk Select
	//lcm_dcs_write_seq_static(ctx,0x00, 0x91);
	//lcm_dcs_write_seq_static(ctx,0xC4, 0x08);

	//lcm_dcs_write_seq_static(ctx,0x00,0xA7);
	//lcm_dcs_write_seq_static(ctx,0xCE,0x31,0x05);
	//lcm_dcs_write_seq_static(ctx,0x00,0xB7);
	//lcm_dcs_write_seq_static(ctx,0xCE,0x31,0x05);

	//lcm_dcs_write_seq_static(ctx,0x00,0xFC);
	//lcm_dcs_write_seq_static(ctx,0xC0,0x80);
	////////////FIFO DP60TP120////////////

	lcm_dcs_write_seq_static(ctx,0x00,0x0E);
	lcm_dcs_write_seq_static(ctx,0xF3,0x80,0xFF);

	////CMD2 disable
	lcm_dcs_write_seq_static(ctx,0x00,0x00); 
	lcm_dcs_write_seq_static(ctx,0xFF,0xFF,0xFF,0xFF);

	//lcm_dcs_write_seq_static(ctx,0x00,0x84); 
	//lcm_dcs_write_seq_static(ctx,0xF4,0x57);


	//// BIST Free run Mode
	//lcm_dcs_write_seq_static(ctx,0x00,0xa0);
	//lcm_dcs_write_seq_static(ctx,0xf6,0x0B,0x01,0x23,0x45,0x67,0x89,0xAB,0xCE,0xEF);
	////BIST EN  0: Free Run Mode  1:Single Mode
	//lcm_dcs_write_seq_static(ctx,0x00,0xA9);
	//lcm_dcs_write_seq_static(ctx,0xf6,0x00);
	////BIST PWD
	//lcm_dcs_write_seq_static(ctx,0x00,0x88);
	//lcm_dcs_write_seq_static(ctx,0xf6,0x5A);// F688=A5 BIST disable


	//lcm_dcs_write_seq_static(ctx,0x00,0x80);
	//lcm_dcs_write_seq_static(ctx,0xf6,0x69,0x10)

	lcm_dcs_write_seq_static(ctx,0x35,0x00);	
	lcm_dcs_write_seq_static(ctx,0x51,0xff,0x0f);	
	lcm_dcs_write_seq_static(ctx,0x53,0x24);	

	lcm_dcs_write_seq_static(ctx,0x55,0x01);	
	lcm_dcs_write_seq_static(ctx,0x26,0x02);


	//----------------------LCD initial code End----------------------//			
	//SLPOUT and DISPON			
	lcm_dcs_write_seq_static(ctx,0x11);			
	msleep(120);						
	lcm_dcs_write_seq_static(ctx,0x29);						
	msleep(20);

	
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
bool gesture_status = false;
EXPORT_SYMBOL(gesture_status);
static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;
	lcm_dcs_write_seq_static(ctx, 0xAC,0x0A,0x00);
	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(20);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(120);

	ctx->error = 0;
	ctx->prepared = false;
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	if (gesture_status == false) {

		ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
			"bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
				__func__, PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
	
		udelay(1000);
	
		ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
			"bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
				__func__, PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
		udelay(1000);
	
	//prize-add gpio ldo1.8v-pengzhipeng-20220514-start
		ctx->ldo18_gpio = devm_gpiod_get(ctx->dev, "ldo18", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->ldo18_gpio)) {
			dev_info(ctx->dev, "cannot get ldo18-gpios %ld\n",
				PTR_ERR(ctx->ldo18_gpio));
			return PTR_ERR(ctx->ldo18_gpio);
		}
		gpiod_set_value(ctx->ldo18_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->ldo18_gpio);
		udelay(1000);
	//prize-add gpio ldo1.8v-pengzhipeng-20220514-end
	}
	pr_info("%s ok\n", __func__);

	return 0;
}

#if defined(TPS65132_BIAS_SUPPORT)
extern int tps65132_write_byte(unsigned char cmd, unsigned char writeData);
#endif
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

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	pr_info("%s ret=%d\n", __func__, ret);
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

#define HAC (1080)
#define VAC (2400)

/*60HZ */
#define HSA (10)
#define HBP (12)
#define HFP (12)


#define VSA (10)
#define VBP (20)
#define VFP (2470)
#define VFP_90 (840)
#define VFP_120 (20)

#define PHYSICAL_WIDTH              67930
#define PHYSICAL_HEIGHT             156588

#define PCLK_IN_KHZ \
    ((HAC+HFP+HSA+HBP)*(VAC+VFP+VSA+VBP)*(60)/1000) 
#define PCLK2_IN_KHZ \
    ((HAC+HFP+HSA+HBP)*(VAC+VFP_90+VSA+VBP)*(90)/1000) 
#define PCLK3_IN_KHZ \
    ((HAC+HFP+HSA+HBP)*(VAC+VFP_120+VSA+VBP)*(120)/1000) 

#define PLL_CLK (412)
#define DATA_RATE (PLL_CLK*2)
static const struct drm_display_mode default_mode = {
	.clock = PCLK_IN_KHZ,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,//1140
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,//2199
	// .vrefresh = 60,

};

static const struct drm_display_mode performance_mode = {
	.clock = PCLK2_IN_KHZ,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,//1140
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_90,
	.vsync_end = VAC + VFP_90 + VSA,
	.vtotal = VAC + VFP_90 + VSA + VBP,//2199
	// .vrefresh = 90,

};

static struct drm_display_mode performance_mode1 = {
	.clock = PCLK3_IN_KHZ,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_120,
	.vsync_end = VAC + VFP_120 + VSA,
	.vtotal = VAC + VFP_120 + VSA + VBP,
	// .vrefresh = 120,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = PLL_CLK,
	//.vfp_low_power = 2591,//45hz
		//.ssc_disable = 1,
	.physical_width_um  = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
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
                 .pic_height = 2400,
                 .pic_width = 1080,
                 .slice_height = 8,
                 .slice_width = 540,
                 .chunk_size = 540,
                 .xmit_delay = 512,
                 .dec_delay = 616,
                 .scale_value = 32,
                 .increment_interval = 187,
                 .decrement_interval = 7,
                 .line_bpg_offset = 12,
                 .nfl_bpg_offset = 3511,
                 .slice_bpg_offset = 3255,
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
	.data_rate = DATA_RATE,
	.dyn_fps = {
		.switch_en = 1, .vact_timing_fps = 90,
	},
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = PLL_CLK,
	//.vfp_low_power = 1323,//60hz
	.physical_width_um  = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {

		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
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
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 8,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 616,
		.scale_value = 32,
		.increment_interval = 187,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 3255,
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
	.data_rate = DATA_RATE,
	.dyn_fps = {
		.switch_en = 1, .vact_timing_fps = 90,
	},
};

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = PLL_CLK,
	//.vfp_low_power = 1323,//60hz
	.physical_width_um  = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
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
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 8,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 616,
		.scale_value = 32,
		.increment_interval = 187,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 3255,
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
	.data_rate = DATA_RATE,
	.dyn_fps = {
		.switch_en = 1, .vact_timing_fps = 90,
	},
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

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset-gpios %ld\n",
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
	//unsigned char id[3] = {0x0, 0x80, 0x0};
	ssize_t ret;
	pr_err("panel----exit------%s-----%d\n",__func__,__LINE__);

	// prize baibo for lcm ata test begin
	//lcm_dcs_write_seq_static(ctx,0xFE,0xC2);
	ret = mipi_dsi_dcs_read(dsi, 0x04, data, 3);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	printk("panel_ata_check-ATA read 0x04 data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == 0x30 || data[1] == 0x80) {
		return 1;
	}
	
	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
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

	mode2 = drm_mode_duplicate(connector->dev, &performance_mode);
	if (!mode2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode.hdisplay, performance_mode.vdisplay,
			drm_mode_vrefresh(&performance_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);

	mode3 = drm_mode_duplicate(connector->dev, &performance_mode1);
	if (!mode3) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode1.hdisplay, performance_mode1.vdisplay,
			drm_mode_vrefresh(&performance_mode1));
		return -ENOMEM;
	}

	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode3);


	connector->display_info.width_mm = 68;
	connector->display_info.height_mm = 156;

	pr_info("%s end!\n", __func__);
	return 3;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

/*
static void check_is_need_fake_resolution(struct device *dev)
{
	unsigned int ret = 0;

	ret = of_property_read_u32(dev->of_node, "fake_heigh", &fake_heigh);
	if (ret)
		need_fake_resolution = false;
	ret = of_property_read_u32(dev->of_node, "fake_width", &fake_width);
	if (ret)
		need_fake_resolution = false;
	if (fake_heigh > 0 && fake_heigh < VAC)
		need_fake_resolution = true;
	if (fake_width > 0 && fake_width < HAC)
		need_fake_resolution = true;
}
*/

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
			 | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET;

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

	ctx->ldo18_gpio = devm_gpiod_get(dev, "ldo18", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->ldo18_gpio)) {
		dev_info(dev, "cannot get ldo18-gpios %ld\n",
			PTR_ERR(ctx->ldo18_gpio));
		return PTR_ERR(ctx->ldo18_gpio);
	}
	devm_gpiod_put(dev, ctx->ldo18_gpio);

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
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
	pr_info("%s-CONFIG_MTK_PANEL_EXT\n", __func__);
#endif
	//check_is_need_fake_resolution(dev);
	pr_info("%s-\n", __func__);

#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
    strcpy(current_lcm_info.chip,"ft8725");
    strcpy(current_lcm_info.vendor,"QCG");
    sprintf(current_lcm_info.id,"0x%02x",0x02);
    strcpy(current_lcm_info.more,"1080*2400");
#endif
	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

#ifndef BYPASSI2C
	_lcm_i2c_exit();
#endif

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "qcg,ft8725,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel_ft8725_fhdp_dsi_vdo_n9",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("MEDIATEK");
MODULE_DESCRIPTION("sc ft8725 VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
