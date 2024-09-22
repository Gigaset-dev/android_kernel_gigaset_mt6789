/*
 * UPM6910D battery charging driver
 *
 * Copyright (C) 2018 Unisemipower
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define pr_fmt(fmt)	"[upm6910]:%s: " fmt, __func__

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/math64.h>
//#include <mt-plat/v1/charger_type.h>

//#include "mtk_charger_intf.h"
#include "upm6910_reg.h"
#include "upm6910.h"
#include "charger_class.h"
#include "mtk_charger.h"
#include "sc89601a_reg.h"

//#include <mt-plat/upmu_common.h>

#define POWER_DETECT_ICHG		1020
#define POWER_DETECT_ICL		2000
#define POWER_DETECT_VINDPM		4500

/*prize liuyong, modify for kpoc charging, 20230410, start*/
#define KPOC_CHECK_DELAY 1000

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};
/*prize liuyong, modify for kpoc charging, 20230410, end*/

enum {
	PN_UPM6910D,
};

enum upm6910_part_no {
	UPM6910D = 0x02,
};

static int pn_data[] = {
	[PN_UPM6910D] = 0x02,
};

//static char *pn_str[] = {
//	[PN_UPM6910D] = "upm6910d",
//};


struct upm6910 {
	struct device *dev;
	struct i2c_client *client;

	enum upm6910_part_no part_no;
	int revision;
	int input_current_limit;
	int constant_chargevolt;
	int term_current;
	int input_current_max;

	const char *chg_dev_name;
	const char *eint_name;

	bool chg_det_enable;
	int chg_en_gpio;
	int status;
	int irq;
	u32 intr_gpio;

	struct mutex i2c_rw_lock;
	struct mutex pe_access_lock;

	bool charge_enabled;	/* Register bit status */
	bool power_good;
	bool vbus_gd;

	struct upm6910_platform_data *platform_data;
	struct charger_device *chg_dev;

	struct power_supply_desc psy_desc;
	struct power_supply *psy;
	struct power_supply *bat_psy;
	struct power_supply *usb_psy;
	struct power_supply *chg_psy;

	struct delayed_work psy_dwork;
	struct delayed_work prob_dwork;
	int psy_usb_type;
	/*prize liuyong, modify for kpoc charging, 20230410, start*/
	u32 bootmode;
	u32 boottype;
	struct delayed_work kpoc_dwork;
	/*prize liuyong, modify for charger detect init, 20230510*/
	bool is_need_detect_init;
	/*prize liuyong, modify for kpoc charging, 20230410, end*/
};

extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
static bool is_upm_main_chg = false;

static const struct charger_properties upm6910_chg_props = {
	.alias_name = "upm6910",
};

static int __upm6910_read_reg(struct upm6910 *upm, u8 reg, u8 *data)
{
	s32 ret;
	int retry_cnt = 5;

	if (is_upm_main_chg == true) {
		ret = i2c_smbus_read_byte_data(upm->client, reg);
		if (ret < 0) {
			pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
			return ret;
		}

		*data = (u8) ret;
	} else {
		do
		{

			ret = i2c_smbus_read_byte_data(upm->client, reg);
			if (ret < 0) {
				pr_err("i2c read fail: can't read from reg 0x%02X,ret = %d,retry_cnt = %d\n", reg,ret,retry_cnt);
				retry_cnt--;
				msleep(2);
			}
			else{
				*data = (u8) ret;
				return 0;
			}
		}while(retry_cnt > 0);
	}

	return 0;
}

static int __upm6910_write_reg(struct upm6910 *upm, int reg, u8 val)
{
	s32 ret;
	int retry_cnt = 5;

	if (is_upm_main_chg == true) {
		ret = i2c_smbus_write_byte_data(upm->client, reg, val);
		if (ret < 0) {
			pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
				val, reg, ret);
			return ret;
		}
	} else {
		do
		{
			if(reg == 0x00){
				//pr_err("%s write reg[0x00]:%x\n",__func__, val);
				if(val & 0x80){
					val &= 0x7f;
					pr_err("%s write reg[0x00]:%x\n",__func__, val);
				}
			}
			ret = i2c_smbus_write_byte_data(upm->client, reg, val);
			if (ret < 0) {
				pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d,retry_cnt = %d\n",val, reg, ret,retry_cnt);
				retry_cnt--;
				msleep(2);
			}
			else{
				return 0;
			}
		}while(retry_cnt > 0);
	}
	return 0;
}

static int upm6910_read_byte(struct upm6910 *upm, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&upm->i2c_rw_lock);
	ret = __upm6910_read_reg(upm, reg, data);
	mutex_unlock(&upm->i2c_rw_lock);

	return ret;
}

static int upm6910_write_byte(struct upm6910 *upm, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&upm->i2c_rw_lock);
	ret = __upm6910_write_reg(upm, reg, data);
	mutex_unlock(&upm->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}

static int upm6910_update_bits(struct upm6910 *upm, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&upm->i2c_rw_lock);
	ret = __upm6910_read_reg(upm, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __upm6910_write_reg(upm, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&upm->i2c_rw_lock);
	return ret;
}

static int upm6910_enable_otg(struct upm6910 *upm)
{
	u8 val;

	if (is_upm_main_chg == true) {
		val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;

		return upm6910_update_bits(upm, UPM6910_REG_01, REG01_OTG_CONFIG_MASK,
					val);
	} else {
		val = SC89601A_OTG_ENABLE << SC89601A_OTG_CONFIG_SHIFT;

		return upm6910_update_bits(upm, SC89601A_REG_03,
					SC89601A_OTG_CONFIG_MASK, val);
	}
}

static int upm6910_disable_otg(struct upm6910 *upm)
{
	u8 val;
	if (is_upm_main_chg == true) {
		val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;

		return upm6910_update_bits(upm, UPM6910_REG_01, REG01_OTG_CONFIG_MASK,
					val);
	} else {
		val = SC89601A_OTG_DISABLE << SC89601A_OTG_CONFIG_SHIFT;

		return upm6910_update_bits(upm, SC89601A_REG_03,
					SC89601A_OTG_CONFIG_MASK, val);
	}
}

static int upm6910_enable_charger(struct upm6910 *upm)
{
	int ret;
	u8 val;
	if (is_upm_main_chg == true) {
		val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;

		ret =
			upm6910_update_bits(upm, UPM6910_REG_01, REG01_CHG_CONFIG_MASK, val);
	} else {
		val = SC89601A_CHG_ENABLE << SC89601A_CHG_CONFIG_SHIFT;

		ret = upm6910_update_bits(upm, SC89601A_REG_03, 
							SC89601A_CHG_CONFIG_MASK, val);
	}

	return ret;
}

static int upm6910_disable_charger(struct upm6910 *upm)
{
	int ret;
	u8 val;
	if (is_upm_main_chg == true) {
		val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

		ret =
			upm6910_update_bits(upm, UPM6910_REG_01, REG01_CHG_CONFIG_MASK, val);
	} else {
		val = SC89601A_CHG_DISABLE << SC89601A_CHG_CONFIG_SHIFT;

		ret = upm6910_update_bits(upm, SC89601A_REG_03, 
					SC89601A_CHG_CONFIG_MASK, val);
	}
	return ret;
}

int upm6910_set_chargecurrent(struct upm6910 *upm, int curr)
{
	u8 ichg;

	if (is_upm_main_chg == true) {
		if (curr < REG02_ICHG_BASE)
			curr = REG02_ICHG_BASE;

		if (curr < REG02_ICHG_MIN)
			curr = REG02_ICHG_MIN;

		ichg = (curr - REG02_ICHG_BASE) / REG02_ICHG_LSB;
		return upm6910_update_bits(upm, UPM6910_REG_02, REG02_ICHG_MASK,
					ichg << REG02_ICHG_SHIFT);
	} else {
		if (curr < SC89601A_ICHG_BASE)
			curr = SC89601A_ICHG_BASE;

		ichg = (curr - SC89601A_ICHG_BASE)/SC89601A_ICHG_LSB;

		return upm6910_update_bits(upm, SC89601A_REG_04, 
							SC89601A_ICHG_MASK, ichg << SC89601A_ICHG_SHIFT);
	}
}

int upm6910_set_term_current(struct upm6910 *upm, int curr)
{
	u8 iterm;

	if (is_upm_main_chg == true) {
		if (curr < REG03_ITERM_BASE)
			curr = REG03_ITERM_BASE;

		iterm = (curr - REG03_ITERM_BASE) / REG03_ITERM_LSB;
	
		return upm6910_update_bits(upm, UPM6910_REG_03, REG03_ITERM_MASK,
					iterm << REG03_ITERM_SHIFT);
	} else {
		if (curr < SC89601A_ITERM_BASE)
			curr = SC89601A_ITERM_BASE;

		iterm = (curr - SC89601A_ITERM_BASE) / SC89601A_ITERM_LSB;

		return upm6910_update_bits(upm, SC89601A_REG_05, 
							SC89601A_ITERM_MASK, iterm << SC89601A_ITERM_SHIFT);
	}
}
EXPORT_SYMBOL_GPL(upm6910_set_term_current);

