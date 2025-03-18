// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
*/

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
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/regmap.h>
#include <linux/version.h>
#include <linux/phy/phy.h>

#define CONFIG_MTK_CLASS //drv mod by liuruiqian,20240327

#ifdef CONFIG_MTK_CLASS
#include "charger_class.h"
#include "mtk_charger.h"
#endif /*CONFIG_MTK_CLASS */

#define SC8989X_DRV_VERSION         "1.1.0_G"

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

enum sc8960x_part_no {
    SC89890H_PN_NUM = 0x04,
    SC89895_PN_NUM = 0x04,
    SC8950_PN_NUM = 0x02,
    SC89890W_PN_NUM = 0x07,
};

#define SC8989X_VINDPM_MAX           15300      //mV

#define SC8989X_REG7D                0x7D
#define SC8989X_KEY1                 0x48
#define SC8989X_KEY2                 0x54
#define SC8989X_KEY3                 0x53
#define SC8989X_KEY4                 0x38

#define SC8989X_REG7E                0x7E
#define SC8989X_OWN_KEY1             0x48
#define SC8989X_OWN_KEY2             0x54
#define SC8989X_OWN_KEY3             0x53
#define SC8989X_OWN_KEY4             0x39

#define SC8989X_ADC_EN               0x87
#define SC8989X_DPDM3                0x88
#define SC8989X_PRIVATE              0xF9

#define VINDPM_BEFORCE_BC12         0
#define VIMDPM_DEFAULT_IN_BC12      5000
#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2

static const char *const sc8989x_attach_trig_names[] = {
	"ignore", "pwr_rdy", "typec",
};

enum attach_type {
	ATTACH_TYPE_NONE,
	ATTACH_TYPE_PWR_RDY,
	ATTACH_TYPE_TYPEC,
	ATTACH_TYPE_PD,
	ATTACH_TYPE_PD_SDP,
	ATTACH_TYPE_PD_DCP,
	ATTACH_TYPE_PD_NONSTD,
};

enum sc8989x_attach_trigger {
	ATTACH_TRIG_IGNORE,
	ATTACH_TRIG_PWR_RDY,
	ATTACH_TRIG_TYPEC,
};

enum sc8989x_usbsw {
	USBSW_CHG = 0,
	USBSW_USB,
};

enum sc8989x_vbus_stat {
    VBUS_STAT_NO_INPUT = 0,
    VBUS_STAT_SDP,
    VBUS_STAT_CDP,
    VBUS_STAT_DCP,
    VBUS_STAT_HVDCP,
    VBUS_STAT_UNKOWN,
    VBUS_STAT_NONSTAND,
    VBUS_STAT_OTG,
};

static const char *const sc8989x_port_stat_names[] = {
	[VBUS_STAT_NO_INPUT] = "No Info",
	[VBUS_STAT_SDP] = "SDP",
	[VBUS_STAT_CDP] = "CDP",
	[VBUS_STAT_DCP] = "DCP",
	[VBUS_STAT_HVDCP] = "HVDCP",
	[VBUS_STAT_UNKOWN] = "Unknown",
	[VBUS_STAT_NONSTAND] = "Nonstand",
	[VBUS_STAT_OTG] = "otg",
};


enum sc8989x_chg_stat {
    CHG_STAT_NOT_CHARGE = 0,
    CHG_STAT_PRE_CHARGE,
    CHG_STAT_FAST_CHARGE,
    CHG_STAT_CHARGE_DONE,
};

enum sc8989x_adc_channel {
    SC8989X_ADC_VBAT,
    SC8989X_ADC_VSYS,
    SC8989X_ADC_VBUS,
    SC8989X_ADC_ICC,
    SC8989X_ADC_IBUS,

};

enum vindpm_track {
    SC8989X_TRACK_DIS,
    SC8989X_TRACK_200,
    SC8989X_TRACK_250,
    SC8989X_TRACK_300,
};

enum {
    SC8989X_VBUSSTAT_NOINPUT = 0,
    SC8989X_VBUSSTAT_SDP,
    SC8989X_VBUSSTAT_CDP,
    SC8989X_VBUSSTAT_DCP,
    SC8989X_VBUSSTAT_HVDCP,
    SC8989X_VBUSSTAT_FLOAT,
    SC8989X_VBUSSTAT_NON_STD,
    SC8989X_VBUSSTAT_OTG,
};

enum sc8989x_fields {
    F_EN_HIZ, F_EN_ILIM, F_IINDPM,
    F_DP_DRIVE, F_DM_DRIVE, F_VINDPM_OS,
    F_CONV_START, F_CONV_RATE, F_BOOST_FRE, F_ICO_EN, F_HVDCP_EN, F_FORCE_DPDM,
	F_AUTO_DPDM_EN,
    F_FORCE_DSEL, F_WD_RST, F_OTG_CFG, F_CHG_CFG, F_VSYS_MIN, F_VBATMIN_SEL,
    F_EN_PUMPX, F_ICC,
    F_ITC, F_ITERM,
    F_CV, F_VBAT_LOW, F_VRECHG,
    F_EN_ITERM, F_STAT_DIS, F_TWD, F_EN_TIMER, F_TCHG, F_JEITA_ISET,
    F_BAT_COMP, F_VCLAMP, F_TJREG,
    F_FORCE_ICO, F_TMR2X_EN, F_BATFET_DIS, F_JETTA_VSET_WARM, F_BATFET_DLY, F_BATFET_RST_EN,
	F_PUMPX_UP, F_PUMPX_DN,
    F_V_OTG, F_PFM_OTG_DIS, F_IBOOST_LIM,
    F_VBUS_STAT, F_CHG_STAT, F_PG_STAT, F_VSYS_STAT,
    F_FORCE_VINDPM, F_VINDPM,
    F_ADC_VBAT,
    F_ADC_VSYS,
    F_VBUS_GD,F_ADC_VBUS,
    F_ADC_ICC,
    F_VINDPM_STAT, F_IINDPM_STAT,
    F_REG_RST, F_ICO_STAT, F_PN, F_NTC_PROFILE, F_DEV_VERSION,
    F_VBAT_REG_LSB,
    F_ADC_IBUS,
    F_DP3P3V_DM0V_EN,
    F_IIC_OPTION,
    F_VINDPM_TRACK,

    F_MAX_FIELDS,
};

enum sc8989x_reg_range {
    SC8989X_IINDPM,
    SC8989X_ICHG,
    SC8989X_IBOOST,
    SC8989X_VBAT_REG,
    SC8989X_VINDPM,
    SC8989X_ITERM,
    SC8989X_VBAT,
    SC8989X_VSYS,
    SC8989X_VBUS,
    SC8989X_IBUS,
    SC8989X_ICC,
};

struct reg_range {
    u32 min;
    u32 max;
    u32 step;
    u32 offset;
    const u32 *table;
    u16 num_table;
    bool round_up;
};

struct sc8989x_cfg_e {
    const char *chg_name;
    int ico_en;
    int hvdcp_en;
    int auto_dpdm_en;
    int vsys_min;
    int vbatmin_sel;
    int itrick;
    int iterm;
    int vbat_cv;
    int vbat_low;
    int vrechg;
    int en_term;
    int stat_dis;
    int wd_time;
    int en_timer;
    int charge_timer;
    int bat_comp;
    int vclamp;
    int votg;
    int iboost;
    int force_vindpm;
    int vindpm;
};

/* These default values will be applied if there's no property in dts */
static struct sc8989x_cfg_e sc8989x_default_cfg = {
    .chg_name = "primary_chg",
    .ico_en = 0,
    .hvdcp_en = 1,
    .auto_dpdm_en = 1,
    .vsys_min = 5,
    .vbatmin_sel = 0,
    .itrick = 3,
    .iterm = 4,
    .vbat_cv = 23,
    .vbat_low = 1,
    .vrechg = 0,
    .en_term = 1,
    .stat_dis = 0,
    .wd_time = 0,
    .en_timer = 1,
    .charge_timer = 2,
    .bat_comp = 0,
    .vclamp = 0,
    .votg = 12,
    .iboost = 7,
    .force_vindpm = 0,
    .vindpm = 18,
};

struct sc8989x_chip {
    struct device *dev;
    struct i2c_client *client;
    struct regmap *regmap;
    struct regmap_field *rmap_fields[F_MAX_FIELDS];

    struct charger_device *chg_dev;
    struct delayed_work psy_dwork;

	int bc12_en;
    int chg_type;
    int psy_usb_type;

    int irq_gpio;
    int irq;

    struct delayed_work force_detect_dwork;
    int force_detect_count;
    bool pr_swap;

    int power_good;
    bool true_online;
    int vbus_good;
    uint8_t dev_id;

    struct	mutex request_dpdm_lock;
	struct	regulator *dpdm_reg;
	bool	dpdm_enabled;

    struct power_supply_desc psy_desc;
    struct sc8989x_cfg_e *cfg;
    struct power_supply *psy;
    struct power_supply *chg_psy;
	struct power_supply *pmic; //drv add by liuhai 20240416

	struct regulator_desc otg_rdesc;
	struct regulator_dev *otg_rdev;

	u32 bootmode;
	u32 boottype;

	// drv add tankaikun, add external charger type detect, 20231213 start
	enum sc8989x_attach_trigger attach_trig;
	struct mutex attach_lock;
	bool bc12_dn;
	bool pwr_rdy;
	atomic_t attach;
	struct work_struct bc12_work;
	struct workqueue_struct *wq;
	// drv add tankaikun, add external charger type detect, 20231213 end
	struct completion pumpx_done;
	atomic_t pe_complete;
	struct mutex pe_lock;
};

static const u32 sc8989x_iboost[] = {
    500, 750, 1200, 1400, 1650, 1875, 2150, 2450,
};

#define SC8989X_CHG_RANGE(_min, _max, _step, _offset, _ru) \
{ \
    .min = _min, \
    .max = _max, \
    .step = _step, \
    .offset = _offset, \
    .round_up = _ru, \
}

#define SC8989X_CHG_RANGE_T(_table, _ru) \
    { .table = _table, .num_table = ARRAY_SIZE(_table), .round_up = _ru, }

static const struct reg_range sc8989x_reg_range_ary[] = {
    [SC8989X_IINDPM] = SC8989X_CHG_RANGE(100, 3250, 50, 100, false),
    [SC8989X_ICHG] = SC8989X_CHG_RANGE(0, 5040, 60, 0, false),
    [SC8989X_ITERM] = SC8989X_CHG_RANGE(30, 930, 60, 30, false),
    [SC8989X_VBAT_REG] = SC8989X_CHG_RANGE(3840, 4848, 16, 3840, false),
    [SC8989X_VINDPM] = SC8989X_CHG_RANGE(3900, 15300, 100, 2600, false),
    [SC8989X_IBOOST] = SC8989X_CHG_RANGE_T(sc8989x_iboost, false),
    [SC8989X_VBAT] = SC8989X_CHG_RANGE(2304, 4848, 20, 2304, false),
    [SC8989X_VSYS] = SC8989X_CHG_RANGE(2304, 4848, 20, 2304, false),
    [SC8989X_VBUS] = SC8989X_CHG_RANGE(2600, 15300, 100, 2600, false),
    [SC8989X_IBUS] = SC8989X_CHG_RANGE(0, 6350, 50, 0, false),
    [SC8989X_ICC] = SC8989X_CHG_RANGE(0, 6350, 50, 0, false),
};

