/*
 * Copyright (C) 2012 Isentek Inc.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <linux/regulator/consumer.h>
#include <linux/input.h>
#include <linux/regmap.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/ioctl.h>
#include <linux/hrtimer.h>
#include <linux/kthread.h>
#include <linux/sched/rt.h>
#include <linux/version.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include "ist-input-core.h"
#include "ist8309.h"
/*
 * System configuration
 */

/*
 * IST8309 Driver Configurations and Data Structure
 */
#ifndef USE_SINGLE_MEASURE
#define USE_SINGLE_MEASURE	0
#endif

#define IST8309_I2C_NAME		"ist8309"
#define IST8309_DEVICE_ID		0x89
#define SENSOR_DATA_SIZE	    11 // ST + 6 byte data (x,y,z raw data) + 2 T data + 2 angle data

/* operation mode */
#define IST8309_MODE_SNG_MEASURE	IST8309_VAL_CNTL1_ODR_SINGLE_MODE
#define	IST8309_MODE_POWERDOWN		IST8309_VAL_CNTL1_ODR_STANDBY

/* 0.1515 uT resoluton */
#define IST8309_RAW_TO_MICRO_UT                 151500 // 0.1515 uT
#define IST8309_MAX_CONVERSION_TIMEOUT_MS       50
#define IST8309_CONVERSION_DONE_POLL_TIME_MS    20//5

//#define IST_HERE dev_info(&ist->i2c->dev, "%s %d", __func__, __LINE__)
struct ist8309_data {
	struct ist_base base;
	u8 reg_persint;
	u8 reg_intrmode;
};

/*
 * IST8309 Private Functions
 *
 */
static int ist8309_set_mode(struct ist_base *ist, u8 mode)
{
	int rc;
	bool wait;
	u8 buffer[2];

	/* when transitioning to power-down mode, wait at least 100us */
	if (mode == IST8309_MODE_POWERDOWN)
		wait = true;
	else
		wait = false;

	buffer[0] = IST8309_REG_CNTL1;
	buffer[1] = mode;
	rc = ist_i2c_txdata(ist->i2c, buffer, 2);
	if (rc < 0) {
		dev_err(&ist->i2c->dev, "set mode error");
		return rc;
	}

	if (wait)
		usleep_range(100, 200);

	return 0;
}

static int wait_conversion_complete_polled(struct ist_base *ist)
{
	u8 val;
	int timeout_ms = IST8309_MAX_CONVERSION_TIMEOUT_MS;
	int ret;

	/* Wait for the conversion to complete. */
	while (timeout_ms > 0) {
		msleep_interruptible(IST8309_CONVERSION_DONE_POLL_TIME_MS);
		val = IST8309_REG_STAT1;
		ret = ist_i2c_rxdata(ist->i2c, &val, 1);
		if (ret < 0) {
			dev_err(&ist->i2c->dev, "Error in reading ST1\n");
			return ret;
		}
		if (val & IST8309_VAL_STAT1_DRDY)
			break;
		timeout_ms -= IST8309_CONVERSION_DONE_POLL_TIME_MS;
	}
	if (timeout_ms <= 0) {
		dev_err(&ist->i2c->dev, "conversion timeout happened\n");
		return -EIO;
	}

	return 0;
}

static int ist8309_chip_reset(struct ist_base *ist)
{
	unsigned char buffer[2];

	buffer[0] = IST8309_REG_SRST;	//register address
	buffer[1] = IST8309_VAL_SRST_RESET;	//register value
	if (ist_i2c_txdata(ist->i2c, buffer, 2)) {
		dev_err(&ist->i2c->dev, "reg %x write %x failed at %d\n", buffer[0], buffer[1], __LINE__);
	}
	usleep_range(25000, 30000);	//delay 30ms
	return 0;

}