int upm6910_set_prechg_current(struct upm6910 *upm, int curr)
{
	u8 iprechg;

	if (is_upm_main_chg == true) {
		if (curr < REG03_IPRECHG_BASE)
			curr = REG03_IPRECHG_BASE;
	
		iprechg = (curr - REG03_IPRECHG_BASE) / REG03_IPRECHG_LSB;

		return upm6910_update_bits(upm, UPM6910_REG_03, REG03_IPRECHG_MASK,
					iprechg << REG03_IPRECHG_SHIFT);
	} else {
		if (curr < SC89601A_IPRECHG_BASE)
			curr = SC89601A_IPRECHG_BASE;

		iprechg = (curr - SC89601A_IPRECHG_BASE) / SC89601A_IPRECHG_LSB;

		return upm6910_update_bits(upm, SC89601A_REG_05, 
							SC89601A_IPRECHG_MASK, iprechg << SC89601A_IPRECHG_SHIFT);
	}
}
EXPORT_SYMBOL_GPL(upm6910_set_prechg_current);

int upm6910_set_chargevolt(struct upm6910 *upm, int volt)
{
	u8 val;

	if (is_upm_main_chg == true) {
		if (volt < REG04_VREG_BASE)
			volt = REG04_VREG_BASE;

		val = (volt - REG04_VREG_BASE) / REG04_VREG_LSB;
		return upm6910_update_bits(upm, UPM6910_REG_04, REG04_VREG_MASK,
					val << REG04_VREG_SHIFT);
	} else {
		if (volt < SC89601A_VREG_BASE)
			volt = SC89601A_VREG_BASE;

		val = (volt - SC89601A_VREG_BASE)/SC89601A_VREG_LSB;
		return upm6910_update_bits(upm, SC89601A_REG_06, 
							SC89601A_VREG_MASK, val << SC89601A_VREG_SHIFT);
	}
}

int upm6910_set_force_vindpm(struct upm6910 *upm, bool en)
{
    u8 val;

    if (en)
        val = SC89601A_FORCE_VINDPM_ENABLE;
    else
        val = SC89601A_FORCE_VINDPM_DISABLE;

    return upm6910_update_bits(upm, SC89601A_REG_0D, 
						SC89601A_FORCE_VINDPM_MASK, val << SC89601A_FORCE_VINDPM_SHIFT);
}

