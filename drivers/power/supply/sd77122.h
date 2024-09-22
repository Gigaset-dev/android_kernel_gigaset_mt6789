/*
 * Copyright (C) 2018 BIGMENT Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

 #ifndef SD77122_H_
 #define SD77122_H_

#define PEC_ENABLE	1
#define I2C_PEC_POLY 0x07
#define SD77122_SLAVE_ADDRESS   0x50

#define SD77122_REG_NUM 29

#define SD77122_R00		0x00
#define SD77122_R01		0x01
#define SD77122_R02		0x02
#define SD77122_R03		0x03
#define SD77122_R04		0x04
#define SD77122_R05		0x05
#define SD77122_R06		0x06
#define SD77122_R07		0x07
#define SD77122_R08		0x08
#define SD77122_R09		0x09
#define SD77122_R0A		0x0A //Reserved
#define SD77122_R0B		0x0B
#define SD77122_R0C		0x0C
#define SD77122_R0D		0x0D
#define SD77122_R0E		0x0E
#define SD77122_R0F		0x0F
#define SD77122_R10		0x10
#define SD77122_R11		0x11
#define SD77122_R12		0x12
#define SD77122_R13		0x13
#define SD77122_R14		0x14
#define SD77122_R15		0x15
#define SD77122_R16		0x16
#define SD77122_R17		0x17  //Reserved
#define SD77122_R18		0x18  //Reserved
#define SD77122_R19		0x19  //Reserved
#define SD77122_R1A	    0x1A  //Reserved
#define SD77122_R1B	    0x1B  
#define SD77122_R1C	    0x1C 

#define SD77122_VENDOR_ID		0x02
#define SD77122_DEVICE_ID		0x00
#define SD77122_ICVERSION		0xB0
#define SD77122_CONFIG_ID       0x00

/* REG04 */
#define QN_CTRL_SW_MASK	    0x03
#define QN_CTRL_SW_SHIFT	0

#define QP_CTRL_SW_MASK	    0x0C
#define QP_CTRL_SW_SHIFT    2

#define QE_DRV_SW_MASK	    0x20
#define QE_DRV_SW_SHIFT	    5


/* REG05 */
#define QN_CTRL_MASK	    0x3
#define QN_CTRL_SHIFT	    0

#define QP_CTRL_MASK	    0xC
#define QP_CTRL_SHIFT	    2

#define QE_DRV_MASK	        0x10
#define QE_DRV_SHIFT	    5

#define CP_INT_ENABLE_MASK	0x20
#define CP_INT_ENABLESHIFT	6

#define CP_EXT_ENABLE_MASK  0x40
#define CP_EXT_ENABLESHIFT  7

#define CP_INT_OK_MASK	    0x80
#define CP_INT_OK_SHIFT	    8

#define CP_EXT_OK_MASK	    0x100
#define CP_EXT_OK_SHIFT	    9


/* REG06 */
#define SYS_MODE_MASK		0x0F
#define SYS_MODE_SHIFT		0

#define IN_SHIP_MODE 		1

#define SLEPP_MODE_MASK		0x4000
#define SLEEP_MODE_SHIFT	14

#define IN_SLEEP_MODE 		1

#define SHIP_MODE_MASK		0x8000
#define SHIP_MODE_SHIFT		15

#define STARTUP_MODE		    0x0000
#define ACTIVE_PROTECT_MODE		0x0010
#define INI_PARALLEL_MODE		0x0101
#define ACTIVE_PRE_P_MODE		0x0110
#define ACTIVE_PARALLEL_MODE	0x0111
#define SLEEP_PARALLEL_MODE		0x1000

#define PARALLEL_MODE           0xDAAD
#define ENTER_SHIP_MODE         0xABBA
#define EXIT_SHIP_MODE          0xBAAB
#define ENTER_SLEEP_MODE        0xACCA
#define EXIT_SLEEP_MODE         0xCAAC