//REGISTER
static const struct reg_field sc8989x_reg_fields[] = {
    /*reg00 */
    [F_EN_HIZ] = REG_FIELD(0x00, 7, 7),
    [F_EN_ILIM] = REG_FIELD(0x00, 6, 6),
    [F_IINDPM] = REG_FIELD(0x00, 0, 5),
    /*reg01 */
    [F_DP_DRIVE] = REG_FIELD(0x01, 5, 7),
    [F_DM_DRIVE] = REG_FIELD(0x01, 2, 4),
    [F_VINDPM_OS] = REG_FIELD(0x01, 0, 0),
    /*reg02 */
    [F_CONV_START] = REG_FIELD(0x02, 7, 7),
    [F_CONV_RATE] = REG_FIELD(0x02, 6, 6),
    [F_BOOST_FRE] = REG_FIELD(0x02, 5, 5),
    [F_ICO_EN] = REG_FIELD(0x02, 4, 4),
    [F_HVDCP_EN] = REG_FIELD(0x02, 3, 3),
    [F_FORCE_DPDM] = REG_FIELD(0x02, 1, 1),
    [F_AUTO_DPDM_EN] = REG_FIELD(0x02, 0, 0),
    /*reg03 */
    [F_FORCE_DSEL] = REG_FIELD(0x03, 7, 7),
    [F_WD_RST] = REG_FIELD(0x03, 6, 6),
    [F_OTG_CFG] = REG_FIELD(0x03, 5, 5),
    [F_CHG_CFG] = REG_FIELD(0x03, 4, 4),
    [F_VSYS_MIN] = REG_FIELD(0x03, 1, 3),
    [F_VBATMIN_SEL] = REG_FIELD(0x03, 0, 0),
    /*reg04 */
    [F_EN_PUMPX] = REG_FIELD(0x04, 7, 7),
    [F_ICC] = REG_FIELD(0x04, 0, 6),
    /*reg05 */
    [F_ITC] = REG_FIELD(0x05, 4, 7),
    [F_ITERM] = REG_FIELD(0x05, 0, 3),
    /*reg06 */
    [F_CV] = REG_FIELD(0x06, 2, 7),
    [F_VBAT_LOW] = REG_FIELD(0x06, 1, 1),
    [F_VRECHG] = REG_FIELD(0x06, 0, 0),
    /*reg07 */
    [F_EN_ITERM] = REG_FIELD(0x07, 7, 7),
    [F_STAT_DIS] = REG_FIELD(0x07, 6, 6),
    [F_TWD] = REG_FIELD(0x07, 4, 5),
    [F_EN_TIMER] = REG_FIELD(0x07, 3, 3),
    [F_TCHG] = REG_FIELD(0x07, 1, 2),
    [F_JEITA_ISET] = REG_FIELD(0x07, 0, 0),
    /*reg08 */
    [F_BAT_COMP] = REG_FIELD(0x08, 5, 7),
    [F_VCLAMP] = REG_FIELD(0x08, 2, 4),
    [F_TJREG] = REG_FIELD(0x08, 0, 1),
    /*reg09 */
    [F_FORCE_ICO] = REG_FIELD(0x09, 7, 7),
    [F_TMR2X_EN] = REG_FIELD(0x09, 6, 6),
    [F_BATFET_DIS] = REG_FIELD(0x09, 5, 5),
    [F_JETTA_VSET_WARM] = REG_FIELD(0x09, 4, 4),
    [F_BATFET_DLY] = REG_FIELD(0x09, 3, 3),
    [F_BATFET_RST_EN] = REG_FIELD(0x09, 2, 2),
    [F_PUMPX_UP] = REG_FIELD(0x09, 1, 1),
    [F_PUMPX_DN] = REG_FIELD(0x09, 0, 0),
    /*reg0A */
    [F_V_OTG] = REG_FIELD(0x0A, 4, 7),
    [F_PFM_OTG_DIS] = REG_FIELD(0x0A, 3, 3),
    [F_IBOOST_LIM] = REG_FIELD(0x0A, 0, 2),
    /*reg0B */
    [F_VBUS_STAT] = REG_FIELD(0x0B, 5, 7),
    [F_CHG_STAT] = REG_FIELD(0x0B, 3, 4),
    [F_PG_STAT] = REG_FIELD(0x0B, 2, 2),
    [F_VSYS_STAT] = REG_FIELD(0x0B, 0, 0),
    /*reg0D */
    [F_FORCE_VINDPM] = REG_FIELD(0x0D, 7, 7),
    [F_VINDPM] = REG_FIELD(0x0D, 0, 6),
    /*reg0E */
    [F_ADC_VBAT] = REG_FIELD(0x0E, 0, 6),
    /*reg0F */
    [F_ADC_VSYS] = REG_FIELD(0x0F, 0, 6),
    /*reg11 */
    [F_VBUS_GD] = REG_FIELD(0x11, 7, 7),
    [F_ADC_VBUS] = REG_FIELD(0x11, 0, 6),
    /*reg12 */
    [F_ADC_ICC] = REG_FIELD(0x12, 0, 6),
    /*reg13 */
    [F_VINDPM_STAT] = REG_FIELD(0x13, 7, 7),
    [F_IINDPM_STAT] = REG_FIELD(0x13, 6, 6),
    /*reg14 */
    [F_REG_RST] = REG_FIELD(0x14, 7, 7),
    [F_ICO_STAT] = REG_FIELD(0x14, 6, 6),
    [F_PN] = REG_FIELD(0x14, 3, 5),
    [F_NTC_PROFILE] = REG_FIELD(0x14, 2, 2),
    [F_DEV_VERSION] = REG_FIELD(0x14, 0, 1),
    /*reg40 */
    [F_VBAT_REG_LSB] = REG_FIELD(0x40, 6, 6),
    /*reg82 */
    [F_IIC_OPTION] = REG_FIELD(0x82, 0, 1),
    /*reg86 */
    [F_ADC_IBUS] = REG_FIELD(0x86, 1, 7),
    /*reg83 */
    [F_DP3P3V_DM0V_EN] = REG_FIELD(0x83, 5, 5),
    [F_VINDPM_TRACK] = REG_FIELD(0x85, 1, 2),
};
static int sc8989x_regmap_write(void *context, unsigned int reg,
				       unsigned int val)
{
    struct sc8989x_chip *sc = context;
    struct i2c_client *i2c = to_i2c_client(sc->dev);
    int ret = 0;

    if (val > 0xff || reg > 0xff)
        return -EINVAL;

    if (reg == 0x2) {
        ret = i2c_smbus_read_byte_data(i2c, reg);
        ret &= 0x2;
        if (ret) {
            val &= 0xFD;
            dev_err(sc->dev, "bc12 do not finish, clear force dpdm bit, new val = 0x%x\n", val);
        }
    }

    return i2c_smbus_write_byte_data(i2c, reg, val);
}

static int sc8989x_regmap_read(void *context, unsigned int reg,
				      unsigned int *val)
{
	struct sc8989x_chip *sc = context;
	struct i2c_client *i2c = to_i2c_client(sc->dev);
	int ret;

	if (reg > 0xff)
		return -EINVAL;

	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret < 0)
		return ret;

	*val = ret;

	return 0;
}

static const struct regmap_bus sc8989x_regmap_bus = {
	.reg_write = sc8989x_regmap_write,
	.reg_read = sc8989x_regmap_read,
};

static const struct regmap_config sc8989x_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,

    .max_register = 0xFF,
};

/********************COMMON API***********************/
static u8 val2reg(enum sc8989x_reg_range id, u32 val) {
    int i;
    u8 reg;
    const struct reg_range *range = &sc8989x_reg_range_ary[id];

    if (!range)
        return val;

    if (range->table) {
        if (val <= range->table[0])
            return 0;
        for (i = 1; i < range->num_table - 1; i++) {
            if (val == range->table[i])
                return i;
            if (val > range->table[i] && val < range->table[i + 1])
                return range->round_up ? i + 1 : i;
        }
        return range->num_table - 1;
    }
    if (val <= range->min)
        reg = (range->min - range->offset) / range->step;
    else if (val >= range->max)
        reg = (range->max - range->offset) / range->step;
    else if (range->round_up)
        reg = (val - range->offset) / range->step + 1;
    else
        reg = (val - range->offset) / range->step;
    return reg;
}

static u32 reg2val(enum sc8989x_reg_range id, u8 reg) {
    const struct reg_range *range = &sc8989x_reg_range_ary[id];
    if (!range)
        return reg;
    return range->table ? range->table[reg] : range->offset + range->step * reg;
}

/*********************I2C API*********************/
static int sc8989x_field_read(struct sc8989x_chip *sc,
                            enum sc8989x_fields field_id, int *val) {
    int ret;

	int retry_cnt = 5;
	do{
		ret = regmap_field_read(sc->rmap_fields[field_id], val);
		if (ret < 0) {
			retry_cnt--;
			dev_err(sc->dev, "sc8989x read field %d fail: %d,retry_cnt %d\n", field_id, ret,retry_cnt);
			msleep(2);
		}
		else
		{
			return ret;
		}
	}while(retry_cnt > 0);
    return ret;
}

static int sc8989x_field_write(struct sc8989x_chip *sc,
                            enum sc8989x_fields field_id, int val) {
    int ret;
	int retry_cnt = 5;	
	do
	{
		ret = regmap_field_write(sc->rmap_fields[field_id], val);
		if (ret < 0) {
			retry_cnt--;
			dev_err(sc->dev, "sc8989x write field %d fail: %d  retry_cnt %d\n", field_id, ret,retry_cnt);
			msleep(2);
		}
		else
		{
			return ret;
		}
	}while(retry_cnt > 0 );
    return ret;
}

/*********************CHIP API*********************/
static int sc8989x_set_key(struct sc8989x_chip *sc) {
    regmap_write(sc->regmap, SC8989X_REG7D, SC8989X_KEY1);
    regmap_write(sc->regmap, SC8989X_REG7D, SC8989X_KEY2);
    regmap_write(sc->regmap, SC8989X_REG7D, SC8989X_KEY3);
    return regmap_write(sc->regmap, SC8989X_REG7D, SC8989X_KEY4);
}

static int sc8989x_get_key_status(struct sc8989x_chip *sc) {
    int reg_val;
    int ret;

    ret = regmap_read(sc->regmap, SC8989X_REG7D, &reg_val);
    if (ret < 0) {
        dev_err(sc->dev, "%s: sc8989x read reg 0x7D fail: %d\n", __func__, ret);
        return ret;
    }  
    return reg_val;
}

