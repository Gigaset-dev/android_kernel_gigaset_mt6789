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
#include "sd77428.h"
#include "parallel_driver.h"

#ifndef pr_fmt
#define pr_fmt(fmt) "[bmt]%s: " fmt, __func__
#endif

static int32_t debug_print = 1;
#ifndef batt_dbg
#define batt_dbg(args...) \
do {\
    if(debug_print)\
        pr_err(args);\
} while(0)
#endif

// echo 8 4 1 7 > /proc/sys/kernel/printk           //内核终端打印指令


static bool sd77428_is_ok = false;


static struct sd77428_data *g_chip_data = NULL;

//-----------------------------------------------parameters set-----------------------------------------------------//
const static  uint16_t  parameters_down_enable  =   0;                           //是否开启参数配置下载
const static  uint16_t  BATT_DESIGN_CAPACITY    =   2800;
const static  uint16_t  BATT_DESIGN_FCC         =   2716;
const static  uint16_t  BATT_DESIGN_CV          =   4400;
const static  uint16_t  BATT_DESIGN_EOC         =   180;
const static  uint16_t  BATT_DESIGN_EOD         =   3400;
const static  uint16_t  BATT_DESIGN_RSENSE      =   1000;
const static  uint16_t  BATT_DESIGN_CADC_OFFSET =   0x0;
// const static  uint16_t  SOFT_RESET              =   0x2301;
// const static  uint16_t  BATT_DESIGN_NTC_SRCTAB  =   0x0102;

static uint16_t gdm_thresh_chg = 1,  gdm_thresh_dsg = 1;    // SBSA0_CONFIGDSG_CHG
//static uint8_t calib_sample = 32;                            // SBSA1_CALIBRATION_TIME

static uint8_t effect_mid_windows = 21;                     // SBSA3_FILTER_WIND_LOC
static uint8_t effect_mean_windows = 6;
static uint8_t effect_first_lock_th = 1;
static uint8_t effect_noise_range_gate = 5;

static uint16_t effect_init_delay_ratio = 900;  //0~1000;    //SBSA4_FILTER_RATIO
static uint16_t effect_noise_ratio = 20;        //0~1000;

static uint8_t SECOND_LOCK_FLAG = 1;                        //SBSA5_FILTER_ENABLE
static uint8_t filter_switcher = 1;
static uint8_t calib_switcher = 1;
static uint8_t filter_offset = 1;

static uint8_t SECOND_LOCK_TH = 3;                         //SBSA6_FILTER_SECOND_LOCK
static uint8_t SECOND_LOCK_TIMER = 240;
static uint8_t SECOND_UNLOCK_TIMER = 240;

//static uint8_t _IDLE_CHARGE_EST_ = 1;                      //SBSA7_FG_IDLE_SOH
//static uint8_t FG_idle_60s_enable = 1;                        
//static uint8_t FG_cc_soh_ratio = 25;                        
//static uint8_t FG_dc_soh_ratio = 15;  

static uint16_t FG_idle_delay_curr = 300;                  //SBSA8_FG_IDLE_SYNCCURR
static uint16_t FG_idle_wave_curr = 20;        

static uint8_t FG_charge_thm_comp1 = 10;                      //SBSA9_FG_CC_THM_COMP
static uint8_t FG_charge_thm_comp2 = 7;                        
static uint8_t FG_charge_thm_comp3 = 25;                        
static uint8_t FG_charge_thm_comp4 = 30;  

static uint8_t FG_charge_thm_comp5 = 50;                      //SBSAA_FG_CC_THM_RANGE
static uint8_t FG_charge_thm_range1 = 0;                        
static uint8_t FG_charge_thm_range2 = 20;                        
static uint8_t FG_charge_thm_range3 = 40; 

static uint16_t FG_charge_maxdfcc = 12000;                     //SBSAB_FG_CC_DFCC_RANGE
static uint16_t FG_charge_mindfcc = 900;    

static uint8_t FG_charge_cv_eoctimes = 25;                      //SBSAC_FG_CC_CV_CURR
static uint8_t FG_charge_tail_ccratio = 20;                        
static uint8_t FG_charge_thm_curr = 5;                        
static uint8_t FG_charge_fast_curr = 5; 

static uint16_t FG_dc_max_dfcc = 12000;                     //SBSAD_FG_CC_DFCC_RANGE
static uint16_t FG_dc_min_dfcc = 900;    

static uint8_t FG_dc_tail_th = 5;                      //SBSAE_FG_DC_TAIL
static uint8_t FG_dc_soc_des1 = 10;                        
static uint8_t FG_dc_soc_des2 = 6;                        
static uint8_t FG_dc_tail_cc = 10; 

//static uint16_t FG_CC_DFCC_MODE = 0;                     //SBSAF_FG_CC_DFCC_SET
//static uint16_t FG_CC_hardset_DFCC = 0;  


//static uint16_t ntc_reg_enable = 0;                 //SBSB0_NTC_HOLD_TIME
//static uint16_t ntc_reg_holdtime = 0;

//static uint16_t NTC_calculate_mode = 0;                 //SBSB1_NTC_PULL_MODE
//static uint16_t NTC_pullup_sense = 0;

//-----------------dfcc----------------//
static short ocv_volt_data[OCV_DATA_NUM] = { 3103, 3372, 3523, 3617, 3675, 3690, 3694, 3696, 3697, 3706, 3717, 3727, 3736, 3744, 3750, 3757, 3765, 3772, 3779, 
                                             3785, 3792, 3798, 3804, 3809, 3815, 3822, 3829, 3837, 3845, 3852, 3860, 3869, 3878, 3888, 3899, 3912, 3926, 3943, 
                                             3963, 3982, 4000, 4016, 4030, 4046, 4062, 4078, 4094, 4110, 4127, 4145, 4162, 4179, 4198, 4216, 4234, 4252, 4270, 
                                             4288, 4306, 4323, 4339, 4355, 4369, 4384, 4400 };

//RC table X Axis value, in mV format
static short    XAxisElement[XAxis] = { 3100, 3160, 3220, 3280, 3340, 3400, 3460, 3520, 3560, 3600, 3638, 3662, 3704, 3730, 3769, 3780, 3826, 3855, 3890, 3928, 
                                        3956, 3996, 4021, 4047, 4073, 4110, 4158, 4187, 4216, 4246, 4275, 4302, 4328, 4367, 4402, 4444};

//RC table Y Axis value, in mA format
static short    YAxisElement[YAxis] = {200, 1200, 2000, 2500};
// RC table Z Axis value, in 10*'C format
static short    ZAxisElement[ZAxis] = {-100, 0, 200, 400};
// contents of RC table, its unit is 10000C, 1C = DesignCapacity
static uint8_t  RCtable[YAxis*ZAxis][XAxis] = {
//temp = -10 ^C,
{5,5,6,8,10,13,17,21,25,30,36,40,48,52,57,58,63,66,70,73,76,80,82,85,87,91,95,98,99,100,100,100,100,100,100,100},
{19,23,28,33,41,49,56,63,68,72,75,77,81,84,87,88,92,94,97,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100},
{22,26,33,40,48,56,63,69,73,77,80,82,86,88,92,93,99,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100},
{23,27,34,42,50,58,64,70,74,78,81,83,88,99,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100},
//temp = 0 ^C,
{4,4,4,5,5,6,7,9,12,16,19,22,29,36,44,46,53,56,60,64,66,69,72,74,76,80,84,87,89,92,95,97,99,100,100,100},
{11,12,15,18,23,29,37,47,53,58,62,65,69,71,74,75,79,81,84,87,89,93,95,97,98,100,100,100,100,100,100,100,100,100,100,100},
{21,25,31,36,44,52,60,66,70,73,76,78,81,83,86,87,91,94,97,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100},
{23,29,37,45,55,62,68,74,77,80,83,85,89,93,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100},
//temp = 20 ^C,
{1,1,1,1,2,2,3,3,4,5,6,7,16,20,29,32,42,47,52,56,59,62,65,67,69,73,77,79,82,84,87,89,92,95,99,100},
{2,2,2,3,3,4,5,7,11,15,20,23,34,39,46,48,54,57,61,64,67,70,72,75,77,80,84,86,89,92,95,97,99,100,100,100},
{2,3,3,4,5,6,9,14,18,23,30,36,44,48,53,54,60,62,66,69,71,75,77,79,81,84,89,91,94,97,99,100,100,100,100,100},
{3,3,4,5,6,8,12,17,22,29,37,41,48,52,57,58,63,65,68,72,74,77,79,82,84,87,92,94,97,99,100,100,100,100,100,100},
//temp = 40 ^C,
{0,1,1,1,1,2,2,3,4,5,6,7,16,20,28,29,40,46,51,55,57,61,64,67,69,73,77,79,82,84,87,89,91,95,98,100},
{0,0,0,1,1,2,3,3,5,6,11,15,23,28,37,39,47,51,55,59,62,65,68,70,72,76,80,82,85,87,90,93,95,98,100,100},
{1,1,1,2,2,3,4,6,8,11,18,22,30,36,43,45,51,55,59,62,65,68,71,73,75,78,82,85,88,90,93,96,98,100,100,100},
{1,1,1,2,3,4,5,7,9,14,21,25,34,39,46,47,54,57,60,64,67,70,72,75,77,80,84,87,89,92,96,98,99,100,100,100}
};

//static int cell_temp_data[TEMPERATURE_DATA_NUM] = {1264, 1451, 1672, 1928, 2231, 2591, 3021, 3536, 4156, 4907, 5820, 6936, 8307, 10000, 
//                                            12100, 14740,18050,22250,27620,34520,43470,55170,70610,90990,118300};

//SOC区间
static short DFCC_XDATA[DFCC_X] = { 500, 1000, 3000, 5000, 7000, 9000 };

//电流区间
static short DFCC_YDATA[DFCC_Y] = { 500, 1000, 2000 };

//温度区间
static short DFCC_ZDATA[DFCC_Z] = { -100, 0, 250, 400  };

static unsigned char DFCC_table[DFCC_Y * DFCC_Z][DFCC_X] = {
    //temp = -10 ^C
    {85,85,85,85,85,85}, // 600mA           最佳
    {85,88,90,76,76,85}, //1200mA               最佳 2
    {40,60,75,75,75,85}, //2000mA           最佳

    //temp = 0 ^C
    {85,85,90,85,90,90}, // 600mA           最佳
    {85,85,85,85,85,85}, //1200mA               最佳 2
    {65,75,75,75,75,85}, //2000mA           最佳

    //temp = 25 ^C
    {128,125,100,100,100,100}, //1200mA               最佳 2
    {100,125,115,100,100,100}, //1200mA               最佳 2
    {90,95,95,85,95,95}, //2000mA           最佳

     //temp = 40 ^C
    {110,110,100,100,100,100}, //1200mA               最佳 2
    {105,95,80,85,95,95}, //1200mA               最佳 2
    {120,110,95,90,95,95}, //2000mA           最佳
};

