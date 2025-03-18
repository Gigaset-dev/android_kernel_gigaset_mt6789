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

#include "../../../misc/mediatek/gate_ic/gate_i2c.h"
#include "include/panel-nt37700c-vdo-120hz-vfp-6382-new.h"
/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */
/****************TPS65132***********/
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
//#include "lcm_i2c.h"

//static char bl_tb0[] = { 0x51, 0xff };
static int current_fps = 60;



struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	struct gpio_desc *pm_enable_gpio;
	struct gpio_desc *ldo_33v;
	bool prepared;
	bool enabled;

	unsigned int gate_ic;
	bool oled_screen;/* drv-add oled sysfs-pengzhipeng-20230306-end */
	bool hbm_en;
	bool hbm_wait;
	bool hbm_stat;   
	bool doze_en; //drv-Fixed the issue of entering aod and TP having touch-pengzhipeng-20230516

	int error;
};

struct lcm *g_ctx;

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

unsigned int bl_level;
static unsigned int g_current_level = 0;
/* prize add by liaoxingen for for G5GF, refer to x9, 20230207 start */
extern int mtk_drm_esd_check_status(void);
extern void mtk_drm_esd_set_status(int status);
/* prize add by liaoxingen for for G5GF, refer to x9, 20230207 end */

bool panel_fod_is_enabled(void)
{
	pr_info("panel %s g_ctx->doze_en=%d,g_ctx->prepared=%d\n", __func__,g_ctx->doze_en,g_ctx->prepared);
	if(g_ctx->doze_en)
	    return g_ctx->doze_en;
	else
		return !g_ctx->prepared;

}
EXPORT_SYMBOL(panel_fod_is_enabled);

/* prize add by liaoxingen for for G5GF, refer to X9-678, 20230207 start */
static void lcm_pannel_reconfig_blk(struct lcm *ctx)
{
	char bl_tb0[] = {0x51,0x0F,0xFF};
	unsigned int reg_level = 0;
	pr_err("[%s][%d]bl_level:%d , esd:%d \n",__func__,__LINE__,bl_level ,mtk_drm_esd_check_status());
	if(mtk_drm_esd_check_status()){
		/*PRIZE:Added by lvyuanchuan,X9-534,20230103*/
		
		reg_level = bl_level;
		bl_tb0[1] = (reg_level>>8)&0xf;
		bl_tb0[2] = (reg_level)&0xff;
		lcm_dcs_write(ctx,bl_tb0,ARRAY_SIZE(bl_tb0));
		//mtk_drm_esd_set_status(0);
	}
}
#if IS_ENABLED(CONFIG_PRIZE_COMMON_NODE)
extern void prize_common_node_show_register(char* name,bool(*hbm_set)(void));
#endif
bool get_hbmstate(void)
{
	printk("%s g_ctx->hbm_stat:%d",__func__, g_ctx->hbm_stat);
	return g_ctx->hbm_stat;
}

/* prize add by liaoxingen for for G5GF, refer to X9-678, 20230207 end */
static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	 char bl_tb0[] = {0x51,0x07,0xFF};
	 char hbm_tb[] = {0x51,0x0F,0xFF};
	 unsigned int level_normal = 0;

	 if(level > 4095)
	 {
	 	if(level == 4096)
	 	{
	 		printk("panel into HBM\n");
			if (!cb)
				return -1;
			g_ctx->hbm_stat = true;
			cb(dsi, handle, hbm_tb, ARRAY_SIZE(hbm_tb));
	 	}
	 	else if(level == 4097)
	 	{
	 		/*PRIZE:Added by lvyuanchuan,X9-534,20230103*/
	 		level_normal = bl_level;
			bl_tb0[1] = (level_normal>>8)&0xf;
			bl_tb0[2] = (level_normal)&0xff;
			if (!cb)
				return -1;
			printk("panel out HBM bl_level = %d\n",bl_level);
			g_ctx->hbm_stat = false;
			cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
			g_current_level = bl_level;
	 	}
	 }
	 else
	 {
	 	/*PRIZE:Added by lvyuanchuan,X9-534,20230103 */
		//if (level > 3600)
		///	level = 3600;//Modified maximum brightness to 500lux
	 	bl_tb0[1] = (level>>8)&0xf;
		bl_tb0[2] = (level)&0xff;
		pr_err("level %d \n",level);
		if (!cb)
			return -1;
		if(g_ctx->hbm_stat == false || level == 0) /* prize temp modify, sometimes Screen turns black after unlocking */ 
			cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
		/*PRIZE:Added by lvyuanchuan,X9-678,20221230*/
		if(level != 0 )  /* prize temp modify, sometimes Screen turns black after unlocking */
			bl_level = level;
		g_current_level = level;
	 }
	 return 0;
}

//prize add by gongtaitao for sensorhub get backlight 20221028 start
unsigned short led_level_disp_get(char *name)
{
    int trans_level = 0;
	trans_level = g_current_level;
	pr_err("[%s]: name: %s, level : %d",__func__, name, trans_level);
	return trans_level;
}
EXPORT_SYMBOL(led_level_disp_get);
//prize add by gongtaitao for sensorhub get backlight 20221028 end

