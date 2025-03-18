/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 IMX682mipi_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "imx682mipiraw_Sensor.h"
//#include "imx682_eeprom.h"

#define PFX "IMX682_camera_sensor"

#define DEVICE_VERSION_IMX682     "imx682"
#define LOG_INF(format, args...)	pr_info(PFX "[%s] " format, __func__, ##args)
static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 765	/* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 3
#endif

// prize add chenwenhui 20240117 for QSC/LRC start
#define LRC_QSC_CALIBRATION  1
#if LRC_QSC_CALIBRATION
#define EEPROM_READ_ID 0xA0  //eeprom

#define LRC_START_ADDR 0x1B7A  //lrc start add
#define LRC_CALIBRATION_DATA_LENGTH 504

#define QSC_START_ADDR 0x0FAA //qsc start add
#define QSC_CALIBRATION_DATA_LENGTH 3024

#define ADDR_SENSOR_QSC   0xCA00
#define ADDR_SENSOR_LRC_0   0x7B00
#define ADDR_SENSOR_LRC_1   0x7C00

static kal_uint16 imx682_QSC_setting[QSC_CALIBRATION_DATA_LENGTH * 2]= {0};
static kal_uint16 imx682_LRC_setting[LRC_CALIBRATION_DATA_LENGTH*2]= {0};
#endif
// prize add chenwenhui 20240117 for QSC/LRC end

#define PDAF_MODE_SUPPORT  1

/* Prize add chenwenhui 20240228 for long exposure start */
#define LONG_EXP 1
/* Prize add chenwenhui 20240228 for long exposure end */

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX682_SENSOR_ID,
	.checksum_value = 0xb340d5a6,

	.pre = { /* reg_J 4000x3000 @59.89fps*/
		.pclk = 1097884800,
		.linelength = 9432,
		.framelength = 3880,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4624,
		.grabwindow_height = 3472,
		.mipi_data_lp2hs_settle_dc = 85,//0x22,
		/* following for GetDefaultFramerateByScenario() */
		.mipi_pixel_rate = 757030000,
		.max_framerate = 300, /* 60fps */
	},
	.cap = { /*reg_A 12M@30fps*/
		.pclk = 1097884800,
		.linelength = 9432,
		.framelength = 3880,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4624,
		.grabwindow_height = 3472,
		.mipi_data_lp2hs_settle_dc = 85,//0x22,
		.mipi_pixel_rate = 757030000,
		.max_framerate = 300,
	},
	// .cap1= {
		// /*setting for normal binning*/
		// .pclk = 1140000000,
		// .linelength = 10208,
		// .framelength = 3516,
		// .startx = 0,
		// .starty = 0,
		// .grabwindow_width = 4624,
		// .grabwindow_height = 3472,
		// .mipi_data_lp2hs_settle_dc = 85,
		// .mipi_pixel_rate = 727200000,
		// .max_framerate = 300,

	// },

	.normal_video = { /*reg_C-2 4000*2600@30fps*/
// 30fps start
		.pclk = 1097884800,
		.linelength = 10288,
		.framelength = 3557,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4624,
		.grabwindow_height = 2608,
		.mipi_data_lp2hs_settle_dc = 85,//0x22,
		.mipi_pixel_rate = 1138000000,
		.max_framerate = 300,
// 30fps end
	},
//prize add by lipengpeng 20201028 start
	.hs_video = {
		.pclk = 1097965440,
		.linelength = 2728,
		.framelength = 3354,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2312,
		.grabwindow_height = 1304,
		.mipi_data_lp2hs_settle_dc = 85,//0x22,
		.mipi_pixel_rate = 1217830000,
		.max_framerate = 1200,
	},
 .slim_video = { /* reg_C-1 4000x2256@30fps */
		.pclk = 1097884800,
		.linelength = 9432,
		.framelength = 3880,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4624,
		.grabwindow_height = 3472,
		.mipi_data_lp2hs_settle_dc = 85,//0x22,
		.mipi_pixel_rate = 757030000,
		.max_framerate = 300,
	},
	.custom1 = { //reg_F 2312x1304 @240fps
		.pclk = 1097310720,
		.linelength = 2728,
		.framelength = 1676,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2312,
		.grabwindow_height = 1304,
		.mipi_data_lp2hs_settle_dc = 85,//0x22,
		.mipi_pixel_rate = 1217830000,
		.max_framerate = 2400,
	},
	.custom2 = { //reg_C 4624x2608 @60fps
		.pclk = 1097832480,
		.linelength = 5144,
		.framelength = 3557,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4624,
		.grabwindow_height = 2608,
		.mipi_data_lp2hs_settle_dc = 85,//0x22,
		.mipi_pixel_rate = 1139660000,
		.max_framerate = 600,
	},
	.custom3 = { //reg_A 9248x6944 @15fps
		.pclk = 1097986800,
		.linelength = 10288,
		.framelength = 7115,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 9248,
		.grabwindow_height = 6944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1692000000,
		.max_framerate = 150,
	},

	.margin = 64,
	.min_gain = 64, /*1x gain*/
	.max_gain = 992, /*15.5x gain*/
	.min_gain_iso = 100,
	.gain_step = 4, /*minimum step = 4 in 1x~2x gain*/
	.gain_type = 3,/*to be modify,no gain table for sony*/
	.min_shutter = 16,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	  /* 1, support; 0,not support */
	.ihdr_le_firstline = 0,  /* 1,le first ; 0, se first */
	.sensor_mode_num = 8,	  /* support sensor mode num */
	.cap_delay_frame = 1,
	.pre_delay_frame = 1,
	.video_delay_frame = 1,
	.hs_video_delay_frame = 1,
	.slim_video_delay_frame = 1,

	.custom1_delay_frame = 1,	/*32M */
	.custom2_delay_frame = 1,	/*48M@15fps*/
	.custom3_delay_frame = 1,	/*stero@34fps*/
	.frame_time_delay_frame = 3,
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY, //MIPI_OPHY_NCSI2, /* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = 0, /* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
	//[agold][xfl][20190313][end]

	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_3_LANE, //SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x34,0xff},//0x20,0x34
	.i2c_speed = 1000, /* i2c read/write speed */
};


static struct imgsensor_struct imgsensor = {
	//[agold][xfl][20190313][start]
	.mirror = IMAGE_HV_MIRROR,				/* mirrorflip information */
	//[agold][xfl][20190313][end]

	/* IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview,
	*  Capture, Video,High Speed Video, Slim Video
	*/
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,					/* current shutter */
	.gain = 0x100,						/* current gain */
	.dummy_pixel = 0,					/* current dummypixel */
	.dummy_line = 0,					/* current dummyline */
	.current_fps = 0,  /* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	/* auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker */
	.autoflicker_en = KAL_FALSE,
	/* test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output */
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,/* current scenario id */
	.ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
	.hdr_mode = 0, /* HDR mODE : 0: disable HDR, 1:IHDR, 2:HDR, 9:ZHDR */
	.i2c_write_id = 0x34,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[8] = {
	{9248, 6944, 0,   0, 9248, 6944, 4624, 3472,
	0, 0, 4624, 3472,  0,  0, 4624, 3472}, /* Preview */

	{9248, 6944, 0,   0, 9248, 6944, 4624, 3472,
	0, 0, 4624, 3472,  0,  0, 4624, 3472}, /* capture */

	{9248, 6944, 0, 864, 9248, 5216, 4624, 2608,
	0, 0, 4624, 2608,  0,  0, 4624, 2608}, /* normal video */

	{9248, 6944, 0, 864, 9248, 5216, 2312, 1304,
	0, 0, 2312, 1304,  0,  0, 2312, 1304}, // hs_video reg_E 2312x1304 @120fps

//	{9248, 6944, 0, 424, 9248, 6520, 2312, 1304,
//	0, 0, 2312, 1304,  0,  0, 2312, 1304}, // slim video  reg_D 2312x1304 @30fps

	{9248, 6944, 0, 0, 9248, 6944, 4624, 3472,
	0,   0, 4624, 3472,  0,  0, 4624, 3472}, /* slim video */

	{9248, 6944, 0, 864, 9248, 5216, 2312, 1304,
	0, 0, 2312, 1304,  0,  0, 2312, 1304},// custom1 reg_F 2312x1304 @240fps

	{9248, 6944, 0,  864, 9248, 5216, 4624, 2608,
	4, 0, 4624, 2608,  0,  0, 4624, 2608}, // custom2 //reg_C 4624x2608 @60fps

	{9248, 6944, 0,  0, 9248, 6944, 9248, 6944,
	0, 0, 9248, 6944,  0,  0, 9248, 6944}, // custom3 reg_A 9248x6944 @15fps

};

static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3] = {
	/* Preview mode setting */
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1200, 0x0D80, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x30, 0x05A0, 0x06B8, 0x00, 0x00, 0x0000, 0x0000},
	/* Normal_Video mode setting */
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1200, 0x0D80, 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x30, 0x05A0, 0x0508, 0x00, 0x00, 0x0000, 0x0000},
	/* Video60fps mode setting */
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1200, 0x0D80, 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x30, 0x03B6, 0x0338, 0x00, 0x00, 0x0000, 0x0000}
};


static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 17,
	.i4OffsetY = 16,
	.i4PitchX  =  8,
	.i4PitchY  = 16,
	.i4PairNum  = 8,
	.i4SubBlkW  = 8,
	.i4SubBlkH  = 2,
	.i4PosL = { {20, 17}, {18, 19}, {22, 21}, {24, 23},{20, 25}, {18, 27}, {22, 29}, {24, 31} },
	.i4PosR = { {19, 17}, {17, 19}, {21, 21}, {23, 23},{19, 25}, {17, 27}, {21, 29}, {23, 31} },
	.i4BlockNumX = 574,
	.i4BlockNumY = 215,
	.iMirrorFlip = 3,
	.i4Crop = { {0, 0}, {0, 0}, {0, 432}, {0, 0}, {8, 440},{0, 0}, {0, 0}, {784, 872}, {0, 0}, {0, 0} },
};


#define IMX682MIPI_MaxGainIndex (223)
kal_uint16 IMX682MIPI_sensorGainMapping[IMX682MIPI_MaxGainIndex][2] ={
	{72,114},
	{76,162},
	{80,205},
	{84,244},
	{88,279},
	{92,312},
	{96,341},
	{100,369},
	{104,394},
	{108,417},
	{112,439},
	{116,459},
	{120,478},
	{124,495},
	{128,512},
	{132,528},
	{136,542},
	{140,556},
	{144,569},
	{148,581},
	{152,593},
	{156,604},
	{160,614},
	{164,624},
	{168,634},
	{172,643},
	{176,652},
	{180,660},
	{184,668},
	{188,675},
	{192,683},
	{196,690},
	{200,696},
	{204,703},
	{208,709},
	{212,715},
	{216,721},
	{220,726},
	{224,731},
	{228,737},
	{232,742},
	{236,746},
	{240,751},
	{244,755},
	{248,760},
	{252,764},
	{256,768},
	{260,772},
	{264,776},
	{267,779},
	{272,783},
	{277,787},
	{280,790},
	{284,793},
	{287,796},
	{293,800},
	{297,803},
	{301,806},
	{303,808},
	{308,811},
	{312,814},
	{317,817},
	{320,819},
	{324,822},
	{328,824},
	{333,827},
	{336,829},
	{340,831},
	{343,833},
	{349,836},
	{352,838},
	{356,840},
	{360,842},
	{364,844},
	{368,846},
	{372,848},
	{377,850},
	{381,852},
	{383,853},
	{388,855},
	{392,857},
	{397,859},
	{400,860},
	{405,862},
	{407,863},
	{412,865},
	{415,866},
	{420,868},
	{423,869},
	{428,871},
	{431,872},
	{437,874},
	{440,875},
	{443,876},
	{449,878},
	{452,879},
	{455,880},
	{462,882},
	{465,883},
	{468,884},
	{471,885},
	{475,886},
	{478,887},
	{485,889},
	{489,890},
	{493,891},
	{496,892},
	{500,893},
	{504,894},
	{508,895},
	{512,896},
	{516,897},
	{520,898},
	{524,899},
	{529,900},
	{533,901},
	{537,902},
	{542,903},
	{546,904},
	{551,905},
	{555,906},
	{560,907},
	{565,908},
	{570,909},
	{575,910},
	{580,911},
	{585,912},
	{590,913},
	{596,914},
	{601,915},
	{607,916},
	{612,917},
	{618,918},
	{624,919},
	{630,920},
	{636,921},
	{643,922},
	{649,923},
	{655,924},
	{662,925},
	{669,926},
	{676,927},
	{683,928},
	{690,929},
	{697,930},
	{705,931},
	{712,932},
	{720,933},
	{728,934},
	{736,935},
	{745,936},
	{753,937},
	{762,938},
	{771,939},
	{780,940},
	{790,941},
	{799,942},
	{809,943},
	{819,944},
	{830,945},
	{840,946},
	{851,947},
	{862,948},
	{874,949},
	{886,950},
	{898,951},
	{910,952},
	{923,953},
	{936,954},
	{950,955},
	{964,956},
	{978,957},
	{993,958},
	{1008,959},
	{1024,960},
	{1040,961},
	{1057,962},
	{1074,963},
	{1092,964},
	{1111,965},
	{1130,966},
	{1150,967},
	{1170,968},
	{1192,969},
	{1214,970},
	{1237,971},
	{1260,972},
	{1285,973},
	{1311,974},
	{1337,975},
	{1365,976},
	{1394,977},
	{1425,978},
	{1456,979},
	{1489,980},
	{1524,981},
	{1560,982},
	{1598,983},
	{1638,984},
	{1680,985},
	{1725,986},
	{1771,987},
	{1820,988},
	{1872,989},
	{1928,990},
	{1986,991},
	{2048,992},
	{2114,993},
	{2185,994},
	{2260,995},
	{2341,996},
	{2427,997},
	{2521,998},
	{2621,999},
	{2731,1000},
	{2849,1001},
	{2979,1002},
	{3121,1003},
	{3277,1004},
	{3449,1005},
	{3641,1006},
	{3855,1007},
	{4096,1008},
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 2, imgsensor.i2c_write_id);
	return ((get_byte<<8)&0xff00)|((get_byte>>8)&0x00ff);
}


