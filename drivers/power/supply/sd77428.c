/*****************************************************************************
* Copyright(c) BMT, 2023. All rights reserved.
*       
* BMT [sd77428] Source Code Reference Design
* [kernel-4.19]
* File:[sd77428.c]
*       
* This Source Code Reference Design for BMT [sd77428] access 
* ("Reference Design") is solely for the use of PRODUCT INTEGRATION REFERENCE ONLY, 
* and contains confidential and privileged information of BMT International 
* Limited. BMT shall have no liability to any PARTY FOR THE RELIABILITY, 
* SERVICEABILITY FOR THE RESULT OF PRODUCT INTEGRATION, or results from: (i) any 
* modification or attempted modification of the Reference Design by any party, or 
* (ii) the combination, operation or use of the Reference Design with non-BMT 
* Reference Design. Use of the Reference Design is at user's discretion to qualify 
* the final work result.
*****************************************************************************/
#define pr_fmt(fmt)	"[bmt]%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
//#include <linux/wakelock.h>
#include <linux/suspend.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/of.h>
//#include <asm/uaccess.h>
#include <asm/unaligned.h>
//#include <mt-plat/mtk_boot.h>
#include "sd77428.h"

static int32_t debug_print = 1;
bool sd77428_is_ok = false;

#define batt_dbg(args...)\
do {\
	if(debug_print)\
	    pr_err(args);\
} while(0)

struct sd77428_data *g_chip_data = NULL;

//-----------------------------------------------parameters set-----------------------------------------------------//
#define parameters_down_enable      1                           //是否开启参数配置下载
#define CELL_NUM 6

static uint16_t gdm_thresh_chg = 2,  gdm_thresh_dsg = 1;    // SBSA0_CONFIGDSG_CHG
//static uint8_t calib_sample = 4;                            // SBSA1_CALIBRATION_TIME

static uint8_t effect_mid_windows = 24;                     // SBSA3_FILTER_WIND_LOC
static uint8_t effect_mean_windows = 8;
static uint8_t effect_first_lock_th = 1;
static uint8_t effect_noise_range_gate = 4;

static uint16_t effect_init_delay_ratio = 920;	//0~1000;    //SBSA4_FILTER_RATIO
static uint16_t effect_noise_ratio = 25;		//0~1000;

static uint8_t SECOND_LOCK_FLAG = 0;                        //SBSA5_FILTER_ENABLE
static uint8_t filter_switcher = 1;
static uint8_t calib_switcher = 0;
static uint8_t filter_offset = 1;

static uint8_t SECOND_LOCK_TH = 4;			               //SBSA6_FILTER_SECOND_LOCK
static uint8_t SECOND_LOCK_TIMER = 210;
static uint8_t SECOND_UNLOCK_TIMER = 200;

//static uint8_t _IDLE_CHARGE_EST_ = 1;                      //SBSA7_FG_IDLE_SOH
//static uint8_t FG_idle_60s_enable = 1;                        
//static uint8_t FG_cc_soh_ratio = 20;                        
//static uint8_t FG_dc_soh_ratio = 15;  

static uint16_t FG_idle_delay_curr = 350;                  //SBSA8_FG_IDLE_SYNCCURR
static uint16_t FG_idle_wave_curr = 25;        

static uint8_t FG_charge_thm_comp1 = 11;                      //SBSA9_FG_CC_THM_COMP
static uint8_t FG_charge_thm_comp2 = 10;                        
static uint8_t FG_charge_thm_comp3 = 26;                        
static uint8_t FG_charge_thm_comp4 = 40;  

static uint8_t FG_charge_thm_comp5 = 11;                      //SBSAA_FG_CC_THM_RANGE
static uint8_t FG_charge_thm_range1 = 5;                        
static uint8_t FG_charge_thm_range2 = 25;                        
static uint8_t FG_charge_thm_range3 = 45; 

static uint16_t FG_charge_maxdfcc = 9000;                     //SBSAB_FG_CC_DFCC_RANGE
static uint16_t FG_charge_mindfcc = 800;    

static uint8_t FG_charge_cv_eoctimes = 26;                      //SBSAC_FG_CC_CV_CURR
static uint8_t FG_charge_tail_ccratio = 25;                        
static uint8_t FG_charge_thm_curr = 4;                        
static uint8_t FG_charge_fast_curr = 6; 

static uint16_t FG_dc_max_dfcc = 9500;                     //SBSAD_FG_CC_DFCC_RANGE
static uint16_t FG_dc_min_dfcc = 1800;    

static uint8_t FG_dc_tail_th = 8;                      //SBSAE_FG_DC_TAIL
static uint8_t FG_dc_soc_des1 = 15;                        
static uint8_t FG_dc_soc_des2 = 10;                        
static uint8_t FG_dc_tail_cc = 25; 

static uint16_t FG_CC_DFCC_MODE = 2;                     //SBSAF_FG_CC_DFCC_SET
static uint16_t FG_CC_hardset_DFCC = 4800;  


//static uint16_t ntc_reg_enable = 0;					//SBSB0_NTC_HOLD_TIME
//static uint16_t ntc_reg_holdtime = 0;

//static uint16_t NTC_calculate_mode = 0x0132;  //mode1               //SBSB1_NTC_PULL_MODE
//static uint16_t NTC_delay_parrsense = 0;

//static uint16_t NTC_pullup_sense = 0x0032;  //50k               //b2
//static uint16_t NTC_pullup_refvol = 0x05dc;//1500mv

#define OCV_DATA_NUM 	 65
#define XAxis 		 36//83
#define YAxis 		 4//6
#define ZAxis 		 4//8
#define TEMPERATURE_DATA_NUM 	40

#define DFCC_X  6
#define DFCC_Y  3  
#define DFCC_Z  4 

static short ocv_volt_data[OCV_DATA_NUM] = {2991, 3371, 3523, 3618, 3674, 3684, 3686, 3688, 3689, 3703, 3713, 3723, 3733, 3741, 3747, 3756, 
                                    3763, 3772, 3780, 3790, 3797, 3804, 3809, 3816, 3822, 3829, 3837, 3843, 3852, 3861, 3868, 3879, 
                                    3889, 3900, 3913, 3928, 3945, 3966, 3986, 4004, 4021, 4036, 4051, 4070, 4087, 4106, 4125, 4142, 
                                    4160, 4179, 4198, 4217, 4237, 4256, 4274, 4294, 4312, 4330, 4346, 4363, 4378, 4393, 4407, 4422, 4441};
//RC table X Axis value, in mV format
static short	XAxisElement[XAxis] = { 3000, 3040, 3080, 3120, 3160, 3200, 3260, 3300, 3340, 3380, 3420, 3460, 
                                        3500, 3542, 3580, 3615, 3664, 3705, 3743, 3772, 3794, 3817, 3845, 3877, 
                                        3920, 3981, 4034, 4090, 4149, 4179, 4229, 4259, 4282, 4316, 4352, 4431};

//RC table Y Axis value, in mA format
static short	YAxisElement[YAxis] = {600, 1200, 2000, 2500};
// RC table Z Axis value, in 10*'C format
static short	ZAxisElement[ZAxis] = {-100, 0, 250, 450};
// contents of RC table, its unit is 10000C, 1C = DesignCapacity
static uint8_t	RCtable[YAxis*ZAxis][XAxis] = {
// # {//temp = -10 ^C},
{6, 7, 7, 7, 8, 8, 9, 10, 11, 12, 14, 16, 19, 22, 26, 31, 39, 46, 52, 55, 58, 61, 63, 66, 70, 75, 80, 85, 90, 93, 97, 99, 100, 100, 100, 100},
{9, 9, 9, 10, 11, 12, 13, 15, 17, 19, 22, 26, 30, 35, 41, 46, 53, 58, 63, 66, 68, 70, 72, 75, 79, 84, 89, 94, 99, 100, 100, 100, 100, 100, 100, 100},
{10, 10, 11, 12, 13, 14, 17, 19, 22, 25, 29, 34, 39, 46, 51, 56, 61, 66, 69, 72, 74, 76, 78, 81, 84, 89, 92, 95, 97, 98, 99, 100, 100, 100, 100, 100},
{10, 11, 11, 12, 13, 15, 18, 20, 23, 27, 32, 37, 42, 49, 54, 58, 63, 67, 71, 74, 76, 78, 80, 83, 87, 94, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100},
// # {//temp = 0 ^C},
{6, 6, 6, 6, 7, 7, 7, 7, 7, 8, 8, 9, 10, 11, 14, 17, 22, 31, 40, 45, 49, 52, 56, 59, 64, 69, 73, 78, 83, 85, 90, 92, 95, 98, 100, 100},
{9, 9, 9, 10, 10, 10, 10, 11, 11, 12, 13, 15, 17, 20, 23, 28, 37, 45, 51, 55, 58, 60, 63, 66, 70, 75, 79, 83, 88, 91, 95, 97, 99, 100, 100, 100},
{10, 11, 11, 11, 11, 12, 12, 13, 14, 16, 18, 21, 24, 29, 35, 41, 49, 55, 59, 62, 65, 67, 69, 72, 75, 80, 84, 89, 93, 96, 99, 100, 100, 100, 100, 100},
{11, 11, 12, 12, 12, 13, 14, 15, 16, 18, 21, 25, 29, 35, 41, 47, 54, 59, 63, 66, 68, 70, 72, 75, 78, 83, 87, 91, 97, 99, 100, 100, 100, 100, 100, 100},
// # {//temp = 25 ^C},
{1, 1, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 5, 6, 12, 18, 26, 34, 39, 44, 49, 53, 58, 63, 68, 73, 78, 80, 84, 87, 89, 92, 95, 100},
{1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 5, 5, 7, 9, 17, 24, 34, 40, 44, 48, 53, 56, 60, 66, 70, 75, 80, 82, 86, 89, 91, 94, 97, 100},
{1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 5, 6, 8, 13, 21, 30, 39, 45, 48, 52, 55, 59, 62, 68, 73, 77, 82, 84, 88, 91, 93, 96, 99, 100},
{1, 1, 2, 2, 2, 2, 2, 3, 3, 4, 4, 5, 6, 8, 10, 16, 25, 35, 42, 48, 51, 54, 57, 60, 64, 69, 74, 79, 83, 86, 90, 92, 94, 98, 100, 100},
// # {//temp = 45 ^C},
{0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 4, 4, 5, 10, 18, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 78, 82, 84, 86, 89, 93, 100},
{0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 7, 15, 22, 29, 35, 40, 44, 48, 53, 57, 62, 67, 72, 77, 79, 83, 86, 88, 91, 95, 100},
{1, 1, 1, 1, 1, 1, 2, 2, 2, 3, 3, 4, 4, 6, 8, 10, 20, 27, 35, 41, 45, 48, 52, 56, 60, 65, 70, 75, 79, 82, 86, 88, 90, 94, 97, 100},
{1, 1, 1, 1, 1, 1, 2, 2, 2, 3, 3, 4, 5, 7, 9, 14, 23, 31, 39, 44, 47, 50, 54, 58, 61, 67, 71, 76, 81, 83, 87, 90, 92, 95, 99, 100},
};