#define IST8309_INIT_SETTING_NUM 11
static int ist8309_chip_init(struct ist_base *ist)
{
	// uint8_t ist8309_init_setting[IST8309_INIT_SETTING_NUM][2] =
	// {
	//     {0x48, 0x00}, {0x4C, 0x1F}, {0x62, 0x00}, {0x63, 0x00}, {0x6A, 0x00},
	//     {0x6B, 0x00}, {0x72, 0x00}, {0x73, 0x00}, {0x04, 0x00}
	// };
	unsigned char ist8309_init_reg[IST8309_INIT_SETTING_NUM] =
	{
		0x48, 0x4C, 0x62, 0x63, 0x6A,
		0x6B, 0x72, 0x73, 0x04, 0x20,
		0x21,
	};
	unsigned char ist8309_init_value[IST8309_INIT_SETTING_NUM] =
	{
		0x00, 0x1F, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x41,
		0x83,
	};
	unsigned char buffer[2];
	int i = 0;
	struct ist8309_data *ist8309_data = (struct ist8309_data *)ist;

	for( i = 0 ; i < IST8309_INIT_SETTING_NUM; i++ ) {
		buffer[0] = ist8309_init_reg[i];	//register address
		buffer[1] = ist8309_init_value[i];	//register value

		if (ist_i2c_txdata(ist->i2c, buffer, 2)) {
			dev_err(&ist->i2c->dev, "reg %x write %x failed at %d\n", buffer[0], buffer[1], __LINE__);
		}
		usleep_range(500, 1000);	//delay 1ms
	}
	ist8309_data->reg_persint = 0x41; // 4 times | clear interrupt
	ist8309_data->reg_intrmode = IST8309_VAL_INTCNTL2_INTTYP_BESIDE | \
			IST8309_VAL_INTCNTL2_INTONZ_INT_MODE_ON | \
			IST8309_VAL_INTCNTL2_INTONY_INT_MODE_ON | \
			IST8309_VAL_INTCNTL2_INTONX_INT_MODE_ON;
	return 0;
}

static int ist8309_check_device(struct ist_base *ist)
{
	int rc;
	unsigned char buffer[2];

	buffer[0] = IST8309_REG_WAI;
	rc = ist_i2c_rxdata(ist->i2c, buffer, 1);
	if (rc) {
		dev_err(&ist->i2c->dev, "read reg id failed.(%d)\n", rc);
		return rc;
	}
	dev_info(&ist->i2c->dev, "read device id: 0x%x\n", buffer[0]);

	if (buffer[0] != IST8309_DEVICE_ID)
	{
		dev_err(&ist->i2c->dev, "read device id not match, exppected(%d), read(%d).\n", IST8309_DEVICE_ID, buffer[0]);
		return -ENODEV;
	}
	return 0;
}

static int ist8309_read_data(struct ist_base *ist, int16_t vec[4])
{
	unsigned char data[SENSOR_DATA_SIZE];
	int rc = 0;
	unsigned char stat;

	memset(data, 0, sizeof(data));
	/* read xyz raw data */
	data[0] = IST8309_REG_STAT1;
	rc = ist_i2c_rxdata(ist->i2c, data, SENSOR_DATA_SIZE);
	if (rc) {
		dev_err(&ist->i2c->dev, "read reg id failed.(%d)\n", rc);
		return rc;
	}
	stat = data[0];
	//dev_info(&ist->i2c->dev,
	//	"ST=%2X DATA: [%02X%02X, %02X%02X, %02X%02X]", data[0], data[2], data[1], data[4], data[3], data[6], data[5]);
#if USE_SINGLE_MEASURE
	/*
	 * ST : data ready -
	 * Measurement has been completed and data is ready to be read.
	 */
	if ((stat & IST8309_VAL_STAT1_DRDY) != IST8309_VAL_STAT1_DRDY) {
		dev_info(&ist->i2c->dev, "%s:ST is not set. st = 0x%02x\n", __func__, stat);
		rc = -1;
	}
#endif
	vec[0] = data[2] << 8 | data[1];
	vec[1] = data[4] << 8 | data[3];
	vec[2] = data[6] << 8 | data[5];
	vec[3] = data[10] << 8 | data[9];
	dev_info(&ist->i2c->dev,
		"RAW: [ %hd, %hd, %hd ] A:[ %hd ] ", vec[0], vec[1], vec[2], vec[3]);
	return 4;
}

/*
 * ist-input-core Implementation Functions
 *
 */