static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF)};

	/*kdSetI2CSpeed(imgsensor_info.i2c_speed);*/ /* Add this func to set i2c speed by each sensor */
	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	/*kdSetI2CSpeed(imgsensor_info.i2c_speed);*/ /* Add this func to set i2c speed by each sensor */
	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void imx682_get_pdaf_reg_setting(MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		regDa[idx + 1] = read_cmos_sensor_8(regDa[idx]);
		pr_debug("%x %x", regDa[idx], regDa[idx+1]);
	}
}
static void imx682_set_pdaf_reg_setting(MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		write_cmos_sensor_8(regDa[idx], regDa[idx + 1]);
		pr_debug("%x %x", regDa[idx], regDa[idx+1]);
	}
}

// prize add chenwenhui 20240117 for QSC/LRC start
#if LRC_QSC_CALIBRATION
static kal_uint16 imx682_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len);
static void selective_read_eeprom(kal_uint16 addr, BYTE* data)
{
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF)};
	if(iReadRegI2C(pusendcmd, 2, (u8*)data, 1, EEPROM_READ_ID)<0) {
	    LOG_INF("ERR selective_read_eeprom read 0x%x failed",addr);
	}
}
static void read_eeprom_data( kal_uint16 addr, BYTE* data, kal_uint32 size)
{
	int i = 0;
	for(i = 0; i < size; i++) {
	    selective_read_eeprom(addr+i, &data[i]);
	}
}

static void read_QSCAndLRC_from_otp(void)
{
    kal_uint16 idx = 0;
    BYTE imx682_QSC_value[QSC_CALIBRATION_DATA_LENGTH]= {0};
    BYTE imx682_LRC_value[LRC_CALIBRATION_DATA_LENGTH]= {0};

	read_eeprom_data(QSC_START_ADDR,&imx682_QSC_value[0],QSC_CALIBRATION_DATA_LENGTH);//get QSC calibration
	for(idx = 0;idx < 2 * QSC_CALIBRATION_DATA_LENGTH;idx += 2) {
	    imx682_QSC_setting[idx] =  ADDR_SENSOR_QSC + idx/2; //addr //write sensor stat add CA00
	    imx682_QSC_setting[idx+1] = imx682_QSC_value[idx/2];//value
	}

	read_eeprom_data(LRC_START_ADDR,&imx682_LRC_value[0],LRC_CALIBRATION_DATA_LENGTH); //get LRC calibration
	for(idx = 0;idx < 2 * LRC_CALIBRATION_DATA_LENGTH;idx += 2) {
	    if (idx >= LRC_CALIBRATION_DATA_LENGTH)
	        imx682_LRC_setting[idx] = ADDR_SENSOR_LRC_1+(idx-LRC_CALIBRATION_DATA_LENGTH)/2; //LRC_table 1 :addr 0x7c00
	    else
	        imx682_LRC_setting[idx] = ADDR_SENSOR_LRC_0+idx/2; //LRC_table 0 :addr 0x7b00

	    imx682_LRC_setting[idx+1] = imx682_LRC_value[idx/2];  //value
	}
}

static void write_sensor_QSC(void)
{
	write_cmos_sensor_8(0x32D6, read_cmos_sensor_8(0x32D6)|0x01);
	#if MULTI_WRITE
  	imx682_table_write_cmos_sensor(imx682_QSC_setting,
		sizeof(imx682_QSC_setting)/sizeof(kal_uint16));
	#else
  	for (int j =0; j < QSC_CALIBRATION_DATA_LENGTH; j++) {
	    write_cmos_sensor_8(ADDR_SENSOR_QSC, imx682_QSC_setting[j]);
		//LOG_INF("QSC data[%d]=0x%x",j,imx682_QSC_setting[j]);
	}
	#endif
}

static void write_sensor_LRC(void)
{
	#if  MULTI_WRITE
  	imx682_table_write_cmos_sensor(imx682_LRC_setting,
		sizeof(imx682_LRC_setting)/sizeof(kal_uint16));
	#else
  	for (int i = 0; i < LRC_CALIBRATION_DATA_LENGTH/2; i++) {
		write_cmos_sensor_8(ADDR_SENSOR_LRC_0+i,imx682_LRC_setting[ i]); // for left pd pixel  gain
		write_cmos_sensor_8(ADDR_SENSOR_LRC_1+i,imx682_LRC_setting[i+192]);// for right pd pixel  gain
		//LOG_INF("LRC data[%d]=0x%x",i,imx682_LRC_setting[i]);
  	}
	#endif
}
#endif
// prize add chenwenhui 20240117 for QSC/LRC end

static void set_dummy(void)
{
	LOG_INF("frame_length = %d, line_length = %d\n",
		imgsensor.frame_length,
		imgsensor.line_length);

	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
//	write_cmos_sensor_8(0x0342, imgsensor.line_length >> 8);
//	write_cmos_sensor_8(0x0343, imgsensor.line_length & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
}	/*	set_dummy  */

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{

	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable %d\n", framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	if (frame_length >= imgsensor.min_frame_length)
		imgsensor.frame_length = frame_length;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;

#if LONG_EXP
	int longexposure_times = 0;
	static int long_exposure_status;
#endif

	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
		/* Extend frame length */
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	}
	} else {
		/* Extend frame length */
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	}

#if LONG_EXP
	while (shutter >= 65535) {
		shutter = shutter / 2;
		longexposure_times += 1;
	}

	if (read_cmos_sensor_8(0x0350) != 0x01) {
		LOG_INF("single cam scenario enable auto-extend");
		write_cmos_sensor_8(0x0350, 0x01);
	}

	if (longexposure_times > 0) {
		LOG_INF("enter long exposure mode, time is %d",longexposure_times);
		long_exposure_status = 1;
		write_cmos_sensor_8(0x3100, longexposure_times & 0x07);
	} else if (long_exposure_status == 1) {
		long_exposure_status = 0;
		write_cmos_sensor_8(0x3100, 0x00);
		LOG_INF("exit long exposure mode");
	}
#endif

	/* Update Shutter */
	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, shutter  & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);

}	/*	write_shutter  */



/*************************************************************************
* FUNCTION
*	set_shutter
*
* DESCRIPTION
*	This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*	iShutter : exposured lines
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}	/*	set_shutter */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint8 i;

	for (i = 0; i < IMX682MIPI_MaxGainIndex; i++) {
		if(gain <= IMX682MIPI_sensorGainMapping[i][0]){
			break;
		}
	}
	if(gain != IMX682MIPI_sensorGainMapping[i][0])
		LOG_INF("Gain mapping don't correctly:%d %d \n", gain, IMX682MIPI_sensorGainMapping[i][0]);
	return IMX682MIPI_sensorGainMapping[i][1];
}


/*************************************************************************
* FUNCTION
*	set_gain
*
* DESCRIPTION
*	This function is to set global gain to sensor.
*
* PARAMETERS
*	iGain : sensor global gain(base: 0x40)
*
* RETURNS
*	the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

    /* gain=1024;//for test */
    /* return; //for test */

	//if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
	//if (gain < 72 || gain > 64 * BASEGAIN) {
	if (gain < imgsensor_info.min_gain || gain > imgsensor_info.max_gain) {
		LOG_INF("Error gain setting");

	if (gain < imgsensor_info.min_gain)
		gain = imgsensor_info.min_gain;
	else if (gain > imgsensor_info.max_gain)
		gain = imgsensor_info.max_gain;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0204, (reg_gain>>8) & 0xFF);
	write_cmos_sensor_8(0x0205, reg_gain & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);

	return gain;
}	/*	set_gain  */

/* ITD: Modify Dualcam By Jesse 190924 Start */
static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 target_frame_length)
{

	spin_lock(&imgsensor_drv_lock);
	if (target_frame_length > 1)
		imgsensor.dummy_line = target_frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + imgsensor.dummy_line;
	imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_shutter(shutter);
}
/* ITD: Modify Dualcam By Jesse 190924 End */
static void ihdr_write_shutter_gain(kal_uint16 le,
				kal_uint16 se, kal_uint16 gain)
{
}

static void set_mirror_flip(kal_uint8 image_mirror)
{
	kal_uint8 itemp;

	LOG_INF("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor_8(0x0101);
	itemp &= ~0x03;

	switch (image_mirror) {

	case IMAGE_NORMAL:
	write_cmos_sensor_8(0x0101, itemp | 0x00);
	break;

	case IMAGE_V_MIRROR:
	write_cmos_sensor_8(0x0101, itemp | 0x02);
	break;

	case IMAGE_H_MIRROR:
	write_cmos_sensor_8(0x0101, itemp | 0x01);
	break;

	case IMAGE_HV_MIRROR:
	write_cmos_sensor_8(0x0101, itemp | 0x03);
	break;
	}
}

/*************************************************************************
* FUNCTION
*	night_mode
*
* DESCRIPTION
*	This function night mode of sensor.
*
* PARAMETERS
*	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 imx682_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;

	while (len > IDX) {
		addr = para[IDX];

		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
#if MULTI_WRITE
		/* Write when remain buffer size is less than 3 bytes or reach end of data */
		if ((I2C_BUFFER_LEN - tosend) < 3 || IDX == len || addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd,
								tosend,
								imgsensor.i2c_write_id,
								3,
								imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);
		tosend = 0;

#endif
	}
	return 0;
}