static signed int param_board_cfg[(PARM_BCFG_MAX)] =
{
    //Board Configuration
    4900,                   //0x0D48, PARM_BCFG_DESIGNCAPACITY, 3400mAhr
    4400,                   //0x0E74, PARM_BCFG_LMCHG_VOLTAGE, 4200mV
    200,                    //Fully charge end current 200mA
    30,                     //Fully charge end timeout  30 sec
    3400,                   //Discharge end voltage 3400
    0,                      //256 这里去掉其他地方也需要同步调整
    10,                     // Charge threshold    这里都是模拟量，lsb是4.39453125uV， 需要根据电阻来转化，比如10表示10mA，如果是1毫欧电阻，则表示4.39453125 * 2 /1 mA   ( mV/欧姆 = mA)
    10,                     // DisCharge threshold 模拟量
    7500,                   // fast wkup threshold  模拟量 放大了10 倍
    10,                     // CADC DB threshold  模拟量
    300,                     // CADC wakeup threshold  模拟量 放大了10 倍
    1000,  //0x09C4, PARM_BCFG_RSENSE, 2500 => 2.5mOhm
    0,                     // Debug mode 0-diasble  1-enbale
    5,                     //0x0005, PARM_BCFG_IDLETOSLEEP, 5 seconds
    60,                    //0x003C, PARM_BCFG_SLEEPTIME, 60 seconds
    2,                    //0x003C, PARM_BCFG_DEEPSLPTIME, 10 minutes
    270,                   //0x00C8, PARM_BCFG_PWRSLEEP, 270uA
    100,                   //0x0064, PARM_BCFG_PWRDEEPSLP, 100uA
    0x2710,                //0x2710, PARM_BCFG_CHGSLOPE
    0x0000,                //0x0000, PARM_BCFG_CHGOFFSET
    0x2710,                //0x2710, PARM_BCFG_DSGSLOPE
    0x0000,                  //0x0000, PARM_BCFG_DSGOFFSET  
    0,                     //Voltage offset
    0,                     //External temperature offset
    10,                    //SOC1 SET THRESHOLD
    15,                    //SOC1 CLEAR THRESHOLD
    2,                     //SOCF SET THRESHOLD 
    5,                     //SOCF CLEAR THRESHOLD
    10,                     //SOC1 DELTA
    1 ,                    //OpConfig BATLOWEN=1
    0,                     // HIstorical CC
    0,                     // RCTable Indx
    0,                     // Charge IR
    -150,                  // LowTemp
    0,                     // Init Soc
    10,                    //sysim
    80,                     // EOC_CDV
    0,                    // EOD_DDV
    0,//BATT_DESIGN_NTC_SRCTAB,// Temp table indx
    0,                     // cell num
};
//-------------------------------------------------------------------------------------------------------------------//

//-----------------------------------------------I2C read/write-----------------------------------------------------//
#if 1
static int32_t sd77428_i2c_read_bytes(struct sd77428_data* chip, uint8_t cmd, uint8_t *val, uint8_t bytes)
{
    int32_t ret = 0;
    uint8_t i   = 0;    
    uint8_t sendbuf[48] = {0};
    uint8_t i2caddr_backup = 0;
    struct i2c_msg msgs[2];
    usleep_range(1000,2000);

    mutex_lock(&chip->i2c_rw_lock);
    i2caddr_backup = chip->client->addr;
    chip->client->addr = SD77428_I2C_ADDR;

    sendbuf[0]      = cmd;

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

    chip->client->addr = i2caddr_backup;
    mutex_unlock(&chip->i2c_rw_lock);

    return (ARRAY_SIZE(msgs) == ret) ? 0 : -1;
}
static int32_t sd77428_i2c_write_bytes(struct sd77428_data* chip,uint8_t cmd,uint8_t *val, uint8_t bytes)
{
    int32_t ret = 0;
    uint8_t i = 0;
    uint8_t sendbuf[48] = {0};
    uint8_t i2caddr_backup = 0;
    struct i2c_msg msgs[1];
    usleep_range(1000,2000);
    mutex_lock(&chip->i2c_rw_lock);
    i2caddr_backup = chip->client->addr;
    chip->client->addr = SD77428_I2C_ADDR;

    sendbuf[0]      = cmd;
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
    chip->client->addr = i2caddr_backup;
    mutex_unlock(&chip->i2c_rw_lock);

    return (ARRAY_SIZE(msgs) == ret) ? 0 : -1;
}

#else
//会受到单次读写最大32字节的约束
static int32_t sd77428_i2c_read_bytes(struct sd77428_data* chip, uint8_t cmd, uint8_t *val, uint8_t bytes)
{
    int32_t ret = 0;
    uint8_t i   = 0;
    uint8_t i2caddr_backup = 0;
    usleep_range(1000,2000);

    mutex_lock(&chip->i2c_rw_lock);
    i2caddr_backup = chip->client->addr;
    chip->client->addr = SD77428_I2C_ADDR;
    for(i=0; i<4; i++)
    {
        ret = i2c_smbus_read_i2c_block_data(chip->client, cmd, bytes, val);
        if(ret >= 0)
            break;
        else
        {
            pr_err("cmd[0x%02X], err %d, %d times\n", cmd, ret, i);
            usleep_range(10000,20000);//delay 10ms,retry
        }
    }
    chip->client->addr = i2caddr_backup;
    mutex_unlock(&chip->i2c_rw_lock);

    return (i >= 4) ? -1 : 0;
}
static int32_t sd77428_i2c_write_bytes(struct sd77428_data* chip,uint8_t cmd,uint8_t *val, uint8_t bytes)
{
    int32_t ret = 0;
    uint8_t i   = 0;
    uint8_t i2caddr_backup = 0;
    usleep_range(1000,2000);

    mutex_lock(&chip->i2c_rw_lock);
    i2caddr_backup = chip->client->addr;
    chip->client->addr = SD77428_I2C_ADDR;
    for(i=0; i<4; i++)
    {
        ret = i2c_smbus_write_i2c_block_data(chip->client, cmd, bytes, val);
        if(ret >= 0) 
            break;
        else
        {
            pr_err("cmd[0x%02X], err %d, %d times\n", cmd, ret, i);
            usleep_range(10000,20000);//delay 10ms,retry
        }
    }
    chip->client->addr = i2caddr_backup;
    mutex_unlock(&chip->i2c_rw_lock);
    
    return (i >= 4) ? -1 : 0;
}
#endif

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
            pec = calculate_pec_byte(SD77428_I2C_ADDR << 1,pec);
            pec = calculate_pec_byte(cmd,pec);
            pec = calculate_pec_byte(SD77428_I2C_ADDR << 1 | 1,pec);
            
            if(rxbuf[2] != calculate_pec_bytes(pec,rxbuf,2))
            {
                dev_err(chip->dev,"PEC verification fail cmd %02x,retry %d\n",cmd,i);
                usleep_range(10000,20000);
                pec = 0;
                continue;
            }
    #endif

            *pdst = get_unaligned_le16(rxbuf);
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
    
    pec = calculate_pec_byte(SD77428_I2C_ADDR << 1,0);
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
            pec = calculate_pec_byte(SD77428_I2C_ADDR << 1,pec);
            pec = calculate_pec_byte(cmd,pec);
            pec = calculate_pec_byte(SD77428_I2C_ADDR << 1 | 1,pec);
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
    // uint8_t write_len = (len >> 2) << 2;
    uint8_t write_len = len;

    for(i=0; i<write_len; i++)
        w_buf[i] = data_buf[i];

    pec = calculate_pec_byte(SD77428_I2C_ADDR << 1,0);
    pec = calculate_pec_byte(cmd,pec);
    pec = calculate_pec_bytes(pec,w_buf,write_len);
    w_buf[write_len] = pec;

    return sd77428_i2c_write_bytes(chip,cmd, w_buf, write_len+1);  
}
#if 0
static int32_t sd77428_read_block(struct sd77428_data *chip, uint8_t *data_buf,uint32_t len)
{
    int32_t  ret = -1;
    uint8_t  r_buf[132] = {0};
    uint8_t  i = 0;
    uint8_t  pec = 0;
    uint8_t     index = SD77428_BLOCK_OP;
    // uint32_t read_len = (len >> 2) << 2;
    uint32_t read_len = len;

    ret = sd77428_i2c_read_bytes(chip,index,r_buf,read_len+2);
    if (ret < 0) 
        return ret;

    pec = calculate_pec_byte(SD77428_I2C_ADDR << 1,pec);
    pec = calculate_pec_byte(index,pec);
    pec = calculate_pec_byte(SD77428_I2C_ADDR << 1 | 1,pec);

    if(r_buf[read_len+1] == calculate_pec_bytes(pec,r_buf,read_len+1))
    {
        for(i=0;i<read_len;i++)
            data_buf[i] = r_buf[i+1];
    }
    else
    {
        pr_err("PEC Error 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x.\n", r_buf[0], r_buf[1], r_buf[2], r_buf[3], r_buf[4], r_buf[5]);
        return -1;
    }

    return ret;
}
static int32_t sd77428_write_block(struct sd77428_data *chip,uint8_t *data_buf,uint32_t len)
{
    uint8_t w_buf[132] = {0};
    uint8_t pec = 0;
    uint32_t i = 0;
    uint32_t write_len = (len >> 2) << 2;

    w_buf[0] = write_len;

    for(i=0;i<w_buf[0];i++)
        w_buf[i+1] = data_buf[i];

    pec = calculate_pec_byte(SD77428_I2C_ADDR << 1,0);
    pec = calculate_pec_byte(SD77428_BLOCK_OP,pec);
    pec = calculate_pec_bytes(pec,w_buf,write_len+1);
    w_buf[write_len+1] = pec;

    return sd77428_i2c_write_bytes(chip,SD77428_BLOCK_OP, w_buf, write_len+2);  
}
#endif
#if 0
static int32_t sd77428_read_addr(struct sd77428_data *chip, uint32_t addr, uint32_t* pdata)
{
    uint8_t buf[4] = {0};
    uint16_t h_addr = (uint16_t)((addr & 0xFFFF0000) >> 16);
    uint16_t l_addr = (uint16_t) (addr & 0x0000FFFF);

    if(sd77428_write_word(chip,0x03,0x0004) < 0) 
        goto error;

    if(sd77428_write_word(chip,0x01,l_addr) < 0) 
        goto error;

    if(sd77428_write_word(chip,0x02,h_addr) < 0) 
        goto error;

    if(sd77428_read_block(chip,buf,4) < 0) 
        goto error; 
    //pr_info("%x %x %x %x\n",buf[0],buf[1],buf[2],buf[3]);
    *pdata = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];  

    return 0;

error:
    pr_err("error\n");
    return -1;
}

static int32_t sd77428_write_addr(struct sd77428_data *chip,uint32_t addr, uint32_t value)
{
    uint8_t buf[4] = {0};
    uint16_t h_addr = (uint16_t)((addr & 0xFFFF0000) >> 16);
    uint16_t l_addr = (uint16_t) (addr & 0x0000FFFF);

    buf[0] = (uint8_t)((value & 0xFF000000) >> 24);
    buf[1] = (uint8_t)((value & 0x00FF0000) >> 16);
    buf[2] = (uint8_t)((value & 0x0000FF00) >> 8);
    buf[3] = (uint8_t)((value & 0x000000FF) >> 0);

    if(sd77428_write_word(chip,0x03,0x0004) < 0) 
        goto error;

    if(sd77428_write_word(chip,0x01,l_addr) < 0) 
        goto error;

    if(sd77428_write_word(chip,0x02,h_addr) < 0) 
        goto error;

    if(sd77428_write_block(chip,buf,4) < 0) 
        goto error;

    return 0;

error:
    pr_err("error\n");
    return -1;  
}

static int32_t sd77428_unlock(struct sd77428_data *chip)
{
    //uint16_t reg_value;

    if(sd77428_write_word(chip,0x11,0x6318) < 0) 
        goto error;

    if(sd77428_write_word(chip,0x12,0x6301) < 0) 
        goto error;

    if(sd77428_write_word(chip,0x20,0x8001) < 0) 
        goto error;

    if(sd77428_write_word(chip,0x20,0x8001) < 0) 
        goto error;

     // sd77428_read_word(chip,0x11, &reg_value);
     // sd77428_read_word(chip,0x12, &reg_value);
     // sd77428_read_word(chip,0x20, &reg_value);

    return 0;

error:
    pr_err("sd77428_unlock error\n");
    return -1;  
}
#endif
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
//---------------------------------------------------------------------------------------------------------------------//


//-----------------------------------------------------power_supply-----------------------------------------------------//
static enum power_supply_property sd77428_battery_props[] = {
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CURRENT_NOW,
    POWER_SUPPLY_PROP_CAPACITY,
    POWER_SUPPLY_PROP_TEMP,
    POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
    POWER_SUPPLY_PROP_CHARGE_NOW,
    POWER_SUPPLY_PROP_HEALTH,
};
static char *sd77428_supplied_from[] = {
    "usb",
    "charger",
    "ac",
};
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
                val->intval = POWER_SUPPLY_STATUS_CHARGING; /*charging*/

        }
        else
            val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
        break;

    case POWER_SUPPLY_PROP_PRESENT:
        val->intval = 1;
        break;

    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
        val->intval = chip->batt_info.batt_voltage * 1000;
        break;

    case POWER_SUPPLY_PROP_CURRENT_NOW:
        val->intval = chip->batt_info.batt_current * 1000;
        break;

    case POWER_SUPPLY_PROP_CAPACITY:
        val->intval = chip->batt_info.batt_rsoc;
        break;

    case POWER_SUPPLY_PROP_TEMP:
        val->intval = chip->batt_info.batt_temp;
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