static int ist8309_set_interrupts(struct ist_base *ist, int intr_type, int* intr, size_t size)
{
	struct ist8309_data *ist8309_data = (struct ist8309_data*)ist;
	int ret;
	u8 intrmode;
	u8 off_bits;
	u8 buffer[2];

	mutex_lock(&ist->lock);
	intrmode = ist8309_data->reg_intrmode;
	off_bits = (IST8309_VAL_INTCNTL2_INTTYP_WITHIN|
				IST8309_VAL_INTCNTL2_INTONX_INT_MODE_ON |
				IST8309_VAL_INTCNTL2_INTONY_INT_MODE_ON |
				IST8309_VAL_INTCNTL2_INTONZ_INT_MODE_ON);
	off_bits = ~off_bits; // turn off bits
	intrmode = ist8309_data->reg_intrmode & off_bits; // turn off all
	intrmode |= (intr_type == INTERRUPT_WITHIN) ? IST8309_VAL_INTCNTL2_INTTYP_WITHIN : IST8309_VAL_INTCNTL2_INTTYP_BESIDE;
	if (size >= 1 && intr[0] != 0) {
		intrmode |= (u8)IST8309_VAL_INTCNTL2_INTONX_INT_MODE_ON;
	}
	if (size >= 2 && intr[1] != 0) {
		intrmode |= (u8)IST8309_VAL_INTCNTL2_INTONY_INT_MODE_ON;
	}
	if (size >= 3 && intr[2] != 0)  {
		intrmode |= (u8)IST8309_VAL_INTCNTL2_INTONZ_INT_MODE_ON;
	}
	ist8309_data->reg_intrmode = intrmode; // always rewrite to enable intr, even reg_intrmode is unchanged.
	buffer[0] = IST8309_REG_INTCNTL2;
	buffer[1] = ist8309_data->reg_intrmode;
	dev_info(&ist->i2c->dev, "IST8309_REG_INTCNTL2 reg_intrmode=0x%02X", ist8309_data->reg_intrmode);
	ret = ist_i2c_txdata(ist->i2c, buffer, 2);
	if (ret < 0) {
		dev_err(&ist->i2c->dev, "config interupt fail %d", ret);
		mutex_unlock(&ist->lock);
		return ret;
	}
	mutex_unlock(&ist->lock);
	dev_info(&ist->i2c->dev, "ist8309_set_interrupts done");
	return 0;
}

static int ist8309_get_interrupts(struct ist_base *ist, int* intr_type, int* intr, size_t size)
{
	struct ist8309_data *ist8309_data = (struct ist8309_data*)ist;
	int count;
	u8 reg_intrmode = ist8309_data->reg_intrmode;

	*intr_type = (reg_intrmode & IST8309_VAL_INTCNTL2_INTTYP_WITHIN) ? INTERRUPT_WITHIN : INTERRUPT_BESIDE;

	count = 0;
	if (size >= 1) {
		intr[0] = !!(reg_intrmode & IST8309_VAL_INTCNTL2_INTONX_INT_MODE_ON);
		count = 1;
	}
	if (size >= 2) {
		intr[1] = !!(reg_intrmode & IST8309_VAL_INTCNTL2_INTONY_INT_MODE_ON);
		count = 2;
	}
	if (size >= 3) {
		intr[2] = !!(reg_intrmode & IST8309_VAL_INTCNTL2_INTONZ_INT_MODE_ON);
		count = 3;
	}

	return count;
}

#if !USE_SINGLE_MEASURE
struct odr_reg {
	u32 delay_ms;
	u8 reg_val;
}  odr_table[] = {
	{   5, IST8309_VAL_CNTL1_ODR_200HZ},
	{  10, IST8309_VAL_CNTL1_ODR_100HZ},
	{  20, IST8309_VAL_CNTL1_ODR_50HZ},
	{  25, IST8309_VAL_CNTL1_ODR_40HZ},
	{  50, IST8309_VAL_CNTL1_ODR_20HZ},
	{ 100, IST8309_VAL_CNTL1_ODR_10HZ},
	{ 149, IST8309_VAL_CNTL1_ODR_6_7HZ},
	{ 200, IST8309_VAL_CNTL1_ODR_5HZ},
	{1000, IST8309_VAL_CNTL1_ODR_1HZ},
};
#define ODR_TABLE_SIZE (sizeof(odr_table)/sizeof(odr_table[0]))