static int panel_hbm_set_cmdq(struct drm_panel *panel, void *dsi,
			      dcs_write_gce cb, void *handle, bool en)
{
//	unsigned int level_hbm = 255;
	unsigned int level_normal = 125;
	char normal_tb0[] = {0x51, 0x07,0xFF};
	char hbm_tb[] = {0x51,0x0F,0xFF};
	struct lcm *ctx = panel_to_lcm(panel);

	if (!cb)
		return -1;

	//if (ctx->hbm_en == en)
	//	goto done;

	if (en)
	{
		printk("[panel] %s : set HBM\n",__func__);
	#if 0
		lcm_dcs_write_seq_static(ctx,0xF0,0x55, 0xAA, 0x52, 0x08, 0x00);   //ELVSS
		udelay(100);
		lcm_dcs_write_seq_static(ctx,0xB5,0x80,0x80);
		lcm_dcs_write_seq_static(ctx,0x6F,0x07);
		lcm_dcs_write_seq_static(ctx,0xB5,0x1D);
	#endif
		/*PRIZE:Added by durunshen,X9-677,20230106 start*/
		g_ctx->hbm_stat = true;
		/*PRIZE:Added by durunshen,X9-677,20230106 end*/
		cb(dsi, handle, hbm_tb, ARRAY_SIZE(hbm_tb));
	}
	else
	{
		printk("[panel] %s : set normal = %d\n",__func__,bl_level);
		/*PRIZE:Added by lvyuanchuan,X9-534,20230103*/
		level_normal = bl_level;
		normal_tb0[1] = (level_normal>>8)&0xff;
		normal_tb0[2] = (level_normal)&0xff;
	#if 0
		lcm_dcs_write_seq_static(ctx,0xF0,0x55, 0xAA, 0x52, 0x08, 0x00);   //ELVSS
		udelay(100);
		lcm_dcs_write_seq_static(ctx,0xB5,0x80,0x80);
		lcm_dcs_write_seq_static(ctx,0x6F,0x07);
		lcm_dcs_write_seq_static(ctx,0xB5,0x23);
	#endif
		/*PRIZE:Added by durunshen,X9-677,20230106 start*/
		g_ctx->hbm_stat = false;
		/*PRIZE:Added by durunshen,X9-677,20230106 end*/
		cb(dsi, handle, normal_tb0, ARRAY_SIZE(normal_tb0));
	}

	ctx->hbm_en = en;
	ctx->hbm_wait = true;

// done:
	return 0;
}

static void panel_hbm_get_state(struct drm_panel *panel, bool *state)
{
	struct lcm *ctx = panel_to_lcm(panel);

	*state = ctx->hbm_en;
	
	printk("[panel] %s : ctx->hbm_en = %d\n",__func__,ctx->hbm_en);
}