int upm6910_set_input_volt_limit(struct upm6910 *upm, int volt)
{
	u8 val;

	if (is_upm_main_chg == true) {
		if (volt < REG06_VINDPM_BASE)
			volt = REG06_VINDPM_BASE;

		val = (volt - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;
		return upm6910_update_bits(upm, UPM6910_REG_06, REG06_VINDPM_MASK,
					val << REG06_VINDPM_SHIFT);
	} else {
		if (volt < SC89601A_VINDPM_BASE)
			volt = SC89601A_VINDPM_BASE;

		upm6910_set_force_vindpm(upm, true);

		val = (volt - SC89601A_VINDPM_BASE) / SC89601A_VINDPM_LSB;
		return upm6910_update_bits(upm, SC89601A_REG_0D, 
							SC89601A_VINDPM_MASK, val << SC89601A_VINDPM_SHIFT);
	}
}

static int upm6910_get_input_volt_limit(struct upm6910 *upm, int *volt)
{
	u8 val;
	int ret;
	int vindpm;

	if (is_upm_main_chg == true) {
		ret = upm6910_read_byte(upm, UPM6910_REG_06, &val);
		if (ret == 0){
			pr_err("Reg[%.2x] = 0x%.2x\n", UPM6910_REG_06, val);
		}

		pr_info("vindpm_reg_val:0x%X\n", val);
		vindpm = (val & REG06_VINDPM_MASK) >> REG06_VINDPM_SHIFT;
		vindpm = vindpm * REG06_VINDPM_LSB + REG06_VINDPM_BASE;
		*volt = vindpm;
	} else {
		ret = upm6910_read_byte(upm, SC89601A_REG_0D, &val);
		if (!ret) {
			vindpm = (val & SC89601A_VINDPM_MASK) >> SC89601A_VINDPM_SHIFT;
			vindpm = vindpm * SC89601A_VINDPM_LSB + SC89601A_VINDPM_BASE;
			*volt = vindpm * 1000;
		}
	}
	return ret;
}

int upm6910_set_input_current_limit(struct upm6910 *upm, int curr)
{
	u8 val;

	if (is_upm_main_chg == true) {
		if (curr < REG00_IINLIM_BASE)
			curr = REG00_IINLIM_BASE;

		val = (curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
		return upm6910_update_bits(upm, UPM6910_REG_00, REG00_IINLIM_MASK,
					val << REG00_IINLIM_SHIFT);
	} else {

		if (curr > 3000)
			curr = 3000;

		if (curr < SC89601A_IINLIM_BASE)
			curr = SC89601A_IINLIM_BASE;

		val = (curr - SC89601A_IINLIM_BASE) / SC89601A_IINLIM_LSB;

		return upm6910_update_bits(upm, SC89601A_REG_00, SC89601A_IINLIM_MASK,
							val << SC89601A_IINLIM_SHIFT);
	}
}

int upm6910_set_watchdog_timer(struct upm6910 *upm, u8 timeout)
{
	u8 val;

	if (is_upm_main_chg == true) {
		val = (u8) (((timeout -
				REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

		return upm6910_update_bits(upm, UPM6910_REG_05, REG05_WDT_MASK, val);
	} else {
		val = (timeout - SC89601A_WDT_BASE) / SC89601A_WDT_LSB;
		val <<= SC89601A_WDT_SHIFT;

		return upm6910_update_bits(upm, SC89601A_REG_07, 
						SC89601A_WDT_MASK, val); 
	}
}
EXPORT_SYMBOL_GPL(upm6910_set_watchdog_timer);

int upm6910_disable_watchdog_timer(struct upm6910 *upm)
{
	u8 val;

	if (is_upm_main_chg == true) {
		val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

		return upm6910_update_bits(upm, UPM6910_REG_05, REG05_WDT_MASK, val);
	} else {
		val = SC89601A_WDT_DISABLE << SC89601A_WDT_SHIFT;

		return upm6910_update_bits(upm, SC89601A_REG_07, 
							SC89601A_WDT_MASK, val);
	}
}
EXPORT_SYMBOL_GPL(upm6910_disable_watchdog_timer);

int upm6910_reset_watchdog_timer(struct upm6910 *upm)
{
	u8 val;

	if (is_upm_main_chg == true) {
		val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

		return upm6910_update_bits(upm, UPM6910_REG_01, REG01_WDT_RESET_MASK,
					val);
	} else {
		val = SC89601A_WDT_RESET << SC89601A_WDT_RESET_SHIFT;

		return upm6910_update_bits(upm, SC89601A_REG_03, 
							SC89601A_WDT_RESET_MASK, val);
	}
}
EXPORT_SYMBOL_GPL(upm6910_reset_watchdog_timer);

int upm6910_reset_chip(struct upm6910 *upm)
{
	int ret;
	u8 val;

	if (is_upm_main_chg == true) {
		val = REG0B_REG_RESET << REG0B_REG_RESET_SHIFT;

		ret =
			upm6910_update_bits(upm, UPM6910_REG_0B, REG0B_REG_RESET_MASK, val);
	} else {
		val = SC89601A_RESET << SC89601A_RESET_SHIFT;

		ret = upm6910_update_bits(upm, SC89601A_REG_14, 
							SC89601A_RESET_MASK, val);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(upm6910_reset_chip);

int upm6910_enter_hiz_mode(struct upm6910 *upm)
{
	u8 val;

	if (is_upm_main_chg == true) {
		val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

		return upm6910_update_bits(upm, UPM6910_REG_00, REG00_ENHIZ_MASK, val);
	} else {
		val = SC89601A_HIZ_ENABLE << SC89601A_ENHIZ_SHIFT;
		return upm6910_update_bits(upm, SC89601A_REG_00, 
							SC89601A_ENHIZ_MASK, val);
	}
}
EXPORT_SYMBOL_GPL(upm6910_enter_hiz_mode);

int upm6910_exit_hiz_mode(struct upm6910 *upm)
{
	u8 val;

	if (is_upm_main_chg == true) {
		val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

		return upm6910_update_bits(upm, UPM6910_REG_00, REG00_ENHIZ_MASK, val);
	} else {
		val = SC89601A_HIZ_DISABLE << SC89601A_ENHIZ_SHIFT;

		return upm6910_update_bits(upm, SC89601A_REG_00, 
							SC89601A_ENHIZ_MASK, val);
	}
}
EXPORT_SYMBOL_GPL(upm6910_exit_hiz_mode);

int upm6910_set_hiz_mode(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	if (en)
		ret = upm6910_enter_hiz_mode(upm);
	else
		ret = upm6910_exit_hiz_mode(upm);

	pr_err("%s hiz mode %s\n", en ? "enable" : "disable",
			!ret ? "successfully" : "failed");

	return ret;
}
EXPORT_SYMBOL_GPL(upm6910_set_hiz_mode);
#if 0
static int upm6910_get_hiz_mode(struct charger_device *chg_dev)
{
	int ret;
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	u8 val;
	ret = upm6910_read_byte(upm, UPM6910_REG_00, &val);
	if (ret == 0){
		pr_err("Reg[%.2x] = 0x%.2x\n", UPM6910_REG_00, val);
	}

	ret = (val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;
	pr_err("%s:hiz mode %s\n",__func__, ret ? "enabled" : "disabled");

	return ret;
}
EXPORT_SYMBOL_GPL(upm6910_get_hiz_mode);
#endif
static int upm6910_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
    struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    dev_info(upm->dev, "%s event = %d\n", __func__, event);

    power_supply_changed(upm->psy);

	return 0;
}


static int upm6910_enable_term(struct charger_device *chg_dev, bool enable)
{
	u8 val;
	int ret;
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	if (is_upm_main_chg == true) {
		if (enable)
			val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
		else
			val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

		ret = upm6910_update_bits(upm, UPM6910_REG_05, REG05_EN_TERM_MASK, val);
	} else {
		if (enable)
			val = SC89601A_TERM_ENABLE << SC89601A_EN_TERM_SHIFT;
		else
			val = SC89601A_TERM_DISABLE << SC89601A_EN_TERM_SHIFT;

		ret = upm6910_update_bits(upm, SC89601A_REG_07,
						SC89601A_EN_TERM_MASK, val);
	}

	return ret;
}
//EXPORT_SYMBOL_GPL(upm6910_enable_term);


int upm6910_set_boost_current(struct upm6910 *upm, int curr)
{
	u8 val;

	if (is_upm_main_chg == true) {
		val = REG02_BOOST_LIM_0P5A;
		if (curr >= BOOSTI_1200)
			val = REG02_BOOST_LIM_1P2A;

		return upm6910_update_bits(upm, UPM6910_REG_02, REG02_BOOST_LIM_MASK,
					val << REG02_BOOST_LIM_SHIFT);
	} else {
		if (curr == 500)
			val = SC89601A_BOOST_LIM_500MA;
		else if (curr == 750)
			val = SC89601A_BOOST_LIM_750MA;
		else if (curr == 1200)
			val = SC89601A_BOOST_LIM_1200MA;
		else
			val = SC89601A_BOOST_LIM_1400MA;

		return upm6910_update_bits(upm, SC89601A_REG_0A, 
					SC89601A_BOOST_LIM_MASK, 
					val << SC89601A_BOOST_LIM_SHIFT);
	}
}

int upm6910_set_boost_voltage(struct upm6910 *upm, int volt)
{
	u8 val;

	if (is_upm_main_chg == true) {
		if (volt == BOOSTV_4850)
			val = REG06_BOOSTV_4P85V;
		else if (volt == BOOSTV_5150)
			val = REG06_BOOSTV_5P15V;
		else if (volt == BOOSTV_5300)
			val = REG06_BOOSTV_5P3V;
		else
			val = REG06_BOOSTV_5V;

		return upm6910_update_bits(upm, UPM6910_REG_06, REG06_BOOSTV_MASK,
					val << REG06_BOOSTV_SHIFT);
	} else {
		if (volt < SC89601A_BOOSTV_BASE)
			volt = SC89601A_BOOSTV_BASE;
		if (volt > SC89601A_BOOSTV_BASE 
				+ (SC89601A_BOOSTV_MASK >> SC89601A_BOOSTV_SHIFT) 
				* SC89601A_BOOSTV_LSB)
			volt = SC89601A_BOOSTV_BASE 
				+ (SC89601A_BOOSTV_MASK >> SC89601A_BOOSTV_SHIFT) 
				* SC89601A_BOOSTV_LSB;
	
		val = ((volt - SC89601A_BOOSTV_BASE) / SC89601A_BOOSTV_LSB) 
				<< SC89601A_BOOSTV_SHIFT;
	
		return upm6910_update_bits(upm, SC89601A_REG_0A, 
					SC89601A_BOOSTV_MASK, val);
	}
}
EXPORT_SYMBOL_GPL(upm6910_set_boost_voltage);

static int upm6910_set_acovp_threshold(struct upm6910 *upm, int volt)
{
	u8 val;

	if (volt == VAC_OVP_14000)
		val = REG06_OVP_14P0V;
	else if (volt == VAC_OVP_10500)
		val = REG06_OVP_10P5V;
	else if (volt == VAC_OVP_6500)
		val = REG06_OVP_6P5V;
	else
		val = REG06_OVP_5P5V;

	return upm6910_update_bits(upm, UPM6910_REG_06, REG06_OVP_MASK,
				   val << REG06_OVP_SHIFT);
}
//EXPORT_SYMBOL_GPL(upm6910_set_acovp_threshold);

static int upm6910_set_stat_ctrl(struct upm6910 *upm, int ctrl)
{
	u8 val;

	val = ctrl;

	return upm6910_update_bits(upm, UPM6910_REG_00, REG00_STAT_CTRL_MASK,
				   val << REG00_STAT_CTRL_SHIFT);
}

static int upm6910_set_int_mask(struct upm6910 *upm, int mask)
{
	u8 val;

	val = mask;

	return upm6910_update_bits(upm, UPM6910_REG_0A, REG0A_INT_MASK_MASK,
				   val << REG0A_INT_MASK_SHIFT);
}

static int upm6910_force_dpdm(struct upm6910 *upm)
{
	int ret;
	u8 val;
	if (is_upm_main_chg == true) {
		val = REG07_FORCE_DPDM << REG07_FORCE_DPDM_SHIFT;

		ret =  upm6910_update_bits(upm, UPM6910_REG_07, REG07_FORCE_DPDM_MASK,
					val);
	} else {
		val = SC89601A_FORCE_DPDM << SC89601A_FORCE_DPDM_SHIFT;

		ret = upm6910_update_bits(upm, SC89601A_REG_02, 
							SC89601A_FORCE_DPDM_MASK, val);
	}
	pr_info("Force DPDM %s\n", !ret ? "successfully" : "failed");
	return ret;
}

/*
static int upm6910_enable_batfet(struct upm6910 *upm)
{
	const u8 val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_07, REG07_BATFET_DIS_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(upm6910_enable_batfet);

static int upm6910_disable_batfet(struct upm6910 *upm)
{
	const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_07, REG07_BATFET_DIS_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(upm6910_disable_batfet);

static int upm6910_set_batfet_delay(struct upm6910 *upm, uint8_t delay)
{
	u8 val;

	if (delay == 0)
		val = REG07_BATFET_DLY_0S;
	else
		val = REG07_BATFET_DLY_10S;

	val <<= REG07_BATFET_DLY_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_07, REG07_BATFET_DLY_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(upm6910_set_batfet_delay);
*/

static int upm6910_enable_safety_timer(struct upm6910 *upm)
{
	u8 val;

	if (is_upm_main_chg == true) {
		val = REG05_CHG_TIMER_ENABLE << REG05_EN_TIMER_SHIFT;

		return upm6910_update_bits(upm, UPM6910_REG_05, REG05_EN_TIMER_MASK,
					val);
	} else {
		val = SC89601A_CHG_TIMER_ENABLE << SC89601A_EN_TIMER_SHIFT;

		return upm6910_update_bits(upm, SC89601A_REG_07, SC89601A_EN_TIMER_MASK,
					val);
	}
}
//EXPORT_SYMBOL_GPL(upm6910_enable_safety_timer);

static int upm6910_disable_safety_timer(struct upm6910 *upm)
{
	u8 val;

	if (is_upm_main_chg == true) {
		val = REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT;

		return upm6910_update_bits(upm, UPM6910_REG_05, REG05_EN_TIMER_MASK,
					val);
	} else {
		val = SC89601A_CHG_TIMER_DISABLE << SC89601A_EN_TIMER_SHIFT;

		return upm6910_update_bits(upm, SC89601A_REG_07, SC89601A_EN_TIMER_MASK,
					val);
	}
}
//EXPORT_SYMBOL_GPL(upm6910_disable_safety_timer);

/*
static int upm6910_enable_hvdcp(struct upm6910 *upm, bool en)
{
	const u8 val = en << REG0C_EN_HVDCP_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_0C, REG0C_EN_HVDCP_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(upm6910_enable_hvdcp);

static int upm6910_set_dp(struct upm6910 *upm, int dp_stat)
{
	const u8 val = dp_stat << REG0C_DP_MUX_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_0C, REG0C_DP_MUX_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(upm6910_set_dp);

static int upm6910_set_dm(struct upm6910 *upm, int dm_stat)
{
	const u8 val = dm_stat << REG0C_DM_MUX_SHIFT;

	return upm6910_update_bits(upm, UPM6910_REG_0C, REG0C_DM_MUX_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(upm6910_set_dm);
*/

static int upm6910_dpdm_set_hidden_mode(struct upm6910 *upm)
{
	int ret = 0;
	union power_supply_propval propval;

	if (!upm->bat_psy) {
		upm->bat_psy = power_supply_get_by_name("battery");
		if (!upm->bat_psy) {
			pr_err("%s Couldn't get battery psy\n", __func__);
			return -ENODEV;
		}
	}

	ret = power_supply_get_property(upm->bat_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &propval);
	if (ret || propval.intval <= 3450000) {
		pr_err("%s cannot goto hidden mode\n", __func__);
		return -EINVAL;
	}

	ret = upm6910_write_byte(upm, 0xA9, 0x6E);
	if (ret) {
		pr_err("write 0xA9, 0x6E failed(%d)\n", ret);
	}
	ret = upm6910_write_byte(upm, 0xB1, 0x80);
	if (ret) {
		pr_err("write 0xB1, 0x80 failed(%d)\n", ret);
	}
	ret = upm6910_write_byte(upm, 0xB0, 0x21);
	if (ret) {
		pr_err("write 0xB0, 0x21 failed(%d)\n", ret);
	}
	ret = upm6910_write_byte(upm, 0xB1, 0x00);
	if (ret) {
		pr_err("write 0xB1, 0x00 failed(%d)\n", ret);
	}
	ret = upm6910_write_byte(upm, 0xA9, 0x00);
	if (ret) {
		pr_err("write 0xA9, 0x00 failed(%d)\n", ret);
	}

	return ret;
}
//EXPORT_SYMBOL_GPL(upm6910_dpdm_set_hidden_mode);

static int upm6910_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg;
	int ret;

	if (is_upm_main_chg == true) {
		ret = upm6910_read_byte(upm, UPM6910_REG_02, &reg_val);
		if (!ret) {
			ichg = (reg_val & REG02_ICHG_MASK) >> REG02_ICHG_SHIFT;
			ichg = ichg * REG02_ICHG_LSB + REG02_ICHG_BASE;
			*curr = ichg * 1000;
		}
	} else {
		ret = upm6910_read_byte(upm, SC89601A_REG_04, &reg_val);
		if (!ret) {
			ichg = (reg_val & SC89601A_ICHG_MASK) >> SC89601A_ICHG_SHIFT;
			ichg = ichg * SC89601A_ICHG_LSB + SC89601A_ICHG_BASE;
			*curr = ichg * 1000;
		}
	}
	return ret;
}

static int upm6910_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int icl;
	int ret;

	if (is_upm_main_chg == true) {
		ret = upm6910_read_byte(upm, UPM6910_REG_00, &reg_val);
		if (!ret) {
			icl = (reg_val & REG00_IINLIM_MASK) >> REG00_IINLIM_SHIFT;
			icl = icl * REG00_IINLIM_LSB + REG00_IINLIM_BASE;
			*curr = icl * 1000;
		}
	} else {
		ret = upm6910_read_byte(upm, SC89601A_REG_00, &reg_val);
		if (!ret) {
			icl = (reg_val & SC89601A_IINLIM_MASK) >> SC89601A_IINLIM_SHIFT;
			icl = icl * SC89601A_IINLIM_LSB + SC89601A_IINLIM_BASE;
			*curr = icl * 1000;
		}
	}
	return ret;
}

static struct upm6910_platform_data *upm6910_parse_dt(struct device_node *np,
						      struct upm6910 *upm)
{
	int ret;
	struct upm6910_platform_data *pdata;
	/*prize liuyong, modify for kpoc charging, 20230410, start*/
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
	/*prize liuyong, modify for kpoc charging, 20230410, end*/

	pdata = devm_kzalloc(&upm->client->dev, sizeof(struct upm6910_platform_data),
			     GFP_KERNEL);
	if (!pdata)
		return NULL;

	/*prize liuyong, modify for kpoc charging, 20230410, start*/
	boot_node = of_parse_phandle(upm->dev->of_node, "bootmode", 0);
	if (!boot_node)
		pr_err("%s: failed to get boot mode phandle\n", __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (!tag)
			pr_err("%s: failed to get atag,boot\n", __func__);
		else {
			pr_err("%s: size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
				__func__, tag->size, tag->tag,
				tag->bootmode, tag->boottype);
			upm->bootmode = tag->bootmode;
			upm->boottype = tag->boottype;
		}
	}
	/*prize liuyong, modify for kpoc charging, 20230410, end*/

	if (of_property_read_string(np, "charger_name", &upm->chg_dev_name) < 0) {
		upm->chg_dev_name = "primary_chg";
		pr_warn("no charger name\n");
	}

	if (of_property_read_string(np, "eint_name", &upm->eint_name) < 0) {
		upm->eint_name = "chr_stat";
		pr_warn("no eint name\n");
	}

	ret = of_get_named_gpio(np, "upm,intr_gpio", 0);
	if (ret < 0) {
		pr_err("%s no upm,intr_gpio(%d)\n",
				      __func__, ret);
	} else
		upm->intr_gpio = ret;

	pr_info("%s intr_gpio = %u\n",
			    __func__, upm->intr_gpio);

	upm->chg_det_enable =
	    of_property_read_bool(np, "upm,upm6910,charge-detect-enable");

	ret = of_property_read_u32(np, "upm,upm6910,usb-vlim", &pdata->usb.vlim);
	if (ret) {
		pdata->usb.vlim = 4500;
		pr_err("Failed to read node of upm,upm6910,usb-vlim\n");
	}

	upm->chg_en_gpio = of_get_named_gpio(np, "upm,chg-en-gpio", 0);
	if (upm->chg_en_gpio < 0)
		pr_err("upm, chg-en-gpio is not available\n");
	gpio_direction_output(upm->chg_en_gpio, 0);

	ret = of_property_read_u32(np, "upm,upm6910,usb-ilim", &pdata->usb.ilim);
	if (ret) {
		pdata->usb.ilim = 2000;
		pr_err("Failed to read node of upm,upm6910,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "upm,upm6910,usb-vreg", &pdata->usb.vreg);
	if (ret) {
		pdata->usb.vreg = 4200;
		pr_err("Failed to read node of upm,upm6910,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "upm,upm6910,usb-ichg", &pdata->usb.ichg);
	if (ret) {
		pdata->usb.ichg = 2000;
		pr_err("Failed to read node of upm,upm6910,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "upm,upm6910,stat-pin-ctrl",
				   &pdata->statctrl);
	if (ret) {
		pdata->statctrl = 0;
		pr_err("Failed to read node of upm,upm6910,stat-pin-ctrl\n");
	}

	ret = of_property_read_u32(np, "upm,upm6910,precharge-current",
				   &pdata->iprechg);
	if (ret) {
		pdata->iprechg = 180;
		pr_err("Failed to read node of upm,upm6910,precharge-current\n");
	}

	ret = of_property_read_u32(np, "upm,upm6910,termination-current",
				   &pdata->iterm);
	if (ret) {
		pdata->iterm = 180;
		pr_err
		    ("Failed to read node of upm,upm6910,termination-current\n");
	}

	ret =
	    of_property_read_u32(np, "upm,upm6910,boost-voltage",
				 &pdata->boostv);
	if (ret) {
		pdata->boostv = 5000;
		pr_err("Failed to read node of upm,upm6910,boost-voltage\n");
	}

	ret =
	    of_property_read_u32(np, "upm,upm6910,boost-current",
				 &pdata->boosti);
	if (ret) {
		pdata->boosti = 1200;
		pr_err("Failed to read node of upm,upm6910,boost-current\n");
	}

	ret = of_property_read_u32(np, "upm,upm6910,vac-ovp-threshold",
				   &pdata->vac_ovp);
	if (ret) {
		pdata->vac_ovp = 6500;
		pr_err("Failed to read node of upm,upm6910,vac-ovp-threshold\n");
	}

	return pdata;
}

static bool is_hiz_mode_trigger_interrupt(struct upm6910 *upm)
{
	bool exist = false;
	int ret = 0, vbus = 0, hiz_en = 0;
	u8 reg_val = 0, vbus_stat = 0;
	union power_supply_propval propval;

	if (!upm->usb_psy) {
		upm->usb_psy = power_supply_get_by_name("battery");
		if (!upm->usb_psy) {
			pr_err("%s Couldn't get usb psy\n", __func__);
			return -ENODEV;
		}
	}

	ret = power_supply_get_property(upm->usb_psy,
				POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT, &propval);
	if (!ret) {
		vbus = propval.intval;
	}

	ret = upm6910_read_byte(upm, UPM6910_REG_08, &reg_val);
	if (!ret) {
		vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
		vbus_stat >>= REG08_VBUS_STAT_SHIFT;
	}

	ret = upm6910_read_byte(upm, UPM6910_REG_00, &reg_val);
	if (!ret) {
		hiz_en = !!(reg_val & REG00_ENHIZ_MASK);
	}

	if (vbus > 4000 && vbus_stat != REG08_VBUS_TYPE_OTG && hiz_en) {
		exist = true;
	}

	return exist;
}

static int upm6910_get_charger_type(struct upm6910 *upm, int *type)
{
	int ret;

	u8 reg_val = 0;
	int vbus_stat = 0;
	int chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	if (is_upm_main_chg == true) {
		ret = upm6910_read_byte(upm, UPM6910_REG_08, &reg_val);
		if (ret)
			return ret;
		
		vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
		vbus_stat >>= REG08_VBUS_STAT_SHIFT;
	} else {
		ret = upm6910_read_byte(upm, SC89601A_REG_0B, &reg_val);
		if (ret)
			return ret;
		vbus_stat = (reg_val & SC89601A_VBUS_STAT_MASK);
		vbus_stat >>= SC89601A_VBUS_STAT_SHIFT;
	}

	pr_info("[%s] reg08: 0x%02x, vbus_stat:%d\n", __func__, reg_val, vbus_stat);

	switch (vbus_stat) {

	case REG08_VBUS_TYPE_NONE:
		chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case REG08_VBUS_TYPE_SDP:
		chg_type = POWER_SUPPLY_USB_TYPE_SDP;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		break;
	case REG08_VBUS_TYPE_CDP:
		chg_type = POWER_SUPPLY_USB_TYPE_CDP;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case REG08_VBUS_TYPE_DCP:
	case REG08__VBUS_TYPE_HVDCP:
		chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case REG08_VBUS_TYPE_UNKNOWN:
		chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case REG08_VBUS_TYPE_NON_STD:
		chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	default:
		chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	}

	if (chg_type != POWER_SUPPLY_USB_TYPE_DCP) {
		Charger_Detect_Release();
	}

	*type = chg_type;

	return 0;
}

/*prize liuyong, modify for kpoc charging, 20230410, start*/
static void upm6910_inform_kpoc_dwork_handler(struct work_struct *work)
{
	struct upm6910 *upm = container_of(work, struct upm6910,
								kpoc_dwork.work);

	pr_err("%s supply changed\n", __func__);
	schedule_delayed_work(&upm->psy_dwork, 0);
	power_supply_changed(upm->psy);
}
/*prize liuyong, modify for kpoc charging, 20230410, end*/

static void upm6910_inform_psy_dwork_handler(struct work_struct *work)
{
	int ret = 0;
	union power_supply_propval propval;
	struct upm6910 *upm = container_of(work, struct upm6910,
								psy_dwork.work);

	if (!upm->psy) {
		upm->psy = power_supply_get_by_name("charger");
		if (!upm->psy) {
			pr_err("%s Couldn't get psy\n", __func__);
			mod_delayed_work(system_wq, &upm->psy_dwork,
					msecs_to_jiffies(2*1000));
			return;
		}
	}

	if (upm->psy_usb_type != POWER_SUPPLY_USB_TYPE_UNKNOWN)
		propval.intval = 1;
	else
		propval.intval = 0;

	ret = power_supply_set_property(upm->psy, POWER_SUPPLY_PROP_ONLINE,
					&propval);
	if (ret < 0)
		pr_notice("inform power supply online failed:%d\n", ret);

	propval.intval = upm->psy_usb_type;

	ret = power_supply_set_property(upm->psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE,
					&propval);

	if (ret < 0)
		pr_notice("inform power supply charge type failed:%d\n", ret);

	return;
}

static irqreturn_t upm6910_irq_handler(int irq, void *data)
{
	int ret;
	u8 reg_val;
	bool prev_pg;
	bool prev_vbus_gd;
	bool hiz_irq = false;
	struct mtk_charger *info = NULL;

    struct upm6910 *upm = (struct upm6910 *)data;

	if (is_upm_main_chg == true) {
		ret = upm6910_read_byte(upm, UPM6910_REG_0A, &reg_val);
		if (ret)
			return IRQ_HANDLED;
	
		prev_vbus_gd = upm->vbus_gd;
	
		upm->vbus_gd = !!(reg_val & REG0A_VBUS_GD_MASK);
	
		ret = upm6910_read_byte(upm, UPM6910_REG_08, &reg_val);
		if (ret)
			return IRQ_HANDLED;
	
		prev_pg = upm->power_good;
	
		upm->power_good = !!(reg_val & REG08_PG_STAT_MASK);
	
		/*prize liuyong, modify for kpoc charging, 20230410, start*/
		if ((upm->bootmode == 8) || (upm->bootmode == 9)) {
			cancel_delayed_work_sync(&upm->kpoc_dwork);
		}
		/*prize liuyong, modify for kpoc charging, 20230410, end*/

		if (!prev_vbus_gd && upm->vbus_gd) {
			pr_notice("adapter/usb inserted\n");
		} else if (prev_vbus_gd && !upm->vbus_gd) {
			hiz_irq = is_hiz_mode_trigger_interrupt(upm);
			if (hiz_irq) {
				pr_notice("hiz mode trigger interrupt\n");
				upm->vbus_gd = true;
				upm->power_good = true;
				return IRQ_HANDLED;
			}
			/*prize liuyong, modify for kpoc charging, 20230410, start*/
			if ((upm->bootmode == 8) || (upm->bootmode == 9)) {
				{
					upm->chg_psy = power_supply_get_by_name("mtk-master-charger");
					info = (struct mtk_charger *)power_supply_get_drvdata(upm->chg_psy);
					if (info == NULL) {
						pr_notice("%s info is NULL\n", __func__);
						return -1;
					}
					pr_notice("%s info is pd_reset=%d\n", __func__,info->pd_reset);
				}
				
				if(!info->pd_reset)
				{
					schedule_delayed_work(&upm->kpoc_dwork,
							msecs_to_jiffies(KPOC_CHECK_DELAY));
					upm->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
					upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
				}
				if(info->pd_reset)
				{
					upm->power_good = true;
				}
				pr_notice("adapter/usb removed\n");
				return IRQ_HANDLED;
			}
			/*prize liuyong, modify for kpoc charging, 20230410, end*/
			upm->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
			upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			schedule_delayed_work(&upm->psy_dwork, 0);
			pr_notice("adapter/usb removed\n");
			//return IRQ_HANDLED;
		}
	} else {
		ret = upm6910_read_byte(upm, SC89601A_REG_0B, &reg_val);
		if (ret)
			return IRQ_HANDLED;
	
		prev_pg = upm->power_good;
	
		upm->power_good = !!(reg_val & SC89601A_PG_STAT_MASK);
	
		ret = upm6910_read_byte(upm, SC89601A_REG_11, &reg_val);
		if (ret)
				return IRQ_HANDLED;
	
		prev_vbus_gd = upm->vbus_gd;
	
		upm->vbus_gd = !!(reg_val & SC89601A_VBUS_GD_MASK);
	
		if (!prev_vbus_gd && upm->vbus_gd){
			//upm->input_curr_limit = 3000;
			pr_err("adapter/usb inserted\n");
			//sc89601a_adc_start(upm, false);
			//sc89601a_enable_charger(upm);
			//ret = sc89601a_get_charger_type(upm, &upm->psy_usb_type);
		} else if (prev_vbus_gd && !upm->vbus_gd) {
			pr_err("adapter/usb removed\n");
			/*prize liuyong, modify for kpoc charging, 20230410, start*/
			if ((upm->bootmode == 8) || (upm->bootmode == 9)) {
				schedule_delayed_work(&upm->kpoc_dwork,
						msecs_to_jiffies(KPOC_CHECK_DELAY));
				upm->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
				upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
				return IRQ_HANDLED;
			}
			/*prize liuyong, modify for kpoc charging, 20230410, end*/
			//sc89601a_adc_stop(upm);
			//ret = sc89601a_get_charger_type(upm, &upm->psy_usb_type);
			upm->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
			upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			schedule_delayed_work(&upm->psy_dwork, 0);
			//return IRQ_HANDLED;
		}
	}


	if (!prev_pg && upm->power_good) {
		ret = upm6910_get_charger_type(upm, &upm->psy_usb_type);
		schedule_delayed_work(&upm->psy_dwork, 0);
	}

	power_supply_changed(upm->psy);

	return IRQ_HANDLED;
}

static int upm6910_register_interrupt(struct upm6910 *upm)
{
	int ret = 0;
	ret = devm_gpio_request_one(upm->dev, upm->intr_gpio, GPIOF_DIR_IN,
			devm_kasprintf(upm->dev, GFP_KERNEL,
			"upm6910_intr_gpio.%s", dev_name(upm->dev)));
	if (ret < 0) {
		pr_err("%s gpio request fail(%d)\n",
				      __func__, ret);
		return ret;
	}
	upm->client->irq = gpio_to_irq(upm->intr_gpio);
	if (upm->client->irq < 0) {
		pr_err("%s gpio2irq fail(%d)\n",
				      __func__, upm->client->irq);
		return upm->client->irq;
	}
	pr_info("%s irq = %d\n", __func__, upm->client->irq);

	ret = devm_request_threaded_irq(upm->dev, upm->client->irq, NULL,
					upm6910_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"upm_irq", upm);
	if (ret < 0) {
		pr_err("request thread irq failed:%d\n", ret);
		return ret;
	}

	enable_irq_wake(upm->irq);

	return 0;
}

static int upm6910_set_compatible_key(struct upm6910 *upm)
{
	upm6910_write_byte(upm, 0x7D, 0x48);
	upm6910_write_byte(upm, 0x7D, 0x54);
	upm6910_write_byte(upm, 0x7D, 0x53);
	return upm6910_write_byte(upm, 0x7D, 0x38);
}

static int upm6910_disable_compatible_hvdcp(struct upm6910 *upm)
{
	int ret;
	u8 val = SC89601A_HVDCP_DISABLE << SC89601A_HVDCPEN_SHIFT;

	ret = upm6910_update_bits(upm, SC89601A_REG_02,
				SC89601A_HVDCPEN_MASK, val);
	return ret;
}

static int upm6910_enable_compatible_ico(struct upm6910 *upm, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = SC89601A_ICO_ENABLE << SC89601A_ICOEN_SHIFT;
	else
		val = SC89601A_ICO_DISABLE << SC89601A_ICOEN_SHIFT;

	ret = upm6910_update_bits(upm, SC89601A_REG_02, SC89601A_ICOEN_MASK, val);

	return ret;

}

static int upm6910_init_device(struct upm6910 *upm)
{
	int ret;

	if (is_upm_main_chg) {
		upm6910_disable_watchdog_timer(upm);

		upm->input_current_max = 3000;

		ret = upm6910_set_stat_ctrl(upm, upm->platform_data->statctrl);
		if (ret)
			pr_err("Failed to set stat pin control mode, ret = %d\n", ret);

		ret = upm6910_set_prechg_current(upm, upm->platform_data->iprechg);
		if (ret)
			pr_err("Failed to set prechg current, ret = %d\n", ret);

		ret = upm6910_set_term_current(upm, upm->platform_data->iterm);
		if (ret)
			pr_err("Failed to set termination current, ret = %d\n", ret);

		ret = upm6910_set_boost_voltage(upm, upm->platform_data->boostv);
		if (ret)
			pr_err("Failed to set boost voltage, ret = %d\n", ret);

		ret = upm6910_set_boost_current(upm, upm->platform_data->boosti);
		if (ret)
			pr_err("Failed to set boost current, ret = %d\n", ret);

		ret = upm6910_set_acovp_threshold(upm, upm->platform_data->vac_ovp);
		if (ret)
			pr_err("Failed to set acovp threshold, ret = %d\n", ret);

		ret = upm6910_set_int_mask(upm,
					REG0A_IINDPM_INT_MASK |
					REG0A_VINDPM_INT_MASK);
		if (ret)
			pr_err("Failed to set vindpm and iindpm int mask\n");
	} else {
		upm6910_reset_chip(upm);

		upm6910_set_compatible_key(upm);
		upm6910_update_bits(upm, 0x88, 0x60, 0x60);
		upm6910_set_compatible_key(upm);

		upm->input_current_max = 3000;

		upm6910_disable_watchdog_timer(upm);
		upm6910_disable_compatible_hvdcp(upm);
		upm6910_enable_compatible_ico(upm, false);

		ret = upm6910_set_prechg_current(upm, upm->platform_data->iprechg);
		if (ret)
			pr_err("Failed to set prechg current, ret = %d\n", ret);

		ret = upm6910_set_term_current(upm, upm->platform_data->iterm);
		if (ret)
			pr_err("Failed to set termination current, ret = %d\n", ret);

		ret = upm6910_set_boost_voltage(upm, upm->platform_data->boostv);
		if (ret)
			pr_err("Failed to set boost voltage, ret = %d\n", ret);

		ret = upm6910_set_boost_current(upm, upm->platform_data->boosti);
		if (ret)
			pr_err("Failed to set boost current, ret = %d\n", ret);
	}

	return 0;
}

static void upm6910_inform_prob_dwork_handler(struct work_struct *work)
{
	struct upm6910 *upm = container_of(work, struct upm6910,
								prob_dwork.work);
	pr_err("upm6910_inform_prob_dwork_handler\n");							
	Charger_Detect_Init();
	msleep(50);
	upm6910_dpdm_set_hidden_mode(upm);
	msleep(700);
	upm6910_force_dpdm(upm);
	msleep(300);
	upm6910_irq_handler(upm->irq, (void *) upm);
	/*prize liuyong, modify for charger detect init, 20230510 start */
	upm->is_need_detect_init = true;
	/*prize liuyong, modify for charger detect init, 20230510 end*/
}

static int upm6910_detect_device(struct upm6910 *upm)
{
	int ret;
	u8 data;

	ret = upm6910_read_byte(upm, UPM6910_REG_0B, &data);
	if (!ret) {
		upm->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
		upm->revision =
		    (data & REG0B_DEV_REV_MASK) >> REG0B_DEV_REV_SHIFT;
	}

	return ret;
}

static void upm6910_dump_regs(struct upm6910 *upm)
{
	int addr;
	u8 val;
	int ret;

	if (is_upm_main_chg == true) {
		for (addr = 0x0; addr <= 0x0B; addr++) {
			ret = upm6910_read_byte(upm, addr, &val);
			if (ret == 0)
				pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
		}
	} else {
			for (addr = 0x0; addr <= 0x14; addr++) {
			ret = upm6910_read_byte(upm, addr, &val);
			if (ret == 0)
				pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
		}
	}
}

static ssize_t
upm6910_show_registers(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct upm6910 *upm = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "upm6910 Reg");
	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = upm6910_read_byte(upm, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
				       "Reg[%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t
upm6910_store_registers(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct upm6910 *upm = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x0B) {
		upm6910_write_byte(upm, (unsigned char) reg,
				   (unsigned char) val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, upm6910_show_registers,
		   upm6910_store_registers);

static struct attribute *upm6910_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group upm6910_attr_group = {
	.attrs = upm6910_attributes,
};

static int upm6910_charging(struct charger_device *chg_dev, bool enable)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val;

	if (enable)
		ret = upm6910_enable_charger(upm);
	else
		ret = upm6910_disable_charger(upm);

	pr_err("%s charger %s\n", enable ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

	if (is_upm_main_chg == true) {
		ret = upm6910_read_byte(upm, UPM6910_REG_01, &val);
	} else {
		ret = upm6910_read_byte(upm, SC89601A_REG_03, &val);
	}

	if (is_upm_main_chg == true) {
		if (!ret)
			upm->charge_enabled = !!(val & REG01_CHG_CONFIG_MASK);
	} else {
		if (!ret)
		upm->charge_enabled = !!(val & SC89601A_CHG_CONFIG_MASK);
	}

	return ret;
}

static int upm6910_plug_in(struct charger_device *chg_dev)
{

	int ret;

	ret = upm6910_charging(chg_dev, true);

	if (ret)
		pr_err("Failed to enable charging:%d\n", ret);

	return ret;
}

static int upm6910_plug_out(struct charger_device *chg_dev)
{
	int ret;

	ret = upm6910_charging(chg_dev, false);

	if (ret)
		pr_err("Failed to disable charging:%d\n", ret);

	return ret;
}

static int upm6910_dump_register(struct charger_device *chg_dev)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	upm6910_dump_regs(upm);

	return 0;
}

static int upm6910_get_charging_state(struct upm6910 *upm, int *en)
{
	//struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	if (upm->charge_enabled == 0)
		*en = false;
	else
		*en = true;

	return 0;
}

static int upm6910_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	*en = upm->charge_enabled;

	return 0;
}

static int upm6910_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;
	int retry = 3;

	if (is_upm_main_chg == true) {
		ret = upm6910_read_byte(upm, UPM6910_REG_08, &val);
		if (!ret) {
			val = val & REG08_CHRG_STAT_MASK;
			val = val >> REG08_CHRG_STAT_SHIFT;
			*done = (val == REG08_CHRG_STAT_CHGDONE);
		}
	} else {
		do{
	
			ret = upm6910_read_byte(upm, SC89601A_REG_0B, &val);
			if (!ret) {
				pr_err("gezi ---%s ----reg0B=0x%x\n",__func__,val);
				val = val & SC89601A_CHRG_STAT_MASK;
				val = val >> SC89601A_CHRG_STAT_SHIFT;
				*done = (val == SC89601A_CHRG_STAT_CHGDONE);
				
				pr_err("%s ----%d %d\n",__func__,*done,retry);
				
				if(*done){
					retry--;
					msleep(2);
				}
				else{
					break;
				}
			}
			else{
				pr_err("read err--%s ----ret = %d\n",__func__,ret);
				ret = 0;
			}
		}while(retry > 0);
	}

	return ret;
}

static int upm6910_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge curr = %d\n", curr);

	return upm6910_set_chargecurrent(upm, curr / 1000);
}

static int upm6910_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
	*curr = 60 * 1000;

	return 0;
}

static int upm6910_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge volt = %d\n", volt);

	return upm6910_set_chargevolt(upm, volt / 1000);
}

static int upm6910_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int vchg;
	int ret;

	if (is_upm_main_chg == true) {
		ret = upm6910_read_byte(upm, UPM6910_REG_04, &reg_val);
		if (!ret) {
			vchg = (reg_val & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
			vchg = vchg * REG04_VREG_LSB + REG04_VREG_BASE;
			*volt = vchg * 1000;
		}
	} else {
		ret = upm6910_read_byte(upm, SC89601A_REG_06, &reg_val);
		if (!ret) {
			vchg = (reg_val & SC89601A_VREG_MASK) >> SC89601A_VREG_SHIFT;
			vchg = vchg * SC89601A_VREG_LSB + SC89601A_VREG_BASE;
			*volt = vchg * 1000;
		}
	}

	return ret;
}

static int upm6910_set_ivl(struct charger_device *chg_dev, u32 volt)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	pr_err("vindpm volt = %d\n", volt);

	return upm6910_set_input_volt_limit(upm, volt / 1000);

}

static int upm6910_set_icl(struct charger_device *chg_dev, u32 curr)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	pr_err("indpm curr = %d\n", curr);

	return upm6910_set_input_current_limit(upm, curr / 1000);
}

static int upm6910_kick_wdt(struct charger_device *chg_dev)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	return upm6910_reset_watchdog_timer(upm);
}

static int upm6910_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

	if (en)
		ret = upm6910_enable_otg(upm);
	else
		ret = upm6910_disable_otg(upm);

	pr_err("%s OTG %s\n", en ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

	return ret;
}

static int upm6910_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret;

	if (en)
		ret = upm6910_enable_safety_timer(upm);
	else
		ret = upm6910_disable_safety_timer(upm);

	return ret;
}

static int upm6910_is_safety_timer_enabled(struct charger_device *chg_dev,
					   bool *en)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = upm6910_read_byte(upm, UPM6910_REG_05, &reg_val);

	if (!ret)
		*en = !!(reg_val & REG05_EN_TIMER_MASK);

	return ret;
}

static int upm6910_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret;

	pr_err("otg curr = %d\n", curr);

	ret = upm6910_set_boost_current(upm, curr / 1000);

	return ret;
}

