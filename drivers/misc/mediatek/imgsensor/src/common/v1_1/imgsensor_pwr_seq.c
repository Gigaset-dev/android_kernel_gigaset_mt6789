// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "kd_imgsensor.h"


#include "imgsensor_hw.h"
#include "imgsensor_cfg_table.h"

/* Legacy design */
struct IMGSENSOR_HW_POWER_SEQ sensor_power_sequence[] = {

#if defined(HI1339_MIPI_RAW)
		{
			SENSOR_DRVNAME_HI1339_MIPI_RAW,
			{
				{RST, Vol_Low, 1},
				{DOVDD, Vol_1800, 1},
				{AVDD, Vol_2800, 0},
				{DVDD, Vol_1200, 0},
				{AFVDD, Vol_2800, 0},
				{SensorMCLK, Vol_High, 1},
				{RST, Vol_High, 2}
			},
		},
#endif
#if defined(OV13B10MAIN_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV13B10MAIN_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{AVDD, Vol_2900, 0},
			{DVDD, Vol_1200, 0},
			{DOVDD, Vol_1800, 1},
			{AFVDD, Vol_2900, 0},
			{RST, Vol_High, 5},
			{SensorMCLK, Vol_High, 1},
		},
	},
#endif
#if defined(S5K4H7FRONT_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K4H7FRONT_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 1},
			{RST, Vol_High, 4},
			{SensorMCLK, Vol_High, 4},
		},
	},
#endif
#if defined(SC800CS_MIPI_RAW)
	{
		SENSOR_DRVNAME_SC800CS_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1804, 2},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2900, 0},
			{RST, Vol_High, 1},
			{SensorMCLK, Vol_High, 4}
		},
	},
#endif

	/* GalaxyCore */
//prize add by huarui, start
#if defined(OV02B10MACRO_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV02B10MACRO_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1000, 0},
			{AVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 10},
		},
	},
#endif
//prize add by huarui, end
#if defined(GC2385_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC2385_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{PDN, Vol_Low, 0},
			{DOVDD, Vol_1800, 5},
			{DVDD, Vol_1200, 5},
			{AVDD, Vol_2800, 5},
			{SensorMCLK, Vol_High, 10},
			{RST, Vol_High, 2},
			{PDN, Vol_High, 0},
		},
	},
#endif
#if defined(S5KHM2SPYL_MIPI_RAW)
        {
                SENSOR_DRVNAME_S5KHM2SPYL_MIPI_RAW,
                {       
				        {PDN, Vol_Low, 5},
                        {RST, Vol_Low, 10},
                        {SensorMCLK, Vol_High, 10},
                        {DVDD, Vol_1000, 5},
                        {AVDD, Vol_2800, 5},
                        {DOVDD, Vol_1800, 5},
                        {RST, Vol_High, 10},
						{PDN, Vol_High, 5},
                },
        },
#endif
#if defined(S5KHM2SPGMS_MIPI_RAW)
        {
                SENSOR_DRVNAME_S5KHM2SPGMS_MIPI_RAW,
                {       
				        {PDN, Vol_Low, 5},
                        {RST, Vol_Low, 10},
                        {SensorMCLK, Vol_High, 10},
                        {DVDD, Vol_1000, 5},
                        {AVDD, Vol_2800, 5},
                        {DOVDD, Vol_1800, 5},
                        {RST, Vol_High, 10},
						{PDN, Vol_High, 5},
                },
        },
#endif

#if defined(GC8034_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC8034_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AFVDD, Vol_High, 1},
			{AVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(IMX766_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX766_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 1},
			{AVDD, Vol_2800, 3},
			{AVDD1, Vol_1800, 0},
			{AFVDD, Vol_2800, 3},
			{DVDD, Vol_1100, 4},
			{DOVDD, Vol_1800, 1},
			{SensorMCLK, Vol_High, 6},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(S5KJD1_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5KJD1_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DVDD, Vol_1100, 0},
			{AVDD, Vol_2800, 0},
			{AFVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{PDN, Vol_High, 0},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(S5KJN1_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5KJN1_MIPI_RAW,
		{
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{SensorMCLK, Vol_High, 15},
			{DOVDD, Vol_1800, 5},
			{DVDD, Vol_1200, 5},
			{AVDD, Vol_2800, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(S5KJN1GMS_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5KJN1GMS_MIPI_RAW,
		{
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{SensorMCLK, Vol_High, 15},
			{DOVDD, Vol_1800, 5},
			{DVDD, Vol_1200, 5},
			{AVDD, Vol_2800, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(IMX586_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX586_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 0},
			{AVDD1, Vol_1800, 0},
			{AFVDD, Vol_2800, 0},
			{DVDD, Vol_1100, 0},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 3}
		},
	},
#endif
#if defined(IMX319_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX319_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 1},
			{AVDD, Vol_2800, 0},
			{AFVDD, Vol_2800, 0},
			{DVDD, Vol_1100, 0},
			{DOVDD, Vol_1800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(S5K3M5SX_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3M5SX_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DVDD, Vol_1100, 1},
			{AVDD, Vol_2800, 1},
			{AFVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 1},
			{RST, Vol_High, 2},
			{SensorMCLK, Vol_High, 1}
		},
	},
#endif
#if defined(GC02M0_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC02M0_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{DOVDD, Vol_1800, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(GC02M1_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC02M1_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{PDN, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 0},
			//{DVDD, Vol_1200, 0},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 2},
			{PDN, Vol_High, 2}
		},
	},
#endif
#if defined(IMX519_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX519_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2800, 0},
			{AFVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{DOVDD, Vol_1800, 1},
			{SensorMCLK, Vol_High, 5},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(IMX519DUAL_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX519DUAL_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2800, 0},
			{AFVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{DOVDD, Vol_1800, 1},
			{SensorMCLK, Vol_High, 5},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(IMX499_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX499_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 10}
		},
	},
#endif
#if defined(IMX481_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX481_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_1800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1800, 2},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 10}
		},
	},