static void lcm_panel_init(struct lcm *ctx)
{
	lcm_dcs_write_seq_static(ctx,0xFE,0xF4);
	lcm_dcs_write_seq_static(ctx,0x00,0xFF);
	lcm_dcs_write_seq_static(ctx,0x01,0x63);
	lcm_dcs_write_seq_static(ctx,0x02,0x88);
	lcm_dcs_write_seq_static(ctx,0x03,0x9B);
	lcm_dcs_write_seq_static(ctx,0x04,0x4C);
	lcm_dcs_write_seq_static(ctx,0x05,0xF4);
	lcm_dcs_write_seq_static(ctx,0x06,0xE0);
	lcm_dcs_write_seq_static(ctx,0x07,0xF3);
	lcm_dcs_write_seq_static(ctx,0x08,0x0F);
	lcm_dcs_write_seq_static(ctx,0x09,0x40);
	lcm_dcs_write_seq_static(ctx,0x0A,0xCC);
	lcm_dcs_write_seq_static(ctx,0x0B,0xA4);
	lcm_dcs_write_seq_static(ctx,0x0C,0x82);
	lcm_dcs_write_seq_static(ctx,0x0D,0x08);
	lcm_dcs_write_seq_static(ctx,0xFE,0xF4);
	lcm_dcs_write_seq_static(ctx,0x1B,0xF0);
	lcm_dcs_write_seq_static(ctx,0x1C,0xFF);
	lcm_dcs_write_seq_static(ctx,0x1D,0xD2);
	lcm_dcs_write_seq_static(ctx,0x1E,0xD2);
	lcm_dcs_write_seq_static(ctx,0x1F,0xFE);
	lcm_dcs_write_seq_static(ctx,0x20,0x49);
	lcm_dcs_write_seq_static(ctx,0x21,0x0F);
	lcm_dcs_write_seq_static(ctx,0x22,0x3E);
	lcm_dcs_write_seq_static(ctx,0x23,0xFF);
	lcm_dcs_write_seq_static(ctx,0x24,0x00);
	lcm_dcs_write_seq_static(ctx,0x25,0xC4);
	lcm_dcs_write_seq_static(ctx,0x26,0x4C);
	lcm_dcs_write_seq_static(ctx,0x27,0x2A);
	lcm_dcs_write_seq_static(ctx,0x28,0x88);
	lcm_dcs_write_seq_static(ctx,0x29,0x00);
	lcm_dcs_write_seq_static(ctx,0xFE,0xF4);
	lcm_dcs_write_seq_static(ctx,0x0D,0xC0);
	lcm_dcs_write_seq_static(ctx,0x0E,0xFF);
	lcm_dcs_write_seq_static(ctx,0x0F,0xCB);
	lcm_dcs_write_seq_static(ctx,0x10,0x4B);
	lcm_dcs_write_seq_static(ctx,0x11,0xCD);
	lcm_dcs_write_seq_static(ctx,0x12,0x2C);
	lcm_dcs_write_seq_static(ctx,0x13,0x3D);
	lcm_dcs_write_seq_static(ctx,0x14,0xF8);
	lcm_dcs_write_seq_static(ctx,0x15,0xFC);
	lcm_dcs_write_seq_static(ctx,0x16,0x03);
	lcm_dcs_write_seq_static(ctx,0x17,0x10);
	lcm_dcs_write_seq_static(ctx,0x18,0x33);
	lcm_dcs_write_seq_static(ctx,0x19,0xA9);
	lcm_dcs_write_seq_static(ctx,0x1A,0x20);
	lcm_dcs_write_seq_static(ctx,0x1B,0x02);
	
//drv-Solving probable black screen problems-pengzhipeng-20230309-start
//ESD recover code
    lcm_dcs_write_seq_static(ctx,0xFE,0xD4);  //Page D4
    lcm_dcs_write_seq_static(ctx,0x40,0x03);  // lvd/mipi error flag enable
    lcm_dcs_write_seq_static(ctx,0xFE,0xFD); //Page FD
    lcm_dcs_write_seq_static(ctx,0x80,0x06);  //error report to 0A00 enable
    lcm_dcs_write_seq_static(ctx,0x83,0x00);  //command 1 OFF  00 Page
    lcm_dcs_write_seq_static(ctx,0xFE,0xA0);  //Page A0
    lcm_dcs_write_seq_static(ctx,0x06,0x36);  //lvd function VCI/VDDI/AVDD
    lcm_dcs_write_seq_static(ctx,0xFE,0xA1);  //Page A1
	lcm_dcs_write_seq_static(ctx,0xB3,0x7F);  //7f=hs cmd off 1f=hs cmd on
    lcm_dcs_write_seq_static(ctx,0x74,0x70);
    lcm_dcs_write_seq_static(ctx,0xC3,0xC3);
    lcm_dcs_write_seq_static(ctx,0xC4,0x9C);
    lcm_dcs_write_seq_static(ctx,0xC5,0x1E);
    lcm_dcs_write_seq_static(ctx,0xC6,0x23);
    lcm_dcs_write_seq_static(ctx,0xFE,0xD0);
    lcm_dcs_write_seq_static(ctx,0x11,0x75);
    lcm_dcs_write_seq_static(ctx,0x92,0x03);
//drv-Solving probable black screen problems-pengzhipeng-20230309-end
	lcm_dcs_write_seq_static(ctx,0xFE,0x40);
	lcm_dcs_write_seq_static(ctx,0xBD,0x00);
	lcm_dcs_write_seq_static(ctx,0xFE,0xD0);
	lcm_dcs_write_seq_static(ctx,0x86,0x14);
//drv-modify 10bit-pengzhipeng-20230429-start	
	 lcm_dcs_write_seq_static(ctx,0xFE,0xD2);// switch to D2 page
 lcm_dcs_write_seq_static(ctx,0x50,0x11);// pps000
 lcm_dcs_write_seq_static(ctx,0x51,0xab);// pps003
 lcm_dcs_write_seq_static(ctx,0x52,0x30);// pps004
 lcm_dcs_write_seq_static(ctx,0x53,0x09);// pps006
 lcm_dcs_write_seq_static(ctx,0x54,0x6c);// pps007
 lcm_dcs_write_seq_static(ctx,0x55,0x04);// pps008
 lcm_dcs_write_seq_static(ctx,0x56,0x38);// pps009
 lcm_dcs_write_seq_static(ctx,0x58,0x00);// pps010
 lcm_dcs_write_seq_static(ctx,0x59,0x0c);// pps011
 lcm_dcs_write_seq_static(ctx,0x5a,0x02);// pps012
 lcm_dcs_write_seq_static(ctx,0x5b,0x1c);// pps013
 lcm_dcs_write_seq_static(ctx,0x5c,0x01);// pps016
 lcm_dcs_write_seq_static(ctx,0x5d,0x9a);// pps017
 lcm_dcs_write_seq_static(ctx,0x5e,0x19);// pps021
 lcm_dcs_write_seq_static(ctx,0x5f,0x01);// pps022
 lcm_dcs_write_seq_static(ctx,0x60,0x03);// pps023
 lcm_dcs_write_seq_static(ctx,0x61,0x00);// pps024
 lcm_dcs_write_seq_static(ctx,0x62,0x0a);// pps025
 lcm_dcs_write_seq_static(ctx,0x63,0x0c);// pps027
 lcm_dcs_write_seq_static(ctx,0x64,0x08);// pps028
 lcm_dcs_write_seq_static(ctx,0x65,0xbb);// pps029
 lcm_dcs_write_seq_static(ctx,0x66,0x0a);// pps030
 lcm_dcs_write_seq_static(ctx,0x67,0x5f);// pps031
 lcm_dcs_write_seq_static(ctx,0x68,0x16);// pps032
 lcm_dcs_write_seq_static(ctx,0x69,0x00);// pps033
 lcm_dcs_write_seq_static(ctx,0x6a,0x10);// pps034
 lcm_dcs_write_seq_static(ctx,0x6b,0xec);// pps035
 lcm_dcs_write_seq_static(ctx,0x6c,0x07);// pps036
 lcm_dcs_write_seq_static(ctx,0x6d,0x10);// pps037
 lcm_dcs_write_seq_static(ctx,0x6e,0x20);// pps038
 lcm_dcs_write_seq_static(ctx,0x6f,0x00);// pps039
 lcm_dcs_write_seq_static(ctx,0x70,0x06);// pps040
 lcm_dcs_write_seq_static(ctx,0x71,0x0f);// pps041
 lcm_dcs_write_seq_static(ctx,0x72,0x0f);// pps042
 lcm_dcs_write_seq_static(ctx,0x73,0x33);// pps043
 lcm_dcs_write_seq_static(ctx,0x74,0x0e);// pps044
 lcm_dcs_write_seq_static(ctx,0x75,0x1c);// pps045
 lcm_dcs_write_seq_static(ctx,0x76,0x2a);// pps046
 lcm_dcs_write_seq_static(ctx,0x77,0x38);// pps047
 lcm_dcs_write_seq_static(ctx,0x78,0x46);// pps048
 lcm_dcs_write_seq_static(ctx,0x79,0x54);// pps049
 lcm_dcs_write_seq_static(ctx,0x7a,0x62);// pps050
 lcm_dcs_write_seq_static(ctx,0x7b,0x69);// pps051
 lcm_dcs_write_seq_static(ctx,0x7c,0x70);// pps052
 lcm_dcs_write_seq_static(ctx,0x7d,0x77);// pps053
 lcm_dcs_write_seq_static(ctx,0x7e,0x79);// pps054
 lcm_dcs_write_seq_static(ctx,0x7f,0x7b);// pps055
 lcm_dcs_write_seq_static(ctx,0x80,0x7d);// pps056
 lcm_dcs_write_seq_static(ctx,0x81,0x7e);// pps057
 lcm_dcs_write_seq_static(ctx,0x82,0x01);// pps058
 lcm_dcs_write_seq_static(ctx,0x83,0xc2);// pps059
 lcm_dcs_write_seq_static(ctx,0x84,0x22);// pps060
 lcm_dcs_write_seq_static(ctx,0x85,0x00);// pps061
 lcm_dcs_write_seq_static(ctx,0x86,0x2a);// pps062
 lcm_dcs_write_seq_static(ctx,0x87,0x40);// pps063
 lcm_dcs_write_seq_static(ctx,0x88,0x32);// pps064
 lcm_dcs_write_seq_static(ctx,0x89,0xbe);// pps065
 lcm_dcs_write_seq_static(ctx,0x8a,0x3a);// pps066
 lcm_dcs_write_seq_static(ctx,0x8b,0xfc);// pps067
 lcm_dcs_write_seq_static(ctx,0x8c,0x3a);// pps068
 lcm_dcs_write_seq_static(ctx,0x8d,0xfa);// pps069
 lcm_dcs_write_seq_static(ctx,0x8e,0x3a);// pps070
 lcm_dcs_write_seq_static(ctx,0x8f,0xf8);// pps071
 lcm_dcs_write_seq_static(ctx,0x90,0x3b);// pps072
 lcm_dcs_write_seq_static(ctx,0x91,0x38);// pps073
 lcm_dcs_write_seq_static(ctx,0x92,0x3b);// pps074
 lcm_dcs_write_seq_static(ctx,0x93,0x78);// pps075
 lcm_dcs_write_seq_static(ctx,0x94,0x3b);// pps076
 lcm_dcs_write_seq_static(ctx,0x95,0x76);// pps077
 lcm_dcs_write_seq_static(ctx,0x96,0x4b);// pps078
 lcm_dcs_write_seq_static(ctx,0x97,0xb6);// pps079
 lcm_dcs_write_seq_static(ctx,0x98,0x4b);// pps080
 lcm_dcs_write_seq_static(ctx,0x99,0xf6);// pps081
 lcm_dcs_write_seq_static(ctx,0x9a,0x4c);// pps082
 lcm_dcs_write_seq_static(ctx,0x9b,0x34);// pps083
 lcm_dcs_write_seq_static(ctx,0x9c,0x5c);// pps084
 lcm_dcs_write_seq_static(ctx,0x9d,0x74);// pps085
 lcm_dcs_write_seq_static(ctx,0x9e,0x8c);// pps086
 lcm_dcs_write_seq_static(ctx,0x9f,0xf4);// pps087
 lcm_dcs_write_seq_static(ctx,0xa2,0x02);// pps014
 lcm_dcs_write_seq_static(ctx,0xa3,0xa3);// pps015
 lcm_dcs_write_seq_static(ctx,0xa4,0x00);// pps088
 lcm_dcs_write_seq_static(ctx,0xa5,0x00);// pps089
 lcm_dcs_write_seq_static(ctx,0xa6,0x00);// pps090
 lcm_dcs_write_seq_static(ctx,0xa7,0x00);// pps091
 lcm_dcs_write_seq_static(ctx,0xa9,0x00);// pps092
 lcm_dcs_write_seq_static(ctx,0xaa,0x00);// pps093
 lcm_dcs_write_seq_static(ctx,0xa0,0xa0);// pps005
//drv-modify 10bit-pengzhipeng-20230429-end
	lcm_dcs_write_seq_static(ctx,0xFE,0xa1);
	lcm_dcs_write_seq_static(ctx,0xCA,0x80);
	lcm_dcs_write_seq_static(ctx,0xCD,0x00);
	lcm_dcs_write_seq_static(ctx,0xCE,0x00);

	lcm_dcs_write_seq_static(ctx,0xFE,0x00);
	lcm_dcs_write_seq_static(ctx,0xFA,0x01); 
	lcm_dcs_write_seq_static(ctx,0xC2,0x08);
	lcm_dcs_write_seq_static(ctx,0x35,0x00);
	//lcm_dcs_write_seq_static(ctx,0x51,0x0D,0xBB);//drv-To solve the problem that wake up from sleep will light up instantly-pengzhipeng-2022300510-start
	
	lcm_pannel_reconfig_blk(ctx);
	lcm_dcs_write_seq_static(ctx,0x11);
	mdelay(120);
	lcm_dcs_write_seq_static(ctx,0x29);
	mdelay(10);
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

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(5);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(110);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (!IS_ERR_OR_NULL(ctx->reset_gpio)) {
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	}
	mdelay(5);
	//vci gpio154
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	usleep_range(2000, 2001);
	//dvdd gpio157
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);
	mdelay(5);
	//vddi gpio150
	ctx->pm_enable_gpio = devm_gpiod_get_index(ctx->dev,
  		"pm-enable", 0, GPIOD_OUT_HIGH);
  	
  	gpiod_set_value(ctx->pm_enable_gpio, 0);
  	devm_gpiod_put(ctx->dev, ctx->pm_enable_gpio);
	mdelay(5);

	/*ctx->ldo_33v = devm_gpiod_get_index(ctx->dev,
  		"ldo-33v", 0, GPIOD_OUT_HIGH);
  	gpiod_set_value(ctx->ldo_33v, 0);
  	devm_gpiod_put(ctx->dev, ctx->ldo_33v);*/
	ctx->error = 0;
	ctx->prepared = false;
	ctx->doze_en = false;
	ctx->hbm_en = false;
	ctx->hbm_stat = false; /* drv modify update hbm_stat, k70 sometimes don not light up */
	pr_info("%s-\n", __func__);
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;
	//vddi gpio150
	ctx->pm_enable_gpio = devm_gpiod_get_index(ctx->dev,
  		"pm-enable", 0, GPIOD_OUT_HIGH);
  	
  	gpiod_set_value(ctx->pm_enable_gpio, 1);
  	devm_gpiod_put(ctx->dev, ctx->pm_enable_gpio);
	mdelay(5);
	//dvdd gpio157
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);
	mdelay(5);
	//vci gpio154
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	mdelay(5);

	// lcd reset H -> L -> L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10000, 10001);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(10);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(60);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	// end

	/*ctx->ldo_33v = devm_gpiod_get_index(ctx->dev,
  		"ldo-33v", 0, GPIOD_OUT_HIGH);
  	gpiod_set_value(ctx->ldo_33v, 1);
  	devm_gpiod_put(ctx->dev, ctx->ldo_33v);
	msleep(20);	*/
	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