static u8 delay_to_odr(struct ist_base *ist, u32 delay_msec) {
	int i=0;
	for (i=0; i<ODR_TABLE_SIZE; i++) {
		if (delay_msec <= odr_table[i].delay_ms)
			return odr_table[i].reg_val;
	}
	dev_info(&ist->i2c->dev, "bad delay_msec=%d. use ODR 50HZ", delay_msec);
	return odr_table[ODR_TABLE_SIZE-1].reg_val;
}

static int ist8309_set_delay(struct ist_base *ist, int delay_msec)
{
	u8 odr;
	u8 buffer[2];

	mutex_lock(&ist->lock);
	odr = delay_to_odr(ist, delay_msec);
	dev_info(&ist->i2c->dev, "delay_msec=%d. ODR reg=0x%02X", delay_msec, odr);
	buffer[0] = IST8309_REG_CNTL1;
	buffer[1] = odr;
	if (ist_i2c_txdata(ist->i2c, buffer, 2)) {
		dev_warn(&ist->i2c->dev, "write reg IST8309_REG_CNTL1 failed");
	}
	mutex_unlock(&ist->lock);
	return 0;
}

#else
static int ist8309_set_delay(struct ist_base *ist, int delay_msec)
{
	return 0;
}
#endif

static void ist8309_poll_func(struct ist_base *ist)
{
	int16_t xyz[4] = { 0 };
	int count;
#if USE_SINGLE_MEASURE
	int rc;
	unsigned char buffer[2];
	unsigned retry = 3;
#endif
	count = ist8309_read_data(ist, xyz);
	if (count < 0) {
		dev_info(&ist->i2c->dev, "ist8309_read_data failed\n");
	} else {
		ist_report_data(ist, xyz, count);
#if USE_SINGLE_MEASURE
		//trigger next measurement
		rc = 0;
		while (retry-- > 0) {
			buffer[0] = IST8309_REG_CNTL1;
			rc = ist_i2c_rxdata(ist->i2c, buffer, 1);
			if (rc < 0) {
				dev_err(&ist->i2c->dev, "%s:fail to read IST8309_REG_CNTL1, rc=%d\n", __func__, rc);
			} else {
				if (buffer[0] == IST8309_MODE_POWERDOWN) {
					buffer[0] = IST8309_REG_CNTL1;
					buffer[1] = IST8309_MODE_SNG_MEASURE;
					rc = ist_i2c_txdata(ist->i2c, buffer, 2);
					if (!rc) {
						break;
					}
					dev_err(&ist->i2c->dev, "%s: SET SINGLE MODE retry. rc=%d\n", __func__, rc);
				} else {
					rc = -1;
				}
			}
			usleep_range(1000, 1000);
		}
		if (rc) {
			dev_err(&ist->i2c->dev, "%s:fail to set IST8309_REG_CNTL1, rc=%d\n", __func__, rc);
		}
#endif
	}
}

static int ist8309_read_once(struct ist_base *ist, int16_t vec[4])
{
	int rc;

	mutex_lock(&ist->lock);
	rc = ist8309_set_mode(ist, IST8309_MODE_SNG_MEASURE);
	if (rc < 0) {
		dev_err(&ist->i2c->dev, "fail to set single mode");
		goto fn_exit;
	}
	rc = wait_conversion_complete_polled(ist);
	if (rc < 0) {
		goto fn_exit;
	}

	rc = ist8309_read_data(ist, vec);
	if (rc < 0) {
		dev_info(&ist->i2c->dev, "ist8309_read_once failed\n");
	}

fn_exit:
	mutex_unlock(&ist->lock);
	return rc;
}