__maybe_unused
static int sc8989x_set_key_own(struct sc8989x_chip *sc) {
    regmap_write(sc->regmap, SC8989X_REG7E, SC8989X_OWN_KEY1);
    regmap_write(sc->regmap, SC8989X_REG7E, SC8989X_OWN_KEY2);
    regmap_write(sc->regmap, SC8989X_REG7E, SC8989X_OWN_KEY3);
    return regmap_write(sc->regmap, SC8989X_REG7E, SC8989X_OWN_KEY4);
}

static int sc8989x_set_wa(struct sc8989x_chip *sc) {
    int ret;
    int val;

    ret = regmap_read(sc->regmap, SC8989X_DPDM3, &val);
    if (ret < 0) {
        sc8989x_set_key(sc);
    }

    regmap_write(sc->regmap, SC8989X_DPDM3, SC8989X_PRIVATE);

    return sc8989x_set_key(sc);
}

__maybe_unused 
static int sc8989x_set_vbat_lsb(struct sc8989x_chip *sc, bool en) {
    int ret;
    int val;

    ret = sc8989x_field_read(sc, F_VBAT_REG_LSB, &val);
    if (ret < 0) {
        sc8989x_set_key(sc);
    }

    sc8989x_field_write(sc, F_VBAT_REG_LSB, en);

    return sc8989x_set_key(sc);
}

__maybe_unused
static int sc8989x_set_vindpm_track(struct sc8989x_chip *sc,
                                     enum vindpm_track track) {
    int reg_val;

    if (!sc8989x_get_key_status(sc)) {
        sc8989x_set_key(sc);
    }

    sc8989x_field_write(sc, F_VINDPM_TRACK, track);
    sc8989x_field_read(sc, F_VINDPM_TRACK, &reg_val);
    dev_info(sc->dev, "%s: successful set vindpm track as 0x%x\n",
                           __func__, reg_val);

    return sc8989x_set_key(sc);
}

static int sc8989x_set_iic_option(struct sc8989x_chip *sc) {
    int ret;
    int val;

    ret = sc8989x_field_read(sc, F_IIC_OPTION, &val);
    if (ret < 0) {
        sc8989x_set_key(sc);
    }    
    sc8989x_field_write(sc, F_IIC_OPTION, 0x00);

    return sc8989x_set_key(sc);
}

static int sc8989x_get_adc_en(struct sc8989x_chip *sc)
{
    int reg_val = 0;
    sc8989x_field_read(sc, F_CONV_RATE, &reg_val);
    return reg_val;
}

//Start 1s Continuous Conversion
static int sc8989x_adc_en(struct sc8989x_chip *sc, bool en)
{
    int reg_val = en;
    return sc8989x_field_write(sc, F_CONV_RATE, reg_val);
}

__maybe_unused
static int sc8989x_adc_ibus_en(struct sc8989x_chip *sc, bool en) {
    int ret;
    int val;

    ret = regmap_read(sc->regmap, SC8989X_ADC_EN, &val);
    if (ret < 0) {
        sc8989x_set_key(sc);
    }
    ret = regmap_read(sc->regmap, SC8989X_ADC_EN, &val);
    val = en ? val | BIT(2) : val & ~BIT(2);

    regmap_write(sc->regmap, SC8989X_ADC_EN, val);

    return sc8989x_set_key(sc);
}

__maybe_unused
static int sc8989x_get_adc_ibus(struct sc8989x_chip *sc, int *val)
{
    int ret;
    int reg = 0;

    ret = sc8989x_field_read(sc, F_ADC_IBUS, &reg);
    if (ret < 0) {
        sc8989x_set_key(sc);
    }
    ret = sc8989x_field_read(sc, F_ADC_IBUS, &reg);
    *val = reg2val(SC8989X_IBUS, (u8)reg);

    return sc8989x_set_key(sc);
}

__maybe_unused
static int __sc8989x_get_adc(struct sc8989x_chip *sc, enum sc8989x_adc_channel chan,
                            int *val) {
    int reg_val, ret = 0;
    enum sc8989x_fields field_id;
    enum sc8989x_reg_range range_id;
    bool pre_adc_en = !! sc8989x_get_adc_en(sc);

    //close adc 1s Continuous Conversion
    sc8989x_adc_en(sc, false);
    //stat adc conversion  default: one shot
    sc8989x_field_write(sc, F_CONV_START, true);
    msleep(20);
    switch (chan) {
    case SC8989X_ADC_VBAT:
        field_id = F_ADC_VBAT;
        range_id = SC8989X_VBAT;
        break;
    case SC8989X_ADC_VSYS:
        field_id = F_ADC_VSYS;
        range_id = SC8989X_VSYS;
        break;
    case SC8989X_ADC_VBUS:
        field_id = F_ADC_VBUS;
        range_id = SC8989X_VBUS;
        break;
    case SC8989X_ADC_ICC:
        field_id = F_ADC_ICC;
        range_id = SC8989X_ICC;
        break;
    case SC8989X_ADC_IBUS:
        sc8989x_get_adc_ibus(sc, val);
		*val = *val*1000;
        dev_info(sc->dev, "get_adc channel: %d val: %d", chan, *val);
        return 0;
    default:
        goto err;
    }

    ret = sc8989x_field_read(sc, field_id, &reg_val);
    if (ret < 0)
        goto err;
    *val = reg2val(range_id, reg_val);
	
	if(SC8989X_ADC_VBUS == chan )
	{
		*val = *val*1000;
	}
    sc8989x_adc_en(sc, pre_adc_en);
    dev_info(sc->dev, "get_adc channel: %d val: %d", chan, *val);
    return 0;
err:
    dev_err(sc->dev, "get_adc fail channel: %d", chan);
    return -EINVAL;
}

static int sc8989x_set_iindpm(struct sc8989x_chip *sc, int curr_ma) {
    int reg_val = val2reg(SC8989X_IINDPM, curr_ma);
    //Configuring hardware IINDPM fails when using software IINDPM
    sc8989x_field_write(sc, F_EN_ILIM, 0);
    return sc8989x_field_write(sc, F_IINDPM, reg_val);
}

static int sc8989x_get_iindpm(struct sc8989x_chip *sc, int *curr_ma) {
    int ret, reg_val;

    ret = sc8989x_field_read(sc, F_IINDPM, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read iindpm failed(%d)\n", ret);
        return ret;
    }

    *curr_ma = reg2val(SC8989X_IINDPM, reg_val);

    return ret;
}

static int sc8989x_set_dpdm_hiz(struct sc8989x_chip *sc)
{
    int ret = 0;
    ret = sc8989x_field_write(sc, F_DP_DRIVE, 0);
    ret |= sc8989x_field_write(sc, F_DM_DRIVE, 0);
    return ret;
}

__maybe_unused
static int sc8989x_reset_wdt(struct sc8989x_chip *sc) {
    return sc8989x_field_write(sc, F_WD_RST, 1);
}

static int sc8989x_set_chg_enable(struct sc8989x_chip *sc, bool enable) {
    int reg_val = enable ? 1 : 0;

    return sc8989x_field_write(sc, F_CHG_CFG, reg_val);
}

__maybe_unused 
static int sc8989x_check_chg_enabled(struct sc8989x_chip *sc,
                                                    bool * enable) {
    int ret, reg_val;

    ret = sc8989x_field_read(sc, F_CHG_CFG, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read charge enable failed(%d)\n", ret);
        return ret;
    }
    *enable = !!reg_val;

    return ret;
}

__maybe_unused
static int sc8989x_set_otg_enable(struct sc8989x_chip *sc,
                                                bool enable) {
    int reg_val = enable ? 1 : 0;

    return sc8989x_field_write(sc, F_OTG_CFG, reg_val);
}

__maybe_unused
static int sc8989x_batfet_rst_en(struct sc8989x_chip *sc,
                                                bool enable) {
    int reg_val = enable ? 1 : 0;

    return sc8989x_field_write(sc, F_BATFET_RST_EN, reg_val);
}

__maybe_unused
static int sc8989x_set_iboost(struct sc8989x_chip *sc,
                                            int curr_ma) {
    int reg_val = val2reg(SC8989X_IBOOST, curr_ma);

    return sc8989x_field_write(sc, F_IBOOST_LIM, reg_val);
}

static int sc8989x_set_ichg(struct sc8989x_chip *sc, int curr_ma) {
    int reg_val = val2reg(SC8989X_ICHG, curr_ma);

    return sc8989x_field_write(sc, F_ICC, reg_val);
}

static int sc8989x_get_ichg(struct sc8989x_chip *sc, int *curr_ma) {
    int ret, reg_val;
    ret = sc8989x_field_read(sc, F_ICC, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read F_ICC failed(%d)\n", ret);
        return ret;
    }

    *curr_ma = reg2val(SC8989X_ICHG, reg_val);

    return ret;
}

static int sc8989x_set_term_curr(struct sc8989x_chip *sc, int curr_ma) {
    int reg_val = val2reg(SC8989X_ITERM, curr_ma);

    return sc8989x_field_write(sc, F_ITERM, reg_val);
}

static int sc8989x_get_term_curr(struct sc8989x_chip *sc, int *curr_ma) {
    int ret, reg_val;

    ret = sc8989x_field_read(sc, F_ITERM, &reg_val);
    if (ret)
        return ret;

    *curr_ma = reg2val(SC8989X_ITERM, reg_val);

    return ret;
}

__maybe_unused
static int sc8989x_set_safet_timer(struct sc8989x_chip *sc,
                                                bool enable) {
    int reg_val = enable ? 1 : 0;

    return sc8989x_field_write(sc, F_EN_TIMER, reg_val);
}

__maybe_unused
static int sc8989x_check_safet_timer(struct sc8989x_chip *sc,
                                                    bool * enabled) {
    int ret, reg_val;

    ret = sc8989x_field_read(sc, F_EN_TIMER, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read ICEN_TIMERC failed(%d)\n", ret);
        return ret;
    }

    *enabled = reg_val ? true : false;

    return ret;
}

static int sc8989x_set_vbat(struct sc8989x_chip *sc, int volt_mv) {
    int reg_val = val2reg(SC8989X_VBAT_REG, volt_mv);

    return sc8989x_field_write(sc, F_CV, reg_val);
}

static int sc8989x_get_vbat(struct sc8989x_chip *sc, int *volt_mv) {
    int ret, reg_val;

    ret = sc8989x_field_read(sc, F_CV, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read vbat reg failed(%d)\n", ret);
        return ret;
    }

    *volt_mv = reg2val(SC8989X_VBAT_REG, reg_val);

    return ret;
}

static int sc8989x_set_vindpm(struct sc8989x_chip *sc, int volt_mv) {
    int reg_val = val2reg(SC8989X_VINDPM, volt_mv);

    sc8989x_field_write(sc, F_FORCE_VINDPM, 1);

    return sc8989x_field_write(sc, F_VINDPM, reg_val);
}