#ifdef VENDOR_EDIT
	// shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
	lcd_queue_load_tp_fw();
#endif
/*modified by driver ---20230223 start*/
	pr_info("%s-ret = %d\n", __func__,ret);
	return 0;
/*modified by driver ---20230223 end*/
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

#define VAC (2412)
#define HAC (1080)
//drv Modify pclk-pengzhipeng-20220217-start
#define PCLK_IN_KHZ_60 \
    ((FRAME_WIDTH+MODE_0_HFP+HSA+HBP)*(FRAME_HEIGHT+MODE_0_VFP+VSA+VBP)*(60)/1000) //2500
#define PCLK_IN_KHZ_90 \
    ((FRAME_WIDTH+MODE_0_HFP+HSA+HBP)*(FRAME_HEIGHT+MODE_0_VFP+VSA+VBP)*(90)/1000)
#define PCLK_IN_KHZ_120 \
    ((FRAME_WIDTH+MODE_0_HFP+HSA+HBP)*(FRAME_HEIGHT+MODE_0_VFP+VSA+VBP)*(120)/1000)
//drv Modify pclk-pengzhipeng-20220217-end	
static const struct drm_display_mode switch_mode_120 = {
	.clock = PCLK_IN_KHZ_120,//drv Modify 120 pclk-pengzhipeng-20220217
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_2_HFP,
	.hsync_end = FRAME_WIDTH + MODE_2_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_2_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_2_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_2_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_2_VFP + VSA + VBP,
};


