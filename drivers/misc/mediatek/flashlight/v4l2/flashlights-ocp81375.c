/*
* Copyright (C) 2022 Awinic Inc.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/of.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include "flashlight.h"
#include "flashlight-dt.h"
#include "flashlight-core.h"

#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>

/* device tree should be defined in flashlight-dt.h */
#ifndef OCP81375_DTNAME
#define OCP81375_DTNAME "mediatek,flashlights_ocp81375"
#endif
#ifndef OCP81375_DTNAME_I2C
#define OCP81375_DTNAME_I2C "mediatek,flashlights_ocp81375"
#endif
#define OCP81375_NAME "flashlights-ocp81375"

#define OCP81375_DRIVER_VERSION "V1.1.0"

/* define registers */
#define OCP81375_REG_ENABLE           (0x01)
#define OCP81375_MASK_ENABLE_LED1     (0x01)
#define OCP81375_MASK_ENABLE_LED2     (0x02)
#define OCP81375_DISABLE              (0x00)
#define OCP81375_ENABLE_LED1          (0x01)
#define OCP81375_ENABLE_LED1_TORCH    (0x09)
#define OCP81375_ENABLE_LED1_FLASH    (0x0D)
#define OCP81375_ENABLE_LED2          (0x02)
#define OCP81375_ENABLE_LED2_TORCH    (0x0A)
#define OCP81375_ENABLE_LED2_FLASH    (0x0E)

#define OCP81375_REG_TORCH_LEVEL_LED1 (0x05)
#define OCP81375_REG_FLASH_LEVEL_LED1 (0x03)
#define OCP81375_REG_TORCH_LEVEL_LED2 (0x06)
#define OCP81375_REG_FLASH_LEVEL_LED2 (0x04)

#define OCP81375_REG_TIMING_CONF      (0x08)
#define OCP81375_TORCH_RAMP_TIME      (0x10)
#define OCP81375_FLASH_TIMEOUT        (0x0F)
#define OCP81375_REG_DEVICE_ID        (0x0C)
#define OCP81375_DEVICE_ID            (0x3A)
#define OCP81375_REG_FLAG1            (0x0A)
#define OCP81375_REG_FLAG2            (0x0B)

/* define channel, level */
#define OCP81375_CHANNEL_NUM          2
#define OCP81375_CHANNEL_CH1          0
#define OCP81375_CHANNEL_CH2          1
#define OCP81375_LEVEL_NUM            26
#define OCP81375_LEVEL_TORCH          7


#define CONFIG_PRIZE_SOFT_LED_ENABLE 1
#define CONFIG_PRIZE_SHUT_CTRL 0
#define OCP81375_DEBUG(format, args...) printk(KERN_WARNING format, ##args)

/* define pinctrl */
#define OCP81375_PINCTRL_PIN_HWEN 8
#define OCP81375_PINCTRL_PINSTATE_LOW 0
#define OCP81375_PINCTRL_PINSTATE_HIGH 1
#define OCP81375_PINCTRL_STATE_HWEN_HIGH "ocp81375_hwen_1"
#define OCP81375_PINCTRL_STATE_HWEN_LOW  "ocp81375_hwen_0"
static struct pinctrl *ocp81375_pinctrl;
static struct pinctrl_state *ocp81375_hwen_high;
static struct pinctrl_state *ocp81375_hwen_low;

/* prize add define pinctrl for shut ctrl */
static struct pinctrl_state *ir_3v3_high;
static struct pinctrl_state *ir_3v3_low;
#if 0
static struct pinctrl_state *shut_in1_high;
static struct pinctrl_state *shut_in1_low;
static struct pinctrl_state *shut_in2_high;
static struct pinctrl_state *shut_in2_low;
#endif

//prize add by zhaolong for tc358xx power 20240626 start
static struct pinctrl_state *tc358xx_power_on_high;
static struct pinctrl_state *tc358xx_power_on_low;
//prize add by zhaolong for tc358xx power 20240626 end
static struct pinctrl_state *cam_i2c2_function;
static struct pinctrl_state *cam_i2c2_gpio;

/* define mutex and work queue */
static DEFINE_MUTEX(ocp81375_mutex);
static struct work_struct ocp81375_work_ch1;
static struct work_struct ocp81375_work_ch2;

struct i2c_client *ocp81375_flashlight_client;

/* define usage count */
static int use_count;

/* define i2c */
static struct i2c_client *ocp81375_i2c_client;

/* platform data
* torch_pin_enable: TX1/TORCH pin isa hardware TORCH enable
* pam_sync_pin_enable: TX2 Mode The ENVM/TX2 is a PAM Sync. on input
* thermal_comp_mode_enable: LEDI/NTC pin in Thermal Comparator Mode
* strobe_pin_disable: STROBE Input disabled
* vout_mode_enable: Voltage Out Mode enable
*/
struct ocp81375_platform_data {
	u8 torch_pin_enable;
	u8 pam_sync_pin_enable;
	u8 thermal_comp_mode_enable;
	u8 strobe_pin_disable;
	u8 vout_mode_enable;

	int channel_num;
	struct flashlight_device_id *dev_id;
};

/* ocp81375 chip data */
struct ocp81375_chip_data {
	struct i2c_client *client;
	struct ocp81375_platform_data *pdata;
	struct mutex lock;
	u8 last_flag;
	u8 no_pdata;
};