__maybe_unused 
static int sc8989x_get_vindpm(struct sc8989x_chip *sc, int *volt_mv) {
    int ret, reg_val;

    ret = sc8989x_field_read(sc, F_VINDPM, &reg_val);
    if (ret)
        return ret;

    *volt_mv = reg2val(SC8989X_VINDPM, reg_val);

    return ret;
}

__maybe_unused 
static int sc8989x_set_hiz(struct sc8989x_chip *sc, bool enable) {
    int reg_val = enable ? 1 : 0;
    sc8989x_set_iindpm(sc, 500);
    sc8989x_set_ichg(sc, 500);
    return sc8989x_field_write(sc, F_EN_HIZ, reg_val);
}

static int sc8989x_force_dpdm(struct sc8989x_chip *sc) {
    int ret;
    int val;

    dev_err(sc->dev, "sc8989x_force_dpdm\n");
	sc8989x_field_write(sc, F_AUTO_DPDM_EN, 0);

    ret = regmap_read(sc->regmap, 0x02, &val);
    if (ret < 0) {
        return ret;
    }

    val |= 0x02;
    ret = regmap_write(sc->regmap, 0x02, val);

	ret = regmap_read(sc->regmap, 0x02, &val);
	dev_err(sc->dev, "%s reg0x02 = 0x%02x\n", __func__, val);

	sc8989x_field_write(sc, F_AUTO_DPDM_EN, 1);
	
    sc->power_good = 0;

    return ret;
}

static int sc8989x_get_charge_stat(struct sc8989x_chip *sc) {
    int ret;
    int chg_stat, vbus_stat;

    ret = sc8989x_field_read(sc, F_CHG_STAT, &chg_stat);
    if (ret) {
		return ret;
	}

    ret = sc8989x_field_read(sc, F_VBUS_STAT, &vbus_stat);
    if (ret) {
		return ret;
	}

    if (vbus_stat == VBUS_STAT_OTG) {
        return POWER_SUPPLY_STATUS_DISCHARGING;
    } else {
        switch (chg_stat) {
        case CHG_STAT_NOT_CHARGE:
			if (sc->vbus_good)
				return POWER_SUPPLY_STATUS_CHARGING;
			else
				return POWER_SUPPLY_STATUS_NOT_CHARGING;
        case CHG_STAT_PRE_CHARGE:
        case CHG_STAT_FAST_CHARGE:
            return POWER_SUPPLY_STATUS_CHARGING;
        case CHG_STAT_CHARGE_DONE:
            return POWER_SUPPLY_STATUS_FULL;
        }
    }

    return POWER_SUPPLY_STATUS_UNKNOWN;
}

__maybe_unused
static int sc8989x_check_charge_done(struct sc8989x_chip *sc,
                                                    bool * chg_done) {
    int ret, reg_val;

    ret = sc8989x_field_read(sc, F_CHG_STAT, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read charge stat failed(%d)\n", ret);
        return ret;
    }

    *chg_done = (reg_val == CHG_STAT_CHARGE_DONE) ? true : false;
    return ret;
}

static bool sc8989x_detect_device(struct sc8989x_chip *sc) {
    int ret;
    int val;

    ret = sc8989x_field_read(sc, F_PN, &val);
    if (ret < 0 || !(val == SC89890H_PN_NUM || val == SC89895_PN_NUM ||
        val == SC8950_PN_NUM || val == SC89890W_PN_NUM)) {
        dev_err(sc->dev, "not find sc8989x, part_no = %d\n", val);
        return false;
    }

    sc->dev_id = val;

    return true;
}

__maybe_unused
static int sc8989x_request_dpdm(struct sc8989x_chip *sc, bool enable)
{
	int ret = 0;

    if (mutex_trylock(&sc->request_dpdm_lock) == 0){
        dev_err(sc->dev, "%s: sc8989x_request_dpdm is working\n", __func__);
        return -EBUSY;
    }

	if (!sc->dpdm_reg) {
		sc->dpdm_reg = devm_regulator_get(sc->dev, "dpdm");
		if (IS_ERR(sc->dpdm_reg)) {
			ret = PTR_ERR(sc->dpdm_reg);
			dev_err(sc->dev, "Couldn't get dpdm regulator ret=%d\n", ret);
			sc->dpdm_reg = NULL;
			mutex_unlock(&sc->request_dpdm_lock);
			return ret;
		}
	}

	if (enable) {
		if (sc->dpdm_reg && !sc->dpdm_enabled) {
		dev_info(sc->dev, "enabling DPDM regulator\n");
		ret = regulator_enable(sc->dpdm_reg);
		if (ret < 0)
			dev_err(sc->dev, "Couldn't enable dpdm regulator ret=%d\n", ret);
		else
			sc->dpdm_enabled = true;
		}
	} else {
		if (sc->dpdm_reg && sc->dpdm_enabled) {
			dev_info(sc->dev, "disabling DPDM regulator\n");
			ret = regulator_disable(sc->dpdm_reg);
			if (ret < 0)
				pr_err("Couldn't disable dpdm regulator ret=%d\n", ret);
			else
				sc->dpdm_enabled = false;
		}
	}
	mutex_unlock(&sc->request_dpdm_lock);

	return ret;
}

static int sc8989x_dump_register(struct sc8989x_chip *sc) {
    int ret;
    int i;
    int val;

    for (i = 0; i <= 0x14; i++) {
        ret = regmap_read(sc->regmap, i, &val);
        if (ret < 0) {
            return ret;
        }
        dev_info(sc->dev, "%s reg[0x%02x] = 0x%02x\n", __func__, i, val);
    }

    return 0;
}

#ifdef CONFIG_MTK_CLASS
/********************MTK OPS***********************/
static int sc8989x_plug_in(struct charger_device *chg_dev) {
    int ret = 0;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s\n", __func__);

    /* Enable charging */
    // ret = sc8989x_set_chg_enable(sc, true);
    // if (ret) {
    //     dev_err(sc->dev, "Failed to enable charging:%d\n", ret);
    // }

    return ret;
}

static int sc8989x_plug_out(struct charger_device *chg_dev) {
    int ret = 0;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s\n", __func__);

    // ret = sc8989x_set_chg_enable(sc, false);
    // if (ret) {
    //     dev_err(sc->dev, "Failed to disable charging:%d\n", ret);
    // }
    return ret;
}

static int sc8989x_enable(struct charger_device *chg_dev, bool en) {
    int ret = 0;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    ret = sc8989x_set_chg_enable(sc, en);

    dev_info(sc->dev, "%s charger %s\n", en ? "enable" : "disable",
            !ret ? "successfully" : "failed");
    sc8989x_dump_register(sc);
    return ret;
}

static int sc8989x_is_enabled(struct charger_device *chg_dev, bool * enabled) {
    int ret;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    ret = sc8989x_check_chg_enabled(sc, enabled);
    dev_info(sc->dev, "charger is %s\n",
            *enabled ? "charging" : "not charging");

    return ret;
}

static int sc8989x_get_charging_current(struct charger_device *chg_dev,
                                        u32 * curr) {
    int ret = 0;
    int curr_ma;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    ret = sc8989x_get_ichg(sc, &curr_ma);
    if (!ret) {
        *curr = curr_ma * 1000;
    }

    return ret;
}

static int sc8989x_set_charging_current(struct charger_device *chg_dev,
                                        u32 curr) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s: charge curr = %duA\n", __func__, curr);

    return sc8989x_set_ichg(sc, curr / 1000);
}

static int sc8989x_set_eoc_current(struct charger_device *chg_dev,
                                        u32 curr) {
	struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

	dev_err(sc->dev, "set eoc curr = %d\n", curr);

	return sc8989x_set_term_curr(sc, curr / 1000);
}

static int sc8989x_get_input_current(struct charger_device *chg_dev, u32 * curr) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int curr_ma;
    int ret;

    dev_info(sc->dev, "%s\n", __func__);

    ret = sc8989x_get_iindpm(sc, &curr_ma);
    if (!ret) {
        *curr = curr_ma * 1000;
    }

    return ret;
}

static int sc8989x_set_input_current(struct charger_device *chg_dev, u32 curr) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s: iindpm curr = %duA\n", __func__, curr);

    return sc8989x_set_iindpm(sc, curr / 1000);
}

static int sc8989x_get_constant_voltage(struct charger_device *chg_dev,
                                        u32 * volt) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int volt_mv;
    int ret;

    dev_info(sc->dev, "%s\n", __func__);

    ret = sc8989x_get_vbat(sc, &volt_mv);
    if (!ret) {
        *volt = volt_mv * 1000;
    }

    return ret;
}

static int sc8989x_set_constant_voltage(struct charger_device *chg_dev,
                                        u32 volt) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s: charge volt = %duV\n", __func__, volt);

    return sc8989x_set_vbat(sc, volt / 1000);
}

static int sc8989x_kick_wdt(struct charger_device *chg_dev) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s\n", __func__);
    return sc8989x_reset_wdt(sc);
}

static int sc8989x_set_ivl(struct charger_device *chg_dev, u32 volt) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s: vindpm volt = %d\n", __func__, volt);

    return sc8989x_set_vindpm(sc, volt / 1000);
}

static int sc8989x_is_charging_done(struct charger_device *chg_dev, bool * done) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;

    ret = sc8989x_check_charge_done(sc, done);
	//sc8989x_dump_register(sc);
    dev_info(sc->dev, "%s: charge %s done\n", __func__, *done ? "is" : "not");
    return ret;
}

static int sc8989x_get_min_ichg(struct charger_device *chg_dev, u32 * curr) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    *curr = 60 * 1000;
    dev_err(sc->dev, "%s\n", __func__);
    return 0;
}

static int sc8989x_dump_registers(struct charger_device *chg_dev) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s\n", __func__);

    return sc8989x_dump_register(sc);
}

static int sc8989x_send_ta_current_pattern(struct charger_device *chg_dev,
                                        bool is_increase) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
	#if 1
    int ret;
    int i;

    dev_info(sc->dev, "%s: %s\n", __func__, is_increase ?
            "pumpx up" : "pumpx dn");
	//
	mutex_lock(&sc->pe_lock);
	sc8989x_set_ichg(sc, 1500);
    //pumpx start
    ret = sc8989x_set_iindpm(sc, 100);
    if (ret)
	{	
		dev_info(sc->dev, "%s:sc8989x_set_iindpm %d\n", __func__, ret);
        //return ret;
	}
    msleep(10);
    for (i = 0; i < 6; i++) {
        if (i < 3) {
            sc8989x_set_iindpm(sc, 800);
            is_increase ? mdelay(95) : mdelay(300);
            if (!(sc->vbus_good))
                goto vbus_absent;
        } else {
            sc8989x_set_iindpm(sc, 800);
            is_increase ? mdelay(300) : mdelay(95);
            if (!(sc->vbus_good))
                goto vbus_absent;
        }
        sc8989x_set_iindpm(sc, 100);
        mdelay(95);
        if (!(sc->vbus_good))
            goto vbus_absent;
    }
    //pumpx stop
    sc8989x_set_iindpm(sc, 800);
    mdelay(500);
    if (!(sc->vbus_good))
        goto vbus_absent;
    //pumpx wdt, max 240ms
    sc8989x_set_iindpm(sc, 100);
    mdelay(85);
    if (!(sc->vbus_good))
        goto vbus_absent;