static const struct drm_display_mode switch_mode_90 = {
	.clock = PCLK_IN_KHZ_90,//drv Modify 90 pclk-pengzhipeng-20220217
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_1_HFP,
	.hsync_end = FRAME_WIDTH + MODE_1_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_1_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_1_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_1_VFP + VSA + VBP,
};

static const struct drm_display_mode default_mode = {
	.clock = PCLK_IN_KHZ_60,//drv Modify 60 pclk-pengzhipeng-20220217
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_0_HFP,
	.hsync_end = FRAME_WIDTH + MODE_0_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_0_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_0_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_0_VFP + VSA + VBP,
};
/*
static const struct drm_display_mode performance_mode_90hz = {
	.clock = 325202,
	.hdisplay = 1080,
	.hsync_start = 1080 + 309,//HFP
	.hsync_end = 1080 + 309 + 12,//HSA
	.htotal = 1080 + 309 + 12 + 56,//HBP
	.vdisplay = 2400,
	.vsync_start = 2400 + 60,//VFP
	.vsync_end = 2400 + 60 + 10,//VSA
	.vtotal = 2400 + 60 + 10 + 10,//VBP
};

static const struct drm_display_mode performance_mode_120hz = {
	.clock = 372000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 102,//HFP
	.hsync_end = 1080 + 102 + 12,//HSA
	.htotal = 1080 + 102 + 12 + 56,//HBP
	.vdisplay = 2400,
	.vsync_start = 2400 + 60,//VFP
	.vsync_end = 2400 + 60 + 10,//VSA
	.vtotal = 2400 + 60 + 10 + 10,//VBP
};
*/
#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = 500,//drv-Solve the problem of the top of the probabilistic screen-pengzhipeng-202230213-start
	//.vfp_low_power = 72,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	/*drv-During the ESD call process, the MIPI may have an Error report callback action, 
		  which may result in an error during the first read. Here, read the 0x0B register first, 
		  but ignore this read back value. Do not use the 0x0B read back value as a reference for recovery, 
		  and use the subsequent 0x0A and 0xFA read back values as a reference for recovery-pengzhipeng-20230324-start*/
	.lcm_esd_check_table[0] = {
		.cmd = 0x0B,
		.count = 1,
		.para_list[0] = 0x00,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0xFA,
		.count = 1,
		.para_list[0] = 0x01,
	},
	/*drv-During the ESD call process, the MIPI may have an Error report callback action, 
		  which may result in an error during the first read. Here, read the 0x0B register first, 
		  but ignore this read back value. Do not use the 0x0B read back value as a reference for recovery, 
		  and use the subsequent 0x0A and 0xFA read back values as a reference for recovery-pengzhipeng-20230324-end*/
	.lp_perline_en = 1,//drv Solve the problem of splash screen at the bottom center of the screen-pengzhipeng-20230228
	//drv-Resolve the AnTuTu detection screen size of 6.57 inches, which is inconsistent with the product definition of 6.7-pengzhipeng-20230324-start
	.physical_width_um = 69552,
	.physical_height_um = 155330,
		//drv-Resolve the AnTuTu detection screen size of 6.57 inches, which is inconsistent with the product definition of 6.7-pengzhipeng-20230324-end
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
	.data_rate = 1000,//drv-Solve the problem of the top of the probabilistic screen-pengzhipeng-202230213-start

};
static struct mtk_panel_params ext_params_90 = {
	.pll_clk = 500,//drv-Solve the problem of the top of the probabilistic screen-pengzhipeng-202230213-start
	//.vfp_low_power = 72,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	/*drv-During the ESD call process, the MIPI may have an Error report callback action, 
		  which may result in an error during the first read. Here, read the 0x0B register first, 
		  but ignore this read back value. Do not use the 0x0B read back value as a reference for recovery, 
		  and use the subsequent 0x0A and 0xFA read back values as a reference for recovery-pengzhipeng-20230324-start*/
	.lcm_esd_check_table[0] = {
		.cmd = 0x0B,
		.count = 1,
		.para_list[0] = 0x00,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0xFA,
		.count = 1,
		.para_list[0] = 0x01,
	},
	/*drv-During the ESD call process, the MIPI may have an Error report callback action, 
		  which may result in an error during the first read. Here, read the 0x0B register first, 
		  but ignore this read back value. Do not use the 0x0B read back value as a reference for recovery, 
		  and use the subsequent 0x0A and 0xFA read back values as a reference for recovery-pengzhipeng-20230324-end*/
	.lp_perline_en = 1,//drv Solve the problem of splash screen at the bottom center of the screen-pengzhipeng-20230228
		//drv-Resolve the AnTuTu detection screen size of 6.57 inches, which is inconsistent with the product definition of 6.7-pengzhipeng-20230324-start
	.physical_width_um = 69552,
	.physical_height_um = 155330,
		//drv-Resolve the AnTuTu detection screen size of 6.57 inches, which is inconsistent with the product definition of 6.7-pengzhipeng-20230324-end
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
	.data_rate = 1000,//drv-Solve the problem of the top of the probabilistic screen-pengzhipeng-202230213-start

};