static int cell_temp_data[TEMPERATURE_DATA_NUM] = {1264, 1451, 1672, 1928, 2231, 2591, 3021, 3536, 4156, 4907, 5820, 6936, 8307, 10000, 
                                            12100, 14740,18050,22250,27620,34520,43470,55170,70610,90990,118300};

//SOC区间
static short DFCC_XDATA[DFCC_X] = { 0, 1000, 3000, 5000, 7000, 9000 };

//电流区间
static short DFCC_YDATA[DFCC_Y] = { 600, 1200, 2000 };

//温度区间
static short DFCC_ZDATA[DFCC_Z] = { -200, -100, 0, 250 };
/*
static unsigned char DFCC_table[DFCC_Y * DFCC_Z][DFCC_X] = {
//temp = -20 ^C
{90,60,60,85,85,85}, // 600mA           最佳
{85,85,85,85,85,85}, //1200mA               最佳 2
{85,85,85,85,85,85}, //2000mA           最佳

//temp = -10 ^C
{105,105,100,100,100,100}, // 600mA         最佳  旧数据
{100,100,100,100,100,100}, // 1200mA            最佳  旧数据
{105,105,100,100,100,100}, // 2000mA        最佳  旧数据

//temp = 0 ^C
{80,85,90,95,100,100},
{80,90,100,105,100,100},
{95,100,100,105,100,100},

//temp = 25 ^C
{80,85,90,95,95,95},
{80,90,90,95,95,95},
{80,90,90,95,95,95}
};
*/
static unsigned char DFCC_table[DFCC_Y * DFCC_Z][DFCC_X] = {
	//temp = -20 ^C
    {80,85,90,95,100,100},
    {80,90,95,100,100,100},
    {80,90,95,100,100,100},

    //temp = -10 ^C
    {80,85,90,95,100,100},
    {80,90,95,100,100,100},
    {80,90,95,100,100,100},

    //temp = 0 ^C
    {80,85,90,95,100,100},
    {80,90,100,105,100,100},
    {95,100,100,105,100,100},

    //temp = 25 ^C
    {80,85,90,95,95,95},
    {80,90,90,95,95,95},
    {80,90,90,95,95,95}

};

//-------------------------------------------------------------------------------------------------------------------//
static uint16_t parameters_down_level8(struct sd77428_data *chip);
static int32_t sd77428_set_sbs_params(struct sd77428_data *chip, uint8_t cmd, uint32_t param, uint8_t len);
static int32_t sd77428_get_sbs_params(struct sd77428_data *chip, uint8_t cmd, uint32_t* param, uint8_t len);
static void sd77428_get_fourbytesparams(uint32_t param, uint8_t *param_l1, uint8_t *param_l2, uint8_t *param_l3, uint8_t *param_l4);
static int32_t sd77428_get_sbs_params(struct sd77428_data *chip, uint8_t cmd, uint32_t* param, uint8_t len);

//-----------------------------------------------I2C read/write-----------------------------------------------------//
static int32_t sd77428_i2c_read_bytes(struct sd77428_data* chip, uint8_t cmd, uint8_t *val, uint8_t bytes)
{
	int32_t ret = 0;
	uint8_t i 	= 0;	
	uint8_t sendbuf[48] = {0};
	struct i2c_msg msgs[2];
	usleep_range(1000,2000);

	mutex_lock(&chip->i2c_rw_lock);
	
	sendbuf[0] 		= cmd;

	msgs[0].addr   = chip->client->addr;;
	msgs[0].flags  = 0;//W cmd
	msgs[0].buf    = sendbuf;
	msgs[0].len    = 1;    

	msgs[1].addr   = chip->client->addr;;
	msgs[1].flags  = 1;//R cmd
	msgs[1].buf    = val;
	msgs[1].len    = bytes;

	for(i=0;i<4;i++)
	{
		ret = i2c_transfer(chip->client->adapter,msgs,ARRAY_SIZE(msgs));
		if(ARRAY_SIZE(msgs) == ret) 
			break;
		else
			usleep_range(10000,20000);//delay 10ms,retry
	}

	mutex_unlock(&chip->i2c_rw_lock);

	return (ARRAY_SIZE(msgs) == ret) ? 0 : -1;
}

static int32_t sd77428_i2c_write_bytes(struct sd77428_data* chip,uint8_t cmd,uint8_t *val, uint8_t bytes)
{
	int32_t ret = 0;
	uint8_t i = 0;
	uint8_t sendbuf[48] = {0};
	struct i2c_msg msgs[1];
	usleep_range(1000,2000);
	mutex_lock(&chip->i2c_rw_lock);
	sendbuf[0] 		= cmd;
	memcpy(sendbuf+1,val,bytes);

	msgs[0].addr   = chip->client->addr;
	msgs[0].flags  = 0;//W cmd
	msgs[0].buf    = sendbuf;
	msgs[0].len    = 1+bytes;   

	for(i=0;i<4;i++)
	{
		ret = i2c_transfer(chip->client->adapter,msgs,ARRAY_SIZE(msgs));
		if(ARRAY_SIZE(msgs) == ret) 
			break;
		else
			usleep_range(10000,20000);//delay 10ms,retry
	}

	mutex_unlock(&chip->i2c_rw_lock);

	return (ARRAY_SIZE(msgs) == ret) ? 0 : -1;
}

static uint8_t calculate_pec_byte(uint8_t data, uint8_t crcin)  
{
	//Calculate CRC-8 value; uses polynomial input
	uint8_t crc8  = data ^ crcin;
	uint8_t index = 0;
	for (index = 0; index < 8; index++) {
		if (crc8 & 0x80)
			crc8 = (crc8 << 1) ^ I2C_PEC_POLY;
		else
			crc8 = (crc8 << 1);
	}
	return crc8;
}

static uint8_t calculate_pec_bytes(uint8_t init_pec,uint8_t* pdata,uint8_t len)
{
	uint8_t i   = 0;
	uint8_t pec = init_pec;
	for(i=0;i<len;i++)
		pec = calculate_pec_byte(pdata[i],pec);

	return pec;
}

static int32_t sd77428_read_word(struct sd77428_data *chip, uint8_t cmd, uint16_t* pdst)
{    
	int32_t  ret = 0;
	uint8_t  rxbuf[32] = {0};
	uint8_t  i = 0;
	uint8_t  pec = 0;

    for(i=0;i<4;i++)
    {
        ret = sd77428_i2c_read_bytes(chip,cmd,rxbuf,3);
        
        if(0 == ret)
        {
    #if (1 == PEC_ENABLE)
            pec = calculate_pec_byte(chip->client->addr << 1,pec);
            pec = calculate_pec_byte(cmd,pec);
            pec = calculate_pec_byte(chip->client->addr << 1 | 1,pec);
            
            if(rxbuf[2] != calculate_pec_bytes(pec,rxbuf,2))
			{
                dev_err(chip->dev,"PEC verification fail cmd %02x,retry %d\n",cmd,i);
                usleep_range(10000,20000);
                pec = 0;
                continue;
			}
    #endif

            *pdst = get_unaligned_le16(rxbuf);//*pdst = rxbuf[0] << 8 | rxbuf[1];
            break;
        }
    }

    if(i>=4)
    {
        chip->err_times++;
        dev_err(chip->dev,"%s cmd[0x%02X], err %d, %d, [0x%02X,0x%02X,0x%02X,0x%02X,0x%02X]\n",__func__, cmd, ret,chip->err_times,rxbuf[0],rxbuf[1],rxbuf[2],rxbuf[3],rxbuf[4]);
    }
    else
        chip->err_times = 0;
        
    return ret;
}

static int32_t sd77428_write_word(struct sd77428_data *chip,uint8_t index,uint16_t data)
{
	uint8_t w_buf[3] = {0};
	uint8_t pec = 0;
	w_buf[0] = (uint8_t)((data & 0xFF00) >> 8);
	w_buf[1] = (uint8_t)(data & 0x00FF);

    // w_buf[0] = (uint8_t)(data & 0x00FF);
    // w_buf[1] = (uint8_t)((data & 0xFF00) >> 8);
    
	pec = calculate_pec_byte(chip->client->addr << 1,0);
	pec = calculate_pec_byte(index,pec);
	pec = calculate_pec_byte(w_buf[0],pec);
	pec = calculate_pec_byte(w_buf[1],pec);
	w_buf[2] = pec;

	return sd77428_i2c_write_bytes(chip,index, w_buf, 3); 
}

static int32_t sd77428_read_sbs(struct sd77428_data *chip, uint8_t cmd, uint8_t *data_buf, uint8_t len)
{
	int32_t  ret = 0;
	uint8_t  rxbuf[132] = {0};
	uint8_t  i = 0, j = 0 ;
	uint8_t  pec = 0;
	// uint8_t  read_len = (len >> 2) << 2;
	uint8_t  read_len = len;

    for(i=0;i<4;i++)
    {
        ret = sd77428_i2c_read_bytes(chip, cmd, rxbuf, read_len+1);
        // pr_info("cmd[0x%02x], read_len:%d : %x, %x, %x, %x, %x, %x, %x, %x,%x, %x, %x, %x,%x, %x, %x, %x, %x, %x, %x, %x, %x, %x, %x, %x,%x, %x, %x, %x,%x, %x, %x, %x, pec: %x\r\n",
        //                     cmd, read_len+1, rxbuf[0],rxbuf[1],rxbuf[2],rxbuf[3],rxbuf[4],rxbuf[5],rxbuf[6],rxbuf[7],
        //                     rxbuf[8],rxbuf[9],rxbuf[10],rxbuf[11],rxbuf[12],rxbuf[13],rxbuf[14],rxbuf[15],
        //                     rxbuf[16],rxbuf[17],rxbuf[18],rxbuf[19],rxbuf[20],rxbuf[21],rxbuf[22],rxbuf[23],
        //                     rxbuf[24],rxbuf[25],rxbuf[26],rxbuf[27],rxbuf[28],rxbuf[29],rxbuf[30],rxbuf[31],rxbuf[read_len]);
        if(0 == ret)
        {
    #if (1 == PEC_ENABLE)
            pec = calculate_pec_byte(chip->client->addr << 1,pec);
            pec = calculate_pec_byte(cmd,pec);
            pec = calculate_pec_byte(chip->client->addr << 1 | 1,pec);
            pec = calculate_pec_bytes(pec, rxbuf, read_len);
            if(rxbuf[read_len] != pec)
			{
                dev_err(chip->dev,"PEC verification fail cmd %02x,retry %d, rec_pec:0x%02x, cal:0x%02x.\n",cmd,i, rxbuf[read_len], pec);
                usleep_range(10000,20000);
                pec = 0;
                continue;
			}
    #endif

            for(j=0; j<read_len; j++)
				data_buf[j] = rxbuf[j];

            break;
        }
    }

    if(i>=4)
    {
        chip->err_times++;
        dev_err(chip->dev,"%s cmd[0x%02X], err %d, %d, [0x%02X,0x%02X,0x%02X,0x%02X,0x%02X]\n",__func__, cmd, ret,chip->err_times,rxbuf[0],rxbuf[1],rxbuf[2],rxbuf[3],rxbuf[4]);
        return -1;
    }
    else
        chip->err_times = 0;
        
    return ret;
}

