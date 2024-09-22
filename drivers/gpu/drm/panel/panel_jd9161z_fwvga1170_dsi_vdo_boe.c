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
#if BITS_PER_LONG == 32
	mdelay(15 * 1000);
#else
	udelay(15 * 1000);
#endif
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
#if BITS_PER_LONG == 32
	mdelay(10 * 1000);
#else
	udelay(10 * 1000);
#endif
	gpiod_set_value(ctx->reset_gpio, 1);
#if BITS_PER_LONG == 32
	mdelay(10 * 1000);
#else
	udelay(10 * 1000);
#endif
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_dcs_write_seq_static(ctx,0xDF,0x91,0x62,0xF3);
	lcm_dcs_write_seq_static(ctx,0xB2,0x00,0x55);
	lcm_dcs_write_seq_static(ctx,0xB3,0x00,0x4E);
	lcm_dcs_write_seq_static(ctx,0xB7,0x00,0x97,0x00,0x00,0x97,0x00);
	lcm_dcs_write_seq_static(ctx,0xB9,0x23);
	lcm_dcs_write_seq_static(ctx,0xBB,0x77,0x16,0xB2,0xB2,0xB2,0x60,0x70,0x20,0x50,0x50,0x60);
	lcm_dcs_write_seq_static(ctx,0xBC,0x73,0x34);
	lcm_dcs_write_seq_static(ctx,0xC0,0x27);
	lcm_dcs_write_seq_static(ctx,0xCC,0x31,0x0C);
	lcm_dcs_write_seq_static(ctx,0xC3,0x04,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x00,0x01,0x82,0x01,0x82);
	lcm_dcs_write_seq_static(ctx,0xC4,0x20,0x49,0x92,0x08,0x17,0x10);
	lcm_dcs_write_seq_static(ctx,0xC8,0x7F,0x72,0x69,0x62,0x63,0x57,0x60,0x49,0x62,0x5F,0x5B,0x75,0x5D,0x63,0x52,0x4A,0x3B,0x27,0x0C,0x7F,0x72,0x69,0x62,0x63,0x57,0x60,0x49,0x62,0x5F,0x5B,0x75,0x5D,0x63,0x52,0x4A,0x3B,0x27,0x0C);
	lcm_dcs_write_seq_static(ctx,0xD0,0x1E,0x03,0x35,0x10,0x15,0x1F,0x01,0x1F,0x05,0x07,0x09,0x0B,0x1F,0x1F,0x1F,0x1F);
	lcm_dcs_write_seq_static(ctx,0xD1,0x1E,0x02,0x35,0x10,0x15,0x1F,0x00,0x1F,0x04,0x06,0x08,0x0A,0x1F,0x1F,0x1F,0x1F);
	lcm_dcs_write_seq_static(ctx,0xD2,0x1F,0x02,0x15,0x10,0x15,0x1E,0x00,0x1F,0x04,0x0A,0x08,0x06,0x1F,0x1F,0x1F,0x1F);
	lcm_dcs_write_seq_static(ctx,0xD3,0x1F,0x03,0x15,0x10,0x15,0x1E,0x01,0x1F,0x05,0x0B,0x09,0x07,0x1F,0x1F,0x1F,0x1F);
	lcm_dcs_write_seq_static(ctx,0xD4,0x30,0x00,0x00,0x03,0x60,0x0D,0x30,0x00,0x0F,0x00,0x60,0x00,0x60,0x09,0x30,0x81,0x02,0x00,0x60,0x73,0x11,0x00,0x60,0x08,0x25,0x00,0x63,0x03,0x00);
	lcm_dcs_write_seq_static(ctx,0xD5,0x01,0x20,0x80,0x08,0x00,0x00,0x00,0x08,0x00,0x00,0x06,0x60,0x00,0x07,0x50,0x00,0x80,0x00,0x00,0x00,0x07,0x02,0x00,0x00,0x00,0x00,0x06,0x10,0x00,0x00,0x0F,0x4F,0x00,0x10,0x1F,0x3F,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xDE,0x02);
	lcm_dcs_write_seq_static(ctx,0xBB,0x03,0x03);
	lcm_dcs_write_seq_static(ctx,0xCD,0x00,0x00,0x02);
	lcm_dcs_write_seq_static(ctx,0xC9,0x71,0x7C);
	lcm_dcs_write_seq_static(ctx,0xC1,0x63,0x22,0x22,0x00,0x00,0x82,0x00,0x82);
	lcm_dcs_write_seq_static(ctx,0xC2,0x00,0x18,0x08,0x1E,0x24,0x3C,0xC7);
	lcm_dcs_write_seq_static(ctx,0xBE,0x00);
	lcm_dcs_write_seq_static(ctx,0xDE,0x00);
	lcm_dcs_write_seq_static(ctx,0x35);
	lcm_dcs_write_seq_static(ctx,0x11);
	msleep(120);
	lcm_dcs_write_seq_static(ctx,0x29);
	msleep(10);
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
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

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

	udelay(5000);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	udelay(5000);

#ifndef BYPASSI2C
	_lcm_i2c_write_bytes(0x0, 0x12);
	_lcm_i2c_write_bytes(0x1, 0x12);
#endif

#if defined(TPS65132_BIAS_SUPPORT)
	tps65132_write_byte(0x0, 0x10);
	tps65132_write_byte(0x1, 0x10);
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
/*vdo time config*/
#define FRAME_WIDTH                 480
#define FRAME_HEIGHT                1170

/*60HZ */
#define HFP (16)
#define HSA (16)
#define HBP (16)
#define VFP (8)
#define VSA (2)
#define VBP (22)
#define VAC (1170)
#define HAC (480)

#define DATA_RATE					 484

#define PHYSICAL_WIDTH              43200
#define PHYSICAL_HEIGHT             105300

#define PCLK_IN_KHZ_60HZ \
    ((HAC+HFP+HSA+HBP)*(VAC+VFP+VSA+VBP)*(60)/1000)
static struct drm_display_mode default_mode = {
	.clock = PCLK_IN_KHZ_60HZ,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + VFP,
	.vsync_end = FRAME_HEIGHT + VFP + VSA,
	.vtotal = FRAME_HEIGHT + VFP + VSA + VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = DATA_RATE/2,

	//.bdg_ssc_disable = 1,
	//.ssc_disable = 1,
	.physical_width_um  = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
/*
	.dyn_fps = {
		.switch_en = 0,
	},
	.dyn = {
		.switch_en = 0,
		.pll_clk = DATA_RATE/2,
	},
*/
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x1C,
	},
	.data_rate = DATA_RATE,
};

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
	struct drm_display_mode *mode_60;

	mode_60 = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode_60) {
		dev_info(connector->dev->dev, "failed to add mode_60 %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode_60);
	mode_60->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_60);

	connector->display_info.width_mm = 67;
	connector->display_info.height_mm = 150;
	return 1;
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
	dsi->lanes = 2;
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
    strcpy(current_lcm_info.chip,"jd9161z");
    strcpy(current_lcm_info.vendor,"boe");
    sprintf(current_lcm_info.id,"0x%02x",0x02);
    strcpy(current_lcm_info.more,"480*1170");
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
	{ .compatible = "tddi,jd9161z,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "jd9161z_fwvga1170_dsi_vdo_boe",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("MEDIATEK");
MODULE_DESCRIPTION("sc jd9161z VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