#endif
#if defined(IMX481GMS_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX481GMS_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{SensorMCLK, Vol_High, 1},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1100, 2},
			{DOVDD, Vol_1800, 0},
			//{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 10}
		},
	},
#endif
#if defined(IMX576_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX576_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 1}, /*data sheet 1050*/
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 8}
		},
	},
#endif
#if defined(IMX350_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX350_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 5},
			{SensorMCLK, Vol_High, 5},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(IMX398_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX398_MIPI_RAW,
		{
			{SensorMCLK, Vol_Low, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 1},
		},
	},
#endif
#if defined(OV23850_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV23850_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 2},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(OV16885_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV16885_MIPI_RAW,
		{
			{PDN, Vol_Low, 1},
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 5},
			{PDN, Vol_High, 2},
			{RST, Vol_High, 2},
		},
	},
#endif
#if defined(OV05A20_MIPI_RAW)
		{
			SENSOR_DRVNAME_OV05A20_MIPI_RAW,
			{
				{SensorMCLK, Vol_High, 1},
				{RST, Vol_Low, 1},
				{AVDD, Vol_2800, 10},
				{DOVDD, Vol_1800, 5},
				{DVDD, Vol_1200, 5},
				{RST, Vol_High, 15}
			},
		},
#endif

#if defined(IMX386_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX386_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1100, 0},
			{DOVDD, Vol_1800, 1},
			{AFVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 2},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 10},
		},
	},
#endif
#if defined(IMX386_MIPI_MONO)
	{
		SENSOR_DRVNAME_IMX386_MIPI_MONO,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1100, 0},
			{DOVDD, Vol_1800, 1},
			{AFVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 2},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 10},
		},
	},
#endif
#if defined(IMX376_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX376_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(IMX338_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX338_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 1},
			{AVDD, Vol_2500, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(S5K2LQSX_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2LQSX_MIPI_RAW,
		{
			{PDN, Vol_Low, 1},
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 4},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 1},
			{DOVDD, Vol_1800, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0},
		},
	},
#endif
#if defined(S5K4H7_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K4H7_MIPI_RAW,
		{
			{PDN, Vol_Low, 1},
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 4},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 1},
			{DOVDD, Vol_1800, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0},
		},
	},
#endif
/*prize add by zhaopengge 20220709---start*/
#if defined(S5K4H7YX_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K4H7YX_MIPI_RAW,
		{
			{PDN, Vol_Low, 1},
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 4},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 1},
			{DOVDD, Vol_1800, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0},
		},
	},
#endif
/*prize add by zhaopengge 20220709---end*/
#if defined(S5K4E6_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K4E6_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2900, 0},
			{DVDD, Vol_1200, 2},
			{AFVDD, Vol_2800, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K2T7SP_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2T7SP_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 2},
		},
	},
#endif
#if defined(S5K3P8SP_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3P8SP_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0},
		},
	},
#endif
#if defined(S5K3P8SX_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3P8SX_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{SensorMCLK, Vol_High, 1},
			{DVDD, Vol_1000, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 2},
		},
	},