static int32_t sd77428_write_sbs(struct sd77428_data* chip, uint8_t cmd, uint8_t *data_buf, uint8_t len)
{
	uint8_t w_buf[132] = {0};
	uint8_t pec = 0;
	uint32_t i = 0;
	//uint8_t write_len = (len >> 2) << 2;
	uint8_t write_len = len;

	for(i=0; i<write_len; i++)
		w_buf[i] = data_buf[i];

	pec = calculate_pec_byte(chip->client->addr << 1,0);
	pec = calculate_pec_byte(cmd,pec);
	pec = calculate_pec_bytes(pec,w_buf,write_len);
	w_buf[write_len] = pec;

	return sd77428_i2c_write_bytes(chip,cmd, w_buf, write_len+1);  
}

//-----------------------------------------------------power_supply-----------------------------------------------------//
static enum power_supply_property sd77428_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_HEALTH,
};
#if 0
static char *sd77428_supplied_from[] = {
	"usb",
	"charger",
	"ac",
};
#endif
static int32_t sd77428_battery_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct sd77428_data *chip = (struct sd77428_data *)power_supply_get_drvdata(psy);

	switch (psp) {

	case POWER_SUPPLY_PROP_STATUS:

		if (chip->adapter_status == O2_CHARGER_BATTERY)
		{
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING; /*discharging*/
		}
		else if(chip->adapter_status == O2_CHARGER_USB ||
		        chip->adapter_status == O2_CHARGER_AC )
		{
			if (chip->batt_info.batt_rsoc == 100)
				val->intval = POWER_SUPPLY_STATUS_FULL;
			else
				val->intval = POWER_SUPPLY_STATUS_CHARGING;	/*charging*/

		}
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
		
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = 1;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chip->batt_info.batt_voltage * 1000;
		//val->intval = 3800* 1000;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = chip->batt_info.batt_current * 1000;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		//val->intval = chip->batt_info.batt_rsoc;
		val->intval =50;
		break;

	case POWER_SUPPLY_PROP_TEMP:
		//val->intval = chip->batt_info.batt_temp;
		val->intval = 250;
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = chip->batt_info.batt_fcc;
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = chip->batt_info.batt_capacity;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chip->batt_info.batt_soh;;
		break;	
	default:
		return -EINVAL;
	}

	return 0;
}

#if 0
static void sd77428_external_power_changed(struct power_supply *psy)
{
	struct sd77428_data *chip = (struct sd77428_data *)power_supply_get_drvdata(psy);
	dev_info(chip->dev,"enter\n");
	if(true == sd77428_is_ok)
	{
		cancel_delayed_work(&chip->work);
		schedule_delayed_work(&chip->work,0);
		power_supply_changed(chip->bat);
	}
}
#endif
static int32_t sd77428_power_supply_init(struct sd77428_data *chip)
{
	chip->bat_cfg.drv_data 			= chip;
	chip->bat_cfg.of_node  			= chip->client->dev.of_node;

	chip->bat_desc.name 			= chip->name;//dev_name(chip->dev); //"sd77428";  chip->name
	chip->bat_desc.type				= POWER_SUPPLY_TYPE_BATTERY;
	chip->bat_desc.properties 		= sd77428_battery_props;
	chip->bat_desc.num_properties = ARRAY_SIZE(sd77428_battery_props);
	chip->bat_desc.get_property 	= sd77428_battery_get_property;
	chip->bat_desc.no_thermal 		= 1;
	//chip->bat_desc.external_power_changed = sd77428_external_power_changed;

	chip->bat = devm_power_supply_register(chip->dev,&chip->bat_desc,&chip->bat_cfg);

	if (IS_ERR(chip->bat)) 
	{
		pr_err("Couldn't register power supply\n");
		return PTR_ERR(chip->bat);
	}
	else
	{
		//chip->bat->supplied_from = sd77428_supplied_from;
		//chip->bat->num_supplies  = ARRAY_SIZE(sd77428_supplied_from);
	}

	return 0;
}
//---------------------------------------------------------------------------------------------------------------------//

//-----------------------------------------------------sysfs接口-------------------------------------------------------//
/*****************************************************************************
 * Description: sd77428_debug_show
 * Parameters:  read example: cat sd77428_debug 
 *****************************************************************************/
static ssize_t sd77428_debug_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	return sprintf(buf,"%d\n", debug_print);
}

/*****************************************************************************
 * Description: sd77428_debug_store
 * Parameters: write example: echo 1 > sd77428_debug ---open debug
 *****************************************************************************/
static ssize_t sd77428_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t _count)
{
	int32_t val = 0;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if(val == 1)
	{
		debug_print = 1;
		pr_info("DEBUG ON \n");
	}
	else if (val == 0)
	{
		debug_print = 0;
		pr_info("DEBUG CLOSE \n");
	}
	else
	{
		pr_err("invalid command\n");
		return -EINVAL;
	}
	return _count;
}

static DEVICE_ATTR(debug, S_IRUGO | (S_IWUSR|S_IWGRP), sd77428_debug_show, sd77428_debug_store);
static struct attribute *sd77428_attributes[] = {
	&dev_attr_debug.attr,
	NULL,
};

static struct attribute_group sd77428_attribute_group = {
	.attrs = sd77428_attributes,
};

int32_t sd77428_get_remaincap(void)
{
	batt_data_t* pinfo = &g_chip_data->batt_info;
	return pinfo->batt_rc;
}
EXPORT_SYMBOL(sd77428_get_remaincap);

int32_t sd77428_get_soc(void)
{
	batt_data_t* pinfo = &g_chip_data->batt_info;
	return pinfo->batt_rsoc;
}
EXPORT_SYMBOL(sd77428_get_soc);

int32_t sd77428_get_soh(void)
{
	batt_data_t* pinfo = &g_chip_data->batt_info;
	return pinfo->batt_soh;
}
EXPORT_SYMBOL(sd77428_get_soh);

int32_t sd77428_get_battry_current(void)
{
	batt_data_t* pinfo = &g_chip_data->batt_info;
	return pinfo->batt_current;
}
EXPORT_SYMBOL(sd77428_get_battry_current);

int32_t sd77428_get_battery_voltage(void)
{
	batt_data_t* pinfo = &g_chip_data->batt_info;
	return pinfo->batt_voltage;
}
EXPORT_SYMBOL(sd77428_get_battery_voltage);

int32_t sd77428_get_battery_temp(void)
{
	batt_data_t* pinfo = &g_chip_data->batt_info;
	return pinfo->batt_temp;
}
EXPORT_SYMBOL(sd77428_get_battery_temp);

struct i2c_client * sd77428_get_client(void)
{
	if (g_chip_data)
		return g_chip_data->client;
	else
	{
		pr_err("NULL pointer\n");
		return NULL;
	}
}
EXPORT_SYMBOL(sd77428_get_client);

int8_t sd77428_get_adapter_status(void)
{
	return g_chip_data->adapter_status;
}
EXPORT_SYMBOL(sd77428_get_adapter_status);

static int32_t sd77428_init_batt_info(struct sd77428_data *chip)
{
	chip->batt_info.batt_voltage = 3800;
	chip->batt_info.batt_current = 0;
	chip->batt_info.batt_temp = 250;
	chip->batt_info.batt_rsoc = 1;
	chip->batt_info.batt_soh = 100;
	chip->batt_info.batt_fcc = BATT_DESIGN_FCC; 
	chip->batt_info.batt_capacity = BATT_DESIGN_FCC;
	return 0;
}