static int upm6910_en_pe_current_partern(struct charger_device *chg_dev, bool is_increase)
{
	int ret = 0;

	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
#if 0
    printk("upm6910_en_pe_current_partern start4\n");

    mutex_lock(&upm->pe_access_lock);
    upm6910_set_icl(upm->chg_dev, 600000);
    upm6910_set_ichg(upm->chg_dev, 1000000);
    mdelay(80);

    upm6910_set_icl(upm->chg_dev, 100000);
    mdelay(70);


    upm6910_set_icl(upm->chg_dev, 600000); //0
    mdelay(50); //50
    upm6910_set_icl(upm->chg_dev, 100000);
    mdelay(100); //100
    upm6910_set_icl(upm->chg_dev, 600000); //0
    mdelay(50);//50
    upm6910_set_icl(upm->chg_dev, 100000);
    mdelay(100); //100
    upm6910_set_icl(upm->chg_dev, 600000); //1
    mdelay(100); //100
    upm6910_set_icl(upm->chg_dev, 100000);
    mdelay(50); //50
    upm6910_set_icl(upm->chg_dev, 600000); //1
    mdelay(100);  //100
    upm6910_set_icl(upm->chg_dev, 100000);
    mdelay(50); //50
    upm6910_set_icl(upm->chg_dev, 600000); //0
    mdelay(50); //50
    upm6910_set_icl(upm->chg_dev, 100000);
    mdelay(100); //100

    upm6910_set_icl(upm->chg_dev, 600000); //stop
    mdelay(150);
    upm6910_set_icl(upm->chg_dev, 100000);
    mdelay(30);//100
    upm6910_set_icl(upm->chg_dev, 600000); //
	mutex_unlock(&upm->pe_access_lock);
	#else
	int curr = 0,temp_curr = 0;

	upm6910_set_ichg(upm->chg_dev, 1000000);
	upm6910_get_icl(upm->chg_dev, &temp_curr);
	
    //mdelay(80);
	mutex_lock(&upm->pe_access_lock);
	curr = 600000;
	pr_err("gezi----%s-----%d----start---curr = %d,is_increase=%d\n",__func__,__LINE__,curr,is_increase);
	upm6910_set_icl(upm->chg_dev, 100000);
	msleep(100);
	upm6910_set_icl(upm->chg_dev, curr);
	msleep(100);
	upm6910_set_icl(upm->chg_dev, 100000);
	msleep(100);
	upm6910_set_icl(upm->chg_dev, curr);
	msleep(100);
	upm6910_set_icl(upm->chg_dev, 100000);
	msleep(100);
	upm6910_set_icl(upm->chg_dev, curr);
	msleep(300);
	upm6910_set_icl(upm->chg_dev, 100000);
	msleep(100);
	upm6910_set_icl(upm->chg_dev, curr);
	msleep(300);
	upm6910_set_icl(upm->chg_dev, 100000);
	msleep(100);
	upm6910_set_icl(upm->chg_dev, curr);
	msleep(300);
	upm6910_set_icl(upm->chg_dev, 100000);
	msleep(100);
    upm6910_set_icl(upm->chg_dev, curr);
	msleep(500);
	upm6910_set_icl(upm->chg_dev, 100000);
	msleep(100);
    mutex_unlock(&upm->pe_access_lock);
    printk("upm6910_en_pe_current_partern end\n");
	upm6910_set_icl(upm->chg_dev, temp_curr);
	msleep(300);
	#endif
    return ret;
}
static int upm6910_set_pe20_efficiency_table(struct charger_device *chg_dev)
{
	return 0;
}