#endif
#if defined(S5K3M2_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3M2_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K3P3SX_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3P3SX_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K5E2YA_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K5E2YA_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K4ECGX_MIPI_YUV)
	{
		SENSOR_DRVNAME_S5K4ECGX_MIPI_YUV,
		{
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{AFVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(OV16880_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV16880_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(S5K2P7_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2P7_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1000, 1},
			{DOVDD, Vol_1800, 1},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_Low, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0},
		},
	},
#endif
#if defined(S5K2P8_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2P8_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX258_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX258_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(IMX258_MIPI_MONO)
	{
		SENSOR_DRVNAME_IMX258_MIPI_MONO,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(IMX377_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX377_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(OV8858_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV8858_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(OV8856_MIPI_RAW)
	{SENSOR_DRVNAME_OV8856_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 2},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(S5K2X8_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2X8_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX214_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX214_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(IMX214_MIPI_MONO)
	{
		SENSOR_DRVNAME_IMX214_MIPI_MONO,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(IMX230_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX230_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 10},
			{DOVDD, Vol_1800, 10},
			{DVDD, Vol_1200, 10},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K3L8_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3L8_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX362_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX362_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 10},
			{DOVDD, Vol_1800, 10},
			{DVDD, Vol_1200, 10},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K2L7_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2L7_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 3},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(IMX318_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX318_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{SensorMCLK, Vol_High, 5},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(OV8865_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV8865_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 5},
			{AVDD, Vol_2800, 5},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(IMX219_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX219_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 10},
			{DOVDD, Vol_1800, 10},
			{DVDD, Vol_1000, 10},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K3M3_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3M3_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(OV5670_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV5670_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 5},
			{AVDD, Vol_2800, 5},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(OV5670_MIPI_RAW_2)
	{
		SENSOR_DRVNAME_OV5670_MIPI_RAW_2,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 5},
			{AVDD, Vol_2800, 5},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(OV2281_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV2281_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 0},
			{RST, Vol_Low, 10},
			{RST, Vol_High, 5},
			{PDN, Vol_Low, 5},
			{PDN, Vol_High, 5},
		},
	},
#endif
#if defined(OV20880_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV20880_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{RST, Vol_Low, 1},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1100, 1},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(OV5645_MIPI_YUV)
	{
		SENSOR_DRVNAME_OV5645_MIPI_YUV,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 5},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 10}
		},
	},
#endif
#if defined(S5K5E9_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K5E9_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 2},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 10}
		},
	},
#endif
#if defined(S5KGD1SP_MIPI_RAW)
		{
			SENSOR_DRVNAME_S5KGD1SP_MIPI_RAW,
			{
				{RST, Vol_Low, 1},
				{AVDD, Vol_2800, 0},
				{DVDD, Vol_1100, 0},
				{DOVDD, Vol_1800, 1},
				{SensorMCLK, Vol_High, 1},
				{RST, Vol_High, 2}
			},
		},
#endif
#if defined(HI846_MIPI_RAW)
		{
			SENSOR_DRVNAME_HI846_MIPI_RAW,
			{
				{RST, Vol_Low, 1},
				{AVDD, Vol_2800, 0},
				{DVDD, Vol_1200, 0},
				{DOVDD, Vol_1800, 1},
				{SensorMCLK, Vol_High, 1},
				{RST, Vol_High, 2}
			},
		},
#endif
#if defined(GC02M0_MIPI_RAW)
		{
			SENSOR_DRVNAME_GC02M0_MIPI_RAW,
			{
				{RST, Vol_Low, 1},
				{AVDD, Vol_2800, 0},
				{DVDD, Vol_1200, 0},
				{DOVDD, Vol_1800, 1},
				{SensorMCLK, Vol_High, 1},
				{RST, Vol_High, 2}
			},
		},
#endif
#if defined(OV02A10_MIPI_MONO)
		{
			SENSOR_DRVNAME_OV02A10_MIPI_MONO,
			{
				{RST, Vol_High, 1},
				{AVDD, Vol_2800, 0},
			/*main3 has no dvdd, compatible with sub2*/
				{DVDD, Vol_1200, 0},
				{DOVDD, Vol_1800, 0},
				{SensorMCLK, Vol_High, 5},
				{RST, Vol_Low, 9}
			},
		},
#endif
#if defined(IMX686_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX686_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{AVDD, Vol_2900, 0},
		/*in alph.dts file, pin avdd controls two gpio pins*/
			/*{AVDD_1, Vol_1800, 0},*/
			{DVDD, Vol_1100, 0},
			{DOVDD, Vol_1800, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(IMX616_MIPI_RAW)
		{
			SENSOR_DRVNAME_IMX616_MIPI_RAW,
			{
				{RST, Vol_Low, 1},
				{AVDD, Vol_2900, 0},
				{DVDD, Vol_1100, 0},
				{DOVDD, Vol_1800, 1},
				{SensorMCLK, Vol_High, 1},
				{RST, Vol_High, 2}
			},
		},
#endif
#if defined(IMX355_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX355_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(OV13B10_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV13B10_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{DOVDD, Vol_1800, 1},
			{RST, Vol_High, 5},
			{SensorMCLK, Vol_High, 1},
		},
	},