static void sd77428_get_batt_info(struct sd77428_data *chip)
{	
	int32_t ret = 0;
	batt_data_t* pinfo = &chip->batt_info;

	ret = sd77428_read_word(chip, (sbsd_cmd_def[SBS02_EXTTMP] >> SBSD_CMD_Pos) & 0xFF, &pinfo->batt_temp);
	if(0 == ret)
		pinfo->batt_temp -= 2730;
	else
		pinfo->batt_temp = 250;

	ret = sd77428_read_word(chip, (sbsd_cmd_def[SBS04_BATTVOLT] >> SBSD_CMD_Pos) & 0xFF, &pinfo->batt_voltage);
	ret += sd77428_read_word(chip, (sbsd_cmd_def[SBS10_BATTCURR] >> SBSD_CMD_Pos) & 0xFF, &pinfo->batt_current);
	ret += sd77428_read_word(chip, (sbsd_cmd_def[SBS1C_RSOC] >> SBSD_CMD_Pos) & 0xFF, &pinfo->batt_rsoc);
	ret += sd77428_read_word(chip, (sbsd_cmd_def[SBS0E_FCC] >> SBSD_CMD_Pos) & 0xFF, &pinfo->batt_fcc);
	ret += sd77428_read_word(chip, (sbsd_cmd_def[SBS0C_RC] >> SBSD_CMD_Pos) & 0xFF, &pinfo->batt_rc);
	ret += sd77428_read_word(chip, (sbsd_cmd_def[SBS20_SOH] >> SBSD_CMD_Pos) & 0xFF, &pinfo->batt_soh);
	ret += sd77428_read_word(chip, (sbsd_cmd_def[SBS66_DSNCAP] >> SBSD_CMD_Pos) & 0xFF, &pinfo->batt_capacity);
	ret += sd77428_read_word(chip, (sbsd_cmd_def[SBS65_CYCLECNT] >> SBSD_CMD_Pos) & 0xFF, &pinfo->batt_cyclecnt);

	ret += sd77428_read_word(chip,(sbsd_cmd_def[SBS91_EXTCHGSTS] >> SBSD_CMD_Pos) & 0xFF, &pinfo->ext_charger);
    if (ret == 0)
    {
	batt_dbg("vbat:%d, ibat:%05d, tbat:%d, rsoc:%03d, fcc:%d, dcap:%d, soh:%d, cycle:%d, rc:%d, ext_chg %d\n",
		pinfo->batt_voltage,
		pinfo->batt_current,
		pinfo->batt_temp/10,
		pinfo->batt_rsoc,
		pinfo->batt_fcc,
		pinfo->batt_capacity,
		pinfo->batt_soh,
		pinfo->batt_cyclecnt,
		pinfo->batt_rc,
		pinfo->ext_charger);
    }
}
#if 0
static void sd77428_check_charge_type(struct sd77428_data *chip)
{
	int32_t ret = 0;
	union power_supply_propval val_usb = {0};
	union power_supply_propval val_ac = {0};
	chip->adapter_status = O2_CHARGER_BATTERY;

	if(!chip->ac_psy)
		chip->ac_psy = power_supply_get_by_name ("ac");

	if(chip->ac_psy)
	{
		ret = power_supply_get_property(chip->ac_psy, POWER_SUPPLY_PROP_ONLINE, &val_ac);
		if (0 == ret && val_ac.intval)
			chip->adapter_status = O2_CHARGER_AC;
	}
	
	if(!chip->usb_psy)
		chip->usb_psy= power_supply_get_by_name ("usb");

	if(chip->usb_psy)
	{
		ret = power_supply_get_property(chip->usb_psy, POWER_SUPPLY_PROP_ONLINE, &val_usb);
		if (0 == ret && val_usb.intval)
			chip->adapter_status = O2_CHARGER_USB;
	}

	// pr_info("val_usb.intval %d adapter_status:%d\n",val_usb.intval, chip->adapter_status);
}
#endif
static void sd77428_update_charger_info(struct sd77428_data *chip)
{
    //int ret = 0;
    //int16_t param_low = 0;
   // uint32_t param = 0;
#if 0//customer need to add charger connect/disconnect,full status 
    sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBS91_EXTCHGSTS] >> SBSD_CMD_Pos) & 0xFF, &param, 2);
    param_low = (int16_t)param;
    //pr_info("SBS SBS91_EXTCHGSTS recall data: (%d).\r\n", param_low);

	if(param_low != charger_disconnected && 0 == attach)
	{
		sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBS91_EXTCHGSTS] >> SBSD_CMD_Pos) & 0xFF, charger_disconnected, 2);
		return;
	}
	
    ret = check_charger_full();

    if(1 == ret) {//charger report full 
        chip->chg_full = 1;
        if (param_low != charger_full) {
            sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBS91_EXTCHGSTS] >> SBSD_CMD_Pos) & 0xFF, charger_full, 2);
        }
    } else {//charger report not full ,get_charge_term()=0 mean adapter disconnect or not charging,get_charge_term()=1 or 2 mean in charging and not full
        if (get_charge_term() == 0) {
            if (param_low != charger_disconnected) {
                sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBS91_EXTCHGSTS] >> SBSD_CMD_Pos) & 0xFF, charger_disconnected, 2);
            }
        } else {
            /*charge_term=1 or 2*/
            if (param_low != charger_connected) {
                sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBS91_EXTCHGSTS] >> SBSD_CMD_Pos) & 0xFF, charger_connected, 2);
            }
        }
        chip->chg_full = 0;
    }
    
    pr_info("full %d, charge_term %d\n", chip->chg_full, get_charge_term());
#endif

	return;
}


static int32_t sd77428_detect_ic(struct sd77428_data *chip)
{
	uint16_t chip_id = 0;
	uint8_t i = 0;
   // uint8_t buf[5] = {0};
	
	for(i=0;i<5;i++)
	{
        sd77428_read_word(chip,(sbsd_cmd_def[SBSF1_CHIPVER] >> SBSD_CMD_Pos) & 0xFF, &chip_id);
        chip_id = chip_id &0xFF;
        pr_info("chip_id = 0x%x\n", chip_id);
		if(chip_id == 0x12)
		{
			sd77428_is_ok = true;
			break;
		}
		else
		{
			sd77428_is_ok = false;
		}
	}

	pr_info("sd77428_is_ok %d\n",sd77428_is_ok);
	
	return (true == sd77428_is_ok) ? 0 : -1;
}


//--------------------------------------------------SBS i2c 指令数据读写----------------------------------------------------//
//SBSA3_FILTER_WIND_LOC, SBSA5_FILTER_ENABLE
static int32_t sd77428_set_sbs_params(struct sd77428_data *chip, uint8_t cmd, uint32_t param, uint8_t len)
{
	uint8_t w_buf[5] = {0};
	
	w_buf[3] = (uint8_t)((param & 0xFF000000) >> 24);
	w_buf[2] = (uint8_t)((param & 0x00FF0000) >> 16);
	w_buf[1] = (uint8_t)((param & 0x0000FF00) >> 8);
	w_buf[0] = (uint8_t)(param & 0x000000FF);

	return sd77428_write_sbs(chip, cmd, w_buf, len); 
}

static int32_t sd77428_get_sbs_params(struct sd77428_data *chip, uint8_t cmd, uint32_t* param, uint8_t len)
{
	int32_t ret = 0;
	
	ret = sd77428_read_sbs(chip, cmd, (uint8_t*)param, len);

	return ret; 
}

static void sd77428_get_fourbytesparams(uint32_t param, uint8_t *param_l1, uint8_t *param_l2, uint8_t *param_l3, uint8_t *param_l4)
{
	*param_l1 = (uint8_t)((param & 0xFF000000) >> 24);
    *param_l2 = (uint8_t)((param & 0x00FF0000) >> 16);
    *param_l3 = (uint8_t)((param & 0x0000FF00) >> 8);
    *param_l4 = (uint8_t)(param & 0x000000FF);

	return;
}

static void sd77428_get_twobytesparams(uint32_t param, uint16_t* param_high, uint16_t* param_low)
{
	*param_low = (uint16_t)param;
	*param_high = (uint16_t)((param & 0xFFFF0000) >> 16);
	
	return ; 
}

//-------------------------------------------------------------table 更新下载------------------------------------------------------------//
//SBSDF_TABLE_INDEX 
static int32_t sd77428_set_table_packidx(struct sd77428_data *chip, uint16_t opera_table_packidx)
{
	//在MCU中，按照lsb对int数据在前的顺序解析；writeword按照高位先发的顺序执行。需要在发送端调换一下顺序
    uint8_t h_addr = (uint8_t)((opera_table_packidx&0xFF00)>>8);
    uint8_t l_addr = (uint8_t)(opera_table_packidx&0x00FF);
    pr_info("opera_table_packidx:0x%04x, h_addr:0x%02x, l_addr:0x%02x \n", opera_table_packidx, h_addr, l_addr);
    opera_table_packidx = (l_addr<<8 | h_addr);
	return sd77428_write_word(chip, (sbsd_cmd_def[SBSDF_TABLE_INDEX] >> SBSD_CMD_Pos) & 0xFF, opera_table_packidx);
}
#if 0
static int32_t sd77428_get_table_packidx(struct sd77428_data *chip, uint16_t* opera_table_packidx)
{
	return sd77428_read_word(chip, (sbsd_cmd_def[SBSDF_TABLE_INDEX] >> SBSD_CMD_Pos) & 0xFF, opera_table_packidx);
}
#endif
//---------------------------------------------------------------------------------------------------------------------------------//
//------------------------------------------------------table 更新下载通用函数--------------------------------------------------------//
// SBSE0_TABLE_OCV, SBSE1_TABLE_VOLT, SBSE4_TABLE_RC 
// SBSE5_TABLE_NTC_TEMP
static int32_t sd77428_set_packtable(struct sd77428_data *chip, uint8_t cmd, uint8_t* packtable, uint16_t packlens)
{
	uint8_t whole_packidx = 0;
	uint8_t last_packlens = 0;

	uint8_t cur_packidx = 1; // 分包有效索引从1开始
	uint8_t w_buf[33] = {0};
	uint16_t offset_pt = 0;
	
	whole_packidx = packlens/30 + 1; //整除也+1,表示包含的有效整包数目; index从1开始

	if (packlens % 30)  last_packlens = packlens - (whole_packidx - 1)* 30;


	for (; cur_packidx < whole_packidx; ++cur_packidx)
	{
		w_buf[0] = whole_packidx;
		w_buf[1] = cur_packidx;
		offset_pt = (cur_packidx - 1) * 30;
		memcpy(w_buf+2, packtable + offset_pt,30);
		
		if(sd77428_write_sbs(chip, cmd, w_buf, 32) < 0)
		{
			pr_info("Update packtable failed, pack idx %d.\r\n", cur_packidx);
			return -1;
		}
		usleep_range(20000,30000);//delay 20ms,retry
	}
	if (last_packlens)
	{
		memset(w_buf, 0, 33);
		w_buf[0] = whole_packidx;
		w_buf[1] = whole_packidx;
		offset_pt = (cur_packidx - 1) * 30;
		memcpy(w_buf+2, packtable + offset_pt, last_packlens);
		if(sd77428_write_sbs(chip, cmd, w_buf, 32) < 0)
		{
			pr_info("Update packtable failed, pack idx %d.\r\n", cur_packidx);
			return -1;
		}
	}
	return 0;
}

static int32_t sd77428_get_packtable(struct sd77428_data *chip,  uint8_t cmd, uint8_t* packtable, uint16_t packlens)
{
	uint8_t whole_packidx = 0;
	uint8_t last_packlens = 0;

	uint8_t cur_packidx = 1; // 分包有效索引从1开始
	uint8_t buf[33] = {0};
	uint16_t offset_pt = 0;

	whole_packidx = packlens/32 + 1;  //整除也+1,表示包含的有效整包数目; index从1开始

	if (packlens % 32)  last_packlens = packlens - (whole_packidx - 1)* 32;


	for (; cur_packidx < whole_packidx; ++cur_packidx)
	{
		offset_pt = (cur_packidx - 1) * 32;

		//step 1, set operature table packidx
		if(sd77428_set_table_packidx(chip, cur_packidx) < 0)
		{
			pr_info("Set packtable idx %d, failed.\r\n", cur_packidx);
			return -1;
		}
        // else
        // {
        //     pr_info("Set packtable idx %d, sucess.\r\n", cur_packidx);
        // }
		usleep_range(20000,30000);//delay 20ms,retry
		//step 2, read table pack
		if(sd77428_read_sbs(chip, cmd, buf, 32) < 0)
		{
			pr_info("get packtable failed. cur_packidx:%d .\r\n", cur_packidx);
			return -1;
		}
		memcpy(packtable + offset_pt, buf, 32);
		usleep_range(20000,30000);//delay 20ms,retry
	}
	if (last_packlens)
	{
		//pack 3
		offset_pt = (cur_packidx - 1) * 32;
		if(sd77428_set_table_packidx(chip, cur_packidx) < 0)
		{
			pr_info("Set packtable idx %d, failed.\r\n", cur_packidx);
			return -1;
		}
        // else
        // {
        //     pr_info("Set packtable idx %d, sucess.\r\n", cur_packidx);
        // }
		usleep_range(20000,30000);//delay 20ms,retry

		if(sd77428_read_sbs(chip, cmd, buf, last_packlens) < 0)
		{
			pr_info("get packtable failed. cur_packidx:%d .\r\n", cur_packidx);
			return -1;
		}
		memcpy(packtable + offset_pt, buf, last_packlens);
	}

	return 0;
}