static void sd77428_external_power_changed(struct power_supply *psy)
{
    struct sd77428_data *chip = (struct sd77428_data *)power_supply_get_drvdata(psy);
    dev_info(chip->dev,"enter\n");
    if(true == sd77428_is_ok)
    {
        cancel_delayed_work(&chip->sd77428_work);
        schedule_delayed_work(&chip->sd77428_work,0);
        power_supply_changed(chip->bat);
    }
}

static int32_t sd77428_power_supply_init(struct sd77428_data *chip)
{
    chip->bat_cfg.drv_data          = chip;
    chip->bat_cfg.of_node           = chip->client->dev.of_node;

    chip->bat_desc.name             = "sd77428";
    chip->bat_desc.type             = POWER_SUPPLY_TYPE_BATTERY;
    chip->bat_desc.properties       = sd77428_battery_props;
    chip->bat_desc.num_properties = ARRAY_SIZE(sd77428_battery_props);
    chip->bat_desc.get_property     = sd77428_battery_get_property;
    chip->bat_desc.no_thermal       = 1;
    chip->bat_desc.external_power_changed = sd77428_external_power_changed;

    chip->bat = devm_power_supply_register(chip->dev,&chip->bat_desc,&chip->bat_cfg);

    if (IS_ERR(chip->bat)) 
    {
        pr_err("Couldn't register power supply\n");
        return PTR_ERR(chip->bat);
    }
    else
    {
        chip->bat->supplied_from = sd77428_supplied_from;
        chip->bat->num_supplies  = ARRAY_SIZE(sd77428_supplied_from);
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
//---------------------------------------------------------------------------------------------------------------------//


//--------------------------------------------------驱动导出符号接口-------------------------------------------------------//
////EXPORT_SYMBOL ,在函数模块后调用，使用EXPORT_SYMBOL可以将一个函数以符号的方式导出给其他模块使用
//符号的意思就是函数的入口地址，或者说是把这些符号和对应的地址保存起来的，在内核运行的过程中，可以找到这些符号对应的地址的。
int32_t sd77428main_get_remaincap(void)
{
    batt_data_t* pinfo = &g_chip_data->batt_info;
    return pinfo->batt_rc;
}
EXPORT_SYMBOL(sd77428main_get_remaincap);

int32_t sd77428main_get_soc(void)
{
    batt_data_t* pinfo = &g_chip_data->batt_info;
    return pinfo->batt_rsoc;
}
EXPORT_SYMBOL(sd77428main_get_soc);

int32_t sd77428main_get_soh(void)
{
    batt_data_t* pinfo = &g_chip_data->batt_info;
    return pinfo->batt_soh;
}
EXPORT_SYMBOL(sd77428main_get_soh);

int32_t sd77428main_get_battry_current(void)
{
    batt_data_t* pinfo = &g_chip_data->batt_info;
    return pinfo->batt_current;
}
EXPORT_SYMBOL(sd77428main_get_battry_current);

int32_t sd77428main_get_battery_voltage(void)
{
    batt_data_t* pinfo = &g_chip_data->batt_info;
    return pinfo->batt_voltage;
}
EXPORT_SYMBOL(sd77428main_get_battery_voltage);

int32_t sd77428main_get_battery_temp(void)
{
    batt_data_t* pinfo = &g_chip_data->batt_info;
    return pinfo->batt_temp;
}
EXPORT_SYMBOL(sd77428main_get_battery_temp);

struct i2c_client * sd77428main_get_client(void)
{
    if (g_chip_data)
        return g_chip_data->client;
    else
    {
        pr_err("NULL pointer\n");
        return NULL;
    }
}
EXPORT_SYMBOL(sd77428main_get_client);

int8_t sd77428main_get_adapter_status(void)
{
    return g_chip_data->adapter_status;
}
EXPORT_SYMBOL(sd77428main_get_adapter_status);
//---------------------------------------------------------------------------------------------------------------------//


//--------------------------------------------------驱动计算子函数模块-------------------------------------------------------//
static int32_t sd77428_init_batt_info(struct sd77428_data *chip)
{
    //初始化芯片的fw算法电池参数参数
    param_board_cfg[PARM_BCFG_DESIGNCAPACITY] = BATT_DESIGN_FCC;
    param_board_cfg[PARM_BCFG_LMCHG_VOLTAGE] = BATT_DESIGN_CV;
    param_board_cfg[PARM_BCFG_FULLCHGENDCURR] = BATT_DESIGN_EOC;
    param_board_cfg[PARM_BCFG_CUTDSG_VOLTAGE] = BATT_DESIGN_EOD;
    param_board_cfg[PARM_BCFG_RSENSE] = BATT_DESIGN_RSENSE;
    //初始化芯片的返回信息
    chip->batt_info.batt_voltage = 3800;
    chip->batt_info.batt_current = 0;
    chip->batt_info.batt_temp = 250;
    chip->batt_info.batt_rsoc = 1;
    chip->batt_info.batt_soh = 100;
    chip->batt_info.batt_fcc = BATT_DESIGN_FCC; 
    chip->batt_info.batt_capacity = BATT_DESIGN_CAPACITY;
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
    batt_dbg("sd77428_mainpack vbat:%d, ibat:%05d, tbat:%d, rsoc:%03d, fcc:%d, dcap:%d, soh:%d, cycle:%d, rc:%d, ext_chg %d\n",
        pinfo->batt_voltage,
        pinfo->batt_current,
        pinfo->batt_temp,
        pinfo->batt_rsoc,
        pinfo->batt_fcc,
        pinfo->batt_capacity,
        pinfo->batt_soh,
        pinfo->batt_cyclecnt,
        pinfo->batt_rc,
        pinfo->ext_charger);
    }
    // power_supply_changed(chip->bat);
}

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

static void sd77428_update_charger_info(struct sd77428_data *chip)
{
    sd77428_check_charge_type(chip);
    //sd77428_check_charge_full(chip);

    // pr_info("status %d, full %d\n",chip->adapter_status,chip->chg_full);
/*
    if(O2_CHARGER_USB == chip->adapter_status || O2_CHARGER_AC == chip->adapter_status)
    {
        if(chip->chg_full) 
            sd77428_write_sbs(chip,EX_CHARGER_TYPE_CMD,charger_full,4);
        else
            sd77428_write_sbs(chip,EX_CHARGER_TYPE_CMD,charger_connected,4);
    }
    else
        sd77428_write_sbs(chip,EX_CHARGER_TYPE_CMD,charger_unknown,4);
*/
}

static int32_t sd77428_detect_ic(struct sd77428_data *chip)
{
    uint16_t chip_id = 0;
    uint8_t i = 0;
    uint8_t buf[5] = {0};
    for(i=0;i<5;i++)
    {
        // sd77428_read_word(chip,(sbsd_cmd_def[SBSF2_FWVER] >> SBSD_CMD_Pos) & 0xFF, &chip_id);
        sd77428_read_sbs(chip, (sbsd_cmd_def[SBSF2_FWVER] >> SBSD_CMD_Pos) & 0xFF, buf, 4);
        pr_info("SBSF2_FWVER = 0x%x %x %x %x\n", buf[0], buf[1], buf[2], buf[3]);
        
        sd77428_read_word(chip,(sbsd_cmd_def[SBSF1_CHIPVER] >> SBSD_CMD_Pos) & 0xFF, &chip_id);
        chip_id = chip_id &0xFF;
        pr_info("chip_id = 0x%x\n", chip_id);
        //if(chip_id == 0x12 || chip_id == 0x00)
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
static int32_t sd77428_set_sbs_charparams(struct sd77428_data *chip, uint8_t cmd, uint8_t param_l1, uint8_t param_l2, uint8_t param_l3, uint8_t param_l4)
{
    uint8_t w_buf[5] = {0};
    // w_buf[0] = param_l1;
    // w_buf[1] = param_l2;
    // w_buf[2] = param_l3;
    // w_buf[3] = param_l4;
    w_buf[3] = param_l1;
    w_buf[2] = param_l2;
    w_buf[1] = param_l3;
    w_buf[0] = param_l4;
    return sd77428_write_sbs(chip, cmd, w_buf, 4); 
}
static int32_t sd77428_get_sbs_charparams(struct sd77428_data *chip, uint8_t cmd, uint8_t* param_l1, uint8_t* param_l2, uint8_t* param_l3, uint8_t* param_l4)
{
    uint8_t buf[5] = {0};
    sd77428_read_sbs(chip, cmd, buf, 4);
    // *param_l1 = buf[0];
    // *param_l2 = buf[1];
    // *param_l3 = buf[2];
    // *param_l4 = buf[3];

    *param_l1 = buf[3];
    *param_l2 = buf[2];
    *param_l3 = buf[1];
    *param_l4 = buf[0];
    return 0; 
}

// SBSA0_CONFIGDSG_CHG, SBSA4_FILTER_RATIO, SBSA5_ALG_SWITCH_TH, SBSA8_FG_dc_hardset_fcc, SBSA9_FG_dfcc_on_and_fratio
// SBSAA_FG_dfcc_compensate_th, SBSAB_FG_dfcc_compensate_ratio
// SBSB0_NTC_HOLD_TIME
static int32_t sd77428_set_sbs_shortparams(struct sd77428_data *chip, uint8_t cmd, uint16_t param_high, uint16_t param_low)
{
    uint8_t w_buf[5] = {0};
    // w_buf[0] = (uint8_t)((param_high & 0xFF00) >> 8);
    // w_buf[1] = (uint8_t)(param_high & 0x00FF);
    // w_buf[2] = (uint8_t)((param_low & 0xFF00) >> 8);
    // w_buf[3] = (uint8_t)(param_low & 0x00FF);
    //低字节在前 
    w_buf[0] = (uint8_t)(param_low & 0x00FF);
    w_buf[1] = (uint8_t)((param_low & 0xFF00) >> 8);
    w_buf[2] = (uint8_t)(param_high & 0x00FF);
    w_buf[3] = (uint8_t)((param_high & 0xFF00) >> 8);
    return sd77428_write_sbs(chip, cmd, w_buf, 4); 
}
static int32_t sd77428_get_sbs_shortparams(struct sd77428_data *chip, uint8_t cmd, uint16_t* param_high, uint16_t* param_low)
{
    uint8_t buf[5] = {0};
    sd77428_read_sbs(chip, cmd, buf, 4);
    // *param_high = (buf[0] << 8) | (buf[1]);
    // *param_low = (buf[2] << 8) | (buf[3]);

    //低字节在前 
    *param_low = (buf[1] << 8) | (buf[0]);
    *param_high = (buf[3] << 8) | (buf[2]); 
    return 0; 
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
#if 0
// SBSE0_TABLE_OCV
static int32_t sd77428_set_ocvtable(struct sd77428_data *chip, uint8_t* ocv_table)
{
    //ocv table , 30*8+20
    uint8_t whole_packidx = 9;
    uint8_t cur_packidx = 1; // 分包有效索引从1开始
    uint8_t w_buf[33] = {0};
    uint16_t offset_pt = 0;
    //pack 1~8
    for (; cur_packidx < whole_packidx; ++cur_packidx)
    {
        w_buf[0] = whole_packidx;
        w_buf[1] = cur_packidx;
        offset_pt = (cur_packidx - 1) * 30;
        memcpy(w_buf+2, ocv_table + offset_pt,30);
        
        if(sd77428_write_sbs(chip, (sbsd_cmd_def[SBSE0_TABLE_OCV] >> SBSD_CMD_Pos) & 0xFF, w_buf, 32) < 0)
        {
            pr_info("Update ocvtable failed, pack idx %d.\r\n", cur_packidx);
            return -1;
        }
        usleep_range(20000,30000);//delay 20ms,retry
    }
    //pack 9
    memset(w_buf, 0, 33);
    w_buf[0] = whole_packidx;
    w_buf[1] = whole_packidx;
    offset_pt = (cur_packidx - 1) * 30;
    memcpy(w_buf+2, ocv_table + offset_pt, 20);
    if(sd77428_write_sbs(chip, (sbsd_cmd_def[SBSE0_TABLE_OCV] >> SBSD_CMD_Pos) & 0xFF, w_buf, 32) < 0)
    {
        pr_info("Update ocvtable failed, pack idx %d.\r\n", cur_packidx);
        return -1;
    }

    return 0;
}
static int32_t sd77428_get_ocvtable(struct sd77428_data *chip, uint8_t* ocv_table)
{
    //ocv table , 32*8+4
    uint8_t whole_packidx = 9;
    uint8_t cur_packidx = 1; // 分包有效索引从1开始
    uint8_t buf[33] = {0};
    uint16_t offset_pt = 0;

    for (; cur_packidx < whole_packidx; ++cur_packidx)
    {
        offset_pt = (cur_packidx - 1) * 32;

        //step 1, set operature table packidx
        if(sd77428_set_table_packidx(chip, cur_packidx) < 0)
        {
            pr_info("Set ocvtable idx %d, failed.\r\n", cur_packidx);
            return -1;
        }
        usleep_range(20000,30000);//delay 20ms,retry
        //step 2, read table pack
        if(sd77428_read_sbs(chip, (sbsd_cmd_def[SBSE0_TABLE_OCV] >> SBSD_CMD_Pos) & 0xFF, buf, 32) < 0)
        {
            pr_info("get ocvtable failed. cur_packidx:%d .\r\n", cur_packidx);
            return -1;
        }
        memcpy(ocv_table + offset_pt, buf, 32);
        usleep_range(20000,30000);//delay 20ms,retry
    }
    //pack 9
    offset_pt = (cur_packidx - 1) * 32;
    if(sd77428_set_table_packidx(chip, cur_packidx) < 0)
    {
        pr_info("Set ocvtable idx %d, failed.\r\n", cur_packidx);
        return -1;
    }
    usleep_range(20000,30000);//delay 20ms,retry

    if(sd77428_read_sbs(chip, (sbsd_cmd_def[SBSE0_TABLE_OCV] >> SBSD_CMD_Pos) & 0xFF, buf, 4) < 0)
    {
        pr_info("get ocvtable failed. cur_packidx:%d .\r\n", cur_packidx);
        return -1;
    }
    memcpy(ocv_table + offset_pt, buf, 4);

    return 0;
}

// SBSE1_TABLE_VOLT
static int32_t sd77428_set_voltable(struct sd77428_data *chip, uint8_t* vol_table)
{
    //ocv table , 30*2+12
    uint8_t whole_packidx = 3;
    uint8_t cur_packidx = 1; // 分包有效索引从1开始
    uint8_t w_buf[33] = {0};
    uint16_t offset_pt = 0;
    //pack 1~8
    for (; cur_packidx < whole_packidx; ++cur_packidx)
    {
        w_buf[0] = whole_packidx;
        w_buf[1] = cur_packidx;
        offset_pt = (cur_packidx - 1) * 30;
        memcpy(w_buf+2, vol_table + offset_pt,30);
        
        if(sd77428_write_sbs(chip, (sbsd_cmd_def[SBSE1_TABLE_VOLT] >> SBSD_CMD_Pos) & 0xFF, w_buf, 32) < 0)
        {
            pr_info("Update ocvtable failed, pack idx %d.\r\n", cur_packidx);
            return -1;
        }
        usleep_range(20000,30000);//delay 20ms,retry
    }
    //pack 3
    memset(w_buf, 0, 33);
    w_buf[0] = whole_packidx;
    w_buf[1] = whole_packidx;
    offset_pt = (cur_packidx - 1) * 30;
    memcpy(w_buf+2, vol_table + offset_pt, 12);
    if(sd77428_write_sbs(chip, (sbsd_cmd_def[SBSE1_TABLE_VOLT] >> SBSD_CMD_Pos) & 0xFF, w_buf, 12) < 0)
    {
        pr_info("Update ocvtable failed, pack idx %d.\r\n", cur_packidx);
        return -1;
    }

    return 0;
}
static int32_t sd77428_get_voltable(struct sd77428_data *chip, uint8_t* vol_table)
{
    //ocv table , 32*2+8
    uint8_t whole_packidx = 3;
    uint8_t cur_packidx = 1; // 分包有效索引从1开始
    uint8_t buf[33] = {0};
    uint16_t offset_pt = 0;

    for (; cur_packidx < whole_packidx; ++cur_packidx)
    {
        offset_pt = (cur_packidx - 1) * 32;

        //step 1, set operature table packidx
        if(sd77428_set_table_packidx(chip, cur_packidx) < 0)
        {
            pr_info("Set ocvtable idx %d, failed.\r\n", cur_packidx);
            return -1;
        }
        usleep_range(20000,30000);//delay 20ms,retry
        //step 2, read table pack
        if(sd77428_read_sbs(chip, (sbsd_cmd_def[SBSE1_TABLE_VOLT] >> SBSD_CMD_Pos) & 0xFF , buf, 32) < 0)
        {
            pr_info("get ocvtable failed. cur_packidx:%d .\r\n", cur_packidx);
            return -1;
        }
        memcpy(vol_table + offset_pt, buf, 32);
        usleep_range(20000,30000);//delay 20ms,retry
    }
    //pack 3
    offset_pt = (cur_packidx - 1) * 32;
    if(sd77428_set_table_packidx(chip, cur_packidx) < 0)
    {
        pr_info("Set ocvtable idx %d, failed.\r\n", cur_packidx);
        return -1;
    }
    usleep_range(20000,30000);//delay 20ms,retry

    if(sd77428_read_sbs(chip, (sbsd_cmd_def[SBSE1_TABLE_VOLT] >> SBSD_CMD_Pos) & 0xFF, buf, 8) < 0)
    {
        pr_info("get ocvtable failed. cur_packidx:%d .\r\n", cur_packidx);
        return -1;
    }
    memcpy(vol_table + offset_pt, buf, 8);

    return 0;
}

//SBSE2_TABLE_CURR 
static int32_t sd77428_set_currtable(struct sd77428_data *chip, uint8_t* curr_table)
{
    uint8_t w_buf[33] = {0};
    memcpy(w_buf, curr_table, 8);
    if(sd77428_write_sbs(chip, (sbsd_cmd_def[SBSE2_TABLE_CURR] >> SBSD_CMD_Pos) & 0xFF, w_buf, 8) < 0)
    {
        pr_info("set curr_table failed.\r\n");
        return -1;
    }
    return 0;
}
static int32_t sd77428_get_currtable(struct sd77428_data *chip, uint8_t* curr_table)
{
    uint8_t buf[33] = {0};
    if(sd77428_read_sbs(chip, (sbsd_cmd_def[SBSE2_TABLE_CURR] >> SBSD_CMD_Pos) & 0xFF, buf, 8) < 0)
    {
        pr_info("get curr_table failed.\r\n");
        return -1;
    }
    memcpy(curr_table, buf, 8);

    return 0;
}

//SBSE3_TABLE_TEMP 
static int32_t sd77428_set_temptable(struct sd77428_data *chip, uint8_t* temp_table)
{
    uint8_t w_buf[33] = {0};
    memcpy(w_buf, temp_table, 8);
    if(sd77428_write_sbs(chip, (sbsd_cmd_def[SBSE3_TABLE_TEMP] >> SBSD_CMD_Pos) & 0xFF, w_buf, 8) < 0)
    {
        pr_info("set temp_table failed.\r\n");
        return -1;
    }
    return 0;
}
static int32_t sd77428_get_temptable(struct sd77428_data *chip, uint8_t* temp_table)
{
    uint8_t buf[33] = {0};
    if(sd77428_read_sbs(chip, (sbsd_cmd_def[SBSE3_TABLE_TEMP] >> SBSD_CMD_Pos) & 0xFF, buf, 8) < 0)
    {
        pr_info("get temp_table failed.\r\n");
        return -1;
    }
    memcpy(temp_table, buf, 8);

    return 0;
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
    //pr_info("set single_table lens:%d, w_buf:%x %x %x %x %x %x. \r\n"packlens, ,w_buf[0],w_buf[1],w_buf[2],w_buf[3],w_buf[4],w_buf[5]);

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

//-----------------------------------------------测试function-----------------------------------------------------//
#if 0
static uint16_t _test_reg_func(struct sd77428_data *chip)
{
    uint16_t test_result = 0;

    uint8_t param_l1 = 0, param_l2 = 0, param_l3=0, param_l4=0;
    uint16_t param_high = 0, param_low = 0;

    //step 1 SBSA3_FILTER_WIND_LOC
    sd77428_get_sbs_charparams(chip, (sbsd_cmd_def[SBSA3_FILTER_WIND_LOC] >> SBSD_CMD_Pos) & 0xFF, &param_l1, &param_l2, &param_l3, &param_l4);
    pr_info("SBSA3_FILTER_WIND_LOC read data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
    sd77428_set_sbs_charparams(chip, (sbsd_cmd_def[SBSA3_FILTER_WIND_LOC] >> SBSD_CMD_Pos) & 0xFF, effect_mid_windows, effect_mean_windows, effect_first_lock_th, effect_noise_range_gate);
    usleep_range(10000, 20000);
    sd77428_get_sbs_charparams(chip, (sbsd_cmd_def[SBSA3_FILTER_WIND_LOC] >> SBSD_CMD_Pos) & 0xFF, &param_l1, &param_l2, &param_l3, &param_l4);
    pr_info("SBSA3_FILTER_WIND_LOC read data-2: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
    if (param_l1 != effect_mid_windows ||
        param_l2 != effect_mean_windows ||
        param_l3 != effect_first_lock_th ||
        param_l4 != effect_noise_range_gate)
    {
        pr_info("SBSA3_FILTER_WIND_LOC read/write test failed.\r\n");
        test_result += (1 << 0);
    }
    else
    {
        pr_info("SBSA3_FILTER_WIND_LOC read/write test success.\r\n");
    }
    usleep_range(10000, 20000);

    //step 2 SBSA5_FILTER_ENABLE
    sd77428_get_sbs_charparams(chip, (sbsd_cmd_def[SBSA6_FG_AGING_and_ENABLE] >> SBSD_CMD_Pos) & 0xFF, &param_l1, &param_l2, &param_l3, &param_l4);
    pr_info("SBSA6_FG_AGING_and_ENABLE read data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
    sd77428_set_sbs_charparams(chip, (sbsd_cmd_def[SBSA6_FG_AGING_and_ENABLE] >> SBSD_CMD_Pos) & 0xFF, FG_AGING_RATIO_MIN, FG_AGING_RATIO_MAX, FG_idle_60s_enable, FG_ccprv_fcc_setenable);
    usleep_range(10000, 20000);
    sd77428_get_sbs_charparams(chip, (sbsd_cmd_def[SBSA6_FG_AGING_and_ENABLE] >> SBSD_CMD_Pos) & 0xFF, &param_l1, &param_l2, &param_l3, &param_l4);
    pr_info("SBSA6_FG_AGING_and_ENABLE read data-2: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
    if (param_l1 != FG_AGING_RATIO_MIN ||
        param_l2 != FG_AGING_RATIO_MAX ||
        param_l3 != FG_idle_60s_enable ||
        param_l4 != FG_ccprv_fcc_setenable)
    {
        pr_info("SBSA6_FG_AGING_and_ENABLE read/write test failed.\r\n");
        test_result += (1 << 1);
    }
    else
    {
        pr_info("SBSA6_FG_AGING_and_ENABLE read/write test success.\r\n");
    }
    usleep_range(10000, 20000);

    //step 3 SBSA0_CONFIGDSG_CHG
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSA0_CONFIGDSG_CHG] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSA0_CONFIGDSG_CHG read data: (%d, %d).\r\n", param_high, param_low);
    sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSA0_CONFIGDSG_CHG] >> SBSD_CMD_Pos) & 0xFF, gdm_thresh_chg, gdm_thresh_dsg);
    usleep_range(10000, 20000);
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSA0_CONFIGDSG_CHG] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSA0_CONFIGDSG_CHG read data-2: (%d, %d).\r\n", param_high, param_low);
    if (param_high != gdm_thresh_chg ||
        param_low != gdm_thresh_dsg )
    {
        pr_info("SBSA0_CONFIGDSG_CHG read/write test failed.\r\n");
        test_result += (1 << 2);
    }
    else
    {
        pr_info("SBSA0_CONFIGDSG_CHG read/write test success.\r\n");
    }
    usleep_range(10000, 20000);

    //step 4 SBSA4_FILTER_RATIO
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSA4_FILTER_RATIO] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSA4_FILTER_RATIO read data: (%d, %d).\r\n", param_high, param_low);
    sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSA4_FILTER_RATIO] >> SBSD_CMD_Pos) & 0xFF, effect_init_delay_ratio, effect_noise_ratio);
    usleep_range(10000, 20000);
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSA4_FILTER_RATIO] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSA4_FILTER_RATIO read data-2: (%d, %d).\r\n", param_high, param_low);
    if (param_high != effect_init_delay_ratio ||
        param_low != effect_noise_ratio )
    {
        pr_info("SBSA4_FILTER_RATIO read/write test failed.\r\n");
        test_result += (1 << 3);
    }
    else
    {
        pr_info("SBSA4_FILTER_RATIO read/write test success.\r\n");
    }
    usleep_range(10000, 20000);

    //step 5 SBSA5_ALG_SWITCH_TH
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSA5_ALG_SWITCH_TH] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSA5_ALG_SWITCH_TH read data: (%d, %d).\r\n", param_high, param_low);
    sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSA5_ALG_SWITCH_TH] >> SBSD_CMD_Pos) & 0xFF, alg_switch_th, FG_dfcc_filter_ratio);
    usleep_range(10000, 20000);
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSA5_ALG_SWITCH_TH] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSA5_ALG_SWITCH_TH read data-2: (%d, %d).\r\n", param_high, param_low);
    if (param_high != alg_switch_th ||
        param_low != FG_dfcc_filter_ratio )
    {
        pr_info("SBSA5_ALG_SWITCH_TH read/write test failed.\r\n");
        test_result += (1 << 4);
    }
    else
    {
        pr_info("SBSA5_ALG_SWITCH_TH read/write test success.\r\n");
    }
    usleep_range(10000, 20000);

    //step 6 SBSA7_FG_cc_hardset_fcc
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSA7_FG_cc_hardset_fcc] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSA7_FG_cc_hardset_fcc read data: (%d, %d).\r\n", param_high, param_low);
    sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSA7_FG_cc_hardset_fcc] >> SBSD_CMD_Pos) & 0xFF, FG_cc_hardset_fcc, FG_mcc_hardset_fcc);
    usleep_range(10000, 20000);
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSA7_FG_cc_hardset_fcc] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSA7_FG_cc_hardset_fcc read data-2: (%d, %d).\r\n", param_high, param_low);
    if (param_high != FG_cc_hardset_fcc ||
        param_low != FG_mcc_hardset_fcc )
    {
        pr_info("SBSA7_FG_cc_hardset_fcc read/write test failed.\r\n");
        test_result += (1 << 5);
    }
    else
    {
        pr_info("SBSA7_FG_cc_hardset_fcc read/write test success.\r\n");
    }
    usleep_range(10000, 20000);

    //step 7 SBSA8_FG_dc_hardset_fcc
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSA8_FG_dc_hardset_fcc] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSA8_FG_dc_hardset_fcc read data: (%d, %d).\r\n", param_high, param_low);
    sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSA8_FG_dc_hardset_fcc] >> SBSD_CMD_Pos) & 0xFF, FG_dc_hardset_fcc, FG_mdc_hardset_fcc);
    usleep_range(10000, 20000);
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSA8_FG_dc_hardset_fcc] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSA8_FG_dc_hardset_fcc read data-2: (%d, %d).\r\n", param_high, param_low);
    if (param_high != FG_dc_hardset_fcc ||
        param_low != FG_mdc_hardset_fcc )
    {
        pr_info("SBSA8_FG_dc_hardset_fcc read/write test failed.\r\n");
        test_result += (1 << 6);
    }
    else
    {
        pr_info("SBSA8_FG_dc_hardset_fcc read/write test success.\r\n");
    }
    usleep_range(10000, 20000);

    //step 8 SBSA9_FG_dfcc_on_socrange
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSA9_FG_dfcc_on_socrange] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSA9_FG_dfcc_on_socrange read data: (%d, %d).\r\n", param_high, param_low);
    sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSA9_FG_dfcc_on_socrange] >> SBSD_CMD_Pos) & 0xFF, FG_dfcc_onsoc_min, FG_dfcc_onsoc_max);
    usleep_range(10000, 20000);
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSA9_FG_dfcc_on_socrange] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSA9_FG_dfcc_on_socrange read data-2: (%d, %d).\r\n", param_high, param_low);
    if (param_high != FG_dfcc_onsoc_min ||
        param_low != FG_dfcc_onsoc_max )
    {
        pr_info("SBSA9_FG_dfcc_on_socrange read/write test failed.\r\n");
        test_result += (1 << 7);
    }
    else
    {
        pr_info("SBSA9_FG_dfcc_on_socrange read/write test success.\r\n");
    }
    usleep_range(10000, 20000);

    //step 9 SBSAA_FG_dfcc_compensate_th
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSAA_FG_dfcc_compensate_th] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSAA_FG_dfcc_compensate_th read data: (%d, %d).\r\n", param_high, param_low);
    sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSAA_FG_dfcc_compensate_th] >> SBSD_CMD_Pos) & 0xFF, FG_dfcc_compensate_th1, FG_dfcc_compensate_th2);
    usleep_range(10000, 20000);
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSAA_FG_dfcc_compensate_th] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSAA_FG_dfcc_compensate_th read data-2: (%d, %d).\r\n", param_high, param_low);
    if (param_high != FG_dfcc_compensate_th1 ||
        param_low != FG_dfcc_compensate_th2 )
    {
        pr_info("SBSAA_FG_dfcc_compensate_th read/write test failed.\r\n");
        test_result += (1 << 8);
    }
    else
    {
        pr_info("SBSAA_FG_dfcc_compensate_th read/write test success.\r\n");
    }
    usleep_range(10000, 20000);

    //step 10 SBSAB_FG_dfcc_compensate_ratio
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSAB_FG_dfcc_compensate_ratio] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSAB_FG_dfcc_compensate_ratio read data: (%d, %d).\r\n", param_high, param_low);
    sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSAB_FG_dfcc_compensate_ratio] >> SBSD_CMD_Pos) & 0xFF, FG_dfcc_compensate_ratio1, FG_dfcc_compensate_ratio2);
    usleep_range(10000, 20000);
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSAB_FG_dfcc_compensate_ratio] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSAB_FG_dfcc_compensate_ratio read data-2: (%d, %d).\r\n", param_high, param_low);
    if (param_high != FG_dfcc_compensate_ratio1 ||
        param_low != FG_dfcc_compensate_ratio2 )
    {
        pr_info("SBSAB_FG_dfcc_compensate_ratio read/write test failed.\r\n");
        test_result += (1 << 9);
    }
    else
    {
        pr_info("SBSAB_FG_dfcc_compensate_ratio read/write test success.\r\n");
    }
    usleep_range(10000, 20000);

    //step 10 SBSB0_NTC_HOLD_TIME
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSB0_NTC_HOLD_TIME] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSB0_NTC_HOLD_TIME read data: (%d, %d).\r\n", param_high, param_low);
    sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSB0_NTC_HOLD_TIME] >> SBSD_CMD_Pos) & 0xFF, ntc_reg_enable, ntc_reg_holdtime);
    usleep_range(10000, 20000);
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSB0_NTC_HOLD_TIME] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSAB_FG_dfcc_compensate_ratio read data-2: (%d, %d).\r\n", param_high, param_low);
    if (param_high != ntc_reg_enable ||
        param_low != ntc_reg_holdtime )
    {
        pr_info("SBSB0_NTC_HOLD_TIME read/write test failed.\r\n");
        test_result += (1 << 10);
    }
    else
    {
        pr_info("SBSB0_NTC_HOLD_TIME read/write test success.\r\n");
    }
    usleep_range(10000, 20000);
    return test_result;
}