/* REG07 */
#define CP_INT_FAIL_MASK_MASK      0x1
#define CP_INT_FAIL_MASK_SHIFT     0

#define CP_EXT_FAIL_MASK_MASK      0x2
#define CP_EXT_FAIL_MASK_SHIFT     1

#define OT_MASK_MASK		       0x4
#define OT_MASK_SHIFT		       2

#define OV_MASK_MASK		       0x8
#define OV_MASK_SHIFT		       3

#define UV_MASK_MASK		       0x10
#define UV_MASK_SHIFT		       4

#define OCP1_QP_MASK_MASK		   0x20
#define OCP1_QP_MASK_SHIFT		   5

#define OCP2_QP_MASK_MASK		   0x40
#define OCP2_QP_MASK_SHIFT		   6

#define OCP1_QN_MASK_MASK		   0x80
#define OCP1_QN_MASK_SHIFT		   7

#define OCP2_QN_MASK_MASK		   0x100
#define OCP2_QN_MASK_SHIFT		   8

#define TRIG_END_MASK_MASK		   0x200
#define TRIG_END_MASK_SHIFT		   9

#define PRE_TIME_OUT_MASK_MASK	   0x1000
#define PRE_TIME_OUT_MASK_SHIFT	   12

#define OHTP_MASK_MASK	           0x4000
#define OHTP_MASK_SHIFT            14

#define ALL_EVENT_MASK             0x7FFF

/* REG08 */
#define CP_INT_FAIL_FLAG_MASK	   0x1
#define CP_INT_FAIL_FLAG_SHIFT     0

#define CP2_FAIL_FLAG_MASK	       0x2
#define CP2_FAIL_FLAG_SHIFT         1

#define OT_FLAG_MASK	           0x4
#define OT_FLAG_SHIFT               2

#define OV_FLAG_MASK	           0x8
#define OV_FLAG_SHIFT               3

#define UV_FLAG_MASK	           0x10
#define UV_FLAG_SHIFT               4

#define OCP1_QP_FLAG_MASK	       0x20
#define OCP1_QP_FLAG_SHIFT          5

#define OCP2_QP_FLAG_MASK	       0x40
#define OCP2_QP_FLAG_SHIFT          6

#define OCP1_QN_FLAG_MASK      	   0x80
#define OCP1_QN_FLAG_SHIFT          7

#define OCP2_QN_FLAG_MASK	       0x100
#define OCP2_QN_FLAG_SHIFT          8

#define TRIG_END_FLAG_MASK	       0x200
#define TRIG_END_FLAG_SHIFT         9

#define PRE_TIMEOUT_FLAG_MASK	   0x1000
#define PRE_TIMEOUT_FLAG_SHIFT      12

#define OHTP_FLAG_MASK	           0x4000
#define OHTP_FLAG_SHIFT             14

#define PORN_FLAG_MASK	           0x8000
#define PORN_FLAG_SHIFT             15

/* REG09 */
#define CP_INT_STATE_MASK		   0x1
#define CP_INT_STATE_SHIFT		    0

#define CP_EXT_STATE_MASK	       0x2
#define CP_EXT_STATE_SHIFT		    1

#define OT_STATE_MASK		       0x4
#define OT_STATE_SHIFT		        2

#define OV_STATE_MASK		       0x8
#define OV_STATE_SHIFT		        3

#define UV_STATE_MASK		       0x10
#define UV_STATE_SHIFT		        4

#define OCP1_QP_STATE_MASK		   0x20
#define OCP1_QP_STATE_SHIFT		    5

#define OCP2_QP_STATE_MASK		   0x40
#define OCP2_QP_STATE_SHIFT		    6

#define OCP1_QN_STATE_MASK		   0x80
#define OCP1_QN_STATE_SHIFT		    7

#define OCP2_QN_STATE_MASK		   0x100
#define OCP2_QN_STATE_SHIFT		    8