static int ist8309_set_threshold_by_axis(struct ist_base *ist, int axis, short lowthd, short highthd)
{
	u8 lthh, lthl, hthh, hthl;
	u8 buffer[5];
	unsigned char reg = 0;
	int rc = 0;

	dev_info(&ist->i2c->dev,"axis[%d] low: %d, high: %d", axis, lowthd, highthd);

	ist_short_to_2byte(highthd, &hthh, &hthl);
	ist_short_to_2byte(lowthd, &lthh, &lthl);
	
	switch(axis)
	{
		case 0:
			reg = IST8309_REG_LTH_XL;
			break;
		case 1:
			reg = IST8309_REG_LTH_YL;
			break;
		case 2:
			reg = IST8309_REG_LTH_ZL;
			break;
		case 3:
			reg = IST8309_REG_LTH_AL;
			break;
		default:
			reg = IST8309_REG_LTH_XL;
			break;
	}

	buffer[0] = reg;
	buffer[1] = lthl;
	buffer[2] = lthh;
	buffer[3] = hthl;
	buffer[4] = hthh;
	rc = ist_i2c_txdata(ist->i2c, buffer, 5);
	if (rc < 0) {
		dev_err(&ist->i2c->dev, "set_threshold[%d] fail rc=%d", axis, rc);
		return rc;
	} else {
		return 0;
	}
}

static int ist8309_get_threshold_by_axis(struct ist_base *ist, int axis, short* low, short* high)
{
	u8 data[4];
	unsigned char reg = 0;
	int rc = 0;

	switch(axis)
	{
		case 0:
			reg = IST8309_REG_LTH_XL;
			break;
		case 1:
			reg = IST8309_REG_LTH_YL;
			break;
		case 2:
			reg = IST8309_REG_LTH_ZL;
			break;
		case 3:
			reg = IST8309_REG_LTH_AL;
			break;
		default:
			reg = IST8309_REG_LTH_XL;
			break;
	}

	data[0] = reg;
	rc = ist_i2c_rxdata(ist->i2c, data, 4);

	if (rc < 0) {
		dev_err(&ist->i2c->dev, "get_threshold[%d] fail rc=%d", axis, rc);
		return rc;
	} else {
		*low = data[1] << 8 | data[0];
		*high = data[3] << 8 | data[2];
		dev_info(&ist->i2c->dev, "get_threshold[%d] [%02X %02X %02X %02X] [%d,%d]", axis, data[0], data[1], data[2], data[3], *low, *high);
		return 0;
	}
}

static int ist8309_set_thresholds(struct ist_base *ist, short* low, short* high, size_t count)
{
	int i, rc;
	for (i=0; i < 3 && i < count; i++) {
		rc = ist8309_set_threshold_by_axis(ist, i, low[i], high[i]);
		if (rc < 0) {
			return rc;
		}
	}
	return i;
}

static int ist8309_get_thresholds(struct ist_base *ist, short* low, short* high, size_t count)
{
	int i, rc;
	for (i=0; i < 3 && i < count; i++) {
		rc = ist8309_get_threshold_by_axis(ist, i, &low[i], &high[i]);
		if (rc < 0) {
			return rc;
		}
	}
	return i;
}

static int ist8309_clear_interrupt(struct ist_base *ist)
{
    int err = 0;
	u8 buffer[2];
	struct ist8309_data *ist8309_data = (struct ist8309_data*)ist;

	ist8309_data->reg_persint = ist8309_data->reg_persint | 0x01; // clear interrupt
	buffer[0] = IST8309_REG_INTCNTL1;
	buffer[1] = ist8309_data->reg_persint;
	dev_info(&ist->i2c->dev, "clear interrupt: value:0x%x", buffer[1]);
	err = ist_i2c_txdata(ist->i2c, buffer, 2);
	if (err < 0) {
		dev_err(&ist->i2c->dev, "clear interupt fail %d", err);
		return err;
	}

	ist8309_data->reg_persint = ist8309_data->reg_persint & 0xfe; // enable interrupt
	buffer[0] = IST8309_REG_INTCNTL1;
	buffer[1] = ist8309_data->reg_persint;
	dev_info(&ist->i2c->dev, "set interrupt: value:0x%x", buffer[1]);
	err = ist_i2c_txdata(ist->i2c, buffer, 2);
	if (err < 0) {
		dev_err(&ist->i2c->dev, "set interupt fail %d", err);
		return err;
	}
	return 0;
}