static struct mtk_panel_params ext_params_120 = {
	.pll_clk = 500,//drv-Solve the problem of the top of the probabilistic screen-pengzhipeng-202230213-start
	//.vfp_low_power = 72,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	/*drv-During the ESD call process, the MIPI may have an Error report callback action, 
		  which may result in an error during the first read. Here, read the 0x0B register first, 
		  but ignore this read back value. Do not use the 0x0B read back value as a reference for recovery, 
		  and use the subsequent 0x0A and 0xFA read back values as a reference for recovery-pengzhipeng-20230324-start*/
	.lcm_esd_check_table[0] = {
		.cmd = 0x0B,
		.count = 1,
		.para_list[0] = 0x00,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0xFA,
		.count = 1,
		.para_list[0] = 0x01,
	},
	/*drv-During the ESD call process, the MIPI may have an Error report callback action, 
		  which may result in an error during the first read. Here, read the 0x0B register first, 
		  but ignore this read back value. Do not use the 0x0B read back value as a reference for recovery, 
		  and use the subsequent 0x0A and 0xFA read back values as a reference for recovery-pengzhipeng-20230324-end*/
	.lp_perline_en = 1,//drv Solve the problem of splash screen at the bottom center of the screen-pengzhipeng-20230228
	//drv-Resolve the AnTuTu detection screen size of 6.57 inches, which is inconsistent with the product definition of 6.7-pengzhipeng-20230324-start
	.physical_width_um = 69552,
	.physical_height_um = 155330,
	//drv-Resolve the AnTuTu detection screen size of 6.57 inches, which is inconsistent with the product definition of 6.7-pengzhipeng-20230324-end
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
	.data_rate = 1000,//drv-Solve the problem of the top of the probabilistic screen-pengzhipeng-202230213-start

};

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
		printk("[panel] %s\n",__func__);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
			lcm_dcs_write_seq_static(ctx, 0xBD, 0x05);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	}
}