#define PRE_TIME_OUT_STATE_MASK	   0x1000
#define PRE_TIME_OUT_STATE_SHIFT    12

#define OHTP_STATE_MASK	           0x4000
#define OHTP_STATE_SHIFT            14

/* REG0A */
#define BAT_DET_INTVAL_MASK        0x2
#define BAT_DET_INTVAL_SHIFT       1

#define BAT_DET_MODE_SEL_MASK      0x4
#define BAT_DET_MODE_SEL_SHIFT     2

#define BAT_DET_SW_CTRL_MASK       0x8
#define BAT_DET_SW_CTRL_SHIFT      3

#define BAT_CURR_LMT_EN_MASK       0x100
#define BAT_CURR_LMT_EN_SHIFT      8

#define BAT_CURR_LMT_TH_MASK       0xE00
#define BAT_CURR_LMT_TH_SHIFT      9

#define BAT1_IN_ACTIVE_MASK        0x4000
#define BAT1_IN_ACTIVE_SHIFT       14

#define BAT2_IN_ACTIVE_MASK        0x8000
#define BAT2_IN_ACTIVE_SHIFT       15

/* REG0B */
#define OC1_QNSET_MASK	             0x3
#define OC1_QNSET_SHIFT              0

#define OC2_QNSEL_MASK	             0x0C
#define OC2_QNSEL_SHIFT              2

#define OC1_QPSET_MASK	             0x30
#define OC1_QPSET_SHIFT              4

#define OC2_QPSEL_MASK	             0xC0
#define OC2_QPSEL_SHIFT              6

#define PRE_CUR_WD_SET_MASK	         0x300
#define PRE_CUR_WD_SET_SHIFT         8

#define QN_PWR_SEL_MASK	             0xC00
#define QN_PWR_SEL_SHIFT             10

#define QP_PWR_SEL_MASK	             0x3000
#define QP_PWR_SEL_SHIFT             12

#define PRE_CURR_SET_OPT_MASK	     0x8000
#define PRE_CURR_SET_OPT_SHIFT       15

#define QN_OCP1_100A	             0x00
#define QN_OCP1_120A	             0x01
#define QN_OCP1_140A	             0x10
#define QN_OCP1_160A	             0x11

#define QN_OCP2_15_OCP1              0x00
#define QN_OCP2_20_OCP1	             0x01
#define QN_OCP2_30_OCP1	             0x10
#define QN_OCP2_40_OCP1	             0x11

#define QP_OCP1_100A	             0x00
#define QP_OCP1_120A	             0x01
#define QP_OCP1_140A	             0x10
#define QP_OCP1_160A	             0x11

#define QP_OCP2_15_OCP1              0x00
#define QP_OCP2_20_OCP1	             0x01
#define QP_OCP2_30_OCP1	             0x10
#define QP_OCP2_40_OCP1	             0x11

#define WDT_SETTING_4S               0x00
#define WDT_SETTING_8S               0x01
#define WDT_SETTING_16S              0x10
#define WDT_SETTING_32S              0x11

#define QN_MAXIMUN_CURRENT_30A       0x00
#define QN_MAXIMUN_CURRENT_20A       0x01
#define QN_MAXIMUN_CURRENT_15A       0x10
#define QN_MAXIMUN_CURRENT_10A       0x11

#define QP_MAXIMUN_CURRENT_30A       0x00
#define QP_MAXIMUN_CURRENT_20A       0x01
#define QP_MAXIMUN_CURRENT_15A       0x10
#define QP_MAXIMUN_CURRENT_10A       0x11


/* REG0C */
#define PRE_CUR_SET_SW_MASK       0x1F
#define PRE_CUR_SET_SW_SHIFT       0

#define PRE_CUR_SET_MASK          0x3E0
#define PRE_CUR_SET_SHIFT          5

#define PRE_CURR_WD_DIS_MASK	    0x8000
#define PRE_CURR_WD_DIS_SHIFT       15