static void ist8309_irq_delayedwork(struct ist_base *ist)
{
	u8 data;
	int rc = 0;

	dev_info(&ist->i2c->dev, "ist8309_irq_delayedwork enter");

	/* read interrupt status */
	data = IST8309_REG_STAT1;
	rc = ist_i2c_rxdata(ist->i2c, &data, 1);
	mutex_lock(&ist->lock);
	if (!rc) {
		u8 value = ((~data) & 0xE0) >> 5; // 0 -> interrupt occurs, negate to 1
		dev_info(&ist->i2c->dev, "interrupted axis=0x%02X, data=(0x%02X)", value, data);
		ist_report_interrupt(ist, value, 3); // 3 axis
	} else {
		dev_err(&ist->i2c->dev, "ist_i2c_rxdata failed.(%d)\n", rc);
	}
	mutex_unlock(&ist->lock);

	dev_info(&ist->i2c->dev, "ist8309_irq_delayedwork leave");
}

static int ist8309_enable_interrupt(struct ist_base *ist, int enable)
{
	u8 buffer[2];
	int err = 0;
	u8 off_bits;
	struct ist8309_data *ist8309_data = (struct ist8309_data*)ist;

	//dev_info(&ist->i2c->dev, "ist8309_enable_interrupt, enable=%d", enable);
	if (enable) {
		buffer[0] = IST8309_REG_INTCNTL2;
		buffer[1] = ist8309_data->reg_intrmode;
		err = ist_i2c_txdata(ist->i2c, buffer, 2);
		if (err < 0) {
			dev_err(&ist->i2c->dev, "config interupt fail %d", err);
			return err;
		}

		err = ist8309_clear_interrupt(ist);
		if (err < 0) {
			dev_err(&ist->i2c->dev, "clear interupt fail %d", err);
			return err;
		}
	} else {
		off_bits = ((IST8309_VAL_INTCNTL2_INTONX_INT_MODE_ON |
					IST8309_VAL_INTCNTL2_INTONY_INT_MODE_ON |
					IST8309_VAL_INTCNTL2_INTONZ_INT_MODE_ON) & 0xFF);
		off_bits = ~off_bits; // turn off bits
		buffer[0] = IST8309_REG_INTCNTL2;
		buffer[1] = ist8309_data->reg_intrmode & off_bits;
		err = ist_i2c_txdata(ist->i2c, buffer, 2);
		if (err < 0) {
			dev_err(&ist->i2c->dev, "config interupt fail %d",err);
			return err;
		}
	}
	return 0;
}

static int ist8309_set_enable(struct ist_base *ist, int enable)
{
	unsigned char buffer[2];
	int rc;

#if USE_SINGLE_MEASURE
	if (enable) {
		buffer[0] = IST8309_REG_CNTL1;
		buffer[1] = IST8309_MODE_SNG_MEASURE;
		rc = ist_i2c_rxdata(ist->i2c, buffer, 1);
		if (rc) {
			dev_err(&ist->i2c->dev, "%s:fail to read reg IST8309_REG_CNTL1\n", __func__);
			//return rc;
		} else {
			if (buffer[0] == IST8309_MODE_POWERDOWN) {
				buffer[0] = IST8309_REG_CNTL1;
				buffer[1] = IST8309_MODE_SNG_MEASURE;
			} else {
				// do nothing
				dev_info(&ist->i2c->dev, "%s: not in standby mode. Do nothing\n", __func__);
				usleep_range(5000, 10000);
				goto SKIP_WRITE_REG;
			}
		}
	} else {
		usleep_range(5000, 10000);
		buffer[0] = IST8309_REG_CNTL1;
		buffer[1] = IST8309_MODE_POWERDOWN;
	}
	rc = ist_i2c_txdata(ist->i2c, buffer, 2);
	if (rc < 0) {
		dev_warn(&ist->i2c->dev, "write reg IST8309_REG_CNTL1 failed at %d\n", __LINE__);
	}

SKIP_WRITE_REG:
#else
	buffer[0] = IST8309_REG_CNTL1;
	if (enable) {
		buffer[1] = delay_to_odr(ist, ist->poll_delay_ms);
	} else {
		buffer[1] = IST8309_MODE_POWERDOWN;
	}
	rc = ist_i2c_txdata(ist->i2c, buffer, 2);
	if (rc < 0) {
		dev_warn(&ist->i2c->dev, "write reg IST8309_REG_CNTL1 failed at %d\n", __LINE__);
	}
#endif
	return 0;
}

