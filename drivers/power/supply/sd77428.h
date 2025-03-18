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
#define BATT_DESIGN_CAPACITY		4900
#define BATT_DESIGN_FCC				BATT_DESIGN_CAPACITY
#define BATT_DESIGN_CV				4450
#define BATT_DESIGN_EOC				180
#define BATT_DESIGN_EOD				3200
#define SOFT_RESET		    		0x2301
#define BATT_DESIGN_NTC_SRCTAB		0x0102
#define BATT_DESIGN_CADC_OFFSET		0x0
#define BATT_DESIGN_RSENSE          5000


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
	
    SBSB0_NTC_HOLD_TIME,                //4 BYTE, short+short, [NTC reg enbale, NTC I2C hold time]
	SBSB1_NTC_PULL_MODE,		        //4 BYTE, short+short, [NTC_calculate_mode, NTC_delay_ms]
    SBSB2_NTC_PULL_VOLT,                //4 BYTE, short+short, [NTC_pullup_sense, NTC_pullup_refvolt]

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


unsigned int sbsd_cmd_def[SBSD_CMD_MAX] = {
    (0x00UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x00000234UL, /* CONTROL */
	(0x02UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x02000235UL, /* Temperature */
	(0x04UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x04000215UL, /* Voltage */
	(0x06UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x06000215UL, /* Flags */
	// (0x08UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x08000215UL, /* NominalAvailableCapacity */
	// (0x0AUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x0A000215UL, /* FullAvailableCapacity */
	(0x0CUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x0C000215UL, /* RemainingCapacity */
	(0x0EUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x0E000215UL, /* FullChargeCapacity */
	(0x10UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_SH),		//0x10000215UL, /* AverageCurrent */
	// (0x18UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x18000215UL, /* AveragePower */
	(0x1CUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x1C000215UL, /* StateOfCharge */
	(0x1EUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x1E000214UL, /* InternalTemperature */
	(0x20UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x20000215UL, /* StateOfHealth */
	// (0x28UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x28000215UL, /* RemainingCapacityUnfiltered */
	// (0x2AUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x2A000215UL, /* RemainingCapacityFiltered */
	// (0x2CUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x2C000214UL, /* FullChargeCapacityUnfiltered */
	// (0x2EUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x2E000214UL, /* FullChargeCapacityFiltered */
	// (0x30UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x30000214UL, /* StateOfChargeUnfiltered */

	(0x60UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x60000214UL, /* CC LSB */
	(0x61UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x61000214UL, /* CADC dither enable */

	// (0x62UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x62000214UL, /* Run time to empty */
	// (0x63UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x63000214UL, /* Average time to empty */
	// (0x64UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x64000214UL, /* Average time to full */
	(0x65UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x65000214UL, /* Cycle count, */	
	(0x66UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x66000215UL, /* Discharge Capacity */
	(0x67UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x67000215UL, /* Charge Capacity */
	(0x68UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x68000215UL, /* Fully charge end current */
	(0x69UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x69000215UL, /* Fully charge end timeout */
	(0x6AUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x6a000215UL, /* Discharge end voltage */
	(0x6BUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x6b000215UL, /* Enable seal via sha256 */
	(0x6CUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x6c000215UL, /* Charge threshold */
	(0x6DUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_SH),		//0x6d000215UL, /* DisCharge threshold */
	(0x6EUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x6e000215UL, /* Fast wkup threshold */
	(0x6FUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x6F000214UL, /* CADC DB threshold */
	(0x70UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x70000214UL, /* CADC wakeup threshold */
	(0x71UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x71000215UL, /* Rsence*/
	(0x72UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x72000215UL, /* Debug mode*/
	(0x73UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x73000215UL, /* Idle to sleep time */
	(0x74UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x74000215UL, /* Sleep time */
	(0x75UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x75000215UL, /* Deep Sleep time  */
	(0x76UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x76000215UL, /* Power of sleep */
	(0x77UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x77000215UL, /* Power of deep sleep */
	
	(0x78UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x78000215UL, /* Charge current slope*/
	(0x79UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x79000215UL, /* Charge current offset */
	(0x7AUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x7A000215UL, /* Discharge current slope */
	(0x7BUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x7B000215UL, /* Discharge current offset  */
	(0x7CUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x7C000215UL, /* Voltage offset */
	(0x7DUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x7D000215UL, /* External temperature offset */	

	(0x7EUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x7E000215UL, /* SOC1 SET THRESHOLD*/
	(0x7FUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x7F000215UL, /* SOC1 CLEAR THRESHOLD */
	(0x80UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x80000215UL, /* SOCF SET THRESHOLD*/
	(0x81UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x81000215UL, /* SOCF CLEAR THRESHOLD */
	(0x82UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x82000215UL, /* SOC1 DELTA */
	(0x83UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x83000215UL, /* OPCONFIG */	
	(0x84UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),		//0x84000215UL, /* Historical CC */		
	(0x85UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x85000215UL, /* Change RCtable */		
	
	(0x86UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x86000215UL, /* Charge IR */	
	(0x87UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_SH),		//0x87000215UL, /* LOWTEMP */	
	(0x88UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x88000234UL, /* INITSOC */
	(0x89UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x89000234UL, /* SYSIM */
	(0x8AUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x8A000234UL, /* EOC CDV */
	(0x8BUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x8B000234UL, /* EOD DDV */
	(0x8CUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),	//0x8C000215UL, /* Temperature table index */

	(0x8DUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_SH),		//0x8D000215UL, /* Cadc zero offset */	
	(0x8FUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x8F000215UL, /* SBS Send finished */	
	(0x90UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x90000234UL, /* Coulomb Counting, */
	(0x91UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x91000234UL, /* External charger status */
	
	(0x92UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x92000234UL, /* Dynamic Remaining Capacity */
	(0x93UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x93000234UL, /* Dynamic FCC */
	(0x94UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x94000234UL, /* Debug ratio */
	(0x95UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x95000234UL, /* Debug CC PREV */
	(0x96UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x96000234UL, /* Debug SOC NOW */
	(0x97UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0x97000234UL, /* Debug SOC END */
	(0x98UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_SH),		//0x97000234UL, /* Debug AVERAGE CURRENT */
	// cablirateion-
	(0xA0UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),			/* SET chg/dis thresh  */
	(0xA1UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),			/* SET chg/dis thresh  */
	(0xA2UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
	// filter
	(0xA3UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),			/* SET chg/dis thresh  */
	(0xA4UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),			/* SET chg/dis thresh  */
	(0xA5UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
	(0xA6UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
	// libfg
	(0xA7UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
	(0xA8UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
	(0xA9UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
	(0xAAUL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
	(0xABUL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
	(0xACUL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
	(0xADUL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
	(0xAEUL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
	(0xAFUL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),

	(0xB0UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
	(0xB1UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),
	(0xB2UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),

	(0xD0UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),	//0xD0002018UL, /* Debug message */
	(0xD1UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),	//0xD1002018UL, /* Debug message */
	(0xD2UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),	//0xD2002018UL, /* Debug message */
	(0xD3UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),	//0xD3002018UL, /* Debug message */
	// table
	(0xDFUL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0xDF000214UL, /* Table msg package index */
	(0xE0UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),	//0xE0002018UL, /* SBSE0_TABLE_OCV */
	(0xE1UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),	//0xE1002018UL, /* SBSE1_TABLE_VOLT */
	(0xE2UL << SBSD_CMD_Pos) | (8UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),		//0xE2002018UL, /* SBSE2_TABLE_CURR */
	(0xE3UL << SBSD_CMD_Pos) | (8UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),		//0xE3002018UL, /* SBSE3_TABLE_TEMP */
	(0xE4UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),	//0xE4002018UL, /* SBSE4_TABLE_RC */
	(0xE5UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),	//0xE4002018UL, /* SBSE5_TABLE_NTC_TEMP */
	
	(0xE6UL << SBSD_CMD_Pos) | (12UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),	//0xE2002018UL, /* SBSE6_TABLE_DFCC_SOC */
	(0xE7UL << SBSD_CMD_Pos) | (6UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),		//0xE3002018UL, /* SBSE7_TABLE_DFCC_CURR */
	(0xE8UL << SBSD_CMD_Pos) | (8UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),		//0xE3002018UL, /* SBSE8_TABLE_DFCC_TEMP */
	(0xE9UL << SBSD_CMD_Pos) | (32UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_AUTH),	//0xE4002018UL, /* SBSE9_TABLE_DFCC_RATIO */

	(0xF1UL << SBSD_CMD_Pos) | (2UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WD),		//0xF1000217UL, /* Chip version */
	(0xF2UL << SBSD_CMD_Pos) | (4UL << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RD << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_WWD),		//0xF2000417UL, /* Firmware version */
	(0xF9UL << SBSD_CMD_Pos) | (SBSF9_WR_LENGTH << SBSD_LEN_Pos) | (SBSD_PRO_DIR_RW << SBSD_PRO_DIR_Pos) | (SBSD_PRO_TYP_PRM),		//0xF9000336UL, /* Parameter Read Write */
	//user defined SBS command
};


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

signed int param_board_cfg[(PARM_BCFG_MAX)] =
{
    //Board Configuration
	BATT_DESIGN_FCC,                   //0x0D48, PARM_BCFG_DESIGNCAPACITY, 3400mAhr
	BATT_DESIGN_CV,                   //0x0E74, PARM_BCFG_LMCHG_VOLTAGE, 4200mV
	BATT_DESIGN_EOC,                    //Fully charge end current 200mA
	30,                     //Fully charge end timeout  30 sec
	BATT_DESIGN_EOD,                   //Discharge end voltage 3400
	0,					    //256 这里去掉其他地方也需要同步调整
	10,                     // Charge threshold    这里都是模拟量，lsb是4.39453125uV， 需要根据电阻来转化，比如10表示10mA，如果是1毫欧电阻，则表示4.39453125 * 2 /1 mA   ( mV/欧姆 = mA)
	10,                     // DisCharge threshold 模拟量
	7500,                   // fast wkup threshold  模拟量 放大了10 倍
	10,                     // CADC DB threshold  模拟量
	300,                     // CADC wakeup threshold  模拟量 放大了10 倍
	BATT_DESIGN_RSENSE,  //0x09C4, PARM_BCFG_RSENSE, 2500 => 2.5mOhm
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
	0,					   //External temperature offset
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
	80,						// EOC_CDV
	0,					  // EOD_DDV
	0,//BATT_DESIGN_NTC_SRCTAB,// Temp table indx
	0,                     // cell num
};


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

typedef struct batt_data {
	uint16_t 	batt_rc;   		//Relative State Of Charged, present percentage of battery capacity
	uint16_t 	batt_rsoc;  	//Relative State Of Charged, present percentage of battery capacity
	uint16_t 	batt_soh;   	//0~100， SOH
	uint16_t 	batt_cyclecnt;  //cycle cnt
	uint16_t 	batt_voltage;	//Voltage of battery, in mV
	int16_t 	batt_current;	//Current of battery, in mA; plus value means charging, minus value means discharging
	int16_t 	batt_temp;		//Temperature of battery
	uint16_t 	batt_capacity;	//adjusted residual capacity
	uint16_t 	batt_fcc;
	
	int16_t    ext_charger;
	
	// int16_t 	discharge_current_th;
	// uint8_t 	charge_end;
}batt_data_t;

struct sd77428_data 
{
	struct i2c_client *client;
	struct device*	dev;
	struct mutex i2c_rw_lock;
    struct delayed_work work;
    struct delayed_work download_work;
	struct delayed_work goldfinger_work;
	struct power_supply *bat;
	struct power_supply_desc bat_desc;
	struct power_supply_config bat_cfg;
	struct power_supply *ac_psy;
	struct power_supply *usb_psy;
	struct notifier_block pm_nb;

	uint32_t interval;
	uint8_t adapter_status;
	bool chg_full;
    uint8_t err_times;
	batt_data_t batt_info;
	const char *name;
};

int32_t sd77428_get_soc(void);
int32_t sd77428_get_battery_voltage(void);
int32_t sd77428_get_battry_current(void);
int32_t sd77428_get_battery_temp(void);

#endif