static int upm6910_en_pe20_current_partern(struct charger_device *chg_dev,u32 is_up)
{
	int ret = 0;	
	
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);

    printk("upm6910_en_pe20_current_partern start4\n");

    mutex_lock(&upm->pe_access_lock);
    upm6910_set_icl(upm->chg_dev, 600000);
    upm6910_set_ichg(upm->chg_dev, 1000000);
    mdelay(80);

    upm6910_set_icl(upm->chg_dev, 100000);
    mdelay(70);


    upm6910_set_icl(upm->chg_dev, 600000); //0
    mdelay(50); //50
    upm6910_set_icl(upm->chg_dev, 100000);
    mdelay(100); //100
    upm6910_set_icl(upm->chg_dev, 600000); //0
    mdelay(50);//50
    upm6910_set_icl(upm->chg_dev, 100000);
    mdelay(100); //100
    upm6910_set_icl(upm->chg_dev, 600000); //1
    mdelay(100); //100
    upm6910_set_icl(upm->chg_dev, 100000);
    mdelay(50); //50
    upm6910_set_icl(upm->chg_dev, 600000); //1
    mdelay(100);  //100
    upm6910_set_icl(upm->chg_dev, 100000); 
    mdelay(50); //50
    upm6910_set_icl(upm->chg_dev, 600000); //0
    mdelay(50); //50
    upm6910_set_icl(upm->chg_dev, 100000);
    mdelay(100); //100

    upm6910_set_icl(upm->chg_dev, 600000); //stop
    mdelay(150);
    upm6910_set_icl(upm->chg_dev, 100000);
    mdelay(30);//100

    upm6910_set_icl(upm->chg_dev, 600000); //
    mutex_unlock(&upm->pe_access_lock);
    printk("upm6910_en_pe20_current_partern end\n");
    return ret;
}
static int upm6910_set_pe20_reset(struct charger_device *chg_dev)
{
	int ret = 0;
	int temp_curr = 0;
	struct upm6910 *upm = dev_get_drvdata(&chg_dev->dev);
	upm6910_get_icl(upm->chg_dev, &temp_curr);
	ret = upm6910_set_icl(upm->chg_dev, 100000);
	if (ret < 0)
		goto out;

	msleep(250);

	ret = upm6910_set_icl(upm->chg_dev, temp_curr);

out:
	return ret;
}
static int upm6910_enable_cable_drop_comp(struct charger_device *chg_dev, bool en)
{
	return 0;
}