//SBSE2_TABLE_CURR SBSE3_TABLE_TEMP 
static int32_t sd77428_set_singletable(struct sd77428_data *chip, uint8_t cmd, uint8_t* single_table, uint16_t packlens)
{
	uint8_t w_buf[33] = {0};
	memcpy(w_buf, single_table, packlens);
	if(sd77428_write_sbs(chip, cmd, w_buf, packlens) < 0)
	{
		pr_info("set single_table failed.\r\n");
		return -1;
	}
	return 0;
}
static int32_t sd77428_get_singletable(struct sd77428_data *chip, uint8_t cmd, uint8_t* single_table, uint16_t packlens)
{
	uint8_t buf[33] = {0};
	if(sd77428_read_sbs(chip, cmd, buf, packlens) < 0)
	{
		pr_info("get single_table failed.\r\n");
		return -1;
	}
	memcpy(single_table, buf, packlens);

	return 0;
}
//------------------------------------------------------------end-------------------------------------------------------------------//

//-------------------------------------------------------------------------------------------------------------------//
//-----------------------------------------------参数下载 function-----------------------------------------------------//
//download algorithm parameters
static uint16_t parameters_down_level1(struct sd77428_data *chip)
{
    uint16_t paramdown_result = 0;
    uint8_t param_l1 = 0, param_l2 = 0, param_l3=0, param_l4=0;
    uint16_t param_high = 0, param_low = 0;
	uint32_t param = 0;

    //step 1 SBSA0_CONFIGDSG_CHG
    sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBSA0_CONFIGDSG_CHG] >> SBSD_CMD_Pos) & 0xFF, (gdm_thresh_chg<<16) | gdm_thresh_dsg, 4);
    usleep_range(5000, 10000);
	sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBSA0_CONFIGDSG_CHG] >> SBSD_CMD_Pos) & 0xFF, &param, 4);
	sd77428_get_twobytesparams(param, &param_high, &param_low);
    //pr_info("SBSA0_CONFIGDSG_CHG recall data: (%d, %d).\r\n", param_high, param_low);
    if (param_high != gdm_thresh_chg ||
        param_low != gdm_thresh_dsg )
    {
        pr_info("SBSA0_CONFIGDSG_CHG Set Failed.\r\n");
        paramdown_result += (1 << 2);
    }
    else
    {
        //pr_info("SBSA0_CONFIGDSG_CHG Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 2 SBSA3_FILTER_WIND_LOC
    param = (effect_mid_windows<<24) | (effect_mean_windows<<16) | (effect_first_lock_th <<8) | effect_noise_range_gate;
    sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBSA3_FILTER_WIND_LOC] >> SBSD_CMD_Pos) & 0xFF, param, 4);
    usleep_range(5000, 10000);
	sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBSA3_FILTER_WIND_LOC] >> SBSD_CMD_Pos) & 0xFF, &param, 4);
	sd77428_get_fourbytesparams(param, &param_l1, &param_l2, &param_l3, &param_l4);
    //pr_info("SBSA3_FILTER_WIND_LOC recall data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
    if (param_l1 != effect_mid_windows ||
        param_l2 != effect_mean_windows ||
        param_l3 != effect_first_lock_th ||
        param_l4 != effect_noise_range_gate)
    {
        pr_info("SBSA3_FILTER_WIND_LOC Set Failed.\r\n");
        paramdown_result += (1 << 0);
    }
    else
    {
        //pr_info("SBSA3_FILTER_WIND_LOC Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 3 SBSA4_FILTER_RATIO
    sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBSA4_FILTER_RATIO] >> SBSD_CMD_Pos) & 0xFF, (effect_init_delay_ratio<<16) | effect_noise_ratio, 4);
	usleep_range(5000, 10000);
	sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBSA4_FILTER_RATIO] >> SBSD_CMD_Pos) & 0xFF, &param, 4);
	sd77428_get_twobytesparams(param, &param_high, &param_low);
    //pr_info("SBSA4_FILTER_RATIO recall data: (%d, %d).\r\n", param_high, param_low);
    if (param_high != effect_init_delay_ratio ||
        param_low != effect_noise_ratio )
    {
        pr_info("SBSA4_FILTER_RATIO Set Failed.\r\n");
        paramdown_result += (1 << 3);
    }
    else
    {
        //pr_info("SBSA4_FILTER_RATIO Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 4 SBSA5_FILTER_ENABLE
	param = (SECOND_LOCK_FLAG<<24) | (filter_switcher<<16) | (calib_switcher <<8) | filter_offset;
    sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBSA5_FILTER_ENABLE] >> SBSD_CMD_Pos) & 0xFF, param, 4);
    usleep_range(5000, 10000);
	sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBSA5_FILTER_ENABLE] >> SBSD_CMD_Pos) & 0xFF, &param, 4);
	sd77428_get_fourbytesparams(param, &param_l1, &param_l2, &param_l3, &param_l4);
    //pr_info("SBSA5_FILTER_ENABLE recall data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
    if (param_l1 != SECOND_LOCK_FLAG ||
        param_l2 != filter_switcher ||
        param_l3 != calib_switcher ||
        param_l4 != filter_offset)
    {
        pr_info("SBSA5_FILTER_ENABLE Set Failed.\r\n");
        paramdown_result += (1 << 1);
    }
    else
    {
        //pr_info("SBSA5_FILTER_ENABLE Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 5 SBSA6_FILTER_SECOND_LOCK
    param = (0<<24) | (SECOND_LOCK_TH<<16) | (SECOND_LOCK_TIMER<<8) | SECOND_UNLOCK_TIMER;
    sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBSA6_FILTER_SECOND_LOCK] >> SBSD_CMD_Pos) & 0xFF, param, 4);
    usleep_range(5000, 10000);
	sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBSA6_FILTER_SECOND_LOCK] >> SBSD_CMD_Pos) & 0xFF, &param, 4);
	sd77428_get_fourbytesparams(param, &param_l1, &param_l2, &param_l3, &param_l4);
    //pr_info("SBSA6_FILTER_SECOND_LOCK recall data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
    if (param_l1 != 0 ||
        param_l2 != SECOND_LOCK_TH ||
        param_l3 != SECOND_LOCK_TIMER ||
        param_l4 != SECOND_UNLOCK_TIMER)
    {
        pr_info("SBSA6_FILTER_SECOND_LOCK Set Failed.\r\n");
        paramdown_result += (1 << 1);
    }
    else
    {
        //pr_info("SBSA6_FILTER_SECOND_LOCK Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 6 SBSA7_FG_IDLE_SOH
	param = (0<<24) | (SECOND_LOCK_TH<<16) | (SECOND_LOCK_TIMER<<8) | SECOND_UNLOCK_TIMER;
    sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBSA7_FG_IDLE_SOH] >> SBSD_CMD_Pos) & 0xFF, param, 4);
    usleep_range(5000, 10000);
	sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBSA7_FG_IDLE_SOH] >> SBSD_CMD_Pos) & 0xFF, &param, 4);
	sd77428_get_fourbytesparams(param, &param_l1, &param_l2, &param_l3, &param_l4);
    //pr_info("SBSA7_FG_IDLE_SOH recall data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
    if (param_l1 != 0 ||
        param_l2 != SECOND_LOCK_TH ||
        param_l3 != SECOND_LOCK_TIMER ||
        param_l4 != SECOND_UNLOCK_TIMER)
    {
        pr_info("SBSA7_FG_IDLE_SOH Set Failed.\r\n");
        paramdown_result += (1 << 1);
    }
    else
    {
        //pr_info("SBSA7_FG_IDLE_SOH Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 7 SBSA8_FG_IDLE_SYNCCURR
	sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBSA8_FG_IDLE_SYNCCURR] >> SBSD_CMD_Pos) & 0xFF, (FG_idle_delay_curr<<16) | FG_idle_wave_curr, 4);
	usleep_range(5000, 10000);
	sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBSA8_FG_IDLE_SYNCCURR] >> SBSD_CMD_Pos) & 0xFF, &param, 4);
	sd77428_get_twobytesparams(param, &param_high, &param_low);
    //pr_info("SBSA8_FG_IDLE_SYNCCURR recall data: (%d, %d).\r\n", param_high, param_low);
    if (param_high != FG_idle_delay_curr ||
        param_low != FG_idle_wave_curr )
    {
        pr_info("SBSA8_FG_IDLE_SYNCCURR Set Failed.\r\n");
        paramdown_result += (1 << 4);
    }
    else
    {
        //pr_info("SBSA8_FG_IDLE_SYNCCURR Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 8 SBSA9_FG_CC_THM_COMP
	param = (FG_charge_thm_comp1<<24) | (FG_charge_thm_comp2<<16) | (FG_charge_thm_comp3<<8) | FG_charge_thm_comp4;
    sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBSA9_FG_CC_THM_COMP] >> SBSD_CMD_Pos) & 0xFF, param, 4);
    usleep_range(5000, 10000);
	sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBSA9_FG_CC_THM_COMP] >> SBSD_CMD_Pos) & 0xFF, &param, 4);
	sd77428_get_fourbytesparams(param, &param_l1, &param_l2, &param_l3, &param_l4);
    //pr_info("SBSA9_FG_CC_THM_COMP recall data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
    if (param_l1 != FG_charge_thm_comp1 ||
        param_l2 != FG_charge_thm_comp2 ||
        param_l3 != FG_charge_thm_comp3 ||
        param_l4 != FG_charge_thm_comp4)
    {
        pr_info("SBSA9_FG_CC_THM_COMP Set Failed.\r\n");
        paramdown_result += (1 << 1);
    }
    else
    {
        //pr_info("SBSA9_FG_CC_THM_COMP Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 9 SBSAA_FG_CC_THM_RANGE
	param = (FG_charge_thm_comp5<<24) | (FG_charge_thm_range1<<16) | (FG_charge_thm_range2<<8) | FG_charge_thm_range3;
    sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBSAA_FG_CC_THM_RANGE] >> SBSD_CMD_Pos) & 0xFF, param, 4);
    usleep_range(5000, 10000);
	sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBSAA_FG_CC_THM_RANGE] >> SBSD_CMD_Pos) & 0xFF, &param, 4);
	sd77428_get_fourbytesparams(param, &param_l1, &param_l2, &param_l3, &param_l4);
    //pr_info("SBSAA_FG_CC_THM_RANGE recall data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
    if (param_l1 != FG_charge_thm_comp5 ||
        param_l2 != FG_charge_thm_range1 ||
        param_l3 != FG_charge_thm_range2 ||
        param_l4 != FG_charge_thm_range3)
    {
        pr_info("SBSAA_FG_CC_THM_RANGE Set Failed.\r\n");
        paramdown_result += (1 << 1);
    }
    else
    {
        //pr_info("SBSAA_FG_CC_THM_RANGE Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 10 SBSAB_FG_CC_DFCC_RANGE
	sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBSAB_FG_CC_DFCC_RANGE] >> SBSD_CMD_Pos) & 0xFF, (FG_charge_maxdfcc<<16) | FG_charge_mindfcc, 4);
	usleep_range(5000, 10000);
	sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBSAB_FG_CC_DFCC_RANGE] >> SBSD_CMD_Pos) & 0xFF, &param, 4);
	sd77428_get_twobytesparams(param, &param_high, &param_low);
    //pr_info("SBSAB_FG_CC_DFCC_RANGE recall data: (%d, %d).\r\n", param_high, param_low);
    if (param_high != FG_charge_maxdfcc ||
        param_low != FG_charge_mindfcc )
    {
        pr_info("SBSAB_FG_CC_DFCC_RANGE Set Failed.\r\n");
        paramdown_result += (1 << 5);
    }
    else
    {
        //pr_info("SBSAB_FG_CC_DFCC_RANGE Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 11 SBSAC_FG_CC_CV_CURR
	param = (FG_charge_cv_eoctimes<<24) | (FG_charge_tail_ccratio<<16) | (FG_charge_thm_curr<<8) | FG_charge_fast_curr;
    sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBSAC_FG_CC_CV_CURR] >> SBSD_CMD_Pos) & 0xFF, param, 4);
    usleep_range(5000, 10000);
	sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBSAC_FG_CC_CV_CURR] >> SBSD_CMD_Pos) & 0xFF, &param, 4);
	sd77428_get_fourbytesparams(param, &param_l1, &param_l2, &param_l3, &param_l4);
    //pr_info("SBSAC_FG_CC_CV_CURR recall data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
    if (param_l1 != FG_charge_cv_eoctimes ||
        param_l2 != FG_charge_tail_ccratio ||
        param_l3 != FG_charge_thm_curr ||
        param_l4 != FG_charge_fast_curr)
    {
        pr_info("SBSAC_FG_CC_CV_CURR Set Failed.\r\n");
        paramdown_result += (1 << 1);
    }
    else
    {
        //pr_info("SBSAC_FG_CC_CV_CURR Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 12 SBSAD_FG_DC_DFCC_RANGE
	sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBSAD_FG_DC_DFCC_RANGE] >> SBSD_CMD_Pos) & 0xFF, (FG_dc_max_dfcc<<16) | FG_dc_min_dfcc, 4);
	usleep_range(5000, 10000);
	sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBSAD_FG_DC_DFCC_RANGE] >> SBSD_CMD_Pos) & 0xFF, &param, 4);
	sd77428_get_twobytesparams(param, &param_high, &param_low);
    //pr_info("SBSAD_FG_DC_DFCC_RANGE recall data: (%d, %d).\r\n", param_high, param_low);
    if (param_high != FG_dc_max_dfcc ||
        param_low != FG_dc_min_dfcc )
    {
        pr_info("SBSAD_FG_DC_DFCC_RANGE Set Failed.\r\n");
        paramdown_result += (1 << 6);
    }
    else
    {
        //pr_info("SBSAD_FG_DC_DFCC_RANGE Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 13 SBSAE_FG_DC_TAIL
	param = (FG_dc_tail_th<<24) | (FG_dc_soc_des1<<16) | (FG_dc_soc_des2<<8) | FG_dc_tail_cc;
    sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBSAE_FG_DC_TAIL] >> SBSD_CMD_Pos) & 0xFF, param, 4);
    usleep_range(5000, 10000);
	sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBSAE_FG_DC_TAIL] >> SBSD_CMD_Pos) & 0xFF, &param, 4);
	sd77428_get_fourbytesparams(param, &param_l1, &param_l2, &param_l3, &param_l4);
    //pr_info("SBSAE_FG_DC_TAIL recall data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
    if (param_l1 != FG_dc_tail_th ||
        param_l2 != FG_dc_soc_des1 ||
        param_l3 != FG_dc_soc_des2 ||
        param_l4 != FG_dc_tail_cc)
    {
        pr_info("SBSAE_FG_DC_TAIL Set Failed.\r\n");
        paramdown_result += (1 << 1);
    }
    else
    {
        //pr_info("SBSAE_FG_DC_TAIL Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 14 SBSAF_FG_CC_DFCC_SET
	sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBSAF_FG_CC_DFCC_SET] >> SBSD_CMD_Pos) & 0xFF, (FG_CC_DFCC_MODE<<16) | FG_CC_hardset_DFCC, 4);
	usleep_range(5000, 10000);
	sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBSAF_FG_CC_DFCC_SET] >> SBSD_CMD_Pos) & 0xFF, &param, 4);
	sd77428_get_twobytesparams(param, &param_high, &param_low);
    //pr_info("SBSAF_FG_CC_DFCC_SET recall data: (%d, %d).\r\n", param_high, param_low);
    if (param_high != FG_CC_DFCC_MODE ||
        param_low != FG_CC_hardset_DFCC )
    {
        pr_info("SBSAF_FG_CC_DFCC_SET Set Failed.\r\n");
        paramdown_result += (1 << 7);
    }
    else
    {
        //pr_info("SBSAF_FG_CC_DFCC_SET Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    return paramdown_result;
}