static kal_uint16 imx682_init_setting[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x33F0, 0x01,
	0x33F1, 0x03,
	0x0111, 0x03,
	0x3076, 0x00,
	0x3077, 0x30,
	0x1F06, 0x06,
	0x1F07, 0x82,
	0x1F04, 0x71,
	0x1F05, 0x01,
	0x1F08, 0x01,
	0x5BFE, 0x14,
	0x5C0D, 0x2D,
	0x5C1C, 0x30,
	0x5C2B, 0x32,
	0x5C37, 0x2E,
	0x5C40, 0x30,
	0x5C50, 0x14,
	0x5C5F, 0x28,
	0x5C6E, 0x28,
	0x5C7D, 0x32,
	0x5C89, 0x37,
	0x5C92, 0x56,
	0x5BFC, 0x12,
	0x5C0B, 0x2A,
	0x5C1A, 0x2C,
	0x5C29, 0x2F,
	0x5C36, 0x2E,
	0x5C3F, 0x2E,
	0x5C4E, 0x06,
	0x5C5D, 0x1E,
	0x5C6C, 0x20,
	0x5C7B, 0x1E,
	0x5C88, 0x32,
	0x5C91, 0x32,
	0x5C02, 0x14,
	0x5C11, 0x2F,
	0x5C20, 0x32,
	0x5C2F, 0x34,
	0x5C39, 0x31,
	0x5C42, 0x31,
	0x5C8B, 0x28,
	0x5C94, 0x28,
	0x5C00, 0x10,
	0x5C0F, 0x2C,
	0x5C1E, 0x2E,
	0x5C2D, 0x32,
	0x5C38, 0x2E,
	0x5C41, 0x2B,
	0x5C61, 0x0A,
	0x5C70, 0x0A,
	0x5C7F, 0x0A,
	0x5C8A, 0x1E,
	0x5C93, 0x2A,
	0x5BFA, 0x2B,
	0x5C09, 0x2D,
	0x5C18, 0x2E,
	0x5C27, 0x30,
	0x5C5B, 0x28,
	0x5C6A, 0x22,
	0x5C79, 0x42,
	0x5BFB, 0x2C,
	0x5C0A, 0x2F,
	0x5C19, 0x2E,
	0x5C28, 0x2E,
	0x5C4D, 0x20,
	0x5C5C, 0x1E,
	0x5C6B, 0x32,
	0x5C7A, 0x32,
	0x5BFD, 0x30,
	0x5C0C, 0x32,
	0x5C1B, 0x2E,
	0x5C2A, 0x30,
	0x5C4F, 0x28,
	0x5C5E, 0x32,
	0x5C6D, 0x37,
	0x5C7C, 0x56,
	0x5BFF, 0x2E,
	0x5C0E, 0x32,
	0x5C1D, 0x2E,
	0x5C2C, 0x2B,
	0x5C51, 0x0A,
	0x5C60, 0x0A,
	0x5C6F, 0x1E,
	0x5C7E, 0x2A,
	0x5C01, 0x32,
	0x5C10, 0x34,
	0x5C1F, 0x31,
	0x5C2E, 0x31,
	0x5C71, 0x28,
	0x5C80, 0x28,
	0x5C4C, 0x2A,
	0x33F2, 0x01,
	0x1F04, 0x73,
	0x1F05, 0x01,
	0x5BFA, 0x35,
	0x5C09, 0x38,
	0x5C18, 0x3A,
	0x5C27, 0x38,
	0x5C5B, 0x25,
	0x5C6A, 0x24,
	0x5C79, 0x47,
	0x5BFC, 0x15,
	0x5C0B, 0x2E,
	0x5C1A, 0x36,
	0x5C29, 0x38,
	0x5C36, 0x36,
	0x5C3F, 0x36,
	0x5C4E, 0x0B,
	0x5C5D, 0x20,
	0x5C6C, 0x2A,
	0x5C7B, 0x25,
	0x5C88, 0x25,
	0x5C91, 0x22,
	0x5BFE, 0x15,
	0x5C0D, 0x32,
	0x5C1C, 0x36,
	0x5C2B, 0x36,
	0x5C37, 0x3A,
	0x5C40, 0x39,
	0x5C50, 0x06,
	0x5C5F, 0x22,
	0x5C6E, 0x23,
	0x5C7D, 0x2E,
	0x5C89, 0x44,
	0x5C92, 0x51,
	0x5D7F, 0x0A,
	0x5C00, 0x17,
	0x5C0F, 0x36,
	0x5C1E, 0x38,
	0x5C2D, 0x3C,
	0x5C38, 0x38,
	0x5C41, 0x36,
	0x5C52, 0x0A,
	0x5C61, 0x21,
	0x5C70, 0x23,
	0x5C7F, 0x1B,
	0x5C8A, 0x22,
	0x5C93, 0x20,
	0x5C02, 0x1A,
	0x5C11, 0x3E,
	0x5C20, 0x3F,
	0x5C2F, 0x3D,
	0x5C39, 0x3E,
	0x5C42, 0x3C,
	0x5C54, 0x02,
	0x5C63, 0x12,
	0x5C72, 0x14,
	0x5C81, 0x24,
	0x5C8B, 0x1C,
	0x5C94, 0x4E,
	0x5D8A, 0x09,
	0x5BFB, 0x36,
	0x5C0A, 0x38,
	0x5C19, 0x36,
	0x5C28, 0x36,
	0x5C4D, 0x2A,
	0x5C5C, 0x25,
	0x5C6B, 0x25,
	0x5C7A, 0x22,
	0x5BFD, 0x36,
	0x5C0C, 0x36,
	0x5C1B, 0x3A,
	0x5C2A, 0x39,
	0x5C4F, 0x23,
	0x5C5E, 0x2E,
	0x5C6D, 0x44,
	0x5C7C, 0x51,
	0x5D63, 0x0A,
	0x5BFF, 0x38,
	0x5C0E, 0x3C,
	0x5C1D, 0x38,
	0x5C2C, 0x36,
	0x5C51, 0x23,
	0x5C60, 0x1B,
	0x5C6F, 0x22,
	0x5C7E, 0x20,
	0x5C01, 0x3F,
	0x5C10, 0x3D,
	0x5C1F, 0x3E,
	0x5C2E, 0x3C,
	0x5C53, 0x14,
	0x5C62, 0x24,
	0x5C71, 0x1C,
	0x5C80, 0x4E,
	0x5D76, 0x09,
	0x5C4C, 0x2A,
	0x33F2, 0x02,
	0x1F04, 0x78,
	0x1F05, 0x01,
	0x5BFA, 0x37,
	0x5C09, 0x36,
	0x5C18, 0x39,
	0x5C27, 0x38,
	0x5C5B, 0x27,
	0x5C6A, 0x2B,
	0x5C79, 0x48,
	0x5BFC, 0x16,
	0x5C0B, 0x32,
	0x5C1A, 0x33,
	0x5C29, 0x37,
	0x5C36, 0x36,
	0x5C3F, 0x35,
	0x5C4E, 0x0D,
	0x5C5D, 0x2D,
	0x5C6C, 0x23,
	0x5C7B, 0x25,
	0x5C88, 0x31,
	0x5C91, 0x2E,
	0x5BFE, 0x15,
	0x5C0D, 0x31,
	0x5C1C, 0x35,
	0x5C2B, 0x36,
	0x5C37, 0x35,
	0x5C40, 0x37,
	0x5C50, 0x0F,
	0x5C5F, 0x31,
	0x5C6E, 0x30,
	0x5C7D, 0x33,
	0x5C89, 0x36,
	0x5C92, 0x5B,
	0x5C00, 0x13,
	0x5C0F, 0x2F,
	0x5C1E, 0x2E,
	0x5C2D, 0x34,
	0x5C38, 0x33,
	0x5C41, 0x32,
	0x5C52, 0x0D,
	0x5C61, 0x27,
	0x5C70, 0x28,
	0x5C7F, 0x1F,
	0x5C8A, 0x25,
	0x5C93, 0x2C,
	0x5C02, 0x15,
	0x5C11, 0x36,
	0x5C20, 0x39,
	0x5C2F, 0x3A,
	0x5C39, 0x37,
	0x5C42, 0x37,
	0x5C54, 0x04,
	0x5C63, 0x1C,
	0x5C72, 0x1C,
	0x5C81, 0x1C,
	0x5C8B, 0x28,
	0x5C94, 0x24,
	0x5BFB, 0x33,
	0x5C0A, 0x37,
	0x5C19, 0x36,
	0x5C28, 0x35,
	0x5C4D, 0x23,
	0x5C5C, 0x25,
	0x5C6B, 0x31,
	0x5C7A, 0x2E,
	0x5BFD, 0x35,
	0x5C0C, 0x36,
	0x5C1B, 0x35,
	0x5C2A, 0x37,
	0x5C4F, 0x30,
	0x5C5E, 0x33,
	0x5C6D, 0x36,
	0x5C7C, 0x5B,
	0x5BFF, 0x2E,
	0x5C0E, 0x34,
	0x5C1D, 0x33,
	0x5C2C, 0x32,
	0x5C51, 0x28,
	0x5C60, 0x1F,
	0x5C6F, 0x25,
	0x5C7E, 0x2C,
	0x5C01, 0x39,
	0x5C10, 0x3A,
	0x5C1F, 0x37,
	0x5C2E, 0x37,
	0x5C53, 0x1C,
	0x5C62, 0x1C,
	0x5C71, 0x28,
	0x5C80, 0x24,
	0x5C4C, 0x2C,
	0x33F2, 0x03,
	0x1F08, 0x00,
	0x0101, 0x00,
	0x32C8, 0x00,
	0x4017, 0x40,
	0x40A2, 0x01,
	0x40AC, 0x01,
	0x4328, 0x00,
	0x4329, 0xB3,
	0x4E15, 0x10,
	0x4E19, 0x2F,
	0x4E21, 0x0F,
	0x4E2F, 0x10,
	0x4E3D, 0x10,
	0x4E41, 0x2F,
	0x4E57, 0x29,
	0x4FFB, 0x2F,
	0x5011, 0x24,
	0x501D, 0x03,
	0x505F, 0x41,
	0x5060, 0xDF,
	0x5065, 0xDF,
	0x5066, 0x37,
	0x506E, 0x57,
	0x5070, 0xC5,
	0x5072, 0x57,
	0x5075, 0x53,
	0x5076, 0x55,
	0x5077, 0xC1,
	0x5078, 0xC3,
	0x5079, 0x53,
	0x507A, 0x55,
	0x507D, 0x57,
	0x507E, 0xDF,
	0x507F, 0xC5,
	0x5081, 0x57,
	0x53C8, 0x01,
	0x53C9, 0xE2,
	0x53CA, 0x03,
	0x5422, 0x7A,
	0x548E, 0x40,
	0x5497, 0x5E,
	0x54A1, 0x40,
	0x54A9, 0x40,
	0x54B2, 0x5E,
	0x54BC, 0x40,
	0x57C6, 0x00,
	0x583D, 0x0E,
	0x583E, 0x0E,
	0x583F, 0x0E,
	0x5840, 0x0E,
	0x5841, 0x0E,
	0x5842, 0x0E,
	0x5900, 0x12,
	0x5901, 0x12,
	0x5902, 0x14,
	0x5903, 0x12,
	0x5904, 0x14,
	0x5905, 0x12,
	0x5906, 0x14,
	0x5907, 0x12,
	0x590F, 0x12,
	0x5911, 0x12,
	0x5913, 0x12,
	0x591C, 0x12,
	0x591E, 0x12,
	0x5920, 0x12,
	0x5948, 0x08,
	0x5949, 0x08,
	0x594A, 0x08,
	0x594B, 0x08,
	0x594C, 0x08,
	0x594D, 0x08,
	0x594E, 0x08,
	0x594F, 0x08,
	0x595C, 0x08,
	0x595E, 0x08,
	0x5960, 0x08,
	0x596E, 0x08,
	0x5970, 0x08,
	0x5972, 0x08,
	0x597E, 0x0F,
	0x597F, 0x0F,
	0x599A, 0x0F,
	0x59DE, 0x08,
	0x59DF, 0x08,
	0x59FA, 0x08,
	0x5A59, 0x22,
	0x5A5B, 0x22,
	0x5A5D, 0x1A,
	0x5A5F, 0x22,
	0x5A61, 0x1A,
	0x5A63, 0x22,
	0x5A65, 0x1A,
	0x5A67, 0x22,
	0x5A77, 0x22,
	0x5A7B, 0x22,
	0x5A7F, 0x22,
	0x5A91, 0x22,
	0x5A95, 0x22,
	0x5A99, 0x22,
	0x5AE9, 0x66,
	0x5AEB, 0x66,
	0x5AED, 0x5E,
	0x5AEF, 0x66,
	0x5AF1, 0x5E,
	0x5AF3, 0x66,
	0x5AF5, 0x5E,
	0x5AF7, 0x66,
	0x5B07, 0x66,
	0x5B0B, 0x66,
	0x5B0F, 0x66,
	0x5B21, 0x66,
	0x5B25, 0x66,
	0x5B29, 0x66,
	0x5B79, 0x46,
	0x5B7B, 0x3E,
	0x5B7D, 0x3E,
	0x5B89, 0x46,
	0x5B8B, 0x46,
	0x5B97, 0x46,
	0x5B99, 0x46,
	0x5C9E, 0x0A,
	0x5C9F, 0x08,
	0x5CA0, 0x0A,
	0x5CA1, 0x0A,
	0x5CA2, 0x0B,
	0x5CA3, 0x06,
	0x5CA4, 0x04,
	0x5CA5, 0x06,
	0x5CA6, 0x04,
	0x5CAD, 0x0B,
	0x5CAE, 0x0A,
	0x5CAF, 0x0C,
	0x5CB0, 0x0A,
	0x5CB1, 0x0B,
	0x5CB2, 0x08,
	0x5CB3, 0x06,
	0x5CB4, 0x08,
	0x5CB5, 0x04,
	0x5CBC, 0x0B,
	0x5CBD, 0x09,
	0x5CBE, 0x08,
	0x5CBF, 0x09,
	0x5CC0, 0x0A,
	0x5CC1, 0x08,
	0x5CC2, 0x06,
	0x5CC3, 0x08,
	0x5CC4, 0x06,
	0x5CCB, 0x0A,
	0x5CCC, 0x09,
	0x5CCD, 0x0A,
	0x5CCE, 0x08,
	0x5CCF, 0x0A,
	0x5CD0, 0x08,
	0x5CD1, 0x08,
	0x5CD2, 0x08,
	0x5CD3, 0x08,
	0x5CDA, 0x09,
	0x5CDB, 0x09,
	0x5CDC, 0x08,
	0x5CDD, 0x08,
	0x5CE3, 0x09,
	0x5CE4, 0x08,
	0x5CE5, 0x08,
	0x5CE6, 0x08,
	0x5CF4, 0x04,
	0x5D04, 0x04,
	0x5D13, 0x06,
	0x5D22, 0x06,
	0x5D23, 0x04,
	0x5D2E, 0x06,
	0x5D37, 0x06,
	0x5D6F, 0x09,
	0x5D72, 0x0F,
	0x5D88, 0x0F,
	0x5DE6, 0x01,
	0x5DE7, 0x01,
	0x5DE8, 0x01,
	0x5DE9, 0x01,
	0x5DEA, 0x01,
	0x5DEB, 0x01,
	0x5DEC, 0x01,
	0x5DF2, 0x01,
	0x5DF3, 0x01,
	0x5DF4, 0x01,
	0x5DF5, 0x01,
	0x5DF6, 0x01,
	0x5DF7, 0x01,
	0x5DF8, 0x01,
	0x5DFE, 0x01,
	0x5DFF, 0x01,
	0x5E00, 0x01,
	0x5E01, 0x01,
	0x5E02, 0x01,
	0x5E03, 0x01,
	0x5E04, 0x01,
	0x5E0A, 0x01,
	0x5E0B, 0x01,
	0x5E0C, 0x01,
	0x5E0D, 0x01,
	0x5E0E, 0x01,
	0x5E0F, 0x01,
	0x5E10, 0x01,
	0x5E16, 0x01,
	0x5E17, 0x01,
	0x5E18, 0x01,
	0x5E1E, 0x01,
	0x5E1F, 0x01,
	0x5E20, 0x01,
	0x5E6E, 0x5A,
	0x5E6F, 0x46,
	0x5E70, 0x46,
	0x5E71, 0x3C,
	0x5E72, 0x3C,
	0x5E73, 0x28,
	0x5E74, 0x28,
	0x5E75, 0x6E,
	0x5E76, 0x6E,
	0x5E81, 0x46,
	0x5E83, 0x3C,
	0x5E85, 0x28,
	0x5E87, 0x6E,
	0x5E92, 0x46,
	0x5E94, 0x3C,
	0x5E96, 0x28,
	0x5E98, 0x6E,
	0x5ECB, 0x26,
	0x5ECC, 0x26,
	0x5ECD, 0x26,
	0x5ECE, 0x26,
	0x5ED2, 0x26,
	0x5ED3, 0x26,
	0x5ED4, 0x26,
	0x5ED5, 0x26,
	0x5ED9, 0x26,
	0x5EDA, 0x26,
	0x5EE5, 0x08,
	0x5EE6, 0x08,
	0x5EE7, 0x08,
	0x6006, 0x14,
	0x6007, 0x14,
	0x6008, 0x14,
	0x6009, 0x14,
	0x600A, 0x14,
	0x600B, 0x14,
	0x600C, 0x14,
	0x600D, 0x22,
	0x600E, 0x22,
	0x600F, 0x14,
	0x601A, 0x14,
	0x601B, 0x14,
	0x601C, 0x14,
	0x601D, 0x14,
	0x601E, 0x14,
	0x601F, 0x14,
	0x6020, 0x14,
	0x6021, 0x22,
	0x6022, 0x22,
	0x6023, 0x14,
	0x602E, 0x14,
	0x602F, 0x14,
	0x6030, 0x14,
	0x6031, 0x22,
	0x6039, 0x14,
	0x603A, 0x14,
	0x603B, 0x14,
	0x603C, 0x22,
	0x6132, 0x0F,
	0x6133, 0x0F,
	0x6134, 0x0F,
	0x6135, 0x0F,
	0x6136, 0x0F,
	0x6137, 0x0F,
	0x6138, 0x0F,
	0x613E, 0x0F,
	0x613F, 0x0F,
	0x6140, 0x0F,
	0x6141, 0x0F,
	0x6142, 0x0F,
	0x6143, 0x0F,
	0x6144, 0x0F,
	0x614A, 0x0F,
	0x614B, 0x0F,
	0x614C, 0x0F,
	0x614D, 0x0F,
	0x614E, 0x0F,
	0x614F, 0x0F,
	0x6150, 0x0F,
	0x6156, 0x0F,
	0x6157, 0x0F,
	0x6158, 0x0F,
	0x6159, 0x0F,
	0x615A, 0x0F,
	0x615B, 0x0F,
	0x615C, 0x0F,
	0x6162, 0x0F,
	0x6163, 0x0F,
	0x6164, 0x0F,
	0x616A, 0x0F,
	0x616B, 0x0F,
	0x616C, 0x0F,
	0x6226, 0x00,
	0x84F8, 0x01,
	0x8501, 0x00,
	0x8502, 0x01,
	0x8505, 0x00,
	0x8744, 0x00,
	0x883C, 0x01,
	0x8845, 0x00,
	0x8846, 0x01,
	0x8849, 0x00,
	0x9004, 0x1F,
	0x9064, 0x4D,
	0x9065, 0x3D,
	0x922E, 0x91,
	0x922F, 0x2A,
	0x9230, 0xE2,
	0x9231, 0xC0,
	0x9232, 0xE2,
	0x9233, 0xC1,
	0x9234, 0xE2,
	0x9235, 0xC2,
	0x9236, 0xE2,
	0x9237, 0xC3,
	0x9238, 0xE2,
	0x9239, 0xD4,
	0x923A, 0xE2,
	0x923B, 0xD5,
	0x923C, 0x90,
	0x923D, 0x64,
	0xB0B9, 0x10,
	0xBC76, 0x00,
	0xBC77, 0x00,
	0xBC78, 0x00,
	0xBC79, 0x00,
	0xBC7B, 0x28,
	0xBC7C, 0x00,
	0xBC7D, 0x00,
	0xBC7F, 0xC0,
	0xC6B9, 0x01,
	0xECB5, 0x04,
	0xECBF, 0x04,
	0x32D9, 0x01,
	0x85C0, 0x01,
	0xA503, 0x04,
	0xA533, 0x3F,
	0xA53C, 0x06,
	0xA53F, 0x04,
	0xA56F, 0x3F,
	0xA654, 0x01,
	0xA655, 0x68,
	0xA6D2, 0x01,
	0xA6D3, 0x68,
	0xA6D6, 0x00,
	0xA6D7, 0x4C,
	0xA737, 0x20,
	0xAC2C, 0x05,
	0xAC5C, 0x05,
	0xAC8C, 0x05,
	0xB026, 0x00
};