static enum power_supply_usb_type upm6910_chg_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_DCP,
};

static enum power_supply_property upm6910_chg_psy_properties[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static int upm6910_chg_property_is_writeable(struct power_supply *psy,
					    enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		return 1;
	default:
		return 0;
	}
	return 0;
}

static int upm6910_chg_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	int ret = 0;
	u32 _val;
	int data;
	struct upm6910 *upm = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "XinHeChip";
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = upm->power_good;
		//val->intval = upm->vbus_good;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret = upm6910_get_charging_state(upm, &data);
		if (ret < 0)
			break;
		val->intval = data;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
                val->intval = upm->constant_chargevolt * 1000;
                //ret = upm6910_get_chargevol(upm, &data);
                //if (ret < 0)
                    //break;
                //val->intval = data;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		//ret = upm6910_get_input_current_limit(upm, &_val);
        //if (ret < 0)
        //    break;
        val->intval = upm->input_current_limit * 1000;
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = upm6910_get_input_volt_limit(upm, &_val);
        if (ret < 0)
            break;
        val->intval = _val;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	        //ret = upm6910_get_term_current(upm, &data);
                //if (ret < 0)
      	        //    break;
                val->intval = upm->term_current * 1000;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		pr_err("---->POWER_SUPPLY_PROP_USB_TYPE : %d\n", upm->psy_usb_type);
		val->intval = upm->psy_usb_type;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (upm->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (upm->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		pr_err("---->POWER_SUPPLY_PROP_TYPE : %d\n", upm->psy_desc.type);
		val->intval = upm->psy_desc.type;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int upm6910_chg_set_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   const union power_supply_propval *val)
{
	int ret = 0;
	struct upm6910 *upm = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		/*prize liuyong, Add Charger identification initialization before adapter in, 20230426, start*/
		if (upm->is_need_detect_init == true) {
			Charger_Detect_Init();
			mdelay(5);
		}
		/*prize liuyong, Add Charger identification initialization before adapter in, 20230426, end*/
		upm6910_force_dpdm(upm);
		break;
	case POWER_SUPPLY_PROP_STATUS:
        if (val->intval) {
            ret = upm6910_enable_charger(upm);
        } else {
            ret = upm6910_disable_charger(upm);
        }
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = upm6910_set_chargecurrent(upm, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		upm->constant_chargevolt = val->intval / 1000;
		ret = upm6910_set_chargevolt(upm, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		upm->input_current_limit = val->intval / 1000;
		ret = upm6910_set_input_current_limit(upm, upm->input_current_limit);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = upm6910_set_input_volt_limit(upm, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		upm->term_current = val->intval / 1000;
	        ret = upm6910_set_term_current(upm, val->intval / 1000);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static char *upm6910_psy_supplied_to[] = {
	"battery",
	"mtk-master-charger",
};

static const struct power_supply_desc upm6910_psy_desc = {
	.type = POWER_SUPPLY_TYPE_USB,
	.usb_types = upm6910_chg_psy_usb_types,
	.num_usb_types = ARRAY_SIZE(upm6910_chg_psy_usb_types),
	.properties = upm6910_chg_psy_properties,
	.num_properties = ARRAY_SIZE(upm6910_chg_psy_properties),
	.property_is_writeable = upm6910_chg_property_is_writeable,
	.get_property = upm6910_chg_get_property,
	.set_property = upm6910_chg_set_property,
};

static int upm6910_chg_init_psy(struct upm6910 *upm)
{
	struct power_supply_config cfg = {
		.drv_data = upm,
		.of_node = upm->dev->of_node,
		.supplied_to = upm6910_psy_supplied_to,
		.num_supplicants = ARRAY_SIZE(upm6910_psy_supplied_to),
	};

	memcpy(&upm->psy_desc, &upm6910_psy_desc, sizeof(upm->psy_desc));
	upm->psy_desc.name = "charger";//dev_name(upm->dev);
	upm->psy = devm_power_supply_register(upm->dev, &upm->psy_desc,&cfg);
	
	//upm->pdpe_psy = power_supply_get_by_name("pdpe-state");
	//if(upm->pdpe_psy == NULL){
	//	pr_err("gezi get info->pdpe_psy failed\n");
	//}
	
	if (!upm->chg_psy) {
		upm->chg_psy = power_supply_get_by_name("mtk-master-charger");
	}
	
	return IS_ERR(upm->psy) ? PTR_ERR(upm->psy) : 0;
}

bool upm6910_is_need_dual_charger(void)
{
	return is_upm_main_chg;
}
EXPORT_SYMBOL_GPL(upm6910_is_need_dual_charger);

static struct charger_ops upm6910_chg_ops = {
	/* Normal charging */
	.plug_in = upm6910_plug_in,
	.plug_out = upm6910_plug_out,
	.dump_registers = upm6910_dump_register,
	.enable = upm6910_charging,
	.is_enabled = upm6910_is_charging_enable,
	.enable_chip = upm6910_charging,
	.is_chip_enabled = upm6910_is_charging_enable,
	.get_charging_current = upm6910_get_ichg,
	.set_charging_current = upm6910_set_ichg,
	.get_input_current = upm6910_get_icl,
	.set_input_current = upm6910_set_icl,
	.get_constant_voltage = upm6910_get_vchg,
	.set_constant_voltage = upm6910_set_vchg,
	.kick_wdt = upm6910_kick_wdt,
	.set_mivr = upm6910_set_ivl,
	.is_charging_done = upm6910_is_charging_done,
	.get_min_charging_current = upm6910_get_min_ichg,
	.enable_termination = upm6910_enable_term,
	/* Safety timer */
	.enable_safety_timer = upm6910_set_safety_timer,
	.is_safety_timer_enabled = upm6910_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = upm6910_set_otg,
	.set_boost_current_limit = upm6910_set_boost_ilmt,
	.enable_discharge = NULL,

	/* PE+/PE+20 */
	.send_ta20_current_pattern = upm6910_en_pe20_current_partern,
	.send_ta_current_pattern = upm6910_en_pe_current_partern,
	.set_pe20_efficiency_table = upm6910_set_pe20_efficiency_table,
	.reset_ta = upm6910_set_pe20_reset,
	.enable_cable_drop_comp = upm6910_enable_cable_drop_comp,

   /* Event */
    .event = upm6910_do_event,

   /* HIZ */
	//.set_hiz_mode = upm6910_set_hiz_mode,
	//.get_hiz_mode = upm6910_get_hiz_mode,
	.enable_hz = upm6910_set_hiz_mode,
	/* ADC */
	.get_tchg_adc = NULL,
	/* 6pin battery */
	.enable_6pin_battery_charging = NULL,
};

static struct of_device_id upm6910_charger_match_table[] = {
	{
	 .compatible = "upm,upm6910_charger",
	 .data = &pn_data[PN_UPM6910D],
	 },
	{},
};
MODULE_DEVICE_TABLE(of, upm6910_charger_match_table);

static int upm6910_charger_remove(struct i2c_client *client);
static int upm6910_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct upm6910 *upm;
//	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;

	int ret = 0;
	pr_err("upm6910 start\n");
	upm = devm_kzalloc(&client->dev, sizeof(struct upm6910), GFP_KERNEL);
	if (!upm)
		return -ENOMEM;

	client->addr = 0x6B;
	upm->dev = &client->dev;
	upm->client = client;

	i2c_set_clientdata(client, upm);

	mutex_init(&upm->i2c_rw_lock);

	ret = upm6910_detect_device(upm);
	if (ret) {
		pr_err("No upm6910 device found!\n");
	}

	pr_err("upm6910 NO:%d, Version:%d\n", upm->part_no, upm->revision);
	if (upm->part_no == 2)
		is_upm_main_chg = true;
	else
		is_upm_main_chg = false;
#if 0	
	match = of_match_node(upm6910_charger_match_table, node);
	if (match == NULL) {
		pr_err("device tree match not found\n");
		//return -EINVAL;
	}

	if (upm->part_no != *(int *)match->data) {
		pr_info("part no mismatch, hw:%s, devicetree:%s\n",
			pn_str[upm->part_no], pn_str[*(int *) match->data]);
        //upm6910_charger_remove(client);
        //return -EINVAL;
	}
#endif
	upm->platform_data = upm6910_parse_dt(node, upm);
	if (!upm->platform_data) {
		pr_err("No platform data provided.\n");
		//return -EINVAL;
	}

	INIT_DELAYED_WORK(&upm->psy_dwork, upm6910_inform_psy_dwork_handler);
	INIT_DELAYED_WORK(&upm->prob_dwork, upm6910_inform_prob_dwork_handler);
	/*prize liuyong, modify for kpoc charging, 20230410, start*/
	INIT_DELAYED_WORK(&upm->kpoc_dwork, upm6910_inform_kpoc_dwork_handler);
	/*prize liuyong, modify for kpoc charging, 20230410, end*/
	/*prize liuyong, modify for charger detect init, 20230510 start*/
	upm->is_need_detect_init = false;
	/*prize liuyong, modify for charger detect init, 20230510 end*/

	ret = upm6910_chg_init_psy(upm);
	if (ret < 0) {
		pr_err("failed init power supply\n");
		//return -EINVAL;
	}

	ret = upm6910_init_device(upm);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}

	upm6910_register_interrupt(upm);

	upm->chg_dev = charger_device_register(upm->chg_dev_name,
					      &client->dev, upm,
					      &upm6910_chg_ops,
					      &upm6910_chg_props);
	if (IS_ERR_OR_NULL(upm->chg_dev)) {
		ret = PTR_ERR(upm->chg_dev);
		return ret;
	}

	if (!upm->usb_psy) {
		upm->usb_psy = power_supply_get_by_name("battery");
		if (!upm->usb_psy) {
			pr_err("%s Couldn't get usb psy\n", __func__);
		}
	}

	ret = sysfs_create_group(&upm->dev->kobj, &upm6910_attr_group);
	if (ret)
		dev_err(upm->dev, "failed to register sysfs. err: %d\n", ret);

	mod_delayed_work(system_wq, &upm->prob_dwork,
					msecs_to_jiffies(2*1000));

	pr_err("upm6910 probe successfully, Part Num:%d, Revision:%d\n!",
	       upm->part_no, upm->revision);

	return 0;
#if 0
ERROR:
	devm_kfree(&client->dev, upm);
	pr_err("upm6910 probe fail\n!");
	return 0;
#endif
}

static int upm6910_charger_remove(struct i2c_client *client)
{
	struct upm6910 *upm = i2c_get_clientdata(client);

	mutex_destroy(&upm->i2c_rw_lock);

	sysfs_remove_group(&upm->dev->kobj, &upm6910_attr_group);

	return 0;
}

static void upm6910_charger_shutdown(struct i2c_client *client)
{
}

static struct i2c_driver upm6910_charger_driver = {
	.driver = {
		   .name = "upm6910-charger",
		   .owner = THIS_MODULE,
		   .of_match_table = upm6910_charger_match_table,
		   },

	.probe = upm6910_charger_probe,
	.remove = upm6910_charger_remove,
	.shutdown = upm6910_charger_shutdown,

};

module_i2c_driver(upm6910_charger_driver);

MODULE_DESCRIPTION("Unisemipower UPM6910D Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Unisemepower");