/* REG0D */
#define OTP_AUTO_DIS_MASK         0x1
#define OTP_AUTO_DIS_SHIFT         0

#define HICCUP_DIS_MASK           0x2
#define HICCUP_DIS_SHIFT           1

/* REG0E */
#define OCP1_DEGLITH_MASK            0x3
#define OCP1_DEGLITH_SHIFT            0

#define OCP2_DEGLITH_MASK            0xC
#define OCP2_DEGLITH_SHIFT            2

#define CP_INT_FAIL_TIMER_MASK       0x30
#define CP_INT_FAIL_TIMER_SHIFT       4

#define CP_EXT_FAIL_TIMER_MASK       0xC0
#define CP_EXT_FAIL_TIMER_SHIFT       6

#define OV_DEGLITH_MASK              0x300
#define OV_DEGLITH_SHIFT              8

#define UV_DEGLITH_MASK     		0xC00
#define UV_DEGLITH_SHIFT     		10

#define OT_DEGLITH_MASK     		0x3000
#define OT_DEGLITH_SHIFT     		12

#define SLEEP_WAKEUP_TIMER_MASK     0xC000
#define SLEEP_WAKEUP_TIMER_SHIFT     14

/* REG0F */
#define OHTP_DEGLITCH_MASK		    0x10
#define OHTP_DEGLITCH_SHIFT	        0x4

#define PRE_TIMEOUT_SET_MASK		0x60
#define PRE_TIMEOUT_SET_SHIFT	    0x5

#define OCP_DLY_SET_MASK		    0x300
#define OCP_DLY_SET_SHIFT	        0x8

#define EXT_VGS_SEL_MASK   	        0x3000
#define EXT_VGS_SEL_SHIFT	        0x12

/* REG10 */
#define OV_THRESHOLD_MASK		   0x01F
#define OV_THRESHOLD_SHIFT	        0x0

#define OV_HYSTERESIS_MASK		   0x700
#define OV_HYSTERESIS_SHIFT	        0x8

/* REG11 */
#define UV_THRESHOLD_MASK		   0x01F
#define UV_THRESHOLD_SHIFT	        0x0

#define UV_HYSTERESIS_MASK		   0xF00
#define UV_HYSTERESIS_SHIFT	        0x8

/* REG12 */
#define OT_THRESHOLD_MASK		   0x01
#define OT_THRESHOLD_SHIFT	        0x0

#define OT_HYSTERESIS_MASK		   0x100
#define OT_HYSTERESIS_SHIFT	        0x8

/* REG13 */
#define MAX_CURR_SET_MASK		   0x1F00
#define MAX_CURR_SET_SHIFT	        0x8

/* REG14 */
#define ADC_SEL_EN_MASK		       0x7F
#define ADC_SEL_EN_SHIFT	        0x0

#define ADC_SCAN_CYCLE_MASK		   0x180
#define ADC_SCAN_CYCLE_SHIFT	    0x7

/* REG15 */
#define ADC_STOP_MASK		       0x1
#define ADC_STOP_SHIFT	           0x0

#define ADC_TRIGGER_MASK		   0x2
#define ADC_TRIGGER_SHIFT	       0x1

#define ADC_AVG_SEL_MASK		   0x4
#define ADC_AVG_SEL_SHIFT	       0x2

/* REG16 */
#define BAT2_SEN_VAL_MASK		   0xFFF
#define BAT2_SEN_VAL_SHIFT	       0x0

/* REG17 */
#define BAT1_SEN_VAL_MASK		   0xFFF
#define BAT1_SEN_VAL_SHIFT	       0x0

/* REG1B */
#define VCC_VAL_MASK		   0xFFF
#define VCC_VAL_SHIFT	       0x0

/* REG1C */
#define TEMP_VAL_MASK		   0xFFF
#define TEMP_VAL_SHIFT	       0x0




#endif // _SD77122_SW_H_