vbus_absent:
	mutex_unlock(&sc->pe_lock);
	return sc8989x_set_iindpm(sc, 1500);
    #else
	//pumpx start
	int curr = 0,temp_curr = 1000;
    dev_info(sc->dev, "%s: %s\n", __func__, is_increase ?
            "pumpx up" : "pumpx dn");
	sc8989x_set_ichg(sc, 1000);
	//upm6910_get_icl(upm->chg_dev, &temp_curr);
	
    //mdelay(80);
	mutex_lock(&sc->pe_lock);
	curr = 600;
	pr_err("gezi----%s-----%d----start---curr = %d,is_increase=%d\n",__func__,__LINE__,curr,is_increase);
	sc8989x_set_iindpm(sc, 100);
	msleep(100);
	sc8989x_set_iindpm(sc, curr);
	msleep(100);
	sc8989x_set_iindpm(sc, 100);
	msleep(100);
	sc8989x_set_iindpm(sc, curr);
	msleep(100);
	sc8989x_set_iindpm(sc, 100);
	msleep(100);
	sc8989x_set_iindpm(sc, curr);
	msleep(300);
	sc8989x_set_iindpm(sc, 100);
	msleep(100);
	sc8989x_set_iindpm(sc, curr);
	msleep(300);
	sc8989x_set_iindpm(sc, 100);
	msleep(100);
	sc8989x_set_iindpm(sc, curr);
	msleep(300);
	sc8989x_set_iindpm(sc, 100);
	msleep(100);
    sc8989x_set_iindpm(sc, curr);
	msleep(500);
	sc8989x_set_iindpm(sc, 100);
	msleep(100);
    mutex_unlock(&sc->pe_lock);
    printk("upm6910_en_pe_current_partern end\n");
	sc8989x_set_iindpm(sc, temp_curr);
	msleep(300);
    return 0;
	#endif
}

static int sc8989x_send_ta20_current_pattern(struct charger_device *chg_dev,
                                            u32 uV) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    u8 val = 0;
    int i;

    if (uV < 5500000) {
        uV = 5500000;
    } else if (uV > 15000000) {
        uV = 15000000;
    }

    val = (uV - 5500000) / 500000;

    dev_info(sc->dev, "%s ta20 vol=%duV, val=%d\n", __func__, uV, val);

    sc8989x_set_iindpm(sc, 100);
    msleep(150);
    for (i = 4; i >= 0; i--) {
        sc8989x_set_iindpm(sc, 800);
        (val & (1 << i)) ? msleep(100) : msleep(50);
        sc8989x_set_iindpm(sc, 100);
        (val & (1 << i)) ? msleep(50) : msleep(100);
    }
    sc8989x_set_iindpm(sc, 800);
    msleep(150);
    sc8989x_set_iindpm(sc, 100);
    msleep(240);

    return sc8989x_set_iindpm(sc, 800);
}

static int sc8989x_set_ta20_reset(struct charger_device *chg_dev) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int curr;
    int ret;

    ret = sc8989x_get_iindpm(sc, &curr);

    ret = sc8989x_set_iindpm(sc, 100);
    msleep(300);
    return sc8989x_set_iindpm(sc, curr);
}

static int sc8989x_get_adc(struct charger_device *chg_dev,
                        enum adc_channel chan, int *min, int *max) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    enum sc8989x_adc_channel sc_chan;
    switch (chan) {
    case ADC_CHANNEL_VBAT:
        sc_chan = SC8989X_ADC_VBAT;
        break;
    case ADC_CHANNEL_VSYS:
        sc_chan = SC8989X_ADC_VSYS;
        break;
    case ADC_CHANNEL_VBUS:
        sc_chan = SC8989X_ADC_VBUS;
        break;
    case ADC_CHANNEL_IBUS:
        sc_chan = SC8989X_ADC_IBUS;
        break;
    default:
        return -95;
    }

    __sc8989x_get_adc(sc, sc_chan, min);
    *min = *min * 1000;
    *max = *min;
    return 0;
}

static int sc8989x_get_vbus(struct charger_device *chg_dev, u32 *vbus) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    return __sc8989x_get_adc(sc, SC8989X_ADC_VBUS, vbus);
}

static int sc8989x_get_ibat(struct charger_device *chg_dev, u32 *ibat) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    return __sc8989x_get_adc(sc, SC8989X_ADC_ICC, ibat);
}

static int sc8989x_get_ibus(struct charger_device *chg_dev, u32 *ibus) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    return __sc8989x_get_adc(sc, SC8989X_ADC_IBUS, ibus);
}

static int sc8989x_set_otg(struct charger_device *chg_dev, bool enable) {
    int ret;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    ret = sc8989x_set_otg_enable(sc, enable);
    ret |= sc8989x_set_chg_enable(sc, !enable);

    dev_info(sc->dev, "%s OTG %s\n", enable ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int sc8989x_set_safety_timer(struct charger_device *chg_dev, bool enable) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s  %s\n", __func__, enable ? "enable" : "disable");

    return sc8989x_set_safet_timer(sc, enable);
}

static int sc8989x_is_safety_timer_enabled(struct charger_device *chg_dev,
                                        bool * enabled) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    return sc8989x_check_safet_timer(sc, enabled);
}

static int sc8989x_set_boost_ilmt(struct charger_device *chg_dev, u32 curr) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s otg curr = %d\n", __func__, curr);
    return sc8989x_set_iboost(sc, curr / 1000);
}

static int sc8989x_do_event(struct charger_device *chg_dev, u32 event, u32 args) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    dev_info(sc->dev, "%s\n", __func__);

#ifdef CONFIG_MTK_CHARGER_V4P19
    switch (event) {
    case EVENT_EOC:
        charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
    break;
    case EVENT_RECHARGE:
        charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
    break;
    default:
    break;
    }
#else
    switch (event) {
    case EVENT_FULL:
    case EVENT_RECHARGE:
    case EVENT_DISCHARGE:
        power_supply_changed(sc->psy);
        break;
    default:
        break;
    }
#endif /*CONFIG_MTK_CHARGER_V4P19*/

    return 0;
}

static int sc8989x_enable_hz(struct charger_device *chg_dev, bool enable) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s %s\n", __func__, enable ? "enable" : "disable");

    return sc8989x_set_hiz(sc, enable);
}

static int sc8989x_enable_powerpath(struct charger_device *chg_dev, bool en) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    dev_info(sc->dev, "%s en = %d\n", __func__, en);

    return sc8989x_set_chg_enable(sc, en);
}

static int sc8989x_is_powerpath_enabled(struct charger_device *chg_dev,
                                        bool * en) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    return sc8989x_check_chg_enabled(sc, en);
}

static void sc8989x_chg_attach_pre_process(struct sc8989x_chip *sc,
					  enum sc8989x_attach_trigger trig,
					  int attach)
{
	bool bc12_dn;

	pr_err("trig=%s,attach=%d\n",
		   sc8989x_attach_trig_names[trig], attach);
	/* if attach trigger is not match, ignore it */
	if (sc->attach_trig != trig) {
		dev_info(sc->dev, "trig=%s ignored\n",
			   sc8989x_attach_trig_names[trig]);
		return;
	}

	mutex_lock(&sc->attach_lock);
	if (attach == ATTACH_TYPE_NONE)
		sc->bc12_dn = false;

	bc12_dn = sc->bc12_dn;
	if (!bc12_dn)
		atomic_set(&sc->attach, attach);
	mutex_unlock(&sc->attach_lock);

	if (attach > ATTACH_TYPE_PD && bc12_dn)
		return;

	if (!queue_work(sc->wq, &sc->bc12_work))
		dev_notice(sc->dev, "%s bc12 work already queued\n", __func__);
}

static void sc8989x_chg_pwr_rdy_process(struct sc8989x_chip *sc)
{
	int ret;
	u32 val;

	ret = sc8989x_field_read(sc, F_VBUS_STAT, &val);
	if (ret < 0 || sc->pwr_rdy == val)
		return;
	sc->pwr_rdy = val;
	dev_dbg(sc->dev, "pwr_rdy=%d\n", val);
	sc8989x_chg_attach_pre_process(sc, ATTACH_TRIG_PWR_RDY,
				val ? ATTACH_TYPE_PWR_RDY : ATTACH_TYPE_NONE);
}

static int sc8989x_chg_set_usbsw(struct sc8989x_chip *sc,
				enum sc8989x_usbsw usbsw)
{
	struct phy *phy;
	int ret, mode = (usbsw == USBSW_CHG) ? PHY_MODE_BC11_SET :
						   PHY_MODE_BC11_CLR;

	dev_err(sc->dev, "usbsw=%d\n", usbsw);
	phy = phy_get(sc->dev, "usb2-phy");
	if (IS_ERR_OR_NULL(phy)) {
		dev_err(sc->dev, "failed to get usb2-phy\n");
		return -ENODEV;
	}
	ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
	if (ret)
		dev_err(sc->dev, "failed to set phy ext mode\n");
	phy_put(sc->dev, phy);
	return ret;
}

static bool is_usb_rdy(struct device *dev)
{
	bool ready = true;
	struct device_node *node;

	node = of_parse_phandle(dev->of_node, "usb", 0);
	if (node) {
		ready = !of_property_read_bool(node, "cdp-block");
		dev_dbg(dev, "usb ready = %d\n", ready);
	} else
		dev_warn(dev, "usb node missing or invalid\n");
	return ready;
}

static bool sc8989x_chg_rdy(struct sc8989x_chip *sc)
{
	return sc->vbus_good;
}

static int sc8989x_chg_enable_bc12(struct sc8989x_chip *sc, bool en)
{
	int i, ret, attach;
	static const int max_wait_cnt = 250;

	dev_err(sc->dev, "en=%d\n", en);
	if (en) {
		/* CDP port specific process */
		dev_info(sc->dev, "check CDP block\n");
		for (i = 0; i < max_wait_cnt; i++) {
			if (is_usb_rdy(sc->dev))
				break;
			attach = atomic_read(&sc->attach);
			if (attach == ATTACH_TYPE_TYPEC)
				msleep(100);
			else {
				dev_notice(sc->dev, "%s: change attach:%d, disable bc12\n",
					   __func__, attach);
				en = false;
				break;
			}
		}
		if (i == max_wait_cnt)
			dev_notice(sc->dev, "CDP timeout\n", __func__);
		else
			dev_info(sc->dev, "CDP free\n", __func__);

		for (i = 0; i < 3; i++) {
			if (sc8989x_chg_rdy(sc))
				break;
			msleep(100);
			attach = atomic_read(&sc->attach);
			if (!attach)
				return 0;
		}
		if (i == 3)
			dev_notice(sc->dev, "rdy timeout\n", __func__);
		else
			dev_info(sc->dev, "rdy ok\n", __func__);
	}
	ret = sc8989x_chg_set_usbsw(sc, en ? USBSW_CHG : USBSW_USB);
	if (ret)
		return ret;

	if (en)
		return sc8989x_force_dpdm(sc);
	else
		return ret;
}