static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
		printk("[panel] %s\n",__func__);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
			lcm_dcs_write_seq_static(ctx, 0xBD, 0x06);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	}
}


static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
		printk("[panel] %s\n",__func__);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
			lcm_dcs_write_seq_static(ctx, 0xBD, 0x00);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	}
}


static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}


/*
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
*/
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

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id_hfp(connector, dst_mode);
	printk("[panel] %s cur_mode=%d dst_mode=%d drm_mode_vrefresh(m)=%d\n",__func__, cur_mode,dst_mode, drm_mode_vrefresh(m));
	if (cur_mode == dst_mode)
		return ret;

	if (drm_mode_vrefresh(m) == MODE_0_FPS) { /*switch to 60 */
		mode_switch_to_60(panel, stage);
	} else if (drm_mode_vrefresh(m)== MODE_1_FPS) { /*switch to 90 */
		mode_switch_to_90(panel, stage);
	} else if (drm_mode_vrefresh(m) == MODE_2_FPS) { /*switch to 120 */
		mode_switch_to_120(panel, stage);
	} else
		ret = 1;

	return ret;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id_hfp(connector, mode);

	if (!m) {
		pr_err("%s:%d invalid display_mode\n", __func__, __LINE__);
		return ret;
	}

	if (drm_mode_vrefresh(m) == 60)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == 90)
		ext->params = &ext_params_90;
	else if (drm_mode_vrefresh(m) == 120)
		ext->params = &ext_params_120;
	else
		ret = 1;

	if (!ret)
		current_fps = drm_mode_vrefresh(m);

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
//drv:add aod func-pengzhipeng-202230213-start
/*static int panel_doze_enable_start(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("panel %s\n", __func__);
	panel_ext_reset(panel, 0);
	usleep_range(10 * 1000, 15 * 1000);
	panel_ext_reset(panel, 1);

	lcm_dcs_write_seq_static(ctx, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(120);
	return 0;
}

*/
//drv-Fixed the issue of entering aod and TP having touch-pengzhipeng-20230516-start
bool drv_doze_state(void)
{
	return g_ctx->doze_en;
}
EXPORT_SYMBOL_GPL(drv_doze_state);
//drv-Fixed the issue of entering aod and TP having touch-pengzhipeng-20230516-end
//drv-Solve the problem of high power consumption of aod-pengzhipeng-20230516-start
static int panel_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);
	//int data = MTK_DISP_BLANK_POWERDOWN;
	//drv-Fixed the issue of entering aod and TP having touch-pengzhipeng-20230516-start
	if(!ctx->doze_en)
		ctx->doze_en = true;
	//drv-Fixed the issue of entering aod and TP having touch-pengzhipeng-20230516-end
	pr_info("pzp 3333 panel %s\n", __func__);
	
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
	/*Inter power on*/
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x50);

	/*Page00*/
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	/*Idle mode on*/
	lcm_dcs_write_seq_static(ctx, 0x39);
	/*Enter AOD 30nit*/
	lcm_dcs_write_seq_static(ctx, 0x51, 0x09, 0x56);
	return 0;
}
//drv-Solve the problem of high power consumption of aod-pengzhipeng-20230516-end
//drv-Solve the problem of high power consumption of aod-pengzhipeng-20230516-start
static int panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);
	//drv-Fixed the issue of entering aod and TP having touch-pengzhipeng-20230516-start
	if(ctx->doze_en)
		ctx->doze_en = false;
	//drv-Fixed the issue of entering aod and TP having touch-pengzhipeng-20230516-end
	pr_info("pzp 222 panel %s\n", __func__);
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
	/*Inter power on*/
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x4F);
	/*Page00*/
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	/*Idle mode off*/
	lcm_dcs_write_seq_static(ctx, 0x38);

	return 0;
}
//drv-Solve the problem of high power consumption of aod-pengzhipeng-20230516-end

static int panel_set_aod_light_mode(void *dsi,
	dcs_write_gce cb, void *handle, unsigned int mode)
{
	//int i = 0;

	pr_info("panel %s\n", __func__);

	if (mode >= 1) {
		/*Enter AOD 50nit*/
		pr_info("panel %s Enter AOD 50nit\n", __func__);
		lcm_dcs_write_seq_static(g_ctx, 0x51, 0x09, 0x56);
	} else {
		/*Enter AOD 30nit*/
		pr_info("panel %s Enter AOD 30nit\n", __func__);
		lcm_dcs_write_seq_static(g_ctx, 0x51, 0x00, 0x05);
	}
	pr_info("%s : %d !\n", __func__, mode);

	return 0;
}
//drv:add aod func-pengzhipeng-202230213-end
static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.ata_check = panel_ata_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.mode_switch = mode_switch,
	.hbm_set_cmdq = panel_hbm_set_cmdq,
	.hbm_get_state = panel_hbm_get_state,
