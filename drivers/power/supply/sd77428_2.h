/*****************************************************************************
* Copyright(c) BMT, 2021. All rights reserved.
*       
* ("Reference Design") is solely for the use of PRODUCT INTEGRATION REFERENCE ONLY, 
* and contains confidential and privileged information of BMT International 
* Limited. BMT shall have no liability to any PARTY FOR THE RELIABILITY, 
* SERVICEABILITY FOR THE RESULT OF PRODUCT INTEGRATION, or results from: (i) any 
* modification or attempted modification of the Reference Design by any party, or 
* (ii) the combination, operation or use of the Reference Design with non-BMT 
* Reference Design. Use of the Reference Design is at user's discretion to qualify 
* the final work result.
*****************************************************************************/
#ifndef BMT_SD77561_H_H
#define BMT_SD77561_H_H
//user define parameters, basic on the battery type
#define HARD_DEVICE_REG             0x18                        //硬件I2C寄存器接口
#define SD77428_I2C_ADDR            0x55                        //7bits,8bits-0x50

#define OCV_DATA_NUM            65
#define XAxis                   36
#define YAxis                   4
#define ZAxis                   4
#define TEMPERATURE_DATA_NUM    40

#define DFCC_X  6
#define DFCC_Y  3  
#define DFCC_Z  4  


#define INIT_DELAY         ((HZ)*1)//((HZ)*1)   ((HZ)*10)
#define PEC_ENABLE	       1

#define I2C_PEC_POLY       0x07
#define CHIP_VERSION	   0xF1
#define SD77428_BLOCK_OP   0x0F


#define  SBSD_CMD_Pos      (24U)        //SBSD CMD bits start
#define  SBSD_CMD_Msk      (0xFF000000UL)    //SBSD CMD Mask
#define  SBSD_LEN_Pos      (8U)         //SBSD Data Length bits start
#define  SBSD_LEN_Msk      (0x0000FF00UL)    //SBSD Data Length Mask
#define  SBSD_PRO_DIR_Pos  (4UL)        //SBSD Protocol Direction bits start
#define  SBSD_PRO_DIR_Msk  (0xF0UL)     //SBSD Protocol Direction Mask
#define  SBSD_PRO_TYP_Pos  (0UL)        //SBSD Protocol Data Type bits start
#define  SBSD_PRO_TYP_Msk  (0x0FUL)     //SBSD Protocol Data Type Mask

#define  SBSD_PRO_DIR_RD   (0x1UL)      //SBSD Protocol Direction: Read
#define  SBSD_PRO_DIR_WD   (0x2UL)      //SBSD Protocol Direction: Write
#define  SBSD_PRO_DIR_RW   (0x3UL)      //SBSD Protocol Direction: Read-Write


#define  SBSD_PRO_TYP_WD   (0x02UL)     //SBSD Protocol Data Type: WORD (Unsigned 16Bit)
#define  SBSD_PRO_TYP_SH   (0x03UL)     //SBSD Protocol Data Type: SHORT (Signed 16Bits)
#define  SBSD_PRO_TYP_WWD  (0x04UL)     //SBSD Protocol Data Type: WWORD (Unsigned 32Bits)
#define  SBSD_PRO_TYP_SWD  (0x05UL)     //SBSD Protocol Data Type: SWORD (Signed 32Bits)
#define  SBSD_PRO_TYP_PRM  (0x06UL)     //SBSD Protocol Data Type: PARAMETER (max 31Bytes)
#define  SBSD_PRO_TYP_STR  (0x07UL)     //SBSD Protocol Data Type: String (Max 31Bytes)
#define  SBSD_PRO_TYP_AUTH (0x08UL)     //SBSD Protocol Data Type: Authentication (max 33Bytes)

#define  SBSF9_WR_LENGTH   (0x02UL)     //b[0]: length = 1; b[1]: sub-command#define SBSF9_WR_LENGTH    (0x02UL)     //b[0]: length = 1; b[1]: sub-command