static kal_uint16 imx682_capture_setting[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	0x0342, 0x24,
	0x0343, 0xD8,
	0x0340, 0x0F,
	0x0341, 0x28,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x1B,
	0x034B, 0x1F,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x30D8, 0x04,
	0x3200, 0x41,
	0x3201, 0x41,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x12,
	0x040D, 0x10,
	0x040E, 0x0D,
	0x040F, 0x90,
	0x034C, 0x12,
	0x034D, 0x10,
	0x034E, 0x0D,
	0x034F, 0x90,
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x6E,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x00,
	0x030F, 0xB8,
	0x0310, 0x01,
	0x30D9, 0x00,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x401E, 0x00,
	0x40B8, 0x01,
	0x40B9, 0x2C,
	0x40BC, 0x01,
	0x40BD, 0x18,
	0x40BE, 0x00,
	0x40BF, 0x00,
	0x41A4, 0x00,
	0x5A09, 0x01,
	0x5A17, 0x01,
	0x5A25, 0x01,
	0x5A33, 0x01,
	0x98D7, 0xB4,
	0x98D8, 0x8C,
	0x98D9, 0x0A,
	0x99C4, 0x16,
	0x0202, 0x0E,
	0x0203, 0xF8,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x4018, 0x04,
	0x4019, 0x80,
	0x401A, 0x00,
	0x401B, 0x01,
	0x0B06, 0x01,
	0x3400, 0x02,
	0x3093, 0x01
};


static kal_uint16 imx682_preview_setting[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	0x0342, 0x24,
	0x0343, 0xD8,
	0x0340, 0x0F,
	0x0341, 0x28,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x1B,
	0x034B, 0x1F,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x30D8, 0x04,
	0x3200, 0x41,
	0x3201, 0x41,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x12,
	0x040D, 0x10,
	0x040E, 0x0D,
	0x040F, 0x90,
	0x034C, 0x12,
	0x034D, 0x10,
	0x034E, 0x0D,
	0x034F, 0x90,
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x6E,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x00,
	0x030F, 0xB8,
	0x0310, 0x01,
	0x30D9, 0x00,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x401E, 0x00,
	0x40B8, 0x01,
	0x40B9, 0x2C,
	0x40BC, 0x01,
	0x40BD, 0x18,
	0x40BE, 0x00,
	0x40BF, 0x00,
	0x41A4, 0x00,
	0x5A09, 0x01,
	0x5A17, 0x01,
	0x5A25, 0x01,
	0x5A33, 0x01,
	0x98D7, 0xB4,
	0x98D8, 0x8C,
	0x98D9, 0x0A,
	0x99C4, 0x16,
	0x0202, 0x0E,
	0x0203, 0xF8,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x4018, 0x04,
	0x4019, 0x80,
	0x401A, 0x00,
	0x401B, 0x01,
	0x0B06, 0x01,
	0x3400, 0x02,
	0x3093, 0x01
};

static kal_uint16 imx682_normal_video_setting[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	0x0342, 0x28,
	0x0343, 0x30,
	0x0340, 0x0D,
	0x0341, 0xE5,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x03,
	0x0347, 0x60,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x17,
	0x034B, 0xBF,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x30D8, 0x00,
	0x3200, 0x41,
	0x3201, 0x41,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x12,
	0x040D, 0x10,
	0x040E, 0x0A,
	0x040F, 0x30,
	0x034C, 0x12,
	0x034D, 0x10,
	0x034E, 0x0A,
	0x034F, 0x30,
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x6E,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x01,
	0x030F, 0x15,
	0x0310, 0x01,
	0x30D9, 0x01,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x401E, 0x00,
	0x40B8, 0x01,
	0x40B9, 0xFE,
	0x40BC, 0x00,
	0x40BD, 0xCC,
	0x40BE, 0x00,
	0x40BF, 0xCC,
	0x41A4, 0x00,
	0x5A09, 0x01,
	0x5A17, 0x01,
	0x5A25, 0x01,
	0x5A33, 0x01,
	0x98D7, 0xB4,
	0x98D8, 0x8C,
	0x98D9, 0x0A,
	0x99C4, 0x16,
	0x0202, 0x0D,
	0x0203, 0xB5,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x4018, 0x04,
	0x4019, 0x80,
	0x401A, 0x00,
	0x401B, 0x01,
	0x0B06, 0x01,
	0x3400, 0x02,
	0x3093, 0x01
};