//drv:add aod func-pengzhipeng-202230213-start
	/*aod mode*/
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
	.set_aod_light_mode = panel_set_aod_light_mode,
//drv:add aod func-pengzhipeng-202230213-end
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
	struct drm_display_mode *mode_1;
	struct drm_display_mode *mode_2;


	mode = drm_mode_duplicate(connector->dev, &switch_mode_120);
	printk("switch_mode_120 %ux%ux@%u\n",
			 switch_mode_120.hdisplay, switch_mode_120.vdisplay,
			 drm_mode_vrefresh(&switch_mode_120));
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 switch_mode_120.hdisplay, switch_mode_120.vdisplay,
			 drm_mode_vrefresh(&switch_mode_120));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode);

	mode_1 = drm_mode_duplicate(connector->dev, &default_mode);
	printk("default_mode %ux%ux@%u\n",
			 default_mode.hdisplay, default_mode.vdisplay,
			 drm_mode_vrefresh(&default_mode));
	if (!mode_1) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 default_mode.hdisplay, default_mode.vdisplay,
			 drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_1);
	printk("[panel] %s,222\n",__func__);

	mode_2 = drm_mode_duplicate(connector->dev, &switch_mode_90);
	printk("switch_mode_90 %ux%ux@%u\n",
			 switch_mode_90.hdisplay, switch_mode_90.vdisplay,
			 drm_mode_vrefresh(&switch_mode_90));
	if (!mode_2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 switch_mode_90.hdisplay, switch_mode_90.vdisplay,
			 drm_mode_vrefresh(&switch_mode_90));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_2);
	mode_2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_2);
	printk("[panel] %s,333\n",__func__);

	
	connector->display_info.width_mm = 70;
		//drv-Resolve the AnTuTu detection screen size of 6.57 inches, which is inconsistent with the product definition of 6.7-pengzhipeng-20230324-start
	connector->display_info.height_mm = 155;
		//drv-Resolve the AnTuTu detection screen size of 6.57 inches, which is inconsistent with the product definition of 6.7-pengzhipeng-20230324-end

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};


/* drv-add oled sysfs-pengzhipeng-20230306-start */
static ssize_t oled_show(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
   int count = 0;
   
   count = snprintf(buf, PAGE_SIZE, "oled screens:%s\n",
					g_ctx->oled_screen ? "yes" : "no");  
   return count;
}
 
static ssize_t oled_store(struct device *dev,
           struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}
 
static DEVICE_ATTR(oled, 0664, oled_show, oled_store);

int oled_sysfs_add(struct platform_device *pdev)
{
	int err = 0;
    pr_err("Add gezi device attr groups,gezi_sysfs_add\n");
  	err = device_create_file(&pdev->dev, &dev_attr_oled);
	if (err) {
        pr_err("sys file creation failed\n");
        return -ENODEV;
	}
	return 0;
}

static const struct of_device_id oled_of_match[] = {
	{.compatible = "mediatek,oled",},
	{},
};
MODULE_DEVICE_TABLE(of, oled_of_match);


static int gesture_probe(struct platform_device *pdev)
{

	printk("%s\n",__func__);
	oled_sysfs_add(pdev);
	return 0;
}


static struct platform_driver oled_driver = {
	.probe = gesture_probe,
	.driver = {
		   .name = "oled",
		   .of_match_table = oled_of_match,
	},
};

int oled_init(void)
{
	printk("%s\n",__func__);
	return platform_driver_register(&oled_driver);
}
/* drv-add oled sysfs-pengzhipeng-20230306-end */



static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	unsigned int value;
	int ret;

	pr_info("%s+ lcm,nt36672e,vdo,120hz\n", __func__);

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
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET | MIPI_DSI_CLOCK_NON_CONTINUOUS;;

	ret = of_property_read_u32(dev->of_node, "gate-ic", &value);
	if (ret < 0)
		value = 0;
	else
		ctx->gate_ic = value;

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
    oled_init();/* drv-add oled sysfs-pengzhipeng-20230306-end */
	g_ctx = ctx;
	ctx->hbm_en = false;
	g_ctx->hbm_stat = false;
	g_ctx->oled_screen = true;/* drv-add oled sysfs-pengzhipeng-20230306-end */
	g_ctx->doze_en = false;//drv-Fixed the issue of entering aod and TP having touch-pengzhipeng-20230516
#if IS_ENABLED(CONFIG_PRIZE_COMMON_NODE)
    prize_common_node_show_register("HBMSTATE", &get_hbmstate);
#endif    
	pr_info("%s- lcm,nt36672e,vdo,120hz,hfp\n", __func__);

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
	    .compatible = "jdi,nt36672e,vdo,120hz,hfp",
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-jdi-nt36672e-vdo-120hz-hfp",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("shaohua deng <shaohua.deng@mediatek.com>");
MODULE_DESCRIPTION("lcm NT36672E VDO 120HZ AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");