static void sc8989x_chg_bc12_work_func(struct work_struct *work)
{
	struct sc8989x_chip *sc = container_of(work,
							 struct sc8989x_chip,
							 bc12_work);
	bool bc12_ctrl = true, bc12_en = false, rpt_psy = true;
	int ret, attach;
	u32 val = 0;

	mutex_lock(&sc->attach_lock);
	attach = atomic_read(&sc->attach);
	dev_err(sc->dev, "sc8989x_chg_bc12_work_func attach=%d bc12_dn:%d\n", attach, sc->bc12_dn);

	if (attach > ATTACH_TYPE_NONE && sc->bootmode == 5) {
		/* skip bc12 to speed up ADVMETA_BOOT */
		dev_notice(sc->dev, "force SDP in meta mode\n");
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		goto out;
	}

	switch (attach) {
	case ATTACH_TYPE_NONE:
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		goto out;
	case ATTACH_TYPE_TYPEC:
		if (!sc->bc12_dn) {
			bc12_en = true;
			rpt_psy = false;
			dev_err(sc->dev, "sc->bc12_dn false\n");
			goto out;
		}
		ret = sc8989x_field_read(sc, F_VBUS_STAT, &val);
		if (ret < 0) {
			dev_err(sc->dev, "failed to get port stat\n");
			rpt_psy = false;
			goto out;
		}

		break;
	case ATTACH_TYPE_PD_SDP:
		val = VBUS_STAT_SDP;
		break;
	case ATTACH_TYPE_PD_DCP:
		/* not to enable bc12 */
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		goto out;
	case ATTACH_TYPE_PD_NONSTD:
		val = VBUS_STAT_NONSTAND;
		break;
	default:
		dev_info(sc->dev,
			 "%s: using tradtional bc12 flow!\n", __func__);
		break;
	}

	switch (val) {
	case VBUS_STAT_NO_INPUT:
		bc12_ctrl = false;
		rpt_psy = false;
		dev_info(sc->dev, "%s no info\n", __func__);
		goto out;
	case VBUS_STAT_DCP:
	case VBUS_STAT_HVDCP:
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		//bc12_en = true;
		break;
	case VBUS_STAT_SDP:
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case VBUS_STAT_CDP:
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;
	case VBUS_STAT_NONSTAND:
	case VBUS_STAT_UNKOWN:
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	default:
		bc12_ctrl = false;
		rpt_psy = false;
		dev_err(sc->dev, "Unknown port stat %d\n", val);
		goto out;
	}
	dev_err(sc->dev, "port stat = %s\n", sc8989x_port_stat_names[val]);
out:
	dev_err(sc->dev, "sc8989x_chg_bc12_work_func out\n");
	mutex_unlock(&sc->attach_lock);
	if (bc12_ctrl && (sc8989x_chg_enable_bc12(sc, bc12_en) < 0))
		dev_err(sc->dev, "failed to set bc12 = %d\n", bc12_en);
	if (rpt_psy)
		power_supply_changed(sc->psy);
}

static int sc8989x_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
	int attach = en ? ATTACH_TYPE_TYPEC : ATTACH_TYPE_NONE;

	dev_dbg(sc->dev, "en=%d\n", en);
	sc8989x_chg_attach_pre_process(sc, ATTACH_TRIG_TYPEC, attach);
	return 0;
}

static struct charger_ops sc8989x_chg_ops = {
    /* Normal charging */
    .plug_in = sc8989x_plug_in,
    .plug_out = sc8989x_plug_out,
    .enable = sc8989x_enable,
    .is_enabled = sc8989x_is_enabled,
    .set_eoc_current = sc8989x_set_eoc_current,
    .get_charging_current = sc8989x_get_charging_current,
    .set_charging_current = sc8989x_set_charging_current,
    .get_input_current = sc8989x_get_input_current,
    .set_input_current = sc8989x_set_input_current,
    .get_constant_voltage = sc8989x_get_constant_voltage,
    .set_constant_voltage = sc8989x_set_constant_voltage,
    .kick_wdt = sc8989x_kick_wdt,
    .set_mivr = sc8989x_set_ivl,
    .is_charging_done = sc8989x_is_charging_done,
    .get_min_charging_current = sc8989x_get_min_ichg,
    .dump_registers = sc8989x_dump_registers,

    /* Safety timer */
    .enable_safety_timer = sc8989x_set_safety_timer,
    .is_safety_timer_enabled = sc8989x_is_safety_timer_enabled,

    /* Power path */
    .enable_powerpath = sc8989x_enable_powerpath,
    .is_powerpath_enabled = sc8989x_is_powerpath_enabled,

    /* OTG */
    .enable_otg = sc8989x_set_otg,
    .set_boost_current_limit = sc8989x_set_boost_ilmt,
    .enable_discharge = NULL,

    /* PE+/PE+20 */
    .send_ta_current_pattern = sc8989x_send_ta_current_pattern,
    .set_pe20_efficiency_table = NULL,
    .send_ta20_current_pattern = sc8989x_send_ta20_current_pattern,
    .reset_ta = sc8989x_set_ta20_reset,
    .enable_cable_drop_comp = NULL,

    /* ADC */
    .get_adc = sc8989x_get_adc,
    .get_vbus_adc = sc8989x_get_vbus,
    .get_ibus_adc = sc8989x_get_ibus,
    .get_ibat_adc = sc8989x_get_ibat,

	.enable_hz = sc8989x_enable_hz,

    .event = sc8989x_do_event,

	/* charger type detection */
	.enable_chg_type_det = sc8989x_enable_chg_type_det,
};

static const struct charger_properties sc8989x_chg_props = {
    .alias_name = "sc8989x_chg",
};

static void sc8989x_inform_psy_dwork_handler(struct work_struct *work)
{
    int ret = 0;
    union power_supply_propval propval;
    struct sc8989x_chip *sc = container_of(work,
                                        struct sc8989x_chip,
                                        psy_dwork.work);
    if (!sc->chg_psy) {
        sc->chg_psy = power_supply_get_by_name("charger");
        if (!sc->chg_psy) {
            pr_err("%s get power supply fail\n", __func__);
            mod_delayed_work(system_wq, &sc->psy_dwork,
                    msecs_to_jiffies(2000));
            return ;
        }
    }

    if (sc->psy_usb_type != POWER_SUPPLY_USB_TYPE_UNKNOWN)
        propval.intval = 1;
    else
        propval.intval = 0;

    ret = power_supply_set_property(sc->chg_psy, POWER_SUPPLY_PROP_ONLINE,
                    &propval);

    if (ret < 0)
        pr_notice("inform power supply online failed:%d\n", ret);

    propval.intval = sc->psy_usb_type;

    ret = power_supply_set_property(sc->chg_psy,
                    POWER_SUPPLY_PROP_CHARGE_TYPE,
                    &propval);
    if (ret < 0)
        pr_notice("inform power supply charge type failed:%d\n", ret);

}
#endif /*CONFIG_MTK_CLASS */

/**********************interrupt*********************/
static void sc8989x_force_detection_dwork_handler(struct work_struct *work) {
    int ret;
    struct sc8989x_chip *sc = container_of(work,
                                        struct sc8989x_chip,
                                        force_detect_dwork.work);

    ret = sc8989x_force_dpdm(sc);
    if (ret) {
        dev_err(sc->dev, "%s: force dpdm failed(%d)\n", __func__, ret);
        return;
    }

    sc->force_detect_count++;
}

static void sc8989x_inserted_irq(struct sc8989x_chip *sc)
{
    dev_info(sc->dev, "%s: adapter/usb inserted\n", __func__);
    if (sc->pr_swap){
        dev_info(sc->dev, "%s: pr_swap\n", __func__);
        return;
    }
    sc8989x_set_vindpm_track(sc, SC8989X_TRACK_300);
    sc8989x_set_vindpm(sc, 4600);
    sc->force_detect_count = 0;
    #ifdef CONFIG_MTK_CHARGER_V5P10
    // sc8989x_set_usbsw_state(sc, SC8989X_USBSW_CHG);
    #endif /*CONFIG_MTK_CHARGER_V5P10*/
    //sc8989x_request_dpdm(sc, true);
    sc8989x_field_write(sc, F_CONV_RATE, true);
}

static void sc8989x_removed_irq(struct sc8989x_chip *sc)
{
    dev_info(sc->dev, "%s: adapter/usb removed\n", __func__);
    //sc8989x_request_dpdm(sc, false);
    //cancel_delayed_work_sync(&sc->force_detect_dwork);
    sc8989x_set_dpdm_hiz(sc);
    sc8989x_set_iindpm(sc, 500);
    sc8989x_set_ichg(sc, 500);
    sc8989x_field_write(sc, F_CONV_RATE, false);
}

static bool is_hiz_mode_trigger_interrupt(struct sc8989x_chip *sc)
{
	bool exist = false;
	int ret = 0, vbus = 0;
	int vbus_stat = 0;
	union power_supply_propval propval;
	struct power_supply *usb_psy;

	usb_psy = power_supply_get_by_name("battery");
	if (!usb_psy) {
		pr_err("%s Couldn't get usb psy\n", __func__);
		return -ENODEV;
	}


	ret = power_supply_get_property(usb_psy,
				POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT, &propval);
	if (!ret) {
		vbus = propval.intval;
	}

	ret = sc8989x_field_read(sc, F_VBUS_STAT, &vbus_stat);
    if (ret)
		return false;

	if (vbus > 4000 && vbus_stat != VBUS_STAT_OTG) {
		exist = true;
	}

	return exist;
}

static irqreturn_t sc8989x_irq_handler(int irq, void *data) {
    int ret;
    int reg_val;
	bool prev_pg;
    bool prev_vbus_gd;
	int attach;
	bool hiz_irq = false;
    struct sc8989x_chip *sc = (struct sc8989x_chip *)data;

    dev_info(sc->dev, "%s: sc8989x_irq_handler\n", __func__);

	attach = atomic_read(&sc->attach);
	sc->bc12_dn = (attach == ATTACH_TYPE_NONE) ? false : true;

    ret = sc8989x_field_read(sc, F_VBUS_GD, &reg_val);
    if (ret) {
        return IRQ_HANDLED;
    }
    prev_vbus_gd = sc->vbus_good;
    sc->vbus_good = !!reg_val;

	ret = sc8989x_field_read(sc, F_PG_STAT, &reg_val);
    if (ret) {
        return IRQ_HANDLED;
    }
	prev_pg = sc->power_good;
	sc->power_good = !!reg_val;

	pr_err("prev_vbus_gd=%d  vbus_good=%d \n", prev_vbus_gd, sc->vbus_good);

    if (!prev_vbus_gd && sc->vbus_good) {
        sc8989x_inserted_irq(sc);
    } else if (prev_vbus_gd && !sc->vbus_good) {
		hiz_irq = is_hiz_mode_trigger_interrupt(sc);
		if (hiz_irq) {
			pr_notice("hiz mode trigger interrupt\n");
			sc->vbus_good = true;
			sc->power_good = true;
			return IRQ_HANDLED;
		}

        sc8989x_removed_irq(sc);
    power_supply_changed(sc->psy);
    }

	if (!prev_pg && sc->power_good) {
		pr_err(" prev_pg changed\n");
		if (!queue_work(sc->wq, &sc->bc12_work))
			dev_notice(sc->dev, "%s bc12 work already queued\n", __func__);
        sc8989x_set_iindpm(sc, 500);
        //sc8989x_set_vindpm(sc, sc->vindpm_value);
    power_supply_changed(sc->psy);
	}


    sc8989x_dump_register(sc);
    return IRQ_HANDLED;
}