static kal_uint16 imx682_hs_video_setting[] = {
	//high speed video by lipengpeng 20201013 start
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	0x0342, 0x0A,
	0x0343, 0xA8,
	0x0340, 0x0D,
	0x0341, 0x1A,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x03,
	0x0347, 0x60,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x17,
	0x034B, 0xBF,
	0x0900, 0x01,
	0x0901, 0x44,
	0x0902, 0x0A,
	0x30D8, 0x00,
	0x3200, 0x43,
	0x3201, 0x43,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x09,
	0x040D, 0x08,
	0x040E, 0x05,
	0x040F, 0x18,
	0x034C, 0x09,
	0x034D, 0x08,
	0x034E, 0x05,
	0x034F, 0x18,
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x6E,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x01,
	0x030F, 0x28,
	0x0310, 0x01,
	0x30D9, 0x01,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x401E, 0x4D,
	0x40B8, 0x00,
	0x40B9, 0x32,
	0x40BC, 0x00,
	0x40BD, 0x08,
	0x40BE, 0x00,
	0x40BF, 0x08,
	0x41A4, 0x00,
	0x5A09, 0x00,
	0x5A17, 0x00,
	0x5A25, 0x00,
	0x5A33, 0x00,
	0x98D7, 0xB4,
	0x98D8, 0x8C,
	0x98D9, 0x0A,
	0x99C4, 0x16,
	0x0202, 0x0C,
	0x0203, 0xEA,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x4018, 0x00,
	0x4019, 0x00,
	0x401A, 0x00,
	0x401B, 0x00,
	0x0B06, 0x01,
	0x3400, 0x02,
	0x3093, 0x00
	//high speed video by lipengpeng 20201013 end
};

static kal_uint16 imx682_slim_video_setting[] = {
//prize add by lipengpeng 20201106 slim vide setting start
	/*0x0112,0x0A,
	0x0113,0x0A,
	0x0114,0x02,
	0x0342,0x0A,
	0x0343,0xA8,
	0x0340,0x34,
	0x0341,0x68,
	0x0344,0x00,
	0x0345,0x00,
	0x0346,0x03,
	0x0347,0x60,
	0x0348,0x24,
	0x0349,0x1F,
	0x034A,0x17,
	0x034B,0xBF,
	0x0900,0x01,
	0x0901,0x44,
	0x0902,0x0A,
	0x30D8,0x00,
	0x3200,0x43,
	0x3201,0x43,
	0x0408,0x00,
	0x0409,0x00,
	0x040A,0x00,
	0x040B,0x00,
	0x040C,0x09,
	0x040D,0x08,
	0x040E,0x05,
	0x040F,0x18,
	0x034C,0x09,
	0x034D,0x08,
	0x034E,0x05,
	0x034F,0x18,
	0x0301,0x08,
	0x0303,0x02,
	0x0305,0x04,
	0x0306,0x01,
	0x0307,0x6E,
	0x030B,0x01,
	0x030D,0x04,
	0x030E,0x01,
	0x030F,0x28,
	0x0310,0x01,
	0x30D9,0x01,
	0x32D5,0x00,
	0x32D6,0x00,
	0x401E,0x4D,
	0x40B8,0x00,
	0x40B9,0x32,
	0x40BC,0x00,
	0x40BD,0x08,
	0x40BE,0x00,
	0x40BF,0x08,
	0x41A4,0x00,
	0x5A09,0x00,
	0x5A17,0x00,
	0x5A25,0x00,
	0x5A33,0x00,
	0x98D7,0xB4,
	0x98D8,0x8C,
	0x98D9,0x0A,
	0x99C4,0x16,
	0x0202,0x34,
	0x0203,0x38,
	0x0204,0x00,
	0x0205,0x00,
	0x020E,0x01,
	0x020F,0x00,
	0x4018,0x00,
	0x4019,0x00,
	0x401A,0x00,
	0x401B,0x00,
	0x0B06,0x01,
	0x3400,0x02,
	0x3093,0x00*/

	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x1E,
	0x0343, 0xC0,
	/*Frame Length Lines Setting*/
	0x0340, 0x0E,
	0x0341, 0x9A,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x02,
	0x0347, 0xE8,
	0x0348, 0x1F,
	0x0349, 0x3F,
	0x034A, 0x14,
	0x034B, 0x87,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x3140, 0x00,
	0x3246, 0x81,
	0x3247, 0x81,
	0x3F15, 0x00,
	/*Digital Crop & Scaling*/
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x0F,
	0x040D, 0xA0,
	0x040E, 0x08,
	0x040F, 0xD0,
	/*Output Size Setting*/
	0x034C, 0x0F,
	0x034D, 0xA0,
	0x034E, 0x08,
	0x034F, 0xD0,
	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xB8,
	0x030B, 0x02,
	0x030D, 0x04,
	0x030E, 0x01,
	0x030F, 0x1C,
	0x0310, 0x01,
	/*Other Setting*/
	0x3620, 0x00,
	0x3621, 0x00,
	0x3C11, 0x04,
	0x3C12, 0x03,
	0x3C13, 0x2D,
	0x3F0C, 0x01,
	0x3F14, 0x00,
	0x3F80, 0x01,
	0x3F81, 0x90,
	0x3F8C, 0x00,
	0x3F8D, 0x14,
	0x3FF8, 0x01,
	0x3FF9, 0x2A,
	0x3FFE, 0x00,
	0x3FFF, 0x6C,
	/*Integration Setting*/
	0x0202, 0x0E,
	0x0203, 0x6A,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x01,
	0x3FE1, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x70,
	0x0216, 0x00,
	0x0217, 0x70,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x70,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3E20, 0x02,
	0x3E3B, 0x01,
	0x4434, 0x01,
	0x4435, 0xF0,
	/*cphy global timing*/
	0x0808, 0x02,
	0x084f, 0x08,
	0x0851, 0x07,
	0x0853, 0x0e,
	0x0855, 0x14,
	0x0859, 0x1c
//prize add by lipengpeng 20201106 slim vide setting end
};

static kal_uint16 imx682_custom1_setting[] = {
	0x0112,0x0A,
	0x0113,0x0A,
	0x0114,0x02,
	0x0342,0x0A,
	0x0343,0xA8,
	0x0340,0x06,
	0x0341,0x8C,
	0x0344,0x00,
	0x0345,0x00,
	0x0346,0x03,
	0x0347,0x60,
	0x0348,0x24,
	0x0349,0x1F,
	0x034A,0x17,
	0x034B,0xBF,
	0x0900,0x01,
	0x0901,0x44,
	0x0902,0x0A,
	0x30D8,0x00,
	0x3200,0x43,
	0x3201,0x43,
	0x0408,0x00,
	0x0409,0x00,
	0x040A,0x00,
	0x040B,0x00,
	0x040C,0x09,
	0x040D,0x08,
	0x040E,0x05,
	0x040F,0x18,
	0x034C,0x09,
	0x034D,0x08,
	0x034E,0x05,
	0x034F,0x18,
	0x0301,0x08,
	0x0303,0x02,
	0x0305,0x04,
	0x0306,0x01,
	0x0307,0x6E,
	0x030B,0x01,
	0x030D,0x04,
	0x030E,0x01,
	0x030F,0x28,
	0x0310,0x01,
	0x30D9,0x01,
	0x32D5,0x00,
	0x32D6,0x00,
	0x401E,0x4D,
	0x40B8,0x00,
	0x40B9,0x32,
	0x40BC,0x00,
	0x40BD,0x08,
	0x40BE,0x00,
	0x40BF,0x08,
	0x41A4,0x00,
	0x5A09,0x00,
	0x5A17,0x00,
	0x5A25,0x00,
	0x5A33,0x00,
	0x98D7,0xB4,
	0x98D8,0x8C,
	0x98D9,0x0A,
	0x99C4,0x16,
	0x0202,0x06,
	0x0203,0x5C,
	0x0204,0x00,
	0x0205,0x00,
	0x020E,0x01,
	0x020F,0x00,
	0x4018,0x00,
	0x4019,0x00,
	0x401A,0x00,
	0x401B,0x00,
	0x0B06,0x01,
	0x3400,0x02,
	0x3093,0x00

};

static kal_uint16 imx682_custom2_setting[] = {
//prize add by lipengpeng 20201106 reg_C 4624x2608 @60fps   start
	0x0112,0x0A,
	0x0113,0x0A,
	0x0114,0x02,
	0x0342,0x14,
	0x0343,0x18,
	0x0340,0x0D,
	0x0341,0xE5,
	0x0344,0x00,
	0x0345,0x00,
	0x0346,0x03,
	0x0347,0x60,
	0x0348,0x24,
	0x0349,0x1F,
	0x034A,0x17,
	0x034B,0xBF,
	0x0900,0x01,
	0x0901,0x22,
	0x0902,0x08,
	0x30D8,0x00,
	0x3200,0x41,
	0x3201,0x41,
	0x0408,0x00,
	0x0409,0x00,
	0x040A,0x00,
	0x040B,0x00,
	0x040C,0x12,
	0x040D,0x10,
	0x040E,0x0A,
	0x040F,0x30,
	0x034C,0x12,
	0x034D,0x10,
	0x034E,0x0A,
	0x034F,0x30,
	0x0301,0x08,
	0x0303,0x02,
	0x0305,0x04,
	0x0306,0x01,
	0x0307,0x6E,
	0x030B,0x01,
	0x030D,0x04,
	0x030E,0x01,
	0x030F,0x15,
	0x0310,0x01,
	0x30D9,0x01,
	0x32D5,0x00,
	0x32D6,0x00,
	0x401E,0x00,
	0x40B8,0x01,
	0x40B9,0xFE,
	0x40BC,0x00,
	0x40BD,0xCC,
	0x40BE,0x00,
	0x40BF,0xCC,
	0x41A4,0x00,
	0x5A09,0x01,
	0x5A17,0x01,
	0x5A25,0x01,
	0x5A33,0x01,
	0x98D7,0xB4,
	0x98D8,0x8C,
	0x98D9,0x0A,
	0x99C4,0x16,
	0x0202,0x0D,
	0x0203,0xB5,
	0x0204,0x00,
	0x0205,0x00,
	0x020E,0x01,
	0x020F,0x00,
	0x4018,0x00,
	0x4019,0x00,
	0x401A,0x00,
	0x401B,0x00,
	0x0B06,0x01,
	0x3400,0x02,
	0x3093,0x00
//prize add by lipengpeng 20201106 reg_C 4624x2608 @60fps   start
};

static kal_uint16 imx682_custom3_setting[] = {
//reg_A 9248x6944 @15fps
	0x0112,0x0A,
	0x0113,0x0A,
	0x0114,0x02,
	0x0342,0x28,
	0x0343,0x30,
	0x0340,0x1B,
	0x0341,0xCB,
	0x0344,0x00,
	0x0345,0x00,
	0x0346,0x00,
	0x0347,0x00,
	0x0348,0x24,
	0x0349,0x1F,
	0x034A,0x1B,
	0x034B,0x1F,
	0x0900,0x00,
	0x0901,0x11,
	0x0902,0x0A,
	0x30D8,0x00,
	0x3200,0x01,
	0x3201,0x01,
	0x0408,0x00,
	0x0409,0x00,
	0x040A,0x00,
	0x040B,0x00,
	0x040C,0x24,
	0x040D,0x20,
	0x040E,0x1B,
	0x040F,0x20,
	0x034C,0x24,
	0x034D,0x20,
	0x034E,0x1B,
	0x034F,0x20,
	0x0301,0x08,
	0x0303,0x02,
	0x0305,0x04,
	0x0306,0x01,
	0x0307,0x6E,
	0x030B,0x01,
	0x030D,0x04,
	0x030E,0x01,
	0x030F,0x1A,
	0x0310,0x01,
	0x30D9,0x01,
	0x32D5,0x01,
	0x32D6,0x01,
	0x401E,0x00,
	0x40B8,0x02,
	0x40B9,0x1C,
	0x40BC,0x00,
	0x40BD,0xB0,
	0x40BE,0x00,
	0x40BF,0xB0,
	0x41A4,0x00,
	0x5A09,0x01,
	0x5A17,0x01,
	0x5A25,0x01,
	0x5A33,0x01,
	0x98D7,0x14,
	0x98D8,0x14,
	0x98D9,0x00,
	0x99C4,0x00,
	0x0202,0x1B,
	0x0203,0x9B,
	0x0204,0x00,
	0x0205,0x00,
	0x020E,0x01,
	0x020F,0x00,
	0x4018,0x04,
	0x4019,0x80,
	0x401A,0x00,
	0x401B,0x01,
	0x0B06,0x01,
	0x3400,0x02,
	0x3093,0x01
//reg_A 9248x6944 @15fps

};