static uint16_t _test_reg_table_func1(struct sd77428_data *chip)
{
    uint16_t test_result = 0;

    //step1  SBSE2_TABLE_CURR
    uint8_t single_table[33];
    uint8_t table_lens = 8;
    uint8_t i = 0;
    short curr_table[YAxis];
    short temp_table[ZAxis];

    sd77428_get_singletable(chip, (sbsd_cmd_def[SBSE2_TABLE_CURR] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);
    memcpy((uint8_t*)curr_table, single_table, table_lens);
    pr_info("SBSE2_TABLE_CURR: (%d, %d, %d, %d). \r\n", curr_table[0], curr_table[1], curr_table[2], curr_table[3]);

    usleep_range(10000,20000);
    memcpy(single_table, (uint8_t*)YAxisElement, table_lens);
    sd77428_set_singletable(chip, (sbsd_cmd_def[SBSE2_TABLE_CURR] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);
    usleep_range(10000,20000);
    sd77428_get_singletable(chip, (sbsd_cmd_def[SBSE2_TABLE_CURR] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);
    usleep_range(10000,20000);
    memcpy((uint8_t*)curr_table, single_table, table_lens);
    pr_info("SBSE2_TABLE_CURR-2: (%d, %d, %d, %d). \r\n", curr_table[0], curr_table[1], curr_table[2], curr_table[3]);
    if (calculate_pec_bytes(0, single_table, table_lens)
        != calculate_pec_bytes(0, (uint8_t*)YAxisElement, table_lens))
    {
        pr_info("SBSE2_TABLE_CURR read/write test failed.\r\n");
        test_result += (1 << 0);
    }
    else
    {
        pr_info("SBSE2_TABLE_CURR read/write test success.\r\n");
    }
    
    //step2  SBSE3_TABLE_TEMP
    table_lens = 8;
    sd77428_get_singletable(chip, (sbsd_cmd_def[SBSE3_TABLE_TEMP] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);
    memcpy((uint8_t*)temp_table, single_table, table_lens);
    pr_info("SBSE3_TABLE_TEMP: (%d, %d, %d, %d). \r\n", temp_table[0], temp_table[1], temp_table[2], temp_table[3]);

    usleep_range(10000,20000);
    memcpy(single_table, (uint8_t*)ZAxisElement, table_lens);
    sd77428_set_singletable(chip, (sbsd_cmd_def[SBSE3_TABLE_TEMP] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);
    usleep_range(10000,20000);
    sd77428_get_singletable(chip, (sbsd_cmd_def[SBSE3_TABLE_TEMP] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);
    usleep_range(10000,20000);
    memcpy((uint8_t*)temp_table, single_table, table_lens);
    pr_info("SBSE3_TABLE_TEMP-2: (%d, %d, %d, %d). \r\n", temp_table[0], temp_table[1], temp_table[2], temp_table[3]);
    if (calculate_pec_bytes(0, single_table, table_lens)
        != calculate_pec_bytes(0, (uint8_t*)ZAxisElement, table_lens))
    {
        pr_info("SBSE3_TABLE_TEMP read/write test failed.\r\n");
        test_result += (1 << 1);
    }
    else
    {
        pr_info("SBSE3_TABLE_TEMP read/write test success.\r\n");
    }
    return test_result;
}

static uint16_t _test_reg_table_func2(struct sd77428_data *chip)
{
    uint16_t test_result = 0;

    //step1  SBSE2_TABLE_CURR
    uint8_t packtable[577];
    uint16_t packlens = 260;
    uint16_t i = 0, j =0;
    short ocv_table[OCV_DATA_NUM*2];
    short voltage_table[XAxis];
    //step1  SBSE0_TABLE_OCV 260
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE0_TABLE_OCV] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);

    memcpy((uint8_t*)ocv_table, packtable, packlens);
    printk("SBSE0_TABLE_OCV: \r\n");
    for (i = 0; i < OCV_DATA_NUM*2; ++i)
    {
        printk(KERN_CONT  "%d ", ocv_table[i]);
    }
    printk("\r\n");

    usleep_range(10000,20000);
    memcpy(packtable, (uint8_t*)ocv_data, packlens);
    sd77428_set_packtable(chip, (sbsd_cmd_def[SBSE0_TABLE_OCV] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(10000,20000);
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE0_TABLE_OCV] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(10000,20000);
    memcpy((uint8_t*)ocv_table, packtable, packlens);
    printk("SBSE0_TABLE_OCV-2: \r\n");
    for (i = 0; i < OCV_DATA_NUM*2; ++i)
    {
        printk(KERN_CONT  "%d ", ocv_table[i]);
    }
    printk("\r\n");
    if (calculate_pec_bytes(0, packtable, packlens)
        != calculate_pec_bytes(0, (uint8_t*)ocv_data, packlens))
    {
        pr_info("SBSE0_TABLE_OCV read/write test failed.\r\n");
        test_result += (1 << 0);
    }
    else
    {
        pr_info("SBSE0_TABLE_OCV read/write test success.\r\n");
    }

    //step2  SBSE1_TABLE_VOLT 72
    packlens = 72;
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE1_TABLE_VOLT] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);

    memcpy((uint8_t*)voltage_table, packtable, packlens);
    printk("SBSE1_TABLE_VOLT: \r\n");
    for (i = 0; i < XAxis; ++i)
    {
        printk(KERN_CONT "%d ", voltage_table[i]);
    }
    printk("\r\n");

    usleep_range(10000,20000);
    memcpy(packtable, (uint8_t*)XAxisElement, packlens);
    // printk("SBSE1_TABLE_VOLT-1: \r\n");
    // for (i = 0; i < packlens; ++i)
    // {
    //     printk(KERN_CONT "0x%02x ", packtable[i]);
    // }
    // printk("\r\n");

    sd77428_set_packtable(chip, (sbsd_cmd_def[SBSE1_TABLE_VOLT] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(10000,20000);
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE1_TABLE_VOLT] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(10000,20000);
    memcpy((uint8_t*)voltage_table, packtable, packlens);
    printk("SBSE1_TABLE_VOLT-2: \r\n");
    for (i = 0; i < XAxis; ++i)
    {
        printk(KERN_CONT "%d ", voltage_table[i]);
    }
    printk("\r\n");
    if (calculate_pec_bytes(0, packtable, packlens)
        != calculate_pec_bytes(0, (uint8_t*)XAxisElement, packlens))
    {
        pr_info("SBSE1_TABLE_VOLT read/write test failed.\r\n");
        test_result += (1 << 1);
    }
    else
    {
        pr_info("SBSE1_TABLE_VOLT read/write test success.\r\n");
    }

    //step3  SBSE4_TABLE_RC 576
    packlens = 576;
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE4_TABLE_RC] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    printk("SBSE4_TABLE_RC: \r\n");
    for (i = 0; i < YAxis*ZAxis; ++i)
    {
        for (j = 0; j < XAxis; ++j)
        {
            printk(KERN_CONT "%d ", packtable[i *XAxis +j ]);
        }
        printk("\r\n");
    }
    printk("\r\n");
    // printk("RCtable: \r\n");
    // for (i = 0; i < YAxis*ZAxis; ++i)
    // {
    //     for (j = 0; j < XAxis; ++j)
    //     {
    //         printk(KERN_CONT "%d ", RCtable[i][j]);
    //     }
    //     printk("\r\n");
    // }
    // printk("\r\n");
    usleep_range(10000,20000);
    memcpy(packtable, (uint8_t*)RCtable, packlens);
    sd77428_set_packtable(chip, (sbsd_cmd_def[SBSE4_TABLE_RC] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(10000,20000);
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE4_TABLE_RC] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(10000,20000);
    printk("SBSE4_TABLE_RC: \r\n");
    for (i = 0; i < YAxis*ZAxis; ++i)
    {
        for (j = 0; j < XAxis; ++j)
        {
            printk(KERN_CONT "%d ", packtable[i *XAxis +j ]);
        }
        printk("\r\n");
    }
    printk("\r\n");
    if (calculate_pec_bytes(0, packtable, packlens)
        != calculate_pec_bytes(0, (uint8_t*)RCtable, packlens))
    {
        pr_info("SBSE4_TABLE_RC read/write test failed.\r\n");
        test_result += (1 << 2);
    }
    else
    {
        pr_info("SBSE4_TABLE_RC read/write test success.\r\n");
    }
    return test_result;
}

static uint16_t _test_reg_table_func3(struct sd77428_data *chip)
{
    uint16_t test_result = 0;

    //step1  SBSE2_TABLE_CURR
    uint8_t packtable[323];
    uint16_t packlens = 0;
    uint16_t i = 0;
    int32_t temp_table[TEMPERATURE_DATA_NUM*2];
    
    //step4  SBSE5_TABLE_NTC_TEMP 320
    packlens = 320;
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE5_TABLE_NTC_TEMP] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);

    memcpy((uint8_t*)temp_table, packtable, packlens);
    printk("SBSE5_TABLE_NTC_TEMP: \r\n");
    for (i = 0; i < TEMPERATURE_DATA_NUM*2; ++i)
    {
        printk(KERN_CONT "%d ", temp_table[i]);
    }
    printk("\r\n");

    usleep_range(10000,20000);
    memcpy(packtable, (uint8_t*)cell_temp_data, packlens);
    sd77428_set_packtable(chip, (sbsd_cmd_def[SBSE5_TABLE_NTC_TEMP] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(10000,20000);
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE5_TABLE_NTC_TEMP] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(10000,20000);
    
    memcpy((uint8_t*)temp_table, packtable, packlens);
    printk("SBSE5_TABLE_NTC_TEMP: \r\n");
    for (i = 0; i < TEMPERATURE_DATA_NUM*2; ++i)
    {
        printk(KERN_CONT "%d ", temp_table[i]);
    }
    printk("\r\n");

    if (calculate_pec_bytes(0, packtable, packlens)
        != calculate_pec_bytes(0, (uint8_t*)cell_temp_data, packlens))
    {
        pr_info("SBSE5_TABLE_NTC_TEMP read/write test failed.\r\n");
        test_result += (1 << 0);
    }
    else
    {
        pr_info("SBSE5_TABLE_NTC_TEMP read/write test success.\r\n");
    }
    return test_result;
}
#endif
//-------------------------------------------------------------------------------------------------------------------//
//-----------------------------------------------参数下载 function-----------------------------------------------------//
//download algorithm parameters
static uint16_t parameters_down_level1(struct sd77428_data *chip)
{
    uint16_t paramdown_result = 0;

    uint8_t param_l1 = 0, param_l2 = 0, param_l3=0, param_l4=0;
    uint16_t param_high = 0, param_low = 0;

    //step 1 SBSA0_CONFIGDSG_CHG
    sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSA0_CONFIGDSG_CHG] >> SBSD_CMD_Pos) & 0xFF, gdm_thresh_chg, gdm_thresh_dsg);
    usleep_range(5000, 10000);
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSA0_CONFIGDSG_CHG] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSA0_CONFIGDSG_CHG recall data: (%d, %d).\r\n", param_high, param_low);
    if (param_high != gdm_thresh_chg ||
        param_low != gdm_thresh_dsg )
    {
        pr_info("SBSA0_CONFIGDSG_CHG Set Failed.\r\n");
        paramdown_result += (1 << 2);
    }
    else
    {
        pr_info("SBSA0_CONFIGDSG_CHG Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 2 SBSA3_FILTER_WIND_LOC
    sd77428_set_sbs_charparams(chip, (sbsd_cmd_def[SBSA3_FILTER_WIND_LOC] >> SBSD_CMD_Pos) & 0xFF, effect_mid_windows, effect_mean_windows, effect_first_lock_th, effect_noise_range_gate);
    usleep_range(5000, 10000);
    sd77428_get_sbs_charparams(chip, (sbsd_cmd_def[SBSA3_FILTER_WIND_LOC] >> SBSD_CMD_Pos) & 0xFF, &param_l1, &param_l2, &param_l3, &param_l4);
    pr_info("SBSA3_FILTER_WIND_LOC recall data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
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
        pr_info("SBSA3_FILTER_WIND_LOC Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 3 SBSA4_FILTER_RATIO
    sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSA4_FILTER_RATIO] >> SBSD_CMD_Pos) & 0xFF, effect_init_delay_ratio, effect_noise_ratio);
    usleep_range(5000, 10000);
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSA4_FILTER_RATIO] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSA4_FILTER_RATIO recall data: (%d, %d).\r\n", param_high, param_low);
    if (param_high != effect_init_delay_ratio ||
        param_low != effect_noise_ratio )
    {
        pr_info("SBSA4_FILTER_RATIO Set Failed.\r\n");
        paramdown_result += (1 << 3);
    }
    else
    {
        pr_info("SBSA4_FILTER_RATIO Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 4 SBSA5_FILTER_ENABLE
    sd77428_set_sbs_charparams(chip, (sbsd_cmd_def[SBSA5_FILTER_ENABLE] >> SBSD_CMD_Pos) & 0xFF, SECOND_LOCK_FLAG, filter_switcher, calib_switcher, filter_offset);
    usleep_range(5000, 10000);
    sd77428_get_sbs_charparams(chip, (sbsd_cmd_def[SBSA5_FILTER_ENABLE] >> SBSD_CMD_Pos) & 0xFF, &param_l1, &param_l2, &param_l3, &param_l4);
    pr_info("SBSA5_FILTER_ENABLE recall data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
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
        pr_info("SBSA5_FILTER_ENABLE Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 5 SBSA6_FILTER_SECOND_LOCK
    sd77428_set_sbs_charparams(chip, (sbsd_cmd_def[SBSA6_FILTER_SECOND_LOCK] >> SBSD_CMD_Pos) & 0xFF, 0, SECOND_LOCK_TH, SECOND_LOCK_TIMER, SECOND_UNLOCK_TIMER);
    usleep_range(5000, 10000);
    sd77428_get_sbs_charparams(chip, (sbsd_cmd_def[SBSA6_FILTER_SECOND_LOCK] >> SBSD_CMD_Pos) & 0xFF, &param_l1, &param_l2, &param_l3, &param_l4);
    pr_info("SBSA6_FILTER_SECOND_LOCK recall data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
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
        pr_info("SBSA6_FILTER_SECOND_LOCK Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 6 SBSA7_FG_IDLE_SOH
    sd77428_set_sbs_charparams(chip, (sbsd_cmd_def[SBSA7_FG_IDLE_SOH] >> SBSD_CMD_Pos) & 0xFF, 0, SECOND_LOCK_TH, SECOND_LOCK_TIMER, SECOND_UNLOCK_TIMER);
    usleep_range(5000, 10000);
    sd77428_get_sbs_charparams(chip, (sbsd_cmd_def[SBSA7_FG_IDLE_SOH] >> SBSD_CMD_Pos) & 0xFF, &param_l1, &param_l2, &param_l3, &param_l4);
    pr_info("SBSA7_FG_IDLE_SOH recall data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
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
        pr_info("SBSA7_FG_IDLE_SOH Set Success.\r\n");
    }
    usleep_range(5000, 10000);


    //step 7 SBSA8_FG_IDLE_SYNCCURR
    sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSA8_FG_IDLE_SYNCCURR] >> SBSD_CMD_Pos) & 0xFF, FG_idle_delay_curr, FG_idle_wave_curr);
    usleep_range(5000, 10000);
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSA8_FG_IDLE_SYNCCURR] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSA8_FG_IDLE_SYNCCURR recall data: (%d, %d).\r\n", param_high, param_low);
    if (param_high != FG_idle_delay_curr ||
        param_low != FG_idle_wave_curr )
    {
        pr_info("SBSA8_FG_IDLE_SYNCCURR Set Failed.\r\n");
        paramdown_result += (1 << 4);
    }
    else
    {
        pr_info("SBSA8_FG_IDLE_SYNCCURR Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 8 SBSA9_FG_CC_THM_COMP
    sd77428_set_sbs_charparams(chip, (sbsd_cmd_def[SBSA9_FG_CC_THM_COMP] >> SBSD_CMD_Pos) & 0xFF, FG_charge_thm_comp1, FG_charge_thm_comp2, FG_charge_thm_comp3, FG_charge_thm_comp4);
    usleep_range(5000, 10000);
    sd77428_get_sbs_charparams(chip, (sbsd_cmd_def[SBSA9_FG_CC_THM_COMP] >> SBSD_CMD_Pos) & 0xFF, &param_l1, &param_l2, &param_l3, &param_l4);
    pr_info("SBSA9_FG_CC_THM_COMP recall data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
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
        pr_info("SBSA9_FG_CC_THM_COMP Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 9 SBSAA_FG_CC_THM_RANGE
    sd77428_set_sbs_charparams(chip, (sbsd_cmd_def[SBSAA_FG_CC_THM_RANGE] >> SBSD_CMD_Pos) & 0xFF, FG_charge_thm_comp5, FG_charge_thm_range1, FG_charge_thm_range2, FG_charge_thm_range3);
    usleep_range(5000, 10000);
    sd77428_get_sbs_charparams(chip, (sbsd_cmd_def[SBSAA_FG_CC_THM_RANGE] >> SBSD_CMD_Pos) & 0xFF, &param_l1, &param_l2, &param_l3, &param_l4);
    pr_info("SBSAA_FG_CC_THM_RANGE recall data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
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
        pr_info("SBSAA_FG_CC_THM_RANGE Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 10 SBSAB_FG_CC_DFCC_RANGE
    sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSAB_FG_CC_DFCC_RANGE] >> SBSD_CMD_Pos) & 0xFF, FG_charge_maxdfcc, FG_charge_mindfcc);
    usleep_range(5000, 10000);
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSAB_FG_CC_DFCC_RANGE] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSAB_FG_CC_DFCC_RANGE recall data: (%d, %d).\r\n", param_high, param_low);
    if (param_high != FG_charge_maxdfcc ||
        param_low != FG_charge_mindfcc )
    {
        pr_info("SBSAB_FG_CC_DFCC_RANGE Set Failed.\r\n");
        paramdown_result += (1 << 5);
    }
    else
    {
        pr_info("SBSAB_FG_CC_DFCC_RANGE Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 11 SBSAC_FG_CC_CV_CURR
    sd77428_set_sbs_charparams(chip, (sbsd_cmd_def[SBSAC_FG_CC_CV_CURR] >> SBSD_CMD_Pos) & 0xFF, FG_charge_cv_eoctimes, FG_charge_tail_ccratio, FG_charge_thm_curr, FG_charge_fast_curr);
    usleep_range(5000, 10000);
    sd77428_get_sbs_charparams(chip, (sbsd_cmd_def[SBSAC_FG_CC_CV_CURR] >> SBSD_CMD_Pos) & 0xFF, &param_l1, &param_l2, &param_l3, &param_l4);
    pr_info("SBSAC_FG_CC_CV_CURR recall data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
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
        pr_info("SBSAC_FG_CC_CV_CURR Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 12 SBSAD_FG_DC_DFCC_RANGE
    sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSAD_FG_DC_DFCC_RANGE] >> SBSD_CMD_Pos) & 0xFF, FG_dc_max_dfcc, FG_dc_min_dfcc);
    usleep_range(5000, 10000);
    sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSAD_FG_DC_DFCC_RANGE] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    pr_info("SBSAD_FG_DC_DFCC_RANGE recall data: (%d, %d).\r\n", param_high, param_low);
    if (param_high != FG_dc_max_dfcc ||
        param_low != FG_dc_min_dfcc )
    {
        pr_info("SBSAD_FG_DC_DFCC_RANGE Set Failed.\r\n");
        paramdown_result += (1 << 6);
    }
    else
    {
        pr_info("SBSAD_FG_DC_DFCC_RANGE Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step 13 SBSAE_FG_DC_TAIL
    sd77428_set_sbs_charparams(chip, (sbsd_cmd_def[SBSAE_FG_DC_TAIL] >> SBSD_CMD_Pos) & 0xFF, FG_dc_tail_th, FG_dc_soc_des1, FG_dc_soc_des2, FG_dc_tail_cc);
    usleep_range(5000, 10000);
    sd77428_get_sbs_charparams(chip, (sbsd_cmd_def[SBSAE_FG_DC_TAIL] >> SBSD_CMD_Pos) & 0xFF, &param_l1, &param_l2, &param_l3, &param_l4);
    pr_info("SBSAE_FG_DC_TAIL recall data: (%d, %d, %d, %d).\r\n", param_l1, param_l2, param_l3, param_l4);
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
        pr_info("SBSAE_FG_DC_TAIL Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    // //step 14 SBSAF_FG_CC_DFCC_SET
    // sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSAF_FG_CC_DFCC_SET] >> SBSD_CMD_Pos) & 0xFF, FG_CC_DFCC_MODE, FG_CC_hardset_DFCC);
    // usleep_range(5000, 10000);
    // sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSAF_FG_CC_DFCC_SET] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    // pr_info("SBSAF_FG_CC_DFCC_SET recall data: (%d, %d).\r\n", param_high, param_low);
    // if (param_high != FG_CC_DFCC_MODE ||
    //     param_low != FG_CC_hardset_DFCC )
    // {
    //     pr_info("SBSAF_FG_CC_DFCC_SET Set Failed.\r\n");
    //     paramdown_result += (1 << 7);
    // }
    // else
    // {
    //     pr_info("SBSAF_FG_CC_DFCC_SET Set Success.\r\n");
    // }
    // usleep_range(5000, 10000);

    // //step 15 SBSB0_NTC_HOLD_TIME
    // sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSB0_NTC_HOLD_TIME] >> SBSD_CMD_Pos) & 0xFF, ntc_reg_enable, ntc_reg_holdtime);
    // usleep_range(5000, 10000);
    // sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSB0_NTC_HOLD_TIME] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    // pr_info("SBSB0_NTC_HOLD_TIME recall data: (%d, %d).\r\n", param_high, param_low);
    // if (param_high != ntc_reg_enable ||
    //     param_low != ntc_reg_holdtime )
    // {
    //     pr_info("SBSB0_NTC_HOLD_TIME Set Failed.\r\n");
    //     paramdown_result += (1 << 8);
    // }
    // else
    // {
    //     pr_info("SBSB0_NTC_HOLD_TIME Set Success.\r\n");
    // }
    // usleep_range(5000, 10000);

    // //step 16 SBSB1_NTC_PULL_MODE
    // sd77428_set_sbs_shortparams(chip, (sbsd_cmd_def[SBSB1_NTC_PULL_MODE] >> SBSD_CMD_Pos) & 0xFF, NTC_calculate_mode, NTC_pullup_sense);
    // usleep_range(5000, 10000);
    // sd77428_get_sbs_shortparams(chip, (sbsd_cmd_def[SBSB1_NTC_PULL_MODE] >> SBSD_CMD_Pos) & 0xFF, &param_high, &param_low);
    // pr_info("SBSB1_NTC_PULL_MODE recall data: (%d, %d).\r\n", param_high, param_low);
    // if (param_high != NTC_calculate_mode ||
    //     param_low != NTC_pullup_sense )
    // {
    //     pr_info("SBSB1_NTC_PULL_MODE Set Failed.\r\n");
    //     paramdown_result += (1 << 9);
    // }
    // else
    // {
    //     pr_info("SBSB1_NTC_PULL_MODE Set Success.\r\n");
    // }
    // usleep_range(5000, 10000);

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
    pr_info("SBSE2_TABLE_CURR recall: (%d, %d, %d, %d). \r\n", curr_table[0], curr_table[1], curr_table[2], curr_table[3]);

    if (calculate_pec_bytes(0, single_table, table_lens)
        != calculate_pec_bytes(0, (uint8_t*)YAxisElement, table_lens))
    {
        pr_info("SBSE2_TABLE_CURR Set Failed.\r\n");
        paramdown_result += (1 << 0);
    }
    else
    {
        pr_info("SBSE2_TABLE_CURR Set Success.\r\n");
    }
    usleep_range(5000, 10000);
    
    //step2  SBSE3_TABLE_TEMP
    table_lens = ZAxis * 2;
    memcpy(single_table, (uint8_t*)ZAxisElement, table_lens);
    sd77428_set_singletable(chip, (sbsd_cmd_def[SBSE3_TABLE_TEMP] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);
    usleep_range(5000, 10000);
    sd77428_get_singletable(chip, (sbsd_cmd_def[SBSE3_TABLE_TEMP] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);

    memcpy((uint8_t*)temp_table, single_table, table_lens);
    pr_info("SBSE3_TABLE_TEMP recall: (%d, %d, %d, %d). \r\n", temp_table[0], temp_table[1], temp_table[2], temp_table[3]);

    if (calculate_pec_bytes(0, single_table, table_lens)
        != calculate_pec_bytes(0, (uint8_t*)ZAxisElement, table_lens))
    {
        pr_info("SBSE3_TABLE_TEMP Set Failed.\r\n");
        paramdown_result += (1 << 1);
    }
    else
    {
        pr_info("SBSE3_TABLE_TEMP Set Success.\r\n");
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
    uint16_t i = 0;

    short ocv_table[OCV_DATA_NUM];
    short voltage_table[XAxis];

    //step1  SBSE0_TABLE_OCV 130
    packlens = OCV_DATA_NUM * 2;
    memcpy(packtable, (uint8_t*)ocv_volt_data, packlens);
    sd77428_set_packtable(chip, (sbsd_cmd_def[SBSE0_TABLE_OCV] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(5000, 10000);
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE0_TABLE_OCV] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);

    memcpy((uint8_t*)ocv_table, packtable, packlens);
    printk("SBSE0_TABLE_OCV recall: \r\n");
    for (i = 0; i < OCV_DATA_NUM; ++i)
    {
        printk(KERN_CONT "%d ", ocv_table[i]);
    }
    printk("\r\n");

    if (calculate_pec_bytes(0, packtable, packlens)
        != calculate_pec_bytes(0, (uint8_t*)ocv_volt_data, packlens))
    {
        pr_info("SBSE0_TABLE_OCV Set Failed.\r\n");
        paramdown_result += (1 << 0);
    }
    else
    {
        pr_info("SBSE0_TABLE_OCV Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step2  SBSE1_TABLE_VOLT 72
    packlens = XAxis * 2;
    memcpy(packtable, (uint8_t*)XAxisElement, packlens);
    sd77428_set_packtable(chip, (sbsd_cmd_def[SBSE1_TABLE_VOLT] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(5000, 10000);
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE1_TABLE_VOLT] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    
    memcpy((uint8_t*)voltage_table, packtable, packlens);
    printk("SBSE1_TABLE_VOLT recall: \r\n");
    for (i = 0; i < XAxis; ++i)
    {
        printk(KERN_CONT "%d ", voltage_table[i]);
    }
    printk("\r\n");

    if (calculate_pec_bytes(0, packtable, packlens)
        != calculate_pec_bytes(0, (uint8_t*)XAxisElement, packlens))
    {
        pr_info("SBSE1_TABLE_VOLT Set Failed.\r\n");
        paramdown_result += (1 << 1);
    }
    else
    {
        pr_info("SBSE1_TABLE_VOLT Set Success.\r\n");
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
    uint16_t i = 0, j =0;

    //step1  SBSE4_TABLE_RC 576
    packlens = XAxis*YAxis*ZAxis;
    memcpy(packtable, (uint8_t*)RCtable, packlens);
    sd77428_set_packtable(chip, (sbsd_cmd_def[SBSE4_TABLE_RC] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(5000, 10000);
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE4_TABLE_RC] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    
    printk("SBSE4_TABLE_RC recall: \r\n");
    for (i = 0; i < YAxis*ZAxis; ++i)
    {
        for (j = 0; j < XAxis; ++j)
        {
            printk(KERN_CONT "%d ", packtable[i *XAxis +j ]);
        }
        printk("\r\n");
    }
    printk("\r\n");

    if (calculate_pec_bytes(0, packtable, packlens)
        != calculate_pec_bytes(0, (uint8_t*)RCtable, packlens))
    {
        pr_info("SBSE4_TABLE_RC Set Failed.\r\n");
        paramdown_result += (1 << 0);
    }
    else
    {
        pr_info("SBSE4_TABLE_RC Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    return paramdown_result;
}
#if 0
//download NTC-Temperature table 
static uint16_t parameters_down_level5(struct sd77428_data *chip)
{
    uint16_t paramdown_result = 0;

    uint8_t packtable[TEMPERATURE_DATA_NUM*4 + 1];
    uint16_t packlens = 0;
    uint16_t i = 0;
    int32_t temp_table[TEMPERATURE_DATA_NUM];
    
    //step1  SBSE5_TABLE_NTC_TEMP 160
    packlens = TEMPERATURE_DATA_NUM * 4;
    memcpy(packtable, (uint8_t*)cell_temp_data, packlens);
    sd77428_set_packtable(chip, (sbsd_cmd_def[SBSE5_TABLE_NTC_TEMP] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(5000, 10000);
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE5_TABLE_NTC_TEMP] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    
    memcpy((uint8_t*)temp_table, packtable, packlens);
    printk("SBSE5_TABLE_NTC_TEMP recall: \r\n");
    for (i = 0; i < TEMPERATURE_DATA_NUM; ++i)
    {
        printk(KERN_CONT "%d ", temp_table[i]);
    }
    printk("\r\n");

    if (calculate_pec_bytes(0, packtable, packlens)
        != calculate_pec_bytes(0, (uint8_t*)cell_temp_data, packlens))
    {
        pr_info("SBSE5_TABLE_NTC_TEMP Set Failed.\r\n");
        paramdown_result += (1 << 0);
    }
    else
    {
        pr_info("SBSE5_TABLE_NTC_TEMP Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    return paramdown_result;
}
#endif
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
    pr_info("SBSE6_TABLE_DFCC_SOC recall: (%d, %d, %d, %d, %d, %d). \r\n", soc_table[0], soc_table[1], soc_table[2], soc_table[3], soc_table[4],soc_table[5]);

    if (calculate_pec_bytes(0, single_table, table_lens)
        != calculate_pec_bytes(0, (uint8_t*)DFCC_XDATA, table_lens))
    {
        pr_info("SBSE6_TABLE_DFCC_SOC Set Failed.\r\n");
        paramdown_result += (1 << 0);
    }
    else
    {
        pr_info("SBSE6_TABLE_DFCC_SOC Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    //step1  SBSE7_TABLE_DFCC_CURR
    table_lens = DFCC_Y * 2;
    memcpy(single_table, (uint8_t*)DFCC_YDATA, table_lens);
    //pr_info("SBSE7_TABLE_DFCC_CURR %x %x %x %x %x %x \r\n", single_table[0], single_table[1], single_table[2], single_table[3], single_table[4], single_table[5]);
    sd77428_set_singletable(chip, (sbsd_cmd_def[SBSE7_TABLE_DFCC_CURR] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);
    usleep_range(5000, 10000);
    sd77428_get_singletable(chip, (sbsd_cmd_def[SBSE7_TABLE_DFCC_CURR] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);

    memcpy((uint8_t*)curr_table, single_table, table_lens);
    pr_info("SBSE7_TABLE_DFCC_CURR recall: (%d, %d, %d). \r\n", curr_table[0], curr_table[1], curr_table[2]);

    if (calculate_pec_bytes(0, single_table, table_lens)
        != calculate_pec_bytes(0, (uint8_t*)DFCC_YDATA, table_lens))
    {
        pr_info("SBSE7_TABLE_DFCC_CURR Set Failed.\r\n");
        paramdown_result += (1 << 1);
    }
    else
    {
        pr_info("SBSE7_TABLE_DFCC_CURR Set Success.\r\n");
    }
    usleep_range(5000, 10000);
    
    //step3  SBSE8_TABLE_DFCC_TEMP
    table_lens = DFCC_Z * 2;
    memcpy(single_table, (uint8_t*)DFCC_ZDATA, table_lens);
    sd77428_set_singletable(chip, (sbsd_cmd_def[SBSE8_TABLE_DFCC_TEMP] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);
    usleep_range(5000, 10000);
    sd77428_get_singletable(chip, (sbsd_cmd_def[SBSE8_TABLE_DFCC_TEMP] >> SBSD_CMD_Pos) & 0xFF, single_table, table_lens);

    memcpy((uint8_t*)temp_table, single_table, table_lens);
    pr_info("SBSE8_TABLE_DFCC_TEMP recall: (%d, %d, %d, %d). \r\n", temp_table[0], temp_table[1], temp_table[2], temp_table[3]);

    if (calculate_pec_bytes(0, single_table, table_lens)
        != calculate_pec_bytes(0, (uint8_t*)DFCC_ZDATA, table_lens))
    {
        pr_info("SBSE8_TABLE_DFCC_TEMP Set Failed.\r\n");
        paramdown_result += (1 << 1);
    }
    else
    {
        pr_info("SBSE8_TABLE_DFCC_TEMP Set Success.\r\n");
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
    uint16_t i = 0, j =0;

    //step1  SBSE9_TABLE_DFCC_RATIO 72
    packlens = DFCC_X*DFCC_Y*DFCC_Z;
    memcpy(packtable, (uint8_t*)DFCC_table, packlens);
    sd77428_set_packtable(chip, (sbsd_cmd_def[SBSE9_TABLE_DFCC_RATIO] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    usleep_range(5000, 10000);
    sd77428_get_packtable(chip, (sbsd_cmd_def[SBSE9_TABLE_DFCC_RATIO] >> SBSD_CMD_Pos) & 0xFF, packtable, packlens);
    
    printk("SBSE9_TABLE_DFCC_RATIO recall: \r\n");
    for (i = 0; i < DFCC_Y*DFCC_Z; ++i)
    {
        for (j = 0; j < DFCC_X; ++j)
        {
            printk(KERN_CONT "%d ", packtable[i *DFCC_X +j ]);
        }
        printk("\r\n");
    }
    printk("\r\n");

    if (calculate_pec_bytes(0, packtable, packlens)
        != calculate_pec_bytes(0, (uint8_t*)DFCC_table, packlens))
    {
        pr_info("SBSE9_TABLE_DFCC_RATIO Set Failed.\r\n");
        paramdown_result += (1 << 0);
    }
    else
    {
        pr_info("SBSE9_TABLE_DFCC_RATIO Set Success.\r\n");
    }
    usleep_range(5000, 10000);

    return paramdown_result;
}

static uint16_t parameters_down_level8(struct sd77428_data *chip)
{
    uint16_t paramdown_result = 0;
    //uint8_t param_l1 = 0, param_l2 = 0, param_l3=0, param_l4=0; 
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

//-------------------------------------------------------------------------------------------------------------------//

//---------------------------------------------------------驱动计算接口-----------------------------------------------------------//
//参数表下载任务
static void sd77428_params_down_work(struct work_struct *work)
{
    static uint32_t download_count = 0;
    uint16_t paramdown_result = 0;
    uint32_t ret = 0;
    struct sd77428_data *chip = container_of(work, struct sd77428_data, sd77428_download_work.work);
    
    pr_info("Start parameters down work, %d times\n",download_count++);

#if 0 //测试验证读写功能
    uint16_t _test_reg_result = 0;

    _test_reg_result = _test_reg_func(chip);
    if (_test_reg_result)
    {
        pr_info("-----------------1 _test_reg_func failed, code %02x. -----------------\r\n", _test_reg_result);
    }
    else
    {
        pr_info("+++++++++++++++++1 _test_reg_func pass +++++++++++++++++\r\n");
    }
    _test_reg_result = _test_reg_table_func1(chip);
    if (_test_reg_result)
    {
        pr_info("-----------------2 _test_reg_table_func1 failed, code %02x. -----------------\r\n", _test_reg_result);
    }
    else
    {
        pr_info("+++++++++++++++++2 _test_reg_table_func1 pass +++++++++++++++++\r\n");
    }
    _test_reg_result = _test_reg_table_func2(chip);
    if (_test_reg_result)
    {
        pr_info("-----------------3 _test_reg_table_func2 failed, code %02x. -----------------\r\n", _test_reg_result);
    }
    else
    {
        pr_info("+++++++++++++++++3 _test_reg_table_func2 pass +++++++++++++++++\r\n");
    }

    _test_reg_result = _test_reg_table_func3(chip);
    if (_test_reg_result)
    {
        pr_info("-----------------4 _test_reg_table_func3 failed, code %02x. -----------------\r\n", _test_reg_result);
    }
    else
    {
        pr_info("+++++++++++++++++4 _test_reg_table_func3 pass +++++++++++++++++\r\n");
    }
    pr_info("test finished\r\n");
    // return;
#endif

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

    // paramdown_result = parameters_down_level5(chip);
    // ret += paramdown_result;
    // if (paramdown_result)        pr_info("-----------------5 parameters_down_level5 failed, code %02x. -----------------\r\n", paramdown_result);

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
        schedule_delayed_work(&chip->sd77428_download_work, (2*HZ));
    }
    return;

out:
    schedule_delayed_work(&chip->sd77428_work, (2*HZ));
}

static void sd77428_battery_work(struct work_struct *work)
{
    struct sd77428_data *chip = container_of(work, struct sd77428_data, sd77428_work.work);

    if(false == sd77428_is_ok)
        goto out;
    
    sd77428_update_charger_info(chip);

    sd77428_get_batt_info(chip);
    // sd77428_get_dbg_info(chip);
out:
    schedule_delayed_work(&chip->sd77428_work, INIT_DELAY);
}

int32_t sd77428main_suspend(struct device *dev)
{
    struct sd77428_data *chip  = i2c_get_clientdata(to_i2c_client(dev));
    cancel_delayed_work(&chip->sd77428_work);
    return 0;
}

int32_t sd77428main_resume(struct device *dev)
{
    struct sd77428_data *chip = i2c_get_clientdata(to_i2c_client(dev));
    if(true == sd77428_is_ok)
        schedule_delayed_work(&chip->sd77428_work,INIT_DELAY);
    return 0;
}

int32_t sd77428main_remove(struct i2c_client *client)
{
    struct sd77428_data *chip = i2c_get_clientdata(client);
    
    sysfs_remove_group(&(chip->client->dev.kobj), &sd77428_attribute_group);
    power_supply_unregister(chip->bat);
    cancel_delayed_work(&chip->sd77428_work);
    pr_info("sd77428main is remove\n");

    return 0;
}

void sd77428main_shutdown(struct i2c_client *client)
{
    struct sd77428_data *chip = i2c_get_clientdata(client);
    
    sysfs_remove_group(&(chip->client->dev.kobj), &sd77428_attribute_group);
    power_supply_unregister(chip->bat);
    cancel_delayed_work(&chip->sd77428_work);
    pr_info("sd77428main is shutdown\n");
}
//---------------------------------------------------------------------------------------------------------------------//


int32_t sd77428main_ic_init(struct sd77428_data *chip)
{
    int32_t ret = 0;

    ret = sd77428_detect_ic(chip);
    if(ret < 0)
    {
        pr_err("Detect sd77428 ic failed, exit.\n");
        return -ENODEV;
    }
    
    //init the hardware adc register config
    sd77428_init_batt_info(chip);

    INIT_DELAYED_WORK(&chip->sd77428_work, sd77428_battery_work);
    schedule_delayed_work(&chip->sd77428_work, (15*HZ));   //60                   //linux的队列任务管理，共享工作队列

    if (parameters_down_enable)
    {
        INIT_DELAYED_WORK(&chip->sd77428_download_work, sd77428_params_down_work);
        schedule_delayed_work(&chip->sd77428_download_work, (2*HZ));             //linux的队列任务管理，共享工作队列
    }    

    sd77428_power_supply_init(chip);

    ret = sysfs_create_group(&(chip->dev->kobj), &sd77428_attribute_group);
    if (ret < 0) 
        pr_err("create sysfs failed %d\n",ret);
    
    g_chip_data = chip;
    return 0;
}