static struct platform_device *g_ocp81375_pdev;
static int led_num = -1;


/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
int flashlight_gpio_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	ocp81375_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(ocp81375_pinctrl)) {
		pr_err("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(ocp81375_pinctrl);
	}

	/* Flashlight HWEN pin initialization */
	ocp81375_hwen_high = pinctrl_lookup_state(ocp81375_pinctrl, OCP81375_PINCTRL_STATE_HWEN_HIGH);
	if (IS_ERR(ocp81375_hwen_high)) {
		pr_err("Failed to init (%s)\n", OCP81375_PINCTRL_STATE_HWEN_HIGH);
		ret = PTR_ERR(ocp81375_hwen_high);
	}
	ocp81375_hwen_low = pinctrl_lookup_state(ocp81375_pinctrl, OCP81375_PINCTRL_STATE_HWEN_LOW);
	if (IS_ERR(ocp81375_hwen_low)) {
		pr_err("Failed to init (%s)\n", OCP81375_PINCTRL_STATE_HWEN_LOW);
		ret = PTR_ERR(ocp81375_hwen_low);
	}

	/* prize add define pinctrl for shut ctrl 20240527 start */
	ir_3v3_high = pinctrl_lookup_state(ocp81375_pinctrl, "ir_3v3_1");
	if (IS_ERR(ir_3v3_high)) {
		pr_err("Failed to init (%s)\n", "ir_3v3_1");
		ret = PTR_ERR(ir_3v3_high);
	}
	ir_3v3_low = pinctrl_lookup_state(ocp81375_pinctrl, "ir_3v3_0");
	if (IS_ERR(ir_3v3_low)) {
		pr_err("Failed to init (%s)\n", "ir_3v3_0");
		ret = PTR_ERR(ir_3v3_low);
	}

#if 0
	shut_in1_high = pinctrl_lookup_state(ocp81375_pinctrl, "shut_in1_1");
	if (IS_ERR(shut_in1_high)) {
		pr_err("Failed to init (%s)\n", "shut_in1_1");
		ret = PTR_ERR(shut_in1_high);
	}
	shut_in1_low = pinctrl_lookup_state(ocp81375_pinctrl, "shut_in1_0");
	if (IS_ERR(shut_in1_low)) {
		pr_err("Failed to init (%s)\n", "shut_in1_0");
		ret = PTR_ERR(shut_in1_low);
	}

	shut_in2_high = pinctrl_lookup_state(ocp81375_pinctrl, "shut_in2_1");
	if (IS_ERR(shut_in2_high)) {
		pr_err("Failed to init (%s)\n", "shut_in2_1");
		ret = PTR_ERR(shut_in2_high);
	}
	shut_in2_low = pinctrl_lookup_state(ocp81375_pinctrl, "shut_in2_0");
	if (IS_ERR(shut_in2_low)) {
		pr_err("Failed to init (%s)\n", "shut_in2_0");
		ret = PTR_ERR(shut_in2_low);
	}
#endif
	/* prize add define pinctrl for shut ctrl 20240527 end */

	//prize add by zhaolong for tc358xx power 20240626 start
	tc358xx_power_on_high = pinctrl_lookup_state(ocp81375_pinctrl, "tc358xx_power_on_1");
	if (IS_ERR(tc358xx_power_on_high)) {
		pr_err("Failed to init (%s)\n", "tc358xx_power_on_1");
		ret = PTR_ERR(tc358xx_power_on_high);
	}
	tc358xx_power_on_low = pinctrl_lookup_state(ocp81375_pinctrl, "tc358xx_power_on_0");
	if (IS_ERR(tc358xx_power_on_low)) {
		pr_err("Failed to init (%s)\n", "tc358xx_power_on_0");
		ret = PTR_ERR(tc358xx_power_on_low);
	}
	//prize add by zhaolong for tc358xx power 20240626 end

	cam_i2c2_function = pinctrl_lookup_state(ocp81375_pinctrl, "cam_i2c2_1");
	if (IS_ERR(cam_i2c2_function)) {
		pr_err("Failed to init (%s)\n", "cam_i2c2_1");
		ret = PTR_ERR(cam_i2c2_function);
	}
	cam_i2c2_gpio = pinctrl_lookup_state(ocp81375_pinctrl, "cam_i2c2_0");
	if (IS_ERR(cam_i2c2_gpio)) {
		pr_err("Failed to init (%s)\n", "cam_i2c2_0");
		ret = PTR_ERR(cam_i2c2_gpio);
	}
	return ret;
}

int flashlight_pinctrl_set(int pin, int state)
{
	int ret = 0;

	if (IS_ERR(ocp81375_pinctrl)) {
		pr_err("pinctrl is not available\n");
		return -1;
	}

	switch (pin) {
	case OCP81375_PINCTRL_PIN_HWEN:
		if (state == OCP81375_PINCTRL_PINSTATE_LOW && !IS_ERR(ocp81375_hwen_low))
			pinctrl_select_state(ocp81375_pinctrl, ocp81375_hwen_low);
		else if (state == OCP81375_PINCTRL_PINSTATE_HIGH && !IS_ERR(ocp81375_hwen_high))
			pinctrl_select_state(ocp81375_pinctrl, ocp81375_hwen_high);
		else
			pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	default:
		pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	}
	OCP81375_DEBUG("pin(%d) state(%d)\n", pin, state);

	return ret;
}