static void sensor_init(void)
{
	LOG_INF("E init\n");
	imx682_table_write_cmos_sensor(imx682_init_setting,
		sizeof(imx682_init_setting) / sizeof(kal_uint16));
	LOG_INF("L\n");
} /*	sensor_init  */

static void preview_setting(void)
{
	LOG_INF("E binning_normal_setting\n");

#if 1	
	write_cmos_sensor(0x0100,0x00); //standby

	imx682_table_write_cmos_sensor(imx682_preview_setting,
         sizeof(imx682_preview_setting) / sizeof(kal_uint16));  

	//write_cmos_sensor(0x0100,0x01); //steaming
	
#endif
	
	LOG_INF("L\n");
}	/*	preview_setting  */

static void capture_setting(kal_uint16 currefps, kal_bool stream_on)
{
	LOG_INF("E currefps:%d\n", currefps);
	
	write_cmos_sensor(0x0100,0x00); //standby
	
	imx682_table_write_cmos_sensor(imx682_capture_setting,
		sizeof(imx682_capture_setting) / sizeof(kal_uint16));
	
    //write_cmos_sensor(0x0100,0x01); //streaming

//	capture_setting();
	LOG_INF("L!\n");
}

static void normal_video_setting(void)
{
	LOG_INF("Enter normal_video_setting\n");
	write_cmos_sensor(0x0100,0x00); //standby
	
	imx682_table_write_cmos_sensor(imx682_normal_video_setting,
		sizeof(imx682_normal_video_setting) / sizeof(kal_uint16));
}

static void hs_video_setting(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x0100,0x00); //standby
	
	imx682_table_write_cmos_sensor(imx682_hs_video_setting,
		sizeof(imx682_hs_video_setting) / sizeof(kal_uint16));
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x0100,0x00); //standby
	
	imx682_table_write_cmos_sensor(imx682_slim_video_setting,
		sizeof(imx682_slim_video_setting) / sizeof(kal_uint16));
}

static void custom1_setting(void)
{
	/* custom1 32M setting */
	LOG_INF("E\n");
    write_cmos_sensor(0x0100,0x00); //standby
	
	imx682_table_write_cmos_sensor(imx682_custom1_setting,
		sizeof(imx682_custom1_setting) / sizeof(kal_uint16));
}

static void custom2_setting(void)
{
	/* custom2 48M@15fps setting */
	LOG_INF("E\n");
	write_cmos_sensor(0x0100,0x00); //standby
	
	imx682_table_write_cmos_sensor(imx682_custom2_setting,
		sizeof(imx682_custom2_setting) / sizeof(kal_uint16));
}

static void custom3_setting(void)
{
	/* custom3 stero@34fps setting */
	LOG_INF("E\n");
	write_cmos_sensor(0x0100,0x00); //standby
	
	imx682_table_write_cmos_sensor(imx682_custom3_setting,
		sizeof(imx682_custom3_setting) / sizeof(kal_uint16));
}

/*************************************************************************
* FUNCTION
*	get_imgsensor_id
*
* DESCRIPTION
*	This function get the sensor ID
*
* PARAMETERS
*	*sensorID : return the sensor ID
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
//extern int g_CameraSensorIdx;
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;

    /*sensor have two i2c address 0x34 & 0x20,
     *we should detect the module used i2c address
     */
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            *sensor_id = ((read_cmos_sensor_8(0x0016) << 8)
                    | read_cmos_sensor_8(0x0017));
            LOG_INF(
                "lhh read_0x0000=0x%x, 0x0001=0x%x,0x0000_0001=0x%x\n",
                read_cmos_sensor_8(0x0016),
                read_cmos_sensor_8(0x0017),
                read_cmos_sensor(0x0000));
            if (*sensor_id == imgsensor_info.sensor_id) {
                LOG_INF("lhh i2c write id: 0x%x, sensor id: 0x%x\n",
                    imgsensor.i2c_write_id, *sensor_id);

// prize add chenwenhui 20240117 for QSC/LRC start
#if LRC_QSC_CALIBRATION
                read_QSCAndLRC_from_otp();
#endif
// prize add chenwenhui 20240117 for QSC/LRC end

                return ERROR_NONE;
            }

            LOG_INF("lhh Read sensor id fail, id: 0x%x\n",
                imgsensor.i2c_write_id);
            retry--;
        } while (retry > 0);
        i++;
        retry = 2;
    }
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	
	return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*	open
*
* DESCRIPTION
*	This function initialize the registers of CMOS sensor
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;
	/* sensor have two i2c address 0x35 0x34 & 0x21 0x20, we should detect the module used i2c address */
	printk("XiaoPanTimer: 2023-02-27 12.14\n");
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = ((read_cmos_sensor_8(0x0016) << 8)
                    | read_cmos_sensor_8(0x0017));
            if (sensor_id == imgsensor_info.sensor_id) {
                LOG_INF("lhh i2c write id: 0x%x, sensor id: 0x%x\n",
                    imgsensor.i2c_write_id, sensor_id);
                break;
            }
            LOG_INF("lhh Read sensor id fail, id: 0x%x\n",
                imgsensor.i2c_write_id);
            retry--;
        } while (retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 2;
    }
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;
	/* initail sequence write in  */

	sensor_init();

// prize add chenwenhui 20240117 for QSC/LRC start
#if LRC_QSC_CALIBRATION
	write_sensor_LRC();
#endif
// prize add chenwenhui 20240117 for QSC/LRC end

	//IMX682_MIPI_update_awb(imgsensor.i2c_write_id);

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_mode = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}	/*	open  */



/*************************************************************************
* FUNCTION
*	close
*
* DESCRIPTION
*
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");
	/*No Need to implement this function*/

	return ERROR_NONE;
}	/*	close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*	This function start the sensor preview.
*
* PARAMETERS
*	*image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E imgsensor.hdr_mode=%d\n", imgsensor.hdr_mode);

	LOG_INF("E preview normal\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*	capture
*
* DESCRIPTION
*	This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		LOG_INF(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			imgsensor.current_fps,
			imgsensor_info.cap.max_framerate / 10);
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps, 1);
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

		LOG_INF("E preview normal\n");
		spin_lock(&imgsensor_drv_lock);
		imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
		imgsensor.pclk = imgsensor_info.normal_video.pclk;
		imgsensor.line_length = imgsensor_info.normal_video.linelength;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength;
		imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
		spin_unlock(&imgsensor_drv_lock);
		normal_video_setting();
		set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	slim_video	 */

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	if (imgsensor.current_fps == imgsensor_info.custom1.max_framerate) {
		imgsensor.pclk = imgsensor_info.custom1.pclk;
		imgsensor.line_length = imgsensor_info.custom1.linelength;
		imgsensor.frame_length = imgsensor_info.custom1.framelength;
		imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	if (imgsensor.current_fps == imgsensor_info.custom2.max_framerate) {
		imgsensor.pclk = imgsensor_info.custom2.pclk;
		imgsensor.line_length = imgsensor_info.custom2.linelength;
		imgsensor.frame_length = imgsensor_info.custom2.framelength;
		imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	if (imgsensor.current_fps == imgsensor_info.custom3.max_framerate) {
		imgsensor.pclk = imgsensor_info.custom3.pclk;
		imgsensor.line_length = imgsensor_info.custom3.linelength;
		imgsensor.frame_length = imgsensor_info.custom3.framelength;
		imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	custom3_setting();
	set_mirror_flip(imgsensor.mirror);
// prize add chenwenhui 20240117 for QSC/LRC start
#if LRC_QSC_CALIBRATION
	write_sensor_QSC();
#endif
// prize add chenwenhui 20240117 for QSC/LRC end

	return ERROR_NONE;
}

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth	 = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	 = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth	 = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight	 = imgsensor_info.slim_video.grabwindow_height;
	
	sensor_resolution->SensorCustom1Width = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;
	sensor_resolution->SensorCustom2Width = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height = imgsensor_info.custom2.grabwindow_height;
	sensor_resolution->SensorCustom3Width = imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height = imgsensor_info.custom3.grabwindow_height;


	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; /* inverse with datasheet */
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
#if PDAF_MODE_SUPPORT
	sensor_info->PDAF_Support = 2;
#else
	sensor_info->PDAF_Support = 0;
#endif	
	sensor_info->HDR_Support = 0;	/*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR, 4:four-cell mVHDR*/

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

		switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

			sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
				
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
				
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
			break;

		default:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
	}

	return ERROR_NONE;
}	/*	get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		custom2(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		custom3(image_window, sensor_config_data);
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) /* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else /* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d, hdr_mode = %d\n", scenario_id, framerate, imgsensor.hdr_mode);

		switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:			
				frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
				spin_lock(&imgsensor_drv_lock);
				imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
				(frame_length - imgsensor_info.pre.framelength) : 0;
				imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
				imgsensor.min_frame_length = imgsensor.frame_length;
				spin_unlock(&imgsensor_drv_lock);				
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				frame_length = imgsensor_info.normal_video.pclk / framerate * 10 /
					imgsensor_info.normal_video.linelength;
				spin_lock(&imgsensor_drv_lock);
				imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ?
					(frame_length - imgsensor_info.normal_video.framelength) : 0;
				imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
				imgsensor.min_frame_length = imgsensor.frame_length;
				spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			// if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
				// frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
				// spin_lock(&imgsensor_drv_lock);
					// imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ?
						// (frame_length - imgsensor_info.cap1.framelength) : 0;
					// imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
					// imgsensor.min_frame_length = imgsensor.frame_length;
					// spin_unlock(&imgsensor_drv_lock);
			// } else {
				{if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
					LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
					framerate, imgsensor_info.cap.max_framerate/10);
					frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
					spin_lock(&imgsensor_drv_lock);
					imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ?
						(frame_length - imgsensor_info.cap.framelength) : 0;
					imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
					imgsensor.min_frame_length = imgsensor.frame_length;
					spin_unlock(&imgsensor_drv_lock);
			}
			set_dummy();
	    	break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			frame_length = imgsensor_info.hs_video.pclk / framerate * 10 /
				imgsensor_info.hs_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ?
				(frame_length - imgsensor_info.hs_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			frame_length = imgsensor_info.slim_video.pclk / framerate * 10 /
				imgsensor_info.slim_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ?
				(frame_length - imgsensor_info.slim_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;			
		case MSDK_SCENARIO_ID_CUSTOM1:
			frame_length = imgsensor_info.custom1.pclk / framerate * 10 / 
				imgsensor_info.custom1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ? 
				(frame_length - imgsensor_info.custom1.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			frame_length = imgsensor_info.custom2.pclk / framerate * 10 / 
				imgsensor_info.custom2.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ? 
				(frame_length - imgsensor_info.custom2.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			frame_length = imgsensor_info.custom3.pclk / framerate * 10 / 
				imgsensor_info.custom3.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength) ? 
				(frame_length - imgsensor_info.custom3.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		
		default:  /* coding with  preview scenario by default */
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
				(frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
			break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
		write_cmos_sensor_8(0x0100, 0x01);
	else
		write_cmos_sensor_8(0x0100, 0x00);

	mdelay(10);
	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_uint32 modes,
	struct SET_SENSOR_PATTERN_SOLID_COLOR *pdata)
{
	static char Blk_Value1 = 0x0;
	static char Blk_Value2 = 0x0;
	LOG_INF("%s,modes: %d\n", __FUNCTION__,modes);