enum charger_type_t {
	O2_CHARGER_BATTERY = 0,
	O2_CHARGER_USB,
	O2_CHARGER_AC,
};

typedef  enum
{
    SBS00_CONTROL,         //0, I
    SBS02_EXTTMP,          //1, V
    SBS04_BATTVOLT,        //2, V
    SBS06_FLAGS,           //3, V
    // SBS08_NOMAVACAP,       //4, V
    // SBS0A_FULLAVACAP,      //5, V
    SBS0C_RC,              //6, V
    SBS0E_FCC,             //7, V
    SBS10_BATTCURR,        //8, V
    // SBS18_AVGPOWER,        //9, V
    SBS1C_RSOC,            //10, V
    SBS1E_INTRTMP,         //11, V
    SBS20_SOH,             //12
    // SBS28_UNFTRC,          //13
    // SBS2A_FTRC,            //14
    // SBS2C_UNFTFCC,         //15
    // SBS2E_FTFCC,           //16
    // SBS30_UNFTSOC,         //17

	SBS60_CC_LSB,           //16
	SBS61_CADC_DITHER,      //17
    // SBS62_RTTE,            //18, V
    // SBS63_ATTE,            //19, V
    // SBS64_ATTF,            //20, V
    SBS65_CYCLECNT,        //21

    SBS66_DSNCAP,          //22, I
    SBS67_DSNVOLT,         //23, I
    SBS68_FCHGENDCURR,     //24, I
    SBS69_FCHGENDTMOUT,    //25, I
    SBS6A_DSGENDVOLT,      //26, I
    SBS6B_SEALSHA256,      //27, I
    SBS6C_CHGTH,           //28, I
    SBS6D_DSGTH,           //29, I
    SBS6E_FASTWKUPTH,      //30, I
    SBS6F_CADCDBTH,        //31, I
    SBS70_CADCWKUPTH,      //32, I

    SBS71_RENSE,           //33, I
    SBS72_DEBUGMD,         //34, I
    SBS73_IDLE2SLPTM,      //35, I
    SBS74_SLPTM,           //36, I
    SBS75_DPSLPTM,         //37, I
    SBS76_POWEROFSLP,      //38, I
    SBS77_POWEROFDSLP,     //39, I

    SBS78_CHGCURRSLP,      //40, I
    SBS79_CHGCURROFFSET,   //41, I
    SBS7A_DSGCURRSLP,      //42, I
    SBS7B_DSGCURROFFSET,   //43, I
    SBS7C_VOLTOFFSET,      //44, I
    SBS7D_EXTTMPOFFSET,    //45, I

    SBS7E_SOC1SETTH,       //46
    SBS7F_SOC1CLRTH,       //47
    SBS80_SOCFSETTH,       //48
    SBS81_SOCFCLRTH,       //49
    SBS82_SOCIDELTA,       //50
    SBS83_OPCONFIG,        //51
    SBS84_HISTORICALCC,     //51
	SBS85_CHANGERCTABLE,     //51

	SBS86_CHARGEIR,       //49
	SBS87_LOWTEMP,       //50
	SBS88_INITSOC,        //51
	SBS89_SYSIM,     //51
	SBS8A_EOC_CDV,    //51
	SBS8B_EOD_DDV,    //51
	SBS8C_TEMPTABINDX,    //51
	SBS8D_CADCZEROOFFSET,   //51
	SBS8F_SBSSENDFINISHED,  //51
    SBS90_CC,              //52
    SBS91_EXTCHGSTS,       //53

    SBS92_RCA,             //54
    SBS93_DFCC,       		 //55
    SBS94_DRATIO,          //56
    SBS95_CC_PRV,          //57
    SBS96_SOC_NOW,         //58
    SBS97_SOC_END,         //59
    SBS98_AVGCURR,         //59

	SBSA0_CONFIGDSG_CHG,               //4 BYTE, short+short, [gdm_thresh_chg, gdm_thresh_dsg]
	SBSA1_CALIBRATION_TIME,            //4 BYTE, [XX, XX, XX, calib_sample]
	SBSA2_CALIBRATION_VAL,
	
    SBSA3_FILTER_WIND_LOC,				//4 BYTE, [MID_WINDOWS, MEAN_WINDOWS, 1ST_LOCK_TH, NOISE_RANGE_GATE]
    SBSA4_FILTER_RATIO,                 //4 BYTE, short+short, [INIT_DELAY_RATIO + NOISE_RATIO]
    SBSA5_FILTER_ENABLE,                //4 BYTE, [SECOND_LOCK_FLAG, filter_switcher, calib_switcher, filter_offset]
    SBSA6_FILTER_SECOND_LOCK,           //4 BYTE, [XX, SECOND_LOCK_TH, SECOND_LOCK_TIMER, SECOND_UNLOCK_TIMER]
	
    SBSA7_FG_IDLE_SOH,			         //4 BYTE, [_IDLE_CHARGE_EST_, FG_idle_60s_enable, FG_cc_soh_ratio, FG_dc_soh_ratio]
    SBSA8_FG_IDLE_SYNCCURR,              //4 BYTE, short+short, [FG_idle_delay_curr, FG_idle_wave_curr]
    SBSA9_FG_CC_THM_COMP,               //4 BYTE, [FG_charge_thm_comp1, FG_charge_thm_comp2, FG_charge_thm_comp3, FG_charge_thm_comp4]
    SBSAA_FG_CC_THM_RANGE,			     //4 BYTE, [FG_charge_thm_comp5, FG_charge_thm_range1, FG_charge_thm_range2, FG_charge_thm_range3]
    SBSAB_FG_CC_DFCC_RANGE,              //4 BYTE, short+short, [FG_charge_maxdfcc, FG_charge_mindfcc]
    SBSAC_FG_CC_CV_CURR,                //4 BYTE, [FG_charge_cv_eoctimes, FG_charge_tail_ccratio, FG_charge_thm_curr, FG_charge_fast_curr]
    SBSAD_FG_DC_DFCC_RANGE,              //4 BYTE, short+short, [FG_dc_max_dfcc, FG_dc_min_dfcc]
    SBSAE_FG_DC_TAIL,                   //4 BYTE, [FG_dc_tail_th, FG_dc_soc_des1, FG_dc_soc_des2, FG_dc_tail_cc]
    SBSAF_FG_CC_DFCC_SET,               //4 BYTE,  short+short, [FG_CC_DFCC_MODE, FG_CC_hardset_DFCC]
    
    // SBSA8_FG_dc_hardset_fcc,         //4 BYTE, short+short, [FG_dc_hardset_fcc, FG_mdc_hardset_fcc]
    // SBSA9_FG_dfcc_on_socrange,           //4 BYTE, short+short, [FG_dfcc_onsoc_min, FG_dfcc_onsoc_max]
    // SBSAA_FG_dfcc_compensate_th,     //4 BYTE, short+short, [FG_dfcc_compensate_th1, FG_dfcc_compensate_th2]
    // SBSAB_FG_dfcc_compensate_ratio,     //4 BYTE, short+short, [FG_dfcc_compensate_ratio1, FG_dfcc_compensate_ratio2]

    SBSB0_NTC_HOLD_TIME,                //4 BYTE, short+short, [NTC reg enbale, NTC I2C hold time]
    SBSB1_NTC_PULL_MODE,                //4 BYTE, short+short, [NTC_calculate_mode, NTC_pullup_sense]

    SBSD0_GGMEM0,          //60
    SBSD1_GGMEM1, 				//61
    SBSD2_GGMEM2,					//62
    SBSD3_GGMEM3,					//63

	SBSDF_TABLE_INDEX,		//set current frame read index
	SBSE0_TABLE_OCV,					
	SBSE1_TABLE_VOLT,					
	SBSE2_TABLE_CURR,			
	SBSE3_TABLE_TEMP,			
	SBSE4_TABLE_RC,	
    SBSE5_TABLE_NTC_TEMP,           //NTC tempture table                
    SBSE6_TABLE_DFCC_SOC,                          
    SBSE7_TABLE_DFCC_CURR,                          
    SBSE8_TABLE_DFCC_TEMP,                          
    SBSE9_TABLE_DFCC_RATIO,                          
	
    SBSF1_CHIPVER,         //64
    SBSF2_FWVER,           //65
    SBSF9_SPECIAL,         //66
    SBSD_CMD_MAX           //99
} SBS_DATA_T;