/******************************************************************************
 * ocp81375 operations
 *****************************************************************************/
static const unsigned char ocp81375_torch_level[OCP81375_LEVEL_NUM] = {
	0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// flash:duty * 7.83 + 3.91   MAX:2A
static const unsigned char ocp81375_flash_level[OCP81375_LEVEL_NUM] = {
	0x04, 0x08, 0x0c, 0x10, 0x04, 0x18, 0x1c, 0x26, 0x31, 0x3c,
	0x47, 0x52, 0x5d, 0x68, 0x73, 0x7e, 0x89, 0x94, 0x9f, 0xaa,
	0xb5, 0xC0, 0xcb, 0xd6, 0xe1, 0xec};

static volatile unsigned char ocp81375_reg_enable;
static volatile int ocp81375_level_ch1 = -1;
static volatile int ocp81375_level_ch2 = -1;

static int ocp81375_is_torch(int level)
{
	if (level >= OCP81375_LEVEL_TORCH)
		return -1;

	return 0;
}

static int ocp81375_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= OCP81375_LEVEL_NUM)
		level = OCP81375_LEVEL_NUM - 1;

	return level;
}

/* i2c wrapper function */
static int ocp81375_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct ocp81375_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_err("failed writing at 0x%02x\n", reg);

	return ret;
}

static int ocp81375_read_reg(struct i2c_client *client, u8 reg)
{
	int val;
	struct ocp81375_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	val = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	if (val < 0)
		pr_err("failed read at 0x%02x\n", reg);

	return val;
}

/* flashlight enable function */
static int ocp81375_enable_ch1(void)
{
	unsigned char reg, val;

	reg = OCP81375_REG_ENABLE;
	if (!ocp81375_is_torch(ocp81375_level_ch1)) {
		/* torch mode */
		ocp81375_reg_enable |= OCP81375_ENABLE_LED1_TORCH;
	} else {
		/* flash mode */
		ocp81375_reg_enable |= OCP81375_ENABLE_LED1_FLASH;
	}

	val = ocp81375_reg_enable;

	return ocp81375_write_reg(ocp81375_i2c_client, reg, val);
}

static int ocp81375_enable_ch2(void)
{
	unsigned char reg, val;

	reg = OCP81375_REG_ENABLE;
	if (!ocp81375_is_torch(ocp81375_level_ch2)) {
		/* torch mode */
		ocp81375_reg_enable |= OCP81375_ENABLE_LED2_TORCH;
	} else {
		/* flash mode */
		ocp81375_reg_enable |= OCP81375_ENABLE_LED2_FLASH;
	}
	val = ocp81375_reg_enable;

	return ocp81375_write_reg(ocp81375_i2c_client, reg, val);
}

/* flashlight disable function */
static int ocp81375_disable_ch1(void)
{
	unsigned char reg, val;

	reg = OCP81375_REG_ENABLE;
	if (ocp81375_reg_enable & OCP81375_MASK_ENABLE_LED2) {
		/* if LED 2 is enable, disable LED 1 */
		ocp81375_reg_enable &= (~OCP81375_ENABLE_LED1);
	} else {
		/* if LED 2 is enable, disable LED 1 and clear mode */
		ocp81375_reg_enable &= (~OCP81375_ENABLE_LED1_FLASH);
	}
	val = ocp81375_reg_enable;

	return ocp81375_write_reg(ocp81375_i2c_client, reg, val);
}

static int ocp81375_disable_ch2(void)
{
	unsigned char reg, val;

	reg = OCP81375_REG_ENABLE;
	if (ocp81375_reg_enable & OCP81375_MASK_ENABLE_LED1) {
		/* if LED 1 is enable, disable LED 2 */
		ocp81375_reg_enable &= (~OCP81375_ENABLE_LED2);
	} else {
		/* if LED 1 is enable, disable LED 2 and clear mode */
		ocp81375_reg_enable &= (~OCP81375_ENABLE_LED2_FLASH);
	}
	val = ocp81375_reg_enable;

	return ocp81375_write_reg(ocp81375_i2c_client, reg, val);
}

static int ocp81375_enable(int channel)
{
	if (channel == OCP81375_CHANNEL_CH1)
		ocp81375_enable_ch1();
	else if (channel == OCP81375_CHANNEL_CH2)
		ocp81375_enable_ch2();
	else {
		pr_err("%s, Error channel\n", __func__);
		return -1;
	}

	return 0;
}