/**********************system*********************/
static int sc8989x_parse_dt(struct sc8989x_chip *sc) {
    struct device_node *np = sc->dev->of_node;
    int i;
    int ret = 0;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

    struct {
        char *name;
        int *conv_data;
    } props[] = {
        {"sc,sc8989x,ico-en", &(sc->cfg->ico_en)},
        {"sc,sc8989x,hvdcp-en", &(sc->cfg->hvdcp_en)},
        {"sc,sc8989x,auto-dpdm-en", &(sc->cfg->auto_dpdm_en)},
        {"sc,sc8989x,vsys-min", &(sc->cfg->vsys_min)},
        {"sc,sc8989x,vbatmin-sel", &(sc->cfg->vbatmin_sel)},
        {"sc,sc8989x,itrick", &(sc->cfg->itrick)},
        {"sc,sc8989x,iterm", &(sc->cfg->iterm)},
        {"sc,sc8989x,vbat-cv", &(sc->cfg->vbat_cv)},
        {"sc,sc8989x,vbat-low", &(sc->cfg->vbat_low)},
        {"sc,sc8989x,vrechg", &(sc->cfg->vrechg)},
        {"sc,sc8989x,en-term", &(sc->cfg->en_term)},
        {"sc,sc8989x,stat-dis", &(sc->cfg->stat_dis)},
        {"sc,sc8989x,wd-time", &(sc->cfg->wd_time)},
        {"sc,sc8989x,en-timer", &(sc->cfg->en_timer)},
        {"sc,sc8989x,charge-timer", &(sc->cfg->charge_timer)},
        {"sc,sc8989x,bat-comp", &(sc->cfg->bat_comp)},
        {"sc,sc8989x,vclamp", &(sc->cfg->vclamp)},
        {"sc,sc8989x,votg", &(sc->cfg->votg)},
        {"sc,sc8989x,iboost", &(sc->cfg->iboost)},
        {"sc,sc8989x,vindpm", &(sc->cfg->vindpm)},};

    sc->irq_gpio = of_get_named_gpio(np, "sc,intr-gpio", 0);
    if (sc->irq_gpio < 0)
        dev_err(sc->dev, "%s sc,intr-gpio is not available\n", __func__);

    if (of_property_read_string(np, "charger_name", &sc->cfg->chg_name) < 0)
        dev_err(sc->dev, "%s no charger name\n", __func__);

	ret = of_property_read_u32(np, "bc12_en", &sc->bc12_en);
	dev_info(sc->dev, "%s: bc12_en = %d\n", __func__, sc->bc12_en);

    /* initialize data for optional properties */
    for (i = 0; i < ARRAY_SIZE(props); i++) {
        ret = of_property_read_u32(np, props[i].name, props[i].conv_data);
        if (ret < 0) {
            dev_err(sc->dev, "%s not find\n", props[i].name);
            continue;
        }
    }

	boot_node = of_parse_phandle(sc->dev->of_node, "bootmode", 0);
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
			sc->bootmode = tag->bootmode;
			sc->boottype = tag->boottype;
		}
	}

	sc->attach_trig = ATTACH_TRIG_TYPEC;

    return 0;
}

static int sc8989x_init_device(struct sc8989x_chip *sc) {
    int ret = 0;
    int i;

    struct {
        enum sc8989x_fields field_id;
        int conv_data;
    } props[] = {
        {F_ICO_EN, sc->cfg->ico_en},
        {F_HVDCP_EN, sc->cfg->hvdcp_en},
        {F_AUTO_DPDM_EN, sc->cfg->auto_dpdm_en},
        {F_VSYS_MIN, sc->cfg->vsys_min},
        {F_VBATMIN_SEL, sc->cfg->vbatmin_sel},
        {F_ITC, sc->cfg->itrick},
        {F_ITERM, sc->cfg->iterm},
        {F_CV, sc->cfg->vbat_cv},
        {F_VBAT_LOW, sc->cfg->vbat_low},
        {F_VRECHG, sc->cfg->vrechg},
        {F_EN_ITERM, sc->cfg->en_term},
        {F_STAT_DIS, sc->cfg->stat_dis},
        {F_TWD, sc->cfg->wd_time},
        {F_EN_TIMER, sc->cfg->en_timer},
        {F_TCHG, sc->cfg->charge_timer},
        {F_BAT_COMP, sc->cfg->bat_comp},
        {F_VCLAMP, sc->cfg->vclamp},
        {F_V_OTG, sc->cfg->votg},
        {F_IBOOST_LIM, sc->cfg->iboost},
        {F_VINDPM, sc->cfg->vindpm},};

    //reg reset;
    sc8989x_field_write(sc, F_REG_RST, 1);

    for (i = 0; i < ARRAY_SIZE(props); i++) {
        dev_info(sc->dev, "%d--->%d\n", props[i].field_id, props[i].conv_data);
        ret = sc8989x_field_write(sc, props[i].field_id, props[i].conv_data);
    }
	sc8989x_field_write(sc, F_AUTO_DPDM_EN, 1);

    sc8989x_adc_ibus_en(sc, true);
    sc8989x_set_wa(sc);
    sc8989x_set_iindpm(sc, 500);
    sc8989x_set_ichg(sc, 500);
    sc8989x_set_iic_option(sc);

    return sc8989x_dump_register(sc);
}

static int sc8989x_register_interrupt(struct sc8989x_chip *sc) {
    int ret = 0;

    ret = devm_gpio_request(sc->dev, sc->irq_gpio, "chr-irq");
    if (ret < 0) {
        dev_err(sc->dev, "failed to request GPIO%d ; ret = %d", sc->irq_gpio, ret);
        return ret;
    }

    ret = gpio_direction_input(sc->irq_gpio);
    if (ret < 0) {
        dev_err(sc->dev, "failed to set GPIO%d ; ret = %d", sc->irq_gpio, ret);
        return ret;
    }

    sc->irq = gpio_to_irq(sc->irq_gpio);
    if (ret < 0) {
        dev_err(sc->dev, "failed gpio to irq GPIO%d ; ret = %d", sc->irq_gpio, ret);
        return ret;
    }

    ret = devm_request_threaded_irq(sc->dev, sc->irq, NULL,
                                    sc8989x_irq_handler,
                                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                                    "chr_irq", sc);
    if (ret < 0) {
        dev_err(sc->dev, "request thread irq failed:%d\n", ret);
        return ret;
    } else {
        dev_err(sc->dev, "request thread irq pass:%d  sc->irq =%d\n", ret, sc->irq);
    }

    enable_irq_wake(sc->irq);

    return 0;
}

static void determine_initial_status(struct sc8989x_chip *sc) {
    sc8989x_irq_handler(sc->irq, (void *)sc);
}

//reigster
static ssize_t sc8989x_show_registers(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    struct sc8989x_chip *sc = dev_get_drvdata(dev);
    uint8_t addr;
    unsigned int val;
    uint8_t tmpbuf[300];
    int len;
    int idx = 0;
    int ret;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8989x");
    for (addr = 0x0; addr <= 0x14; addr++) {
        ret = regmap_read(sc->regmap, addr, &val);
        if (ret == 0) {
            len = snprintf(tmpbuf, PAGE_SIZE - idx,
                        "Reg[%.2X] = 0x%.2x\n", addr, val);
            memcpy(&buf[idx], tmpbuf, len);
            idx += len;
        }
    }

    return idx;
}

static ssize_t sc8989x_store_register(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count) {
    struct sc8989x_chip *sc = dev_get_drvdata(dev);
    int ret;
    unsigned int val;
    unsigned int reg;
    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && reg <= 0x14)
        regmap_write(sc->regmap, (unsigned char)reg, (unsigned char)val);

    return count;
}

static DEVICE_ATTR(registers, 0660, sc8989x_show_registers,
                sc8989x_store_register);

static void sc8989x_create_device_node(struct device *dev) {
    device_create_file(dev, &dev_attr_registers);
}

static enum power_supply_property sc8989x_chg_psy_properties[] = {
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

static enum power_supply_usb_type sc8989x_chg_psy_usb_types[] = {
    POWER_SUPPLY_USB_TYPE_UNKNOWN,
    POWER_SUPPLY_USB_TYPE_SDP,
    POWER_SUPPLY_USB_TYPE_CDP,
    POWER_SUPPLY_USB_TYPE_DCP,
};

static int sc8989x_chg_property_is_writeable(struct power_supply *psy,
                                            enum power_supply_property psp) {
    int ret;

    switch (psp) {
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
    case POWER_SUPPLY_PROP_STATUS:
    case POWER_SUPPLY_PROP_ONLINE:
    case POWER_SUPPLY_PROP_ENERGY_EMPTY:
        ret = 1;
        break;

    default:
        ret = 0;
    }

    return ret;
}

static int sc8989x_chg_get_property(struct power_supply *psy,
                                    enum power_supply_property psp,
                                    union power_supply_propval *val) {
    struct sc8989x_chip *sc = power_supply_get_drvdata(psy);
    int ret = 0;
    int data = 0;
	int attach = 0;