//download Current and Temperature table 
static uint16_t parameters_down_level2(struct sd77428_data *chip)
{
    uint16_t paramdown_result = 0;

    uint8_t single_table[33];
    uint8_t table_lens = 0;
  //  uint8_t i = 0;

    short curr_table[YAxis];
    short temp_table[ZAxis];

    //step1  SBSE2_TABLE_CURR
    table_lens = YAxis * 2;
    memcpy(single_table, (uint8_t*)YAxisElement, table_lens);
    sd77428_set_singletable(chip, (sbsd_cmd_def[SBSE2_TABLE_CURR] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);
    usleep_range(5000, 10000);
    sd77428_get_singletable(chip, (sbsd_cmd_def[SBSE2_TABLE_CURR] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);

    memcpy((uint8_t*)curr_table, single_table, table_lens);
    //pr_info("SBSE2_TABLE_CURR recall: (%d, %d, %d, %d). \r\n", curr_table[0], curr_table[1], curr_table[2], curr_table[3]);

    if (calculate_pec_bytes(0, single_table, table_lens)
        != calculate_pec_bytes(0, (uint8_t*)YAxisElement, table_lens))
    {
        pr_info("SBSE2_TABLE_CURR Set Failed.\r\n");
        paramdown_result += (1 << 0);
    }
    else
    {
        //pr_info("SBSE2_TABLE_CURR Set Success.\r\n");
    }
    usleep_range(5000, 10000);
    
    //step2  SBSE3_TABLE_TEMP
    table_lens = ZAxis * 2;
    memcpy(single_table, (uint8_t*)ZAxisElement, table_lens);
    sd77428_set_singletable(chip, (sbsd_cmd_def[SBSE3_TABLE_TEMP] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);
    usleep_range(5000, 10000);
    sd77428_get_singletable(chip, (sbsd_cmd_def[SBSE3_TABLE_TEMP] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);

    memcpy((uint8_t*)temp_table, single_table, table_lens);
    //pr_info("SBSE3_TABLE_TEMP recall: (%d, %d, %d, %d). \r\n", temp_table[0], temp_table[1], temp_table[2], temp_table[3]);

    if (calculate_pec_bytes(0, single_table, table_lens)
        != calculate_pec_bytes(0, (uint8_t*)ZAxisElement, table_lens))
    {
        pr_info("SBSE3_TABLE_TEMP Set Failed.\r\n");
        paramdown_result += (1 << 1);
    }
    else
    {
        //pr_info("SBSE3_TABLE_TEMP Set Success.\r\n");
    }
    usleep_range(5000, 10000);
    return paramdown_result;
}

//download OCV and voltage table 
static uint16_t parameters_down_level3(struct sd77428_data *chip)
{
    uint16_t paramdown_result = 0;

    uint8_t packtable[OCV_DATA_NUM*2+1];
    uint16_t packlens = 0;
  //  uint16_t i = 0;

    short ocv_table[OCV_DATA_NUM];
    short voltage_table[XAxis];

    //step1  SBSE0_TABLE_OCV 130
    packlens = OCV_DATA_NUM * 2;
    memcpy(packtable, (uint8_t*)ocv_volt_data, packlens);
    sd77428_set_packtable(chip, (sbsd_cmd_def[SBSE0_TABLE_OCV] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(5000, 10000);
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE0_TABLE_OCV] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);

    memcpy((uint8_t*)ocv_table, packtable, packlens);
    /*printk("SBSE0_TABLE_OCV recall: \r\n");
    for (i = 0; i < OCV_DATA_NUM; ++i)
    {
        printk(KERN_CONT "%d ", ocv_table[i]);
    }
    printk("\r\n");*/

    if (calculate_pec_bytes(0, packtable, packlens)
        != calculate_pec_bytes(0, (uint8_t*)ocv_volt_data, packlens))
    {
        pr_info("SBSE0_TABLE_OCV Set Failed.\r\n");
        paramdown_result += (1 << 0);
    }
    else
    {
        //pr_info("SBSE0_TABLE_OCV Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step2  SBSE1_TABLE_VOLT 72
    packlens = XAxis * 2;
    memcpy(packtable, (uint8_t*)XAxisElement, packlens);
    sd77428_set_packtable(chip, (sbsd_cmd_def[SBSE1_TABLE_VOLT] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(5000, 10000);
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE1_TABLE_VOLT] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    
    memcpy((uint8_t*)voltage_table, packtable, packlens);
    /*printk("SBSE1_TABLE_VOLT recall: \r\n");
    for (i = 0; i < XAxis; ++i)
    {
        printk(KERN_CONT "%d ", voltage_table[i]);
    }
    printk("\r\n");*/

    if (calculate_pec_bytes(0, packtable, packlens)
        != calculate_pec_bytes(0, (uint8_t*)XAxisElement, packlens))
    {
        pr_info("SBSE1_TABLE_VOLT Set Failed.\r\n");
        paramdown_result += (1 << 1);
    }
    else
    {
        //pr_info("SBSE1_TABLE_VOLT Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    return paramdown_result;
}

//download RC table 
static uint16_t parameters_down_level4(struct sd77428_data *chip)
{
    uint16_t paramdown_result = 0;
    uint8_t packtable[XAxis*YAxis*ZAxis + 1];
    uint16_t packlens = 0;
  //  uint16_t i = 0, j =0;

    //step1  SBSE4_TABLE_RC 576
    packlens = XAxis*YAxis*ZAxis;
    memcpy(packtable, (uint8_t*)RCtable, packlens);
    sd77428_set_packtable(chip, (sbsd_cmd_def[SBSE4_TABLE_RC] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(5000, 10000);
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE4_TABLE_RC] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    
    /*printk("SBSE4_TABLE_RC recall: \r\n");
    for (i = 0; i < YAxis*ZAxis; ++i)
    {
        for (j = 0; j < XAxis; ++j)
        {
            printk(KERN_CONT "%d ", packtable[i *XAxis +j ]);
        }
        printk("\r\n");
    }
    printk("\r\n");*/

    if (calculate_pec_bytes(0, packtable, packlens)
        != calculate_pec_bytes(0, (uint8_t*)RCtable, packlens))
    {
        pr_info("SBSE4_TABLE_RC Set Failed.\r\n");
        paramdown_result += (1 << 0);
    }
    else
    {
        //pr_info("SBSE4_TABLE_RC Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    return paramdown_result;
}

//download NTC-Temperature table 
static uint16_t parameters_down_level5(struct sd77428_data *chip)
{
    uint16_t paramdown_result = 0;

    uint8_t packtable[TEMPERATURE_DATA_NUM*4 + 1];
    uint16_t packlens = 0;
   // uint16_t i = 0;
    int32_t temp_table[TEMPERATURE_DATA_NUM];
    
    //step1  SBSE5_TABLE_NTC_TEMP 160
    packlens = TEMPERATURE_DATA_NUM * 4;
    memcpy(packtable, (uint8_t*)cell_temp_data, packlens);
    sd77428_set_packtable(chip, (sbsd_cmd_def[SBSE5_TABLE_NTC_TEMP] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(5000, 10000);
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE5_TABLE_NTC_TEMP] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    
    memcpy((uint8_t*)temp_table, packtable, packlens);
    /*printk("SBSE5_TABLE_NTC_TEMP recall: \r\n");
    for (i = 0; i < TEMPERATURE_DATA_NUM; ++i)
    {
        printk(KERN_CONT "%d ", temp_table[i]);
    }
    printk("\r\n");*/

    if (calculate_pec_bytes(0, packtable, packlens)
        != calculate_pec_bytes(0, (uint8_t*)cell_temp_data, packlens))
    {
        pr_info("SBSE5_TABLE_NTC_TEMP Set Failed.\r\n");
        paramdown_result += (1 << 0);
    }
    else
    {
        //pr_info("SBSE5_TABLE_NTC_TEMP Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    return paramdown_result;
}
//download DFCC Soc, curr, Temptable 
static uint16_t parameters_down_level6(struct sd77428_data *chip)
{
    uint16_t paramdown_result = 0;

    uint8_t single_table[33];
    uint8_t table_lens = 0;
    //uint8_t i = 0;

    short soc_table[DFCC_X];
    short curr_table[DFCC_Y];
    short temp_table[DFCC_Z];

    //step1  SBSE6_TABLE_DFCC_SOC
    table_lens = DFCC_X * 2;
    memcpy(single_table, (uint8_t*)DFCC_XDATA, table_lens);
    sd77428_set_singletable(chip, (sbsd_cmd_def[SBSE6_TABLE_DFCC_SOC] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);
    usleep_range(5000, 10000);
    sd77428_get_singletable(chip, (sbsd_cmd_def[SBSE6_TABLE_DFCC_SOC] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);

    memcpy((uint8_t*)soc_table, single_table, table_lens);
    //pr_info("SBSE6_TABLE_DFCC_SOC recall: (%d, %d, %d, %d, %d, %d). \r\n", soc_table[0], soc_table[1], soc_table[2], soc_table[3], soc_table[4],soc_table[5]);

    if (calculate_pec_bytes(0, single_table, table_lens)
        != calculate_pec_bytes(0, (uint8_t*)DFCC_XDATA, table_lens))
    {
        pr_info("SBSE6_TABLE_DFCC_SOC Set Failed.\r\n");
        paramdown_result += (1 << 0);
    }
    else
    {
        //pr_info("SBSE6_TABLE_DFCC_SOC Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step1  SBSE7_TABLE_DFCC_CURR
    table_lens = DFCC_Y * 2;
    memcpy(single_table, (uint8_t*)DFCC_YDATA, table_lens);
    sd77428_set_singletable(chip, (sbsd_cmd_def[SBSE7_TABLE_DFCC_CURR] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);
    usleep_range(5000, 10000);
    sd77428_get_singletable(chip, (sbsd_cmd_def[SBSE7_TABLE_DFCC_CURR] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);

    memcpy((uint8_t*)curr_table, single_table, table_lens);
    //pr_info("SBSE7_TABLE_DFCC_CURR recall: (%d, %d, %d). \r\n", curr_table[0], curr_table[1], curr_table[2]);

    if (calculate_pec_bytes(0, single_table, table_lens)
        != calculate_pec_bytes(0, (uint8_t*)DFCC_YDATA, table_lens))
    {
        pr_info("SBSE7_TABLE_DFCC_CURR Set Failed.\r\n");
        paramdown_result += (1 << 0);
    }
    else
    {
        //pr_info("SBSE7_TABLE_DFCC_CURR Set Success.\r\n");
    }
    usleep_range(5000, 10000);
    
    //step3  SBSE8_TABLE_DFCC_TEMP
    table_lens = DFCC_Z * 2;
    memcpy(single_table, (uint8_t*)DFCC_ZDATA, table_lens);
    sd77428_set_singletable(chip, (sbsd_cmd_def[SBSE8_TABLE_DFCC_TEMP] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);
    usleep_range(5000, 10000);
    sd77428_get_singletable(chip, (sbsd_cmd_def[SBSE8_TABLE_DFCC_TEMP] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);

    memcpy((uint8_t*)temp_table, single_table, table_lens);
    //pr_info("SBSE8_TABLE_DFCC_TEMP recall: (%d, %d, %d, %d). \r\n", temp_table[0], temp_table[1], temp_table[2], temp_table[3]);

    if (calculate_pec_bytes(0, single_table, table_lens)
        != calculate_pec_bytes(0, (uint8_t*)DFCC_ZDATA, table_lens))
    {
        pr_info("SBSE8_TABLE_DFCC_TEMP Set Failed.\r\n");
        paramdown_result += (1 << 1);
    }
    else
    {
        //pr_info("SBSE8_TABLE_DFCC_TEMP Set Success.\r\n");
    }
    usleep_range(5000, 10000);
    return paramdown_result;
}

//download DFCC table 
static uint16_t parameters_down_level7(struct sd77428_data *chip)
{
    uint16_t paramdown_result = 0;
    uint8_t packtable[DFCC_X*DFCC_Y*DFCC_Z + 1];
    uint16_t packlens = 0;
    //uint16_t i = 0, j =0;

    //step1  SBSE9_TABLE_DFCC_RATIO 72
    packlens = DFCC_X*DFCC_Y*DFCC_Z;
    memcpy(packtable, (uint8_t*)DFCC_table, packlens);
    sd77428_set_packtable(chip, (sbsd_cmd_def[SBSE9_TABLE_DFCC_RATIO] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(5000, 10000);
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE9_TABLE_DFCC_RATIO] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    
    /*printk("SBSE9_TABLE_DFCC_RATIO recall: \r\n");
    for (i = 0; i < DFCC_Y*DFCC_Z; ++i)
    {
        for (j = 0; j < DFCC_X; ++j)
        {
            printk(KERN_CONT "%d ", packtable[i *DFCC_X +j ]);
        }
        printk("\r\n");
    }
    printk("\r\n");*/

    if (calculate_pec_bytes(0, packtable, packlens)
        != calculate_pec_bytes(0, (uint8_t*)DFCC_table, packlens))
    {
        pr_info("SBSE9_TABLE_DFCC_RATIO Set Failed.\r\n");
        paramdown_result += (1 << 0);
    }
    else
    {
        //pr_info("SBSE9_TABLE_DFCC_RATIO Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    return paramdown_result;
}

static uint16_t parameters_down_level8(struct sd77428_data *chip)
{
    uint16_t paramdown_result = 0;
    //uint8_t param_l1 = 0, param_l2 = 0, param_l3=0, param_l4=0;	param_high = 0,
    int16_t  param_low = 0;
	uint8_t i = 0;
	uint32_t param = 0;

	for(i=0; i<PARM_BCFG_MAX-1; i++)
	{
		if(PARM_BCFG_HISTORICALCC == i)
		{
			sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBS66_DSNCAP+i] >> SBSD_CMD_Pos) & 0xFF, param_board_cfg[i], 4);
			usleep_range(5000, 10000);
			sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBS66_DSNCAP+i] >> SBSD_CMD_Pos) & 0xFF, &param, 4);
		    //pr_info("SBS0x84 hiscc recall data: (%d).\r\n", param);
		    if (param != param_board_cfg[i])
		    {
		        pr_info("SBS0x84 hiscc Set Failed.\r\n");
		        paramdown_result += (1 << 9);
		    }
		    else
		    {
		        //pr_info("SBS0x84 hiscc Set Success.\r\n");
		    }
		    msleep(1);
		}
		else
		{
			sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBS66_DSNCAP+i] >> SBSD_CMD_Pos) & 0xFF, param_board_cfg[i], 2);
			usleep_range(5000, 10000);
			sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBS66_DSNCAP+i] >> SBSD_CMD_Pos) & 0xFF, &param, 2);
			param_low = (int16_t)param;
		    //pr_info("SBS SBS66_DSNCAP+%d recall data: (%d).\r\n", i, param_low);
		    if (param_low != param_board_cfg[i])
		    {
		        pr_info("SBS SBS66_DSNCAP+%d Set Failed.\r\n", i);
		        paramdown_result += (1 << 9);
		    }
		    else
		    {
		        //pr_info("SBS SBS66_DSNCAP+%d Set Success.\r\n", i);
		    }
		    msleep(1);
		}
	}

	//cadc offset 0x8d
	sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBS8D_CADCZEROOFFSET] >> SBSD_CMD_Pos) & 0xFF, BATT_DESIGN_CADC_OFFSET, 2);
	usleep_range(5000, 10000);
	sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBS8D_CADCZEROOFFSET] >> SBSD_CMD_Pos) & 0xFF, &param, 2);
	param_low = (int16_t)param;
    //pr_info("SBS0x8d cadc offset recall data: (%d).\r\n", param_low);
    if (param_low  != BATT_DESIGN_CADC_OFFSET)
    {
        pr_info("SBS0x8d cadc offset Set Failed.\r\n");
        paramdown_result += (1 << 9);
    }
    else
    {
        //pr_info("SBS0x8d cadc offset Set Success.\r\n");
    }
    msleep(1);

	//sbs fin 0x8f
	sd77428_set_sbs_params(chip, (sbsd_cmd_def[SBS8F_SBSSENDFINISHED] >> SBSD_CMD_Pos) & 0xFF, 0, 2);
	usleep_range(5000, 10000);
	sd77428_get_sbs_params(chip, (sbsd_cmd_def[SBS8F_SBSSENDFINISHED] >> SBSD_CMD_Pos) & 0xFF, &param, 2);
	param_low = (int16_t)param;
    //pr_info("SBS0x8f sbs fin recall data: (%d).\r\n", param_low);
    if (param_low != 0)
    {
        pr_info("SBS0x8f sbs fin Set Failed.\r\n");
        paramdown_result += (1 << 9);
    }
    else
    {
        //pr_info("SBS0x8f sbs fin Set Success.\r\n");
    }
    msleep(1);
	
    return paramdown_result;
}

static void sd77428_params_down_work(struct work_struct *work)
{
    static uint32_t download_count = 0;
    uint16_t paramdown_result = 0;
    uint32_t ret = 0;
    struct sd77428_data *chip = container_of(work, struct sd77428_data, download_work.work);
    
    pr_info("Start parameters down work, %d times\n",download_count++);

    if(download_count >= 10)
    {
        pr_info("Parameters Download Failed. \r\n");
        goto out;
    }

    if(false == sd77428_is_ok)
        goto out;
  
    paramdown_result = parameters_down_level1(chip);
    ret += paramdown_result;
    if (paramdown_result)        pr_info("-----------------1 parameters_down_level1 failed, code %02x. -----------------\r\n", paramdown_result);
  
    paramdown_result = parameters_down_level2(chip);
    ret += paramdown_result;
    if (paramdown_result)        pr_info("-----------------2 parameters_down_level2 failed, code %02x. -----------------\r\n", paramdown_result);

    paramdown_result = parameters_down_level3(chip);
    ret += paramdown_result;
    if (paramdown_result)        pr_info("-----------------3 parameters_down_level3 failed, code %02x. -----------------\r\n", paramdown_result);

    paramdown_result = parameters_down_level4(chip);
    ret += paramdown_result;
    if (paramdown_result)        pr_info("-----------------4 parameters_down_level4 failed, code %02x. -----------------\r\n", paramdown_result);

    paramdown_result = parameters_down_level5(chip);
    ret += paramdown_result;
    if (paramdown_result)        pr_info("-----------------5 parameters_down_level5 failed, code %02x. -----------------\r\n", paramdown_result);

    paramdown_result = parameters_down_level6(chip);
    ret += paramdown_result;
    if (paramdown_result)        pr_info("-----------------6 parameters_down_level6 failed, code %02x. -----------------\r\n", paramdown_result);

    paramdown_result = parameters_down_level7(chip);
    ret += paramdown_result;
    if (paramdown_result)        pr_info("-----------------7 parameters_down_level7 failed, code %02x. -----------------\r\n", paramdown_result);

	paramdown_result = parameters_down_level8(chip);
	ret += paramdown_result;
    if (paramdown_result)        pr_info("-----------------8 parameters_down_level8 failed, code %02x. -----------------\r\n", paramdown_result);

    if (!ret)
    {
        pr_info("++++++++++++++++++++Parameters Download Sucess.++++++++++++++++++++\r\n");
        goto out;
    }
    else
    {
        schedule_delayed_work(&chip->download_work, (2*HZ));
    }
    return;

out:
    schedule_delayed_work(&chip->work, (2*HZ));
}

static void sd77428_battery_work(struct work_struct *work)
{
	struct sd77428_data *chip = container_of(work, struct sd77428_data, work.work);

	if(false == sd77428_is_ok)
		goto out;
	
	sd77428_update_charger_info(chip);
	sd77428_get_batt_info(chip);
	
out:
	schedule_delayed_work(&chip->work, INIT_DELAY);
}

static int32_t sd77428_suspend_notifier(struct notifier_block *nb,
				unsigned long event,
				void *dummy)
{
	struct sd77428_data *data = container_of(nb, struct sd77428_data, pm_nb);

	switch (event) {

	case PM_SUSPEND_PREPARE:
		pr_info("BMT PM_SUSPEND_PREPARE \n");
		cancel_delayed_work_sync(&data->work);
		//system_charge_discharge_status(data);
		//oz8806_suspend = 1;
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		pr_info("BMT PM_POST_SUSPEND \n");
		//system_charge_discharge_status(data);
		/*mutex_lock(&update_mutex);
		// if AC charge can't wake up every 1 min,you must remove the if.
		if(adapter_status == O2_CHARGER_BATTERY)
		{
			bmu_wake_up_chip();
		}

		oz8806_update_batt_info(data);
		mutex_unlock(&update_mutex);*/
	
		schedule_delayed_work(&data->work, 0);
		//this code must be here,very carefull.
		/*if(adapter_status == O2_CHARGER_BATTERY)
		{
			if(data->batt_info.batt_current >= data->batt_info.discharge_current_th)
			{
				if (batt_info_ptr) batt_info_ptr->fCurr = -20;

				data->batt_info.batt_current = -20;
				pr_info("drop current\n");
			}
		}
		oz8806_suspend = 0;*/
		return NOTIFY_OK;

	default:
		return NOTIFY_DONE;
	}
}

static int32_t sd77428_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	int32_t ret = 0;
	struct sd77428_data *chip = NULL;
	
	dev_err(&client->dev, "start,i2c addr 0x%02X,name: %s\n",client->addr,client->name);
	
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
	{
		dev_err(&client->dev, "Can't alloc BMT_data struct\n");
		return -ENOMEM;
	} 
	
	i2c_set_clientdata(client, chip);
	mutex_init(&chip->i2c_rw_lock);
	chip->client = client; 
	chip->dev    = &client->dev;
	//detect ic first
	ret = sd77428_detect_ic(chip);
    if(ret < 0)
	{
	 	pr_err("do not detect ic, exit\n");
	 	return -ENODEV;
	}
	

	 if (of_property_read_string(chip->client->dev.of_node, "drv-names", &chip->name)) {
  		dev_err(&client->dev, "name failed \n");
  	}
	else
	{
		dev_err(&client->dev, "client->name: %s,chip->name:%s\n",client->name,chip->name);
	}
	
	//init the hardware adc register config
	sd77428_init_batt_info(chip);

	ret = device_init_wakeup(&client->dev, true);
	if (ret) 
		pr_err("wakeup sd77428 source init failed.\n");

#if parameters_down_enable    
    INIT_DELAYED_WORK(&chip->download_work, sd77428_params_down_work);
    schedule_delayed_work(&chip->download_work, (2*HZ));             
#endif

	INIT_DELAYED_WORK(&chip->work, sd77428_battery_work);
    schedule_delayed_work(&chip->work, (5*HZ));                      

	sd77428_power_supply_init(chip);

	ret = sysfs_create_group(&(chip->dev->kobj), &sd77428_attribute_group);
	if (ret < 0) 
		pr_err("create sysfs failed %d\n",ret);
	
	//alternative suspend/resume method
	chip->pm_nb.notifier_call = sd77428_suspend_notifier;
    register_pm_notifier(&chip->pm_nb);
	g_chip_data = chip;
	
	dev_err(&client->dev, "probe end\n");
	//pr_info("probe end\n");
	return 0;
}

static int32_t sd77428_suspend(struct device *dev)
{
	struct sd77428_data *chip  = i2c_get_clientdata(to_i2c_client(dev));
	cancel_delayed_work(&chip->work);
	return 0;
}

static int32_t sd77428_resume(struct device *dev)
{
	struct sd77428_data *chip = i2c_get_clientdata(to_i2c_client(dev));
	if(true == sd77428_is_ok)
		schedule_delayed_work(&chip->work,INIT_DELAY);
	return 0;
}

static int32_t sd77428_remove(struct i2c_client *client)
{
	struct sd77428_data *chip = i2c_get_clientdata(client);
	
	sysfs_remove_group(&(chip->client->dev.kobj), &sd77428_attribute_group);
	power_supply_unregister(chip->bat);
	cancel_delayed_work(&chip->work);
	pr_info("sd77428 is remove\n");

	return 0;
}

static void sd77428_shutdown(struct i2c_client *client)
{
	struct sd77428_data *chip = i2c_get_clientdata(client);
	
	sysfs_remove_group(&(chip->client->dev.kobj), &sd77428_attribute_group);
	power_supply_unregister(chip->bat);
	cancel_delayed_work(&chip->work);
	pr_info("sd77428 is shutdown\n");
}

static const struct dev_pm_ops pm_ops = 
{
	.suspend	= sd77428_suspend,
	.resume	= sd77428_resume,
};


//static struct name_data_t sd77428_devtype = {
//	.name = "sd77428",
//};

//static struct name_data_t sd77428_1_devtype = {
//	.name = "sd77428_1",
//};

static const struct of_device_id sd77428_of_match[] = 
{
	//{ .compatible = "bmt,sd77428", .data = &sd77428_devtype },
	//{ .compatible = "bmt,sd77428_1", .data = &sd77428_1_devtype },
	{.compatible = "bmt,sd77428"},
	//{.compatible = "bmt,sd77428_1"},
	{ }
};
MODULE_DEVICE_TABLE(of, sd77428_of_match);

static const struct i2c_device_id sd77428_id[] = 
{
	//{ "sd77428",	(kernel_ulong_t)&sd77428_devtype, },
	//{ "sd77428_1",	(kernel_ulong_t)&sd77428_1_devtype, },
	{"sd77428", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sd77428_id);

static struct i2c_driver sd77428_driver = 
{ 
	.driver = {
		.name	= "sd77428",
		.pm   = &pm_ops,
		.of_match_table = sd77428_of_match,
	},
	.probe		= sd77428_probe,
	.remove		= sd77428_remove,
	.shutdown	= sd77428_shutdown,
	.id_table	= sd77428_id, 
};

module_i2c_driver(sd77428_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("sd77428 Battery Monitor IC Driver");
//---------------------------------------------------------------------------------------------------------------------//