static int ocp81375_disable(int channel)
{
	if (channel == OCP81375_CHANNEL_CH1)
		ocp81375_disable_ch1();
	else if (channel == OCP81375_CHANNEL_CH2)
		ocp81375_disable_ch2();
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

/* set flashlight level */
static int ocp81375_set_level_ch1(int level)
{
	int ret;
	unsigned char reg, val;

	level = ocp81375_verify_level(level);

	/* set torch brightness level */
	reg = OCP81375_REG_TORCH_LEVEL_LED1;
	val = ocp81375_torch_level[level];
	ret = ocp81375_write_reg(ocp81375_i2c_client, reg, val);

	ocp81375_level_ch1 = level;

	/* set flash brightness level */
	reg = OCP81375_REG_FLASH_LEVEL_LED1;
	val = ocp81375_flash_level[level];
	ret = ocp81375_write_reg(ocp81375_i2c_client, reg, val);

	return ret;
}

int ocp81375_set_level_ch2(int level)
{
	int ret;
	unsigned char reg, val;

	level = ocp81375_verify_level(level);

	/* set torch brightness level */
	reg = OCP81375_REG_TORCH_LEVEL_LED2;
	val = ocp81375_torch_level[level];
	ret = ocp81375_write_reg(ocp81375_i2c_client, reg, val);

	ocp81375_level_ch2 = level;

	/* set flash brightness level */
	reg = OCP81375_REG_FLASH_LEVEL_LED2;
	val = ocp81375_flash_level[level];
	ret = ocp81375_write_reg(ocp81375_i2c_client, reg, val);

	return ret;
}

static int ocp81375_set_level(int channel, int level)
{
	if (channel == OCP81375_CHANNEL_CH1)
		ocp81375_set_level_ch1(level);
	else if (channel == OCP81375_CHANNEL_CH2)
		ocp81375_set_level_ch2(level);
	else {
		pr_err("%s, Error channel\n", __func__);
		return -1;
	}

	return 0;
}

/* flashlight init */
int ocp81375_init(void)
{
	int ret;
	unsigned char reg, val;

	flashlight_pinctrl_set(OCP81375_PINCTRL_PIN_HWEN, OCP81375_PINCTRL_PINSTATE_HIGH);

	usleep_range(2000, 2500);

	/* clear enable register */
	reg = OCP81375_REG_ENABLE;
	val = OCP81375_DISABLE;
	ret = ocp81375_write_reg(ocp81375_i2c_client, reg, val);

	ocp81375_reg_enable = val;

	/* set torch current ramp time and flash timeout */
	reg = OCP81375_REG_TIMING_CONF;
	val = OCP81375_TORCH_RAMP_TIME | OCP81375_FLASH_TIMEOUT;
	ret = ocp81375_write_reg(ocp81375_i2c_client, reg, val);

	return ret;
}

/* flashlight uninit */
int ocp81375_uninit(void)
{
	ocp81375_disable(OCP81375_CHANNEL_CH1);
	ocp81375_disable(OCP81375_CHANNEL_CH2);

	flashlight_pinctrl_set(OCP81375_PINCTRL_PIN_HWEN, OCP81375_PINCTRL_PINSTATE_LOW);

	return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer ocp81375_timer_ch1;
static struct hrtimer ocp81375_timer_ch2;
static unsigned int ocp81375_timeout_ms[OCP81375_CHANNEL_NUM];

static void ocp81375_work_disable_ch1(struct work_struct *data)
{
	OCP81375_DEBUG("ht work queue callback\n");
	ocp81375_disable_ch1();
}

static void ocp81375_work_disable_ch2(struct work_struct *data)
{
	printk("lt work queue callback\n");
	ocp81375_disable_ch2();
}

static enum hrtimer_restart ocp81375_timer_func_ch1(struct hrtimer *timer)
{
	schedule_work(&ocp81375_work_ch1);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart ocp81375_timer_func_ch2(struct hrtimer *timer)
{
	schedule_work(&ocp81375_work_ch2);
	return HRTIMER_NORESTART;
}


int ocp81375_timer_start(int channel, ktime_t ktime)
{
	if (channel == OCP81375_CHANNEL_CH1)
		hrtimer_start(&ocp81375_timer_ch1, ktime, HRTIMER_MODE_REL);
	else if (channel == OCP81375_CHANNEL_CH2)
		hrtimer_start(&ocp81375_timer_ch2, ktime, HRTIMER_MODE_REL);
	else {
		pr_err("%s, Error channel\n", __func__);
		return -1;
	}

	return 0;
}

int ocp81375_timer_cancel(int channel)
{
	if (channel == OCP81375_CHANNEL_CH1)
		hrtimer_cancel(&ocp81375_timer_ch1);
	else if (channel == OCP81375_CHANNEL_CH2)
		hrtimer_cancel(&ocp81375_timer_ch2);
	else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int ocp81375_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	/* verify channel */
	if (channel < 0 || channel >= OCP81375_CHANNEL_NUM) {
		pr_err("%s, Failed with error channel\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_info("%s, FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				__func__, channel, (int)fl_arg->arg);
		ocp81375_timeout_ms[channel] = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_info("%s. FLASH_IOC_SET_DUTY(%d): %d\n",
				__func__, channel, (int)fl_arg->arg);
		ocp81375_set_level(channel, fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_info("%s. FLASH_IOC_SET_ONOFF(%d): %d\n",
				__func__, channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (ocp81375_timeout_ms[channel]) {
				ktime =
				ktime_set(ocp81375_timeout_ms[channel] / 1000,
				(ocp81375_timeout_ms[channel] % 1000) * 1000000);
				ocp81375_timer_start(channel, ktime);
			}
			ocp81375_enable(channel);
		} else {
			ocp81375_disable(channel);
			ocp81375_timer_cancel(channel);
		}
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int ocp81375_open(void)
{
	/* Actual behavior move to set driver function */
	/*since power saving issue */
	return 0;
}

static int ocp81375_release(void)
{
	/* uninit chip and clear usage count */
	mutex_lock(&ocp81375_mutex);
	use_count--;
	if (!use_count)
		ocp81375_uninit();
	if (use_count < 0)
		use_count = 0;
	mutex_unlock(&ocp81375_mutex);

	pr_info("Release: %d\n", use_count);

	return 0;
}

static int ocp81375_set_driver(int set)
{
	/* init chip and set usage count */
	mutex_lock(&ocp81375_mutex);
	if (!use_count)
		ocp81375_init();
	use_count++;
	mutex_unlock(&ocp81375_mutex);

	pr_info("Set driver: %d\n", use_count);

	return 0;
}

static ssize_t ocp81375_strobe_store(struct flashlight_arg arg)
{
	ocp81375_set_driver(1);
	ocp81375_set_level(arg.ct, arg.level);
	ocp81375_enable(arg.ct);
	msleep(arg.dur);
	ocp81375_disable(arg.ct);
	ocp81375_set_driver(0);

	return 0;
}

static struct flashlight_operations ocp81375_ops = {
	ocp81375_open,
	ocp81375_release,
	ocp81375_ioctl,
	ocp81375_strobe_store,
	ocp81375_set_driver
};

/******************************************************************************
 * I2C device and driver
 *****************************************************************************/
static int ocp81375_chip_init(struct ocp81375_chip_data *chip)
{
	/* NOTE: Chip initialication move to
	*"set driver" operation for power saving issue.
	*/
	int val;

	flashlight_pinctrl_set(OCP81375_PINCTRL_PIN_HWEN, OCP81375_PINCTRL_PINSTATE_HIGH);
	usleep_range(2000, 2500);

	val = ocp81375_read_reg(ocp81375_i2c_client, OCP81375_REG_DEVICE_ID);
	printk("%s:ocp81375 chip id:0x%x\n", __func__, val);
	if (val == OCP81375_DEVICE_ID) {
		printk("%s:ocp81375 get chip id ok\n", __func__);
	} else {
		printk("%s:ocp81375 get chip id fail\n", __func__);
	}

	return 0;
}

/***************************************************************************/
/*OCP81375 Debug file */
/***************************************************************************/
static ssize_t
ocp81375_get_reg(struct device *cd, struct device_attribute *attr, char *buf)
{
	unsigned char reg_val;
	unsigned char i;
	ssize_t len = 0;

	for (i = 0; i < 0x0E; i++) {
		reg_val = ocp81375_read_reg(ocp81375_i2c_client, i);
		len += snprintf(buf+len, PAGE_SIZE-len,
			"reg0x%2X = 0x%2X\n", i, reg_val);
	}
	len += snprintf(buf+len, PAGE_SIZE-len, "\r\n");
	return len;
}

static ssize_t ocp81375_set_reg(struct device *cd,
		struct device_attribute *attr, const char *buf, size_t len)
{
	unsigned int databuf[2];

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2)
		ocp81375_write_reg(ocp81375_i2c_client, databuf[0], databuf[1]);
	return len;
}


static ssize_t ocp81375_store(struct device *cd,
		struct device_attribute *attr, const char *buf, size_t len)
{
	if(buf[0] == '0')
	{
		ocp81375_enable(OCP81375_CHANNEL_CH1);
	}
	else if(buf[0] == '1')
	{
		ocp81375_disable(OCP81375_CHANNEL_CH1);
	}
	else if(buf[0] == '2')
	{
		ocp81375_enable(OCP81375_CHANNEL_CH2);
	}
	else if(buf[0] == '3')
	{
		ocp81375_disable(OCP81375_CHANNEL_CH2);
	}

	return len;
}


static DEVICE_ATTR(reg, 0660, ocp81375_get_reg, ocp81375_set_reg);
static DEVICE_ATTR(set, 0660, NULL, ocp81375_store);

static int ocp81375_create_sysfs(struct i2c_client *client)
{
	int err;
	struct device *dev = &(client->dev);

	err = device_create_file(dev, &dev_attr_reg);

	err = device_create_file(dev, &dev_attr_set);

	return err;
}

static int ocp81375_parse_dt(struct device *dev,
		struct ocp81375_platform_data *pdata)
{
	struct device_node *np, *cnp;
	u32 decouple = 0;
	int i = 0;

	if (!dev || !dev->of_node || !pdata)
		return -ENODEV;

	np = dev->of_node;

	pdata->channel_num = of_get_child_count(np);
	if (!pdata->channel_num) {
		OCP81375_DEBUG("Parse no dt, node.\n");
		return 0;
	}
	OCP81375_DEBUG("Channel number(%d).\n", pdata->channel_num);

	if (of_property_read_u32(np, "decouple", &decouple))
		OCP81375_DEBUG("Parse no dt, decouple.\n");

	if (of_property_read_u32(np, "led_num", &led_num))
		OCP81375_DEBUG("Parse no dt, led_num.\n");

	pdata->dev_id = devm_kzalloc(dev,
			pdata->channel_num *
			sizeof(struct flashlight_device_id),
			GFP_KERNEL);
	if (!pdata->dev_id)
		return -ENOMEM;

	for_each_child_of_node(np, cnp) {
		if (of_property_read_u32(cnp, "type", &pdata->dev_id[i].type))
			goto err_node_put;
		if (of_property_read_u32(cnp, "ct", &pdata->dev_id[i].ct))
			goto err_node_put;
		if (of_property_read_u32(cnp, "part", &pdata->dev_id[i].part))
			goto err_node_put;
		snprintf(pdata->dev_id[i].name, FLASHLIGHT_NAME_SIZE,
				OCP81375_NAME);
		pdata->dev_id[i].channel = i;
		pdata->dev_id[i].decouple = decouple;

		OCP81375_DEBUG("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				pdata->dev_id[i].type, pdata->dev_id[i].ct,
				pdata->dev_id[i].part, pdata->dev_id[i].name,
				pdata->dev_id[i].channel,
				pdata->dev_id[i].decouple);
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

static int
ocp81375_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ocp81375_chip_data *chip;
	struct ocp81375_platform_data *pdata = client->dev.platform_data;
	int err;
	int i;

	OCP81375_DEBUG("%s Probe start.\n", __func__);

	/* check i2c */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}

	/* init chip private data */
	chip = kzalloc(sizeof(struct ocp81375_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}
	chip->client = client;

	/* init platform data */
	if (!pdata) {
		pr_err("Platform data does not exist\n");
		pdata =
		kzalloc(sizeof(struct ocp81375_platform_data), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto err_init_pdata;
		}
		chip->no_pdata = 1;
	}
	chip->pdata = pdata;

	err = ocp81375_parse_dt(&g_ocp81375_pdev->dev, chip->pdata);
	if (err)
		goto err_init_pdata;

	i2c_set_clientdata(client, chip);
	ocp81375_i2c_client = client;

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	/* init work queue */
	INIT_WORK(&ocp81375_work_ch1, ocp81375_work_disable_ch1);
	INIT_WORK(&ocp81375_work_ch2, ocp81375_work_disable_ch2);

	/* init timer */
	hrtimer_init(&ocp81375_timer_ch1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ocp81375_timer_ch1.function = ocp81375_timer_func_ch1;
	hrtimer_init(&ocp81375_timer_ch2, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ocp81375_timer_ch2.function = ocp81375_timer_func_ch2;
	ocp81375_timeout_ms[OCP81375_CHANNEL_CH1] = 100;
	ocp81375_timeout_ms[OCP81375_CHANNEL_CH2] = 100;

	/* init chip hw */
	ocp81375_chip_init(chip);

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++) {
			if (flashlight_dev_register_by_device_id(&pdata->dev_id[i], &ocp81375_ops)) {
				err = -EFAULT;
				goto err_free;
			}
		}
	} else {
		if (flashlight_dev_register(OCP81375_NAME, &ocp81375_ops)) {
			err = -EFAULT;
			goto err_free;
		}
	}

	/* clear usage count */
	use_count = 0;

	ocp81375_create_sysfs(client);

	OCP81375_DEBUG("%s Probe done.\n", __func__);

	return 0;

err_free:
	kfree(chip->pdata);
err_init_pdata:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
err_out:
	return err;
}

static int ocp81375_i2c_remove(struct i2c_client *client)
{
	struct ocp81375_chip_data *chip = i2c_get_clientdata(client);

	OCP81375_DEBUG("Remove start.\n");

	/* flush work queue */
	flush_work(&ocp81375_work_ch1);
	flush_work(&ocp81375_work_ch2);

	/* unregister flashlight operations */
	flashlight_dev_unregister(OCP81375_NAME);

	/* free resource */
	if (chip->no_pdata)
		kfree(chip->pdata);
	kfree(chip);

	OCP81375_DEBUG("Remove done.\n");

	return 0;
}

static const struct i2c_device_id ocp81375_i2c_id[] = {
	{OCP81375_NAME, 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id ocp81375_i2c_of_match[] = {
	{.compatible = OCP81375_DTNAME_I2C},
	{},
};
#endif

static struct i2c_driver ocp81375_i2c_driver = {
	.driver = {
		   .name = OCP81375_NAME,
#ifdef CONFIG_OF
		   .of_match_table = ocp81375_i2c_of_match,
#endif
		   },
	.probe = ocp81375_i2c_probe,
	.remove = ocp81375_i2c_remove,
	.id_table = ocp81375_i2c_id,
};

void ocp81375_read_flag(void)
{
    int flag1,flag2 = 0;
    flag1 = ocp81375_read_reg(ocp81375_i2c_client, OCP81375_REG_FLAG1);
    flag2 = ocp81375_read_reg(ocp81375_i2c_client, OCP81375_REG_FLAG2);
    printk("%s:flag1=0x%x, flag2=0x%x\n", __func__, flag1, flag2);
}
//prize add by zhaolong for ir-led-ctrl 20240626 start
void ocp81375_irled1_enable(int onoff)
{
	if (onoff) {
        ocp81375_read_flag();
		ocp81375_enable(OCP81375_CHANNEL_CH1);
		ocp81375_set_level_ch1(4);
	} else {
		ocp81375_disable(OCP81375_CHANNEL_CH1);
		ocp81375_timer_cancel(OCP81375_CHANNEL_CH1);
		ocp81375_set_level_ch1(0);
	}
}
EXPORT_SYMBOL(ocp81375_irled1_enable);

void ocp81375_irled2_enable(int onoff)
{
	if (onoff) {
        ocp81375_read_flag();
		ocp81375_enable(OCP81375_CHANNEL_CH2);
		ocp81375_set_level_ch2(4);
	} else {
		ocp81375_disable(OCP81375_CHANNEL_CH2);
		ocp81375_timer_cancel(OCP81375_CHANNEL_CH2);
		ocp81375_set_level_ch2(0);
	}
}
EXPORT_SYMBOL(ocp81375_irled2_enable);
//prize add by zhaolong for ir-led-ctrl 20240626 end

void shut_ctrl_power(int onoff)
{
	printk("%s:onoff=%d\n", __func__, onoff);
	if (onoff) {
		pinctrl_select_state(ocp81375_pinctrl, ir_3v3_high);
	} else {
		pinctrl_select_state(ocp81375_pinctrl, ir_3v3_low);
	}
}
EXPORT_SYMBOL(shut_ctrl_power);

//prize add by zhaolong for tc358xx power 20240626 start
void tc358xx_power(int onoff)
{
	printk("%s:onoff=%d\n", __func__, onoff);
	if (onoff) {
		pinctrl_select_state(ocp81375_pinctrl, tc358xx_power_on_high);
	} else {
		pinctrl_select_state(ocp81375_pinctrl, tc358xx_power_on_low);
	}
}
EXPORT_SYMBOL(tc358xx_power);
//prize add by zhaolong for tc358xx power 20240626 end
void i2c2_mode_set(int mode)
{
	printk("%s:mode=%d\n", __func__, mode);
	if (mode) {
		pinctrl_select_state(ocp81375_pinctrl, cam_i2c2_function);
	} else {
		pinctrl_select_state(ocp81375_pinctrl, cam_i2c2_gpio);
	}
}
EXPORT_SYMBOL(i2c2_mode_set);

#if CONFIG_PRIZE_SHUT_CTRL
extern void gpio_shutter_ctrl(unsigned int val);
struct class *shut1_ctrl_class;
//struct class *shut2_ctrl_class;

static volatile int shut1_ctrl_status = 0;
static ssize_t device_shut1_ctrl_value_show(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
	printk("shut1_ctrl_status=%d\n", shut1_ctrl_status);
	return sprintf(buf, "%u\n", shut1_ctrl_status);
}

static ssize_t device_shut1_ctrl_value_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t size)
{
	printk("%s ON/OFF value = %d:\n ", __func__, shut1_ctrl_status);
	if(sscanf(buf, "%u", &shut1_ctrl_status) != 1)
	{
		printk("shut_ctrl values\n");
		return -EINVAL;
	}

	if (shut1_ctrl_status == 1 || shut1_ctrl_status == 2) {
		shut_ctrl_power(1);
	} else {
		shut_ctrl_power(0);
	}
	gpio_shutter_ctrl(shut1_ctrl_status);

	return size;
}

static DEVICE_ATTR(shut1_ctrl_value, 0664, device_shut1_ctrl_value_show, device_shut1_ctrl_value_store);

#if 0
static volatile int shut2_ctrl_status = 0;
static ssize_t device_shut2_ctrl_value_show(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
	printk("shut2_ctrl_status=%d\n", shut2_ctrl_status);
	return sprintf(buf, "%u\n", shut2_ctrl_status);
}

static ssize_t device_shut2_ctrl_value_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t size)
{
	printk("%s ON/OFF value = %d:\n ", __func__, shut2_ctrl_status);
	if(sscanf(buf, "%u", &shut2_ctrl_status) != 1)
	{
		printk("shut_ctrl values\n");
		return -EINVAL;
	}

	if(1 == shut2_ctrl_status){
		pinctrl_select_state(ocp81375_pinctrl, shut_in2_high);
	}else{
		pinctrl_select_state(ocp81375_pinctrl, shut_in2_low);
	}
	return size;
}

static DEVICE_ATTR(shut2_ctrl_value, 0664, device_shut2_ctrl_value_show, device_shut2_ctrl_value_store);
#endif
#endif


// prize add by chenwenhui 20240704 start
#if CONFIG_PRIZE_SOFT_LED_ENABLE
static volatile int cur_irtouch_status = 0;

static ssize_t device_irtouch_value_show(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
	printk("ocp81375 cur_irtouch_status=%d\n", cur_irtouch_status);
	return sprintf(buf, "%u\n", cur_irtouch_status);
}

static ssize_t device_irtouch_value_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t size)
{
	if (sscanf(buf, "%u", &cur_irtouch_status) != 1) {
		printk("ocp81375Invalid values\n");
		return -EINVAL;
	}

    printk("ocp81375 %s ON/OFF value = %d:\n ", __func__, cur_irtouch_status);
	if (1 == cur_irtouch_status) {
		ocp81375_read_flag();
		ocp81375_set_level_ch1(4);
		ocp81375_enable(OCP81375_CHANNEL_CH1);

		ocp81375_set_level_ch2(4);
		ocp81375_enable(OCP81375_CHANNEL_CH2);

	} else {
		ocp81375_set_level_ch1(0);
		ocp81375_disable(OCP81375_CHANNEL_CH1);
		ocp81375_timer_cancel(OCP81375_CHANNEL_CH1);

		ocp81375_set_level_ch2(0);
		ocp81375_disable(OCP81375_CHANNEL_CH2);
		ocp81375_timer_cancel(OCP81375_CHANNEL_CH2);
		ocp81375_set_level_ch2(0);
	}
	return size;
}

static DEVICE_ATTR(irtouch_value, 0664, device_irtouch_value_show, device_irtouch_value_store);

/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
 struct class *flash_irtouch_class;
 #endif
 // prize add by chenwenhui 20240704 end

static int ocp81375_probe(struct platform_device *dev)
{
// prize add by chenwenhui 20240704 start
#if CONFIG_PRIZE_SOFT_LED_ENABLE
	struct device *flash_irtouch_dev;
#endif
// prize add by chenwenhui 20240704 end

#if CONFIG_PRIZE_SHUT_CTRL
	struct device *shut1_ctrl_dev;
	//struct device *shut2_ctrl_dev;
#endif

	g_ocp81375_pdev = dev;
	printk(" ocp81375 Probe start.\n");

	flashlight_gpio_init(g_ocp81375_pdev);

// prize add by chenwenhui 20240704 start
#if CONFIG_PRIZE_SOFT_LED_ENABLE
	flash_irtouch_class = class_create(THIS_MODULE, "flash_irtouch");

	if (IS_ERR(flash_irtouch_class)) {
		printk("Failed to create class(flash_irtouch_class)!");
		return 0;
	}

	//flash_touch_class = class_create(THIS_MODULE, "flashlight_core");

	//if (IS_ERR(flash_touch_class)) {
	//	printk("Failed to create class(flash_touch_class)!");
	//	return 0;
	//}

	flash_irtouch_dev = device_create(flash_irtouch_class, NULL, 0, NULL, "flash_irtouch_data");
	if (IS_ERR(flash_irtouch_dev))
		printk("Failed to create flash_irtouch_dev device");

	//flash_touch_dev = device_create(flash_irtouch_class, NULL, 0, NULL, "flashlight");
	//if (IS_ERR(flash_touch_dev))
	//	printk("Failed to create flash_touch_dev device");

	if (device_create_file(flash_irtouch_dev, &dev_attr_irtouch_value) < 0)
		printk("Failed to create device file(%s)!",dev_attr_irtouch_value.attr.name);

	//if (device_create_file(flash_touch_dev, &dev_attr_touch_value) < 0)
	//	printk("Failed to create device file(%s)!",dev_attr_touch_value.attr.name);
#endif
// prize add by chenwenhui 20240704 end

#if CONFIG_PRIZE_SHUT_CTRL
	shut1_ctrl_class = class_create(THIS_MODULE, "shut1_ctrl");
	if (IS_ERR(shut1_ctrl_class)) {
		printk("Failed to create class(shut1_ctrl_class)!");
		return 0;
	}

	shut1_ctrl_dev = device_create(shut1_ctrl_class, NULL, 0, NULL, "shut1_ctrl_data");
	if (IS_ERR(shut1_ctrl_dev))
		printk("Failed to create shut1_ctrl_dev device");

	if (device_create_file(shut1_ctrl_dev, &dev_attr_shut1_ctrl_value) < 0)
		printk("Failed to create device file(%s)!",dev_attr_shut1_ctrl_value.attr.name);

#if 0
	shut2_ctrl_class = class_create(THIS_MODULE, "shut2_ctrl");
	if (IS_ERR(shut2_ctrl_class)) {
		printk("Failed to create class(shut2_ctrl_class)!");
		return 0;
	}

	shut2_ctrl_dev = device_create(shut2_ctrl_class, NULL, 0, NULL, "shut2_ctrl_data");
	if (IS_ERR(shut2_ctrl_dev))
		printk("Failed to create shut2_ctrl_dev device");

	if (device_create_file(shut2_ctrl_dev, &dev_attr_shut2_ctrl_value) < 0)
		printk("Failed to create device file(%s)!",dev_attr_shut2_ctrl_value.attr.name);
#endif
#endif

	if (i2c_add_driver(&ocp81375_i2c_driver)) {
		pr_err("Failed to add i2c driver.\n");
		return -1;
	}

	pr_info("%s Probe done.\n", __func__);

	return 0;
}

static int ocp81375_remove(struct platform_device *dev)
{
	pr_info("Remove start.\n");

	i2c_del_driver(&ocp81375_i2c_driver);

	pr_info("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id ocp81375_of_match[] = {
	{.compatible = OCP81375_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, ocp81375_of_match);
#else
static struct platform_device ocp81375_platform_device[] = {
	{
		.name = OCP81375_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, ocp81375_platform_device);
#endif

static struct platform_driver ocp81375_platform_driver = {
	.probe = ocp81375_probe,
	.remove = ocp81375_remove,
	.driver = {
		.name = OCP81375_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = ocp81375_of_match,
#endif
	},
};

static int __init flashlight_ocp81375_init(void)
{
	int ret;

	pr_info("%s driver version %s.\n", __func__, OCP81375_DRIVER_VERSION);

#ifndef CONFIG_OF
	ret = platform_device_register(&ocp81375_platform_device);
	if (ret) {
		pr_err("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&ocp81375_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}

	pr_info("flashlight_ocp81375 Init done.\n");

	return 0;
}

static void __exit flashlight_ocp81375_exit(void)
{
	pr_info("flashlight_ocp81375-Exit start.\n");

	platform_driver_unregister(&ocp81375_platform_driver);

	pr_info("flashlight_ocp81375 Exit done.\n");
}

module_init(flashlight_ocp81375_init);
module_exit(flashlight_ocp81375_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Awinic");
MODULE_DESCRIPTION("OCP81375 Flashlight Driver");

