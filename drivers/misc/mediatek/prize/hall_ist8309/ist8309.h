#ifndef __IST8309_H__
#define __IST8309_H__


/* ********************************************************* */
/* register map */
/* ********************************************************* */
#define IST8309_REG_WAI (0x00)
#define IST8309_VAL_WAI (0x89)
							   /*
	*[7:0]	 WAI		 : Device ID 
	*/
/* --------------------------------------------------------- */
// /* --------------------------------------------------------- */
#define IST8309_REG_DERN (0x04)
#define IST8309_VAL_DERN_MAG_SWITCH_TO_INTB (0x00)
#define IST8309_VAL_DERN_DRDY_TO_INTB (0x80)
/*
	*[7]	 DRDY output through INTB pin.		
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_ANGLE_OPTION (0x05)
#define IST8309_VAL_ANGLE_OPTION_ENABLE_Z (0x00)
#define IST8309_VAL_ANGLE_OPTION_ENABLE_Y (0x40)
#define IST8309_VAL_ANGLE_OPTION_DISABLE (0x80)
#define IST8309_VAL_ANGLE_OPTION_ENABLE_X (0xC0)
/*
	*[7:6]	 Select XY/XZ/YZ to Calculate Angle Value		
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_SRST (0x07)
#define IST8309_VAL_SRST_RESET (0x01)
/*
	*[0]	Software Reset	
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_CNTL1 (0x08)
#define IST8309_VAL_CNTL1_ODR_STANDBY (0x00)
#define IST8309_VAL_CNTL1_ODR_10HZ (0x10)
#define IST8309_VAL_CNTL1_ODR_6_7HZ (0x20)
#define IST8309_VAL_CNTL1_ODR_5HZ (0x30)
#define IST8309_VAL_CNTL1_ODR_80HZ (0x40)
#define IST8309_VAL_CNTL1_ODR_40HZ (0x50)
#define IST8309_VAL_CNTL1_ODR_26_7HZ (0x60)
#define IST8309_VAL_CNTL1_ODR_20HZ (0x70)
#define IST8309_VAL_CNTL1_ODR_SINGLE_MODE (0x80)
#define IST8309_VAL_CNTL1_ODR_100HZ (0x90)
#define IST8309_VAL_CNTL1_ODR_50HZ (0xA0)
#define IST8309_VAL_CNTL1_ODR_1HZ (0xB0)
#define IST8309_VAL_CNTL1_ODR_200HZ (0xC0)
#define IST8309_VAL_CNTL1_ODR_250HZ (0xD0)
#define IST8309_VAL_CNTL1_ODR_320HZ (0xE0)
#define IST8309_VAL_CNTL1_ODR_500HZ (0xF0)
/*
	*[7:4]	Operation mode setting	
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_ACTR (0x0B)
#define IST8309_VAL_ACTR_SUSPEND_DISABLE (0x00)
#define IST8309_VAL_ACTR_SUSPEND_ENABLE (0x02)
/*
	*[1]	Suspend Mode.	
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_ANGLE_ZERO_L (0x0C)
/*
	*[7:0]	Low Byte of zero angle
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_ANGLE_ZERO_H (0x0D)
/*
	*[7:0]	High Byte of zero angle	
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_STAT1 (0x10)
#define IST8309_VAL_STAT1_DRDY (0x01)
/*
	*[7]	INTZ: =0, Z Sensor Interrupt Occur	
	*[6]	INTZ: =0, Y Sensor Interrupt Occur	
	*[5]	INTZ: =0, X Sensor Interrupt Occur	
	*[2]	DORZ: Turns to 1 when data has been skipped.	
	*[0]	DRDY: Sensor data ready register	
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_DATAXL (0x11)
/*
	*[7:0]	Low Byte of X-axis data	
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_DATAXH (0x12)
/*
	*[7:0]	High Byte of X-axis data	
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_DATAYL (0x13)
/*
	*[7:0]	Low Byte of Y-axis data	
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_DATAYH (0x14)
/*
	*[7:0]	High Byte of Y-axis data	
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_DATAZL (0x15)
/*
	*[7:0]	Low Byte of Z-axis data	
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_DATAZH (0x16)
/*
	*[7:0]	Low Byte of Z-axis data
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_DATAAL (0x19)
/*
	*[7:0]	Low Byte of angle data	
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_DATAAH (0x1A)
/*
	*[7:0]	High Byte of angle data	
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_INTCNTL1 (0x20)
#define IST8309_VAL_INTCNTL1_PERS_COUNT(n) (n << 4)
#define IST8309_VAL_INTCNTL1_PERS_NOT_USED	(0x00)
// #define IST8309_VAL_INTCNTL1_PERS_1			(0x10)
// #define IST8309_VAL_INTCNTL1_PERS_2			(0x20)
// #define IST8309_VAL_INTCNTL1_PERS_3			(0x30)
#define IST8309_VAL_INTCNTL1_PERS_4			(0x40)
// #define IST8309_VAL_INTCNTL1_PERS_5			(0x50)
// #define IST8309_VAL_INTCNTL1_PERS_6			(0x60)
// #define IST8309_VAL_INTCNTL1_PERS_7			(0x70)
// #define IST8309_VAL_INTCNTL1_PERS_8			(0x80)
// #define IST8309_VAL_INTCNTL1_PERS_9			(0x90)
// #define IST8309_VAL_INTCNTL1_PERS_10		(0xA0)
// #define IST8309_VAL_INTCNTL1_PERS_11		(0xB0)
// #define IST8309_VAL_INTCNTL1_PERS_12		(0xC0)
// #define IST8309_VAL_INTCNTL1_PERS_13		(0xD0)
// #define IST8309_VAL_INTCNTL1_PERS_14		(0xE0)
// #define IST8309_VAL_INTCNTL1_PERS_15		(0xF0)
#define IST8309_VAL_INTCNTL1_INTCLR_CLEAN_INT (0x01)
#define IST8309_VAL_INTCNTL1_INTCLR_ENABLE_INT (0x00)
/*
	*[7:4]	Interrupt PAD (INTB) transits from high to low level after consecutive.	
	*[0]	Set 1 to Clear Interrupt. Must set 0 first before enable interrupt function.
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_INTCNTL2 (0x21)
#define IST8309_VAL_INTCNTL2_INTONZ_INT_MODE_OFF (0x00)
#define IST8309_VAL_INTCNTL2_INTONZ_INT_MODE_ON (0x80)
#define IST8309_VAL_INTCNTL2_CLRTYP_CLEAR_INTCLR (0x00)
#define IST8309_VAL_INTCNTL2_CLRTYP_CLEAR_AUTO (0x40)
#define IST8309_VAL_INTCNTL2_INTTYP_BESIDE (0x00)
#define IST8309_VAL_INTCNTL2_INTTYP_WITHIN (0x10)
#define IST8309_VAL_INTCNTL2_INTONA_INT_MODE_OFF (0x00)
#define IST8309_VAL_INTCNTL2_INTONA_INT_MODE_ON (0x04)
#define IST8309_VAL_INTCNTL2_INTONY_INT_MODE_OFF (0x00)
#define IST8309_VAL_INTCNTL2_INTONY_INT_MODE_ON (0x02)
#define IST8309_VAL_INTCNTL2_INTONX_INT_MODE_OFF (0x00)
#define IST8309_VAL_INTCNTL2_INTONX_INT_MODE_ON (0x01)
#define IST8309_DETECTION_MODE_POLLING 	(IST8309_VAL_INTCNTL2_INTONZ_INT_MODE_OFF | \
										 IST8309_VAL_INTCNTL2_INTONA_INT_MODE_OFF | \
										 IST8309_VAL_INTCNTL2_INTONY_INT_MODE_OFF | \
										 IST8309_VAL_INTCNTL2_INTONX_INT_MODE_OFF)
#define IST8309_DETECTION_MODE_INTERRUPT (IST8309_VAL_INTCNTL2_INTTYP_BESIDE | \
										IST8309_VAL_INTCNTL2_INTONZ_INT_MODE_ON | \
										IST8309_VAL_INTCNTL2_INTONA_INT_MODE_ON | \
										IST8309_VAL_INTCNTL2_INTONY_INT_MODE_ON | \
										IST8309_VAL_INTCNTL2_INTONX_INT_MODE_ON)
/*
	*[7]	Interrupt mode on/off selection.
	*[6]	Threshold (HTH_OFF, LTH_OFF) >= PERS times.
	*[4]	Interrupt type selection, compare in signed digit format.
	*[2]	Interrupt mode on/off selection in Angle.
	*[1]	Interrupt mode on/off selection in Y Axis.
	*[0]	Interrupt mode on/off selection in X Axis.
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_LTH_XL (0x22)
/*
	*[7:0]	Low Byte of Lower Threshold Level X	
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_LTH_XH (0x23)
/*
	*[7:0]	High Byte of Lower Threshold Level X	
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_HTH_XL (0x24)
/*
	*[7:0]	Low Byte of Upper Threshold Level X
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_HTH_XH (0x25)
/*
	*[7:0]	High Byte of Upper Threshold Level X
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_DTH_X (0x26)
/*
	*[7:0]	Magnetic switch hysteresis function of Release point X.
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_LTH_YL (0x28)
/*
	*[7:0]	Low Byte of Lower Threshold Level Y
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_LTH_YH (0x29)
/*
	*[7:0]	High Byte of Lower Threshold Level Y	
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_HTH_YL (0x2A)
/*
	*[7:0]	Low Byte of Upper Threshold Level Y
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_HTH_YH (0x2B)
/*
	*[7:0]	High Byte of Upper Threshold Level Y
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_DTH_Y (0x2C)
/*
	*[7:0]	Magnetic switch hysteresis function of Release point Y.
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_LTH_ZL (0x2E)
/*
	*[7:0]	Low Byte of Lower Threshold Level Z
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_LTH_ZH (0x2F)
/*
	*[7:0]	High Byte of Lower Threshold Level Z	
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_HTH_ZL (0x30)
/*
	*[7:0]	Low Byte of Upper Threshold Level Z
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_HTH_ZH (0x31)
/*
	*[7:0]	High Byte of Upper Threshold Level Z
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_DTH_Z (0x32)
/*
	*[7:0]	Magnetic switch hysteresis function of Release point Z.
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_LTH_AL (0x34)
/*
	*[7:0]	Low Byte of Lower Threshold Level Angle
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_LTH_AH (0x35)
/*
	*[7:0]	High Byte of Lower Threshold Level Angle
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_HTH_AL (0x36)
/*
	*[7:0]	Low Byte of Upper Threshold Level Angle
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_HTH_AH (0x37)
/*
	*[7:0]	High Byte of Upper Threshold Level Angle
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_DTH_A (0x38)
/*
	*[7:0]	Magnetic switch hysteresis function of Release point angle.
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_OSRCNTL_Z (0x4E)
#define IST8309_VAL_OSRCNTL_Z_4_TIMES (0x00)
#define IST8309_VAL_OSRCNTL_Z_16_TIMES (0x10)
#define IST8309_VAL_OSRCNTL_Z_32_TIMES (0x20)
#define IST8309_VAL_OSRCNTL_Z_64_TIMES (0x30)
#define IST8309_VAL_OSRCNTL_Z_128_TIMES (0x40)
#define IST8309_VAL_OSRCNTL_Z_256_TIMES (0x50)
#define IST8309_VAL_OSRCNTL_Z_512_TIMES (0x60)
/*
	*[6:4] Z Axis OSR control.
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_OSRCNTL_XY (0x59)
#define IST8309_VAL_OSRCNTL_XY_4_TIMES (0x00)
#define IST8309_VAL_OSRCNTL_XY_16_TIMES (0x10)
#define IST8309_VAL_OSRCNTL_XY_32_TIMES (0x20)
#define IST8309_VAL_OSRCNTL_XY_64_TIMES (0x30)
#define IST8309_VAL_OSRCNTL_XY_128_TIMES (0x40)
#define IST8309_VAL_OSRCNTL_XY_256_TIMES (0x50)
#define IST8309_VAL_OSRCNTL_XY_512_TIMES (0x60)
/*
	*[6:4] X、Y Axis OSR control.
	*/
/* --------------------------------------------------------- */
#define IST8309_REG_ST_COIL (0x5E)
#define IST8309_VAL_ST_COIL_ST_N		(0x02)
#define IST8309_VAL_ST_COIL_ST_P		(0x01)
#define IST8309_VAL_ST_COIL_OFF			(0x00)
/*
	*[1] Self Test for Negative Current
	*[0] Self Test for Positive Current
	*/
/* --------------------------------------------------------- */
/* --------------------------------------------------------- */

#endif // __IST8309_H__