	streaming_control(KAL_FALSE);
	if (modes) {
		Blk_Value1 = read_cmos_sensor_8(0x020E);
		Blk_Value2 = read_cmos_sensor_8(0x0601);
		if (modes == 5) {
		    write_cmos_sensor_8(0x020E, 0x00);////black
		} else {
		    write_cmos_sensor_8(0x0601, 0x02);////color bar
		}
	} else {
		write_cmos_sensor_8(0x020E, Blk_Value1);
		write_cmos_sensor_8(0x0601, Blk_Value2);
		Blk_Value1 = 0x0;
		Blk_Value2 = 0x0;
	}
	streaming_control(KAL_TRUE);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = modes;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static void hdr_write_tri_shutter(kal_uint16 le, kal_uint16 me, kal_uint16 se)
{
	kal_uint16 realtime_fps = 0;

	LOG_INF("E! le:0x%x, me:0x%x, se:0x%x\n", le, me, se);
	spin_lock(&imgsensor_drv_lock);
	if (le > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = le + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	if (le < imgsensor_info.min_shutter)
		le = imgsensor_info.min_shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor_8(0x0104, 0x01);
			write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8); /*FRM_LENGTH_LINES[15:8]*/
			write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF); /*FRM_LENGTH_LINES[7:0]*/
			write_cmos_sensor_8(0x0104, 0x00);
		}
	} else {
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	}

	write_cmos_sensor_8(0x0104, 0x01);
	/* Long exposure */
	write_cmos_sensor_8(0x0202, (le >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, le & 0xFF);
	/* Muddle exposure */
	write_cmos_sensor_8(0x3FE0, (me >> 8) & 0xFF); /*MID_COARSE_INTEG_TIME[15:8]*/
	write_cmos_sensor_8(0x3FE1, me & 0xFF); /*MID_COARSE_INTEG_TIME[7:0]*/
	/* Short exposure */
	write_cmos_sensor_8(0x0224, (se >> 8) & 0xFF);
	write_cmos_sensor_8(0x0225, se & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);

	LOG_INF("L! le:0x%x, me:0x%x, se:0x%x\n", le, me, se);

}

static void hdr_write_tri_gain(kal_uint16 lg, kal_uint16 mg, kal_uint16 sg)
{
	kal_uint16 reg_lg, reg_mg, reg_sg;

	if (lg < BASEGAIN || lg > 16 * BASEGAIN) {
		LOG_INF("Error gain setting");

		if (lg < BASEGAIN)
			lg = BASEGAIN;
		else if (lg > 16 * BASEGAIN)
			lg = 16 * BASEGAIN;
	}

	reg_lg = gain2reg(lg);
	reg_mg = gain2reg(mg);
	reg_sg = gain2reg(sg);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_lg;
	spin_unlock(&imgsensor_drv_lock);
	write_cmos_sensor_8(0x0104, 0x01);
	/* Long Gian */
	write_cmos_sensor_8(0x0204, (reg_lg>>8) & 0xFF);
	write_cmos_sensor_8(0x0205, reg_lg & 0xFF);
	/* Middle Gian */
	write_cmos_sensor_8(0x3FE2, (reg_mg>>8) & 0xFF);
	write_cmos_sensor_8(0x3FE3, reg_mg & 0xFF);
	/* Short Gian */
	write_cmos_sensor_8(0x0216, (reg_sg>>8) & 0xFF);
	write_cmos_sensor_8(0x0217, reg_sg & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
#if 0
	if (lg > mg) {
		LOG_INF("long gain > medium gain\n");
		write_cmos_sensor_8(0xEB06, 0x00);
		write_cmos_sensor_8(0xEB08, 0x00);
		write_cmos_sensor_8(0xEB0A, 0x00);
		write_cmos_sensor_8(0xEB12, 0x00);
		write_cmos_sensor_8(0xEB14, 0x00);
		write_cmos_sensor_8(0xEB16, 0x00);

		write_cmos_sensor_8(0xEB07, 0x08);
		write_cmos_sensor_8(0xEB09, 0x08);
		write_cmos_sensor_8(0xEB0B, 0x08);
		write_cmos_sensor_8(0xEB13, 0x10);
		write_cmos_sensor_8(0xEB15, 0x10);
		write_cmos_sensor_8(0xEB17, 0x10);
	} else {
		LOG_INF("long gain <= medium gain\n");
		write_cmos_sensor_8(0xEB06, 0x00);
		write_cmos_sensor_8(0xEB08, 0x00);
		write_cmos_sensor_8(0xEB0A, 0x00);
		write_cmos_sensor_8(0xEB12, 0x01);
		write_cmos_sensor_8(0xEB14, 0x01);
		write_cmos_sensor_8(0xEB16, 0x01);

		write_cmos_sensor_8(0xEB07, 0xC8);
		write_cmos_sensor_8(0xEB09, 0xC8);
		write_cmos_sensor_8(0xEB0B, 0xC8);
		write_cmos_sensor_8(0xEB13, 0x2C);
		write_cmos_sensor_8(0xEB15, 0x2C);
		write_cmos_sensor_8(0xEB17, 0x2C);
	}
#endif
	LOG_INF("lg:0x%x, mg:0x%x, sg:0x%x, reg_lg:0x%x, reg_mg:0x%x, reg_sg:0x%x\n",
			lg, mg, sg, reg_lg, reg_mg, reg_sg);

}

static void imx682_set_lsc_reg_setting(kal_uint8 index, kal_uint16 *regDa, MUINT32 regNum)
{



}

static void set_imx682_ATR(kal_uint16 LimitGain, kal_uint16 LtcRate, kal_uint16 PostGain)
{


}

static kal_uint32 imx682_awb_gain(struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{


	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(void)
{
	//UINT32 temperature = 0;
	//INT32 temperature_convert = 0;

	pr_debug("nothing \n");

	return 25;
}
static kal_uint32 seamless_switch(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	kal_uint32 shutter, kal_uint32 gain,
	kal_uint32 shutter_2ndframe, kal_uint32 gain_2ndframe)
{
	
	pr_debug("nothing \n");
	return ERROR_NONE;
}
static MUINT32 imx682_ana_gain_table[] = {
0,1024, 1040, 1056, 1072, 1088, 1104, 1120, 1136, 1152, 1168, 1184, 1200, 1216, 1232, 1248, 1264, 1280, 1296, 1312, 1328,
1344, 1360, 1376, 1392, 1408, 1424, 1440, 1456, 1472, 1488, 1504, 1520, 1536, 1552, 1568, 1584, 1600, 1616, 1632, 1648,
1664, 1680, 1696, 1712, 1728, 1744, 1760, 1776, 1792, 1808, 1824, 1840, 1856, 1872, 1888, 1904, 1920, 1936, 1952, 1968,
1984, 2000, 2016, 2032, 2048, 2064, 2080, 2096, 2112, 2128, 2144, 2160, 2176, 2192, 2208, 2224, 2240, 2256, 2272, 2288,
2304, 2320, 2336, 2352, 2368, 2384, 2400, 2416, 2432, 2448, 2464, 2480, 2496, 2512, 2528, 2544, 2560, 2576, 2592, 2608,
2624, 2640, 2656, 2672, 2688, 2704, 2720, 2736, 2752, 2768, 2784, 2800, 2816, 2832, 2848, 2864, 2880, 2896, 2912, 2928,
2944, 2960, 2976, 2992, 3008, 3024, 3040, 3056, 3072, 3088, 3104, 3120, 3136, 3152, 3168, 3184, 3200, 3216, 3232, 3248,
3264, 3280, 3296, 3312, 3328, 3344, 3360, 3376, 3392, 3408, 3424, 3440, 3456, 3472, 3488, 3504, 3520, 3536, 3552, 3568,
3584, 3600, 3616, 3632, 3648, 3664, 3680, 3696, 3712, 3728, 3744, 3760, 3776, 3792, 3808, 3824, 3840, 3856, 3872, 3888,
3904, 3920, 3936, 3952, 3968, 3984, 4000, 4016, 4032, 4048, 4064, 4080, 4096, 4112, 4128, 4144, 4160, 4176, 4192, 4208,
4224, 4240, 4256, 4272, 4288, 4304, 4320, 4336, 4368, 4384, 4400, 4416, 4432, 4448, 4480, 4496, 4512, 4528, 4544, 4576,
4592, 4608, 4624, 4656, 4672, 4688, 4720, 4736, 4752, 4784, 4800, 4832, 4848, 4864, 4896, 4912, 4944, 4960, 4992, 5008,
5040, 5088, 5104, 5136, 5152, 5184, 5216, 5232, 5264, 5280, 5312, 5344, 5376, 5392, 5424, 5456, 5488, 5504, 5536, 5568,
5600, 5632, 5664, 5696, 5728, 5760, 5792, 5824, 5856, 5888, 5920, 5952, 5984, 6016, 6048, 6096, 6128, 6160, 6192, 6240,
6272, 6304, 6352, 6384, 6432, 6464, 6512, 6544, 6592, 6624, 6672, 6720, 6752, 6800, 6848, 6896, 6944, 6976, 7024, 7072,
7120, 7168, 7216, 7280, 7328, 7376, 7424, 7488, 7536, 7584, 7648, 7696, 7760, 7824, 7872, 7936, 8000, 8064, 8128, 8192,
8256, 8320, 8384, 8448, 8512, 8592, 8656, 8736, 8800, 8880, 8960, 9024, 9104, 9184, 9264, 9360, 9440, 9520, 9616, 9696,
9792, 9888, 9984,10080,10176,10272,10368,10480,10576,10688,10800,10912,11024,11152,11264,11392,11520,11648,11776,11904,
12048,12192,12336,12480,12624,12784,12944,13104,13264,13440,13616,13792,13968,14160,14352,14560,14768,14976,15184,15408,
15648,15872
};
	
static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
							 UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;

	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	UINT32 *pAeCtrls = NULL;
	UINT32 *pScenarios = NULL;
	struct SET_SENSOR_AWB_GAIN *pSetSensorAWB = (struct SET_SENSOR_AWB_GAIN *) feature_para;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
		case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
			pScenarios = (MUINT32 *)((uintptr_t)(*(feature_data+1)));
			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				/*
				**pScenarios = MSDK_SCENARIO_ID_CUSTOM1;
				*break;
				*/
			case MSDK_SCENARIO_ID_CUSTOM1:
				/*
				**pScenarios = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
				*break;
				*/
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			case MSDK_SCENARIO_ID_CUSTOM2:
			case MSDK_SCENARIO_ID_CUSTOM4:
			case MSDK_SCENARIO_ID_CUSTOM5:
			case MSDK_SCENARIO_ID_CUSTOM3:
			default:
				*pScenarios = 0xff;
				break;
			}
			pr_debug("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d %d\n",
				*feature_data, *pScenarios);
			break;
		case SENSOR_FEATURE_SEAMLESS_SWITCH:
			pAeCtrls = (MUINT32 *)((uintptr_t)(*(feature_data+1)));
			if (pAeCtrls)
				seamless_switch((*feature_data), *pAeCtrls,
					*(pAeCtrls+1), *(pAeCtrls+4), *(pAeCtrls+5));
			else
				seamless_switch((*feature_data), 0, 0, 0, 0);
			break;	
		case SENSOR_FEATURE_GET_PERIOD:
			*feature_return_para_16++ = imgsensor.line_length;
			*feature_return_para_16 = imgsensor.frame_length;
			*feature_para_len = 4;
			break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			LOG_INF("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
				imgsensor.pclk, imgsensor.current_fps);
			*feature_return_para_32 = imgsensor.pclk;
			*feature_para_len = 4;
			break;
		case SENSOR_FEATURE_SET_ESHUTTER:
			set_shutter(*feature_data);
			break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
			 /* night_mode((BOOL) *feature_data); */
			break;
		case SENSOR_FEATURE_SET_GAIN:
			set_gain((UINT16) *feature_data);
			break;
		case SENSOR_FEATURE_SET_FLASHLIGHT:
			break;
		case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
			break;
		case SENSOR_FEATURE_SET_REGISTER:
			write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
			break;
		case SENSOR_FEATURE_GET_REGISTER:
			sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
			break;
		case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
			/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
			/* if EEPROM does not exist in camera module. */
			*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
			*feature_para_len = 4;
			break;
		case SENSOR_FEATURE_SET_VIDEO_MODE:
			set_video_mode(*feature_data);
			break;
		case SENSOR_FEATURE_CHECK_SENSOR_ID:
			get_imgsensor_id(feature_return_para_32);
			break;
		case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
			set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16+1));
			break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data+1)));
			break;
		case SENSOR_FEATURE_GET_PDAF_DATA:
			LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
			/* read_3P3_eeprom((kal_uint16 )(*feature_data),(char*)(uintptr_t)(*(feature_data+1)),
			 *(kal_uint32)(*(feature_data+2)));
			 */
			break;
		case SENSOR_FEATURE_SET_TEST_PATTERN:
			set_test_pattern_mode((UINT32)*feature_data,
			(struct SET_SENSOR_PATTERN_SOLID_COLOR *)(uintptr_t)(*(feature_data + 1)));
			break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: /* for factory mode auto testing */
			*feature_return_para_32 = imgsensor_info.checksum_value;
			*feature_para_len = 4;
			break;
		case SENSOR_FEATURE_SET_FRAMERATE:
			LOG_INF("current fps :%d\n", *feature_data_32);
			spin_lock(&imgsensor_drv_lock);
			imgsensor.current_fps = (UINT16)*feature_data_32;
			spin_unlock(&imgsensor_drv_lock);
			break;
		case SENSOR_FEATURE_GET_CROP_INFO:
			/* LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data); */
			wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
			switch (*feature_data_32) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CUSTOM2:
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[6],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[7],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			}
			break;
		case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
			pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
				(UINT16)*feature_data, (UINT16)*(feature_data+1),
				(UINT16)*(feature_data+2));
			ihdr_write_shutter_gain((UINT16)*feature_data,
				(UINT16)*(feature_data+1),
					(UINT16)*(feature_data+2));
			break;			
		/*HDR CMD */
		case SENSOR_FEATURE_SET_HDR_ATR:
			LOG_INF("SENSOR_FEATURE_SET_HDR_ATR Limit_Gain=%d, LTC Rate=%d, Post_Gain=%d\n",
					(UINT16)*feature_data,
					(UINT16)*(feature_data + 1),
					(UINT16)*(feature_data + 2));
			set_imx682_ATR((UINT16)*feature_data,
						(UINT16)*(feature_data + 1),
						(UINT16)*(feature_data + 2));
			break;
		case SENSOR_FEATURE_SET_HDR:
			LOG_INF("hdr enable :%d\n", *feature_data_32);
			spin_lock(&imgsensor_drv_lock);
			imgsensor.hdr_mode = (UINT8)*feature_data_32;
			spin_unlock(&imgsensor_drv_lock);
			break;
		case SENSOR_FEATURE_SET_HDR_SHUTTER:
			LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d, no support\n",
				(UINT16) *feature_data,	(UINT16) *(feature_data + 1));
			/*hdr_write_shutter((UINT16) *feature_data, (UINT16) *(feature_data + 1),
			*	(UINT16) *(feature_data + 2));
			*/
			break;
		case SENSOR_FEATURE_SET_HDR_TRI_SHUTTER:
			LOG_INF("SENSOR_FEATURE_SET_HDR_TRI_SHUTTER LE=%d, ME=%d, SE=%d\n",
					(UINT16) *feature_data,
					(UINT16) *(feature_data + 1),
					(UINT16) *(feature_data + 2));
			hdr_write_tri_shutter((UINT16)*feature_data,
								(UINT16)*(feature_data+1),
								(UINT16)*(feature_data+2));
			break;
		case SENSOR_FEATURE_SET_HDR_TRI_GAIN:
			LOG_INF("SENSOR_FEATURE_SET_HDR_TRI_GAIN LGain=%d, SGain=%d, MGain=%d\n",
					(UINT16) *feature_data,
					(UINT16) *(feature_data + 1),
					(UINT16) *(feature_data + 2));
			hdr_write_tri_gain((UINT16)*feature_data, (UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
			break;
		case SENSOR_FEATURE_SET_AWB_GAIN:
			imx682_awb_gain(pSetSensorAWB);
			break;
		case SENSOR_FEATURE_SET_LSC_TBL:
			{
				kal_uint8 index = *(((kal_uint8 *)feature_para) + (*feature_para_len));

				imx682_set_lsc_reg_setting(index, feature_data_16, (*feature_para_len)/sizeof(UINT16));
			}
			break;
		case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
			/*
			  * SENSOR_VHDR_MODE_NONE  = 0x0,
			  * SENSOR_VHDR_MODE_IVHDR = 0x01,
			  * SENSOR_VHDR_MODE_MVHDR = 0x02,
			  * SENSOR_VHDR_MODE_ZVHDR = 0x09
			  * SENSOR_VHDR_MODE_4CELL_MVHDR = 0x0A
			*/
			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x2;
				break;
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_CUSTOM1:
			case MSDK_SCENARIO_ID_CUSTOM2:
			case MSDK_SCENARIO_ID_CUSTOM3:
			default:
				*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
				break;
			}
			LOG_INF("SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY scenarioId:%llu, HDR:%llu\n"
				, *feature_data, *(feature_data+1));
			break;
			/*END OF HDR CMD */
			
		case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
			*(feature_data + 1) = imgsensor_info.min_gain;
			*(feature_data + 2) = imgsensor_info.max_gain;
			break;
		case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
			*(feature_data + 0) = imgsensor_info.min_gain_iso;
			*(feature_data + 1) = imgsensor_info.gain_step;
			*(feature_data + 2) = imgsensor_info.gain_type;
			break;
		case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
			*(feature_data + 1) = imgsensor_info.min_shutter;
			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			case MSDK_SCENARIO_ID_CUSTOM1:
			case MSDK_SCENARIO_ID_CUSTOM2:
			case MSDK_SCENARIO_ID_CUSTOM4:
			case MSDK_SCENARIO_ID_CUSTOM5:
			*(feature_data + 2) = 2;
			break;
			case MSDK_SCENARIO_ID_CUSTOM3:
			default:
				*(feature_data + 2) = 1;
				break;
			}
			break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
					= imgsensor_info.cap.pclk;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
					= imgsensor_info.normal_video.pclk;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
					= imgsensor_info.hs_video.pclk;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
					= imgsensor_info.slim_video.pclk;
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
					= imgsensor_info.custom1.pclk;
				break;
			case MSDK_SCENARIO_ID_CUSTOM2:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
					= imgsensor_info.custom2.pclk;
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
					= imgsensor_info.custom3.pclk;
				break;
			case MSDK_SCENARIO_ID_CUSTOM4:
			case MSDK_SCENARIO_ID_CUSTOM5:
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
					= imgsensor_info.pre.pclk;
				break;
			}
			break;
		case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.cap.framelength << 16)
					+ imgsensor_info.cap.linelength;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.normal_video.framelength << 16)
					+ imgsensor_info.normal_video.linelength;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.hs_video.framelength << 16)
					+ imgsensor_info.hs_video.linelength;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.slim_video.framelength << 16)
					+ imgsensor_info.slim_video.linelength;
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.custom1.framelength << 16)
					+ imgsensor_info.custom1.linelength;
				break;
			case MSDK_SCENARIO_ID_CUSTOM2:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.custom2.framelength << 16)
					+ imgsensor_info.custom2.linelength;
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.custom3.framelength << 16)
					+ imgsensor_info.custom3.linelength;
				break;
			case MSDK_SCENARIO_ID_CUSTOM4:
			case MSDK_SCENARIO_ID_CUSTOM5:
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.pre.framelength << 16)
					+ imgsensor_info.pre.linelength;
				break;
			}
			break;	
		case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		{
			kal_uint32 rate;

			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				rate = imgsensor_info.cap.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				rate = imgsensor_info.normal_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				rate = imgsensor_info.hs_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				rate = imgsensor_info.slim_video.mipi_pixel_rate;
				break;			
			case MSDK_SCENARIO_ID_CUSTOM1:
				rate = imgsensor_info.custom2.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM2:
				rate = imgsensor_info.custom2.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
				rate = imgsensor_info.custom3.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				rate = imgsensor_info.pre.mipi_pixel_rate;
				break;
			}
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
		}
		break;

		case SENSOR_FEATURE_GET_VC_INFO:
		    LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n", (UINT16)*feature_data);
		    pvcinfo = (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		    switch (*feature_data_32) {
	            case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	            case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	                memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
	                    sizeof(struct SENSOR_VC_INFO_STRUCT));
	                break;
	            case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	            case MSDK_SCENARIO_ID_SLIM_VIDEO:
	                memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
	                    sizeof(struct SENSOR_VC_INFO_STRUCT));
	                break;
	            case MSDK_SCENARIO_ID_CUSTOM3:
	                memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
	                    sizeof(struct SENSOR_VC_INFO_STRUCT));
	                break;
	            case MSDK_SCENARIO_ID_CUSTOM1:
	            case MSDK_SCENARIO_ID_CUSTOM5:
	            default:
	                break;
	            }
		    break;

		case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		    pr_debug("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n", (UINT16)*feature_data);
		    //PDAF capacity enable or not
		    switch (*feature_data) {
	            case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	            case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	            case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	                *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
	                break;
	            case MSDK_SCENARIO_ID_SLIM_VIDEO:
	            case MSDK_SCENARIO_ID_CUSTOM1:
	            case MSDK_SCENARIO_ID_CUSTOM3:
	            case MSDK_SCENARIO_ID_CUSTOM5:
	            default:
	                *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
	                break;
	            }
		    break;
		case SENSOR_FEATURE_GET_PDAF_INFO:
		    pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",(UINT16)*feature_data);
		    PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data + 1));
		    switch (*feature_data) {
	            case MSDK_SCENARIO_ID_CAMERA_PREVIEW: //4624*3472
	            case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	                imgsensor_pd_info.i4BlockNumX = 574;
	                imgsensor_pd_info.i4BlockNumY = 215;
	                memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info,sizeof(struct SET_PD_BLOCK_INFO_T));
	                break;
	            case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	            case MSDK_SCENARIO_ID_SLIM_VIDEO:
	                imgsensor_pd_info.i4BlockNumX = 574;
	                imgsensor_pd_info.i4BlockNumY = 161;
	                memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info,sizeof(struct SET_PD_BLOCK_INFO_T));
	                break;
	            case MSDK_SCENARIO_ID_CUSTOM3: //3056*1728
	                imgsensor_pd_info.i4BlockNumX = 380;
	                imgsensor_pd_info.i4BlockNumY = 107;
	                memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info,sizeof(struct SET_PD_BLOCK_INFO_T));
	                break;
	            case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	            case MSDK_SCENARIO_ID_CUSTOM1:
	            case MSDK_SCENARIO_ID_CUSTOM5:
	            default:
	                break;
			}
			break;

		case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
			pr_debug("SENSOR_FEATURE_GET_PDAF_REG_SETTING %d",
				(*feature_para_len));
			imx682_get_pdaf_reg_setting((*feature_para_len) / sizeof(UINT32)
						   , feature_data_16);
			break;
		case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
			pr_debug("SENSOR_FEATURE_SET_PDAF_REG_SETTING %d",
				(*feature_para_len));
			imx682_set_pdaf_reg_setting((*feature_para_len) / sizeof(UINT32)
						   , feature_data_16);
			break;
		case SENSOR_FEATURE_SET_PDAF:
			LOG_INF("PDAF mode :%d\n", *feature_data_16);
			imgsensor.pdaf_mode= *feature_data_16;
			break;
		case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
			*feature_return_para_i32 = get_sensor_temperature();
			*feature_para_len = 4;
			break;