typedef  enum {
  PARM_BCFG_DESIGNCAPACITY, 	//=0
  PARM_BCFG_LMCHG_VOLTAGE,  	//=1
  PARM_BCFG_FULLCHGENDCURR,  	//=2
  PARM_BCFG_FULLCHGENDTMOUT,  //=3
  PARM_BCFG_CUTDSG_VOLTAGE,  	//=4
  PARM_BCFG_ENABLSHA256,  		//=5	
  PARM_BCFG_CHGTH,  					//=6	
  PARM_BCFG_DSHTH,  					//=7
  PARM_BCFG_FSATWKUPTH,  					//=8
  PARM_BCFG_CADCDBTH,  				//=9
  PARM_BCFG_CADCWKUPTH,  			//=10
  PARM_BCFG_RSENSE,         	//=11
  PARM_BCFG_DEBUGMODE,        //=12	
  PARM_BCFG_IDLETOSLEEP,      //=13
  PARM_BCFG_SLEEPTIME,      	//=14
  PARM_BCFG_DEEPSLPTIME,    	//=15
  PARM_BCFG_PWRSLEEP,       	//=16
  PARM_BCFG_PWRDEEPSLP,     	//=17
  PARM_BCFG_CHGSLOPE,       	//=18
  PARM_BCFG_CHGOFFSET,      	//=19
  PARM_BCFG_DSGSLOPE,       	//=20
  PARM_BCFG_DSGOFFSET,      	//=21
  PARM_BCFG_VOLTOFFSET,     	//=22
  PARM_BCFG_EXTTHMOFFSET,   	//=23

  PARM_BCFG_SOC1SETTH,        //=24
  PARM_BCFG_SOC1CLRTH,        //=25
  PARM_BCFG_SOCFSETTH,        //=26
  PARM_BCFG_SOCFCLRTH,        //=27
  PARM_BCFG_SOC1DELTA,        //=28
  PARM_BCFG_OPCONFIG,         //=29
  PARM_BCFG_HISTORICALCC,     //=30	
  PARM_BCFG_RCTABLEINDX,      //=31	
	
  PARM_BCFG_CHARGEIR,         //=32	
  PARM_BCFG_LOWTEMP,          //=33

  PARM_BCFG_INITSOC,          //=34	
  PARM_BCFG_SYSIM,            //=35	
	
  PARM_BCFG_EOCCDV,           //=36	
  PARM_BCFG_EODDDV,           //=37	
  PARM_BCFG_TEMPTABINDX,      //=38	
  PARM_BCFG_CELLNUM,          //=39
  PARM_BCFG_MAX,              //=40
} PARM_TYPE_BCFG_T;

extern unsigned int sbsd_cmd_def[SBSD_CMD_MAX];

/* cmd 0x81*/
enum charger_status{
	/*it indicates that Fuel Gauge library will not consider External Charger status. This value is also the initial value when power-on
	The host has no idea about charger status*/
	charger_unknown = 0,	

	/*Fuel Gauge library will consider charger is disconnected
	The host detected charger is disconnected, write -1 to SBSx81*/		
	charger_disconnected = -1,	

	/*Fuel Gauge library will consider charger is connected during charging phase
	The host detected charger is connected but is not charged to full, write 1 to SBSx81*/
	charger_connected 	= 1,	
	
	/*The host detected charger is connected and charger reports cell is fully charged, write 0x03 to SBSx81
	Fuel Gauge library will consider cell is fully charged and make RSOC approach 100%*/
	charger_full	= 3,
};

#endif