#endif
#if defined(OV48C_MIPI_RAW)
		{
			SENSOR_DRVNAME_OV48C_MIPI_RAW,
			{
				{RST, Vol_Low, 1},
				{SensorMCLK, Vol_High, 0},
				{DOVDD, Vol_1800, 0},
				{AVDD, Vol_2800, 0},
				{DVDD, Vol_1200, 5},
				{RST, Vol_High, 5},
			},
		},
#endif
#if defined(OV16A10_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV16A10_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{DOVDD, Vol_1800, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 2},
		},
	},
#endif
#if defined(OV48B_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV48B_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 2},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(S5K3P9SP_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3P9SP_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{RST, Vol_Low, 1},
			{DVDD, Vol_1100, 1},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 3},
			{AFVDD, Vol_2800, 5},
			{RST, Vol_High, 2},
		},
	},
#endif
#if defined(GC8054_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC8054_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{RST, Vol_Low,  1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{RST, Vol_High, 1},
			//{AFVDD, Vol_Low, 5}
		},
	},
#endif
#if defined(GC02M0B_MIPI_MONO)
	{
		SENSOR_DRVNAME_GC02M0B_MIPI_MONO,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(GC02M1B_MIPI_MONO)
	{
		SENSOR_DRVNAME_GC02M1B_MIPI_MONO,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(GC02M0B_MIPI_MONO1)
	{
		SENSOR_DRVNAME_GC02M0B_MIPI_MONO1,
		{
			{RST, Vol_Low, 1},
			//{DVDD, Vol_1200, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(GC02M0B_MIPI_MONO2)
	{
		SENSOR_DRVNAME_GC02M0B_MIPI_MONO2,
		{
			{RST, Vol_Low, 1},
			//{DVDD, Vol_1200, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(GC02K0B_MIPI_MONO)
	{
		SENSOR_DRVNAME_GC02K0B_MIPI_MONO,
		{
			{RST, Vol_Low, 1},
			//{DVDD, Vol_1200, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(OV02B10_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV02B10_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 9},
			{RST, Vol_High, 1}
		},
	},
#endif
//prize add by linchong 20220609 start
#if defined(S5KGM1ST_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5KGM1ST_MIPI_RAW,
		{
			{RST, Vol_Low, 10},
			{PDN, Vol_Low, 10},
			{SensorMCLK, Vol_High, 10},
			{DVDD, Vol_1000, 5},
			{AVDD, Vol_2800, 5},
			{DOVDD, Vol_1800, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 10},
			{RST, Vol_High, 10},
		},
	},
#endif
#if defined(OV16A1Q_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV16A1Q_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{PDN, Vol_Low, 1},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{DOVDD, Vol_1800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_High, 2},
			{RST, Vol_High, 2},
		},
	},
#endif
#if defined(GC08A3WIDE_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC08A3WIDE_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 5}
		},
	},
#endif
//prize add by linchong 20220609 end
//drv add by lipengpeng 20230516 start 
#if defined(GC05A2_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC05A2_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{SensorMCLK, Vol_High, 5},
			{DOVDD, Vol_1800, 5},
			{DVDD, Vol_1200, 5},
			{AVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(S5KGD1SP_MIPI_RAW)
		{
			SENSOR_DRVNAME_S5KGD1SP_MIPI_RAW,
			{
				{PDN, Vol_Low, 0},
				{RST, Vol_Low, 1},
				{AVDD, Vol_2800, 0},
				{DVDD, Vol_1100, 0},
				{DOVDD, Vol_1800, 1},
				{SensorMCLK, Vol_High, 1},
				{RST, Vol_High, 2},
				{PDN, Vol_High, 5},
			},
		},
#endif
#if defined(OV32B40_MIPI_RAW)
		{
			SENSOR_DRVNAME_OV32B40_MIPI_RAW,
			{
				{PDN, Vol_Low, 0},
				{RST, Vol_Low, 1},
				{AVDD, Vol_2800, 0},
				{DVDD, Vol_1100, 0},
				{DOVDD, Vol_1800, 1},
				{SensorMCLK, Vol_High, 1},
				{RST, Vol_High, 2},
				{PDN, Vol_High, 5},
			},
		},
#endif
#if defined(OV32B40GMS_MIPI_RAW)
		{
			SENSOR_DRVNAME_OV32B40GMS_MIPI_RAW,
			{
				{PDN, Vol_Low, 0},
				{RST, Vol_Low, 1},
				{AVDD, Vol_2800, 0},
				{DVDD, Vol_1100, 0},
				{DOVDD, Vol_1800, 1},
				{SensorMCLK, Vol_High, 1},
				{RST, Vol_High, 2},
				{PDN, Vol_High, 5},
			},
		},
#endif
//drv add by lipengpeng 20230516 end 
	/* add new sensor before this line */
	{NULL,},
};