    if (!sc) {
        dev_err(sc->dev, "%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return ret;
    }

    switch (psp) {
    case POWER_SUPPLY_PROP_MANUFACTURER:
        val->strval = "SouthChip";
        break;
    case POWER_SUPPLY_PROP_ONLINE:
		pr_err("POWER_SUPPLY_PROP_ONLINE ---- %d\n", sc->vbus_good);
		attach = atomic_read(&sc->attach);
		val->intval = !!attach;
		break;
		/* drv midified by liuhai 20240416 end */
    case POWER_SUPPLY_PROP_STATUS:
        ret = sc8989x_get_charge_stat(sc);
        if (ret < 0)
            break;
		pr_err("POWER_SUPPLY_PROP_STATUS ---- %d\n", ret);
        val->intval = ret;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        ret = sc8989x_get_ichg(sc, &data);
        if (ret)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = sc8989x_get_vbat(sc, &data);
        if (ret < 0)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        ret = sc8989x_get_iindpm(sc, &data);
        if (ret < 0)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
        ret = sc8989x_get_vindpm(sc, &data);
        if (ret < 0)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
        ret = sc8989x_get_term_curr(sc, &data);
        if (ret < 0)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_USB_TYPE:
		pr_err("POWER_SUPPLY_PROP_USB_TYPE ---- %d\n", sc->psy_usb_type);
		val->intval = sc->psy_usb_type;
		break;
    case POWER_SUPPLY_PROP_CURRENT_MAX:
        if (sc->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP)
            val->intval = 500000;
        else
            val->intval = 1500000;
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_MAX:
        if (sc->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP)
            val->intval = 5000000;
        else
            val->intval = 12000000;
        break;
    case POWER_SUPPLY_PROP_TYPE:
		pr_err("POWER_SUPPLY_PROP_TYPE ---- %d\n", sc->psy_desc.type);
		val->intval = sc->psy_desc.type;
		break;
		/* drv midified by liuhai 20240416 end */
    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}

static int sc8989x_chg_set_property(struct power_supply *psy,
                                    enum power_supply_property psp,
                                    const union power_supply_propval *val) {
    struct sc8989x_chip *sc = power_supply_get_drvdata(psy);
    int ret = 0;

    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
		sc8989x_chg_attach_pre_process(sc, ATTACH_TRIG_TYPEC,
					      val->intval);
        break;
    case POWER_SUPPLY_PROP_STATUS:
        //ret = sc8989x_set_chg_enable(sc, !!val->intval);
        //if (val->intval == POWER_SUPPLY_STATUS_DISCHARGING){
        //    sc8989x_set_otg_enable(sc, true);
        //    sc8989x_set_chg_enable(sc, false);
        //}
        ret = sc8989x_force_dpdm(sc);
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        ret = sc8989x_set_ichg(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = sc8989x_set_vbat(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        ret = sc8989x_set_iindpm(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
        ret = sc8989x_set_vindpm(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
        ret = sc8989x_set_term_curr(sc, val->intval / 1000);
        break;
    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}

static char *sc8989x_psy_supplied_to[] = {
    "battery",
    "mtk-master-charger",
};

static const struct power_supply_desc sc8989x_psy_desc = {
    .name = "primary_chg",
    .type = POWER_SUPPLY_TYPE_USB,
    .usb_types = sc8989x_chg_psy_usb_types,
    .num_usb_types = ARRAY_SIZE(sc8989x_chg_psy_usb_types),
    .properties = sc8989x_chg_psy_properties,
    .num_properties = ARRAY_SIZE(sc8989x_chg_psy_properties),
    .property_is_writeable = sc8989x_chg_property_is_writeable,
    .get_property = sc8989x_chg_get_property,
    .set_property = sc8989x_chg_set_property,
};

static int sc8989x_psy_register(struct sc8989x_chip *sc) {
    struct power_supply_config cfg = {
        .drv_data = sc,
        .of_node = sc->dev->of_node,
        .supplied_to = sc8989x_psy_supplied_to,
        .num_supplicants = ARRAY_SIZE(sc8989x_psy_supplied_to),
    };

    memcpy(&sc->psy_desc, &sc8989x_psy_desc, sizeof(sc->psy_desc));
    sc->psy = devm_power_supply_register(sc->dev, &sc->psy_desc, &cfg);
    return IS_ERR(sc->psy) ? PTR_ERR(sc->psy) : 0;
}

static const struct regulator_ops sc8989x_chg_otg_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
};

static const struct regulator_desc sc8989x_otg_rdesc = {
	.of_match = "usb-otg-vbus",
	.name = "usb-otg-vbus",
	.ops = &sc8989x_chg_otg_ops,
	.owner = THIS_MODULE,
	.type = REGULATOR_VOLTAGE,
	.min_uV = 3900000,
	.uV_step = 100000, /* step  100mV */
	.n_voltages = 16, /* 3900mV to 5400mV */
	.vsel_reg = 0x0a,
	.vsel_mask = 0xf0,
	.enable_reg = 0x03,
	.enable_mask = 0x30,
	.disable_val = 0x10,
	.enable_val = 0x20,
};

static struct of_device_id sc8989x_of_device_id[] = {
    //{.compatible = "southchip,sc89890h",},
    //{.compatible = "southchip,sc89890w",},
    //{.compatible = "southchip,sc89895",},
    //{.compatible = "southchip,sc8950",},
	{.compatible = "southchip,sc8989x",},
    {},
};

MODULE_DEVICE_TABLE(of, sc8989x_of_device_id);

static int sc8989x_charger_probe(struct i2c_client *client,
                                const struct i2c_device_id *id) {
    struct sc8989x_chip *sc;
    int ret = 0;
    int i;
	struct regulator_config config = { };

    dev_err(&client->dev, "%s (%s)\n", __func__, SC8989X_DRV_VERSION);

    sc = devm_kzalloc(&client->dev, sizeof(struct sc8989x_chip), GFP_KERNEL);
    if (!sc)
        return -ENOMEM;

    sc->dev = &client->dev;
    sc->client = client;
   // sc->regmap = devm_regmap_init_i2c(client, &sc8989x_regmap_config);
    sc->regmap = devm_regmap_init(sc->dev, &sc8989x_regmap_bus, sc,
                                    &sc8989x_regmap_config);
    if (IS_ERR(sc->regmap)) {
        dev_err(sc->dev, "Failed to initialize regmap\n");
        return -EINVAL;
    }

    for (i = 0; i < ARRAY_SIZE(sc8989x_reg_fields); i++) {
        sc->rmap_fields[i] =
            devm_regmap_field_alloc(sc->dev, sc->regmap, sc8989x_reg_fields[i]);
        if (IS_ERR(sc->rmap_fields[i])) {
            dev_err(sc->dev, "cannot allocate regmap field\n");
            return PTR_ERR(sc->rmap_fields[i]);
        }
    }

    i2c_set_clientdata(client, sc);
    if (!sc8989x_detect_device(sc)) {
        ret = -ENODEV;
        goto err_nodev;
    }

    sc8989x_create_device_node(&(client->dev));

    INIT_DELAYED_WORK(&sc->force_detect_dwork,
                        sc8989x_force_detection_dwork_handler);

    sc->cfg = &sc8989x_default_cfg;
    ret = sc8989x_parse_dt(sc);
    if (ret < 0) {
        dev_err(sc->dev, "parse dt fail(%d)\n", ret);
        goto err_parse_dt;
    }
    mutex_init(&sc->request_dpdm_lock);
    ret = sc8989x_init_device(sc);
    if (ret < 0) {
        dev_err(sc->dev, "init device fail(%d)\n", ret);
        goto err_init_dev;
    }

    ret = sc8989x_psy_register(sc);
    if (ret) {
        dev_err(sc->dev, "%s psy register fail(%d)\n", __func__, ret);
        goto err_psy;
    }
    ret = sc8989x_register_interrupt(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s register irq fail(%d)\n", __func__, ret);
        goto err_register_irq;
    }
	init_completion(&sc->pumpx_done);
	mutex_init(&sc->pe_lock);
	mutex_init(&sc->attach_lock);
	atomic_set(&sc->attach, 0);
	sc->wq = create_singlethread_workqueue(dev_name(sc->dev));
	if (!sc->wq) {
		pr_err( "failed to create workqueue\n");
		ret = -ENOMEM;
		goto err_wq;
	}
	INIT_WORK(&sc->bc12_work, sc8989x_chg_bc12_work_func);

#ifdef CONFIG_MTK_CLASS
    INIT_DELAYED_WORK(&sc->psy_dwork,
                        sc8989x_inform_psy_dwork_handler);

    /* Register charger device */
    sc->chg_dev = charger_device_register(sc->cfg->chg_name,
                                        sc->dev, sc, &sc8989x_chg_ops,
                                        &sc8989x_chg_props);
    if (IS_ERR_OR_NULL(sc->chg_dev)) {
        ret = PTR_ERR(sc->chg_dev);
        dev_notice(sc->dev, "%s register chg dev fail(%d)\n", __func__, ret);
        goto err_register_chg_dev;
    }
#endif                          /*CONFIG_MTK_CLASS */
    device_init_wakeup(sc->dev, 1);

	/* otg regulator */
	config.dev = sc->dev;
	config.regmap = sc->regmap;
	sc->otg_rdev = devm_regulator_register(sc->dev, &sc8989x_otg_rdesc,
						&config);
	if (IS_ERR(sc->otg_rdev))
		return PTR_ERR(sc->otg_rdev);

    determine_initial_status(sc);
    sc8989x_dump_register(sc);

	sc8989x_chg_pwr_rdy_process(sc);

    dev_info(sc->dev, "sc8989x probe successfully\n!");

    return 0;

err_register_irq:
#ifdef CONFIG_MTK_CLASS
err_register_chg_dev:
#endif                          /*CONFIG_MTK_CLASS */
err_psy:
err_init_dev:
err_parse_dt:
err_nodev:
err_wq:
    dev_err(sc->dev, "sc8989x probe failed!\n");
    devm_kfree(&client->dev, sc);
    return -ENODEV;
}

static int sc8989x_charger_remove(struct i2c_client *client) {
    struct sc8989x_chip *sc = i2c_get_clientdata(client);

    if (sc) {
        dev_info(sc->dev, "%s\n", __func__);
        sc8989x_field_write(sc, F_REG_RST, 1);
#ifdef CONFIG_MTK_CLASS
        charger_device_unregister(sc->chg_dev);
#endif                          /*CONFIG_MTK_CLASS */
        power_supply_put(sc->psy);
    }
    return 0;
}

static void sc8989x_charger_shutdown(struct i2c_client *client) {
    struct sc8989x_chip *sc = i2c_get_clientdata(client);

    if (sc) {
        sc8989x_field_write(sc, F_REG_RST, 1);
    }
}

#if 0
static int sc8989x_suspend(struct device *dev) {
    struct sc8989x_chip *sc = dev_get_drvdata(dev);

    dev_info(dev, "%s\n", __func__);
    if (device_may_wakeup(dev))
        enable_irq_wake(sc->irq);
    disable_irq(sc->irq);

    return 0;
}

static int sc8989x_resume(struct device *dev) {
    struct sc8989x_chip *sc = dev_get_drvdata(dev);

    dev_info(dev, "%s\n", __func__);
    enable_irq(sc->irq);
    if (device_may_wakeup(dev))
        disable_irq_wake(sc->irq);

    return 0;
}

static const struct dev_pm_ops sc8989x_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(sc8989x_suspend, sc8989x_resume)
};
#endif                          /* CONFIG_PM_SLEEP */

static struct i2c_driver sc8989x_charger_driver = {
    .driver = {
            .name = "sc8989x",
            .owner = THIS_MODULE,
            .of_match_table = of_match_ptr(sc8989x_of_device_id),
#if 0
            .pm = &sc8989x_pm_ops,
#endif
            },
    .probe = sc8989x_charger_probe,
    .remove = sc8989x_charger_remove,
    .shutdown = sc8989x_charger_shutdown,
};

module_i2c_driver(sc8989x_charger_driver);

MODULE_DESCRIPTION("SC SC8989X Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip <Aiden-yu@southchip.com>");