/* ITD: Modify Dualcam By Jesse 190924 Start */
		case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
			pr_debug("SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME\n");
			set_shutter_frame_length((UINT16)*feature_data, (UINT16)*(feature_data+1));
			break;
/* ITD: Modify Dualcam By Jesse 190924 End */
		case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
			LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
			streaming_control(KAL_FALSE);
			break;
		case SENSOR_FEATURE_SET_STREAMING_RESUME:
			LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n", *feature_data);
			if (*feature_data != 0)
				set_shutter(*feature_data);
			streaming_control(KAL_TRUE);
			break;
		case SENSOR_FEATURE_GET_BINNING_TYPE: //by sensor mode??????binning_ratio
			switch (*(feature_data + 1)) {
			case MSDK_SCENARIO_ID_CUSTOM3:
			*feature_return_para_32 = 1; /*BINNING_NONE*/break;
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			case MSDK_SCENARIO_ID_CUSTOM4:
			default:
			*feature_return_para_32 = 1; /*BINNING_AVERAGED*/
			break;
			}
			pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
			*feature_para_len = 4;
			break;
		case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
			/*
			* 1, if driver support new sw frame sync
			* set_shutter_frame_length() support third para auto_extend_en
			*/
			*(feature_data + 1) = 1;
			/* margin info by scenario */
			*(feature_data + 2) = imgsensor_info.margin;
			break;
		case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
			imx682_ana_gain_table[0]=sizeof(imx682_ana_gain_table)-1;
			if ((void *)(uintptr_t) (*(feature_data + 1)) != NULL) {
				copy_to_user((MUINT32 *)(*(feature_data + 1)),imx682_ana_gain_table, sizeof(imx682_ana_gain_table));
			}
			break;
		
		default:
			break;
	}

	return ERROR_NONE;
}	/*	feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 IMX682_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{

	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}	