/*
 * IST8309 Driver Functions
 *
 */
static int ist8309_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int res = 0;
	struct ist_base *ist = NULL;

	dev_info(&client->dev, "probing %s\n", IST8309_I2C_NAME);

	ist = ist_alloc(client,
			(unsigned)(sizeof(struct ist8309_data)-sizeof(struct ist_base)),
			IST8309_I2C_NAME);
	if (!ist) {
		dev_err(&client->dev, "memory allocation failed.\n");
		res = -ENOMEM;
		goto out_probe_fail;
	}

	ist->scale_val_int = 0;
	ist->scale_val_micro = IST8309_RAW_TO_MICRO_UT;
	ist->timed_poll_func = ist8309_poll_func;
	ist->set_enable = ist8309_set_enable;
	ist->read_once = ist8309_read_once;
	ist->set_delay = ist8309_set_delay;
	ist->set_interrupts = ist8309_set_interrupts;
	ist->get_interrupts = ist8309_get_interrupts;
	ist->set_thresholds = ist8309_set_thresholds;
	ist->get_thresholds = ist8309_get_thresholds;
	ist->enable_interrupt = ist8309_enable_interrupt;
	ist->clear_interrupt = ist8309_clear_interrupt;
	ist->irq_delayedwork = ist8309_irq_delayedwork;
	ist->use_irq = 1; // force using irq

	res = ist8309_check_device(ist);
	if (res) {
		dev_err(&client->dev, "check device failed\n");
		goto out_probe_fail;
	}

	res = ist_common_init(ist);
	if (res) {
		dev_err(&client->dev, "common init failed\n");
		goto out_probe_fail;
	}

	res = ist8309_chip_reset(ist);
	if (res) {
		dev_err(&client->dev, "reset device failed\n");
		goto out_free_ist;
	}

	res = ist8309_chip_init(ist);
	if (res) {
		dev_err(&client->dev, "init device failed\n");
		goto out_free_ist;
	}

	if (ist->use_irq) {
		res = ist_setup_eint(ist);
		if (res) {
			dev_err(&client->dev, "setup eint failed\n");
			res = -ENODEV;
			goto out_free_ist;
		}
	}
	
	dev_info(&client->dev, IST8309_I2C_NAME " probe done\n");

	return 0;

out_free_ist:
	if (ist)
		ist_common_uninit(ist);
out_probe_fail:
	return res;
}

static int ist8309_remove(struct i2c_client *client)
{
	return ist_remove(client);
}

static int ist8309_suspend(struct device *dev)
{
	return ist_suspend(dev);
}

static int ist8309_resume(struct device *dev)
{
	return ist_resume(dev);
}

static const struct i2c_device_id ist8309_id[] = {
	{ IST8309_I2C_NAME, 0 },
	{ }
};

static struct of_device_id ist8309_match_table[] = {
	{ .compatible = "isentek,ist8309", },
	{ },
};

static const struct dev_pm_ops ist8309_pm_ops = {
	.suspend = ist8309_suspend,
	.resume = ist8309_resume,
};

static struct i2c_driver ist8309_driver = {
	.probe 		= ist8309_probe,
	.remove 	= ist8309_remove,
	.id_table	= ist8309_id,
	.driver 	= {
		.owner	= THIS_MODULE,
		.name	= IST8309_I2C_NAME,
		.of_match_table = ist8309_match_table,
		.pm = &ist8309_pm_ops,
	},
};

static int __init ist8309_init(void)
{
	return i2c_add_driver(&ist8309_driver);
}

static void __exit ist8309_exit(void)
{
	i2c_del_driver(&ist8309_driver);
}

MODULE_DESCRIPTION("Isentek IST8309 Magnetic Sensor Driver");
MODULE_LICENSE("GPL");

late_initcall(ist8309_init);
module_exit(ist8309_exit);
