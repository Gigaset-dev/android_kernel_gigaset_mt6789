/*
 * Fuelgauge battery driver
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define pr_fmt(fmt)	"[sh366003] %s: " fmt, __func__
#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/string.h>
//#include <mt-plat/v1/mtk_battery.h>
#include "mtk_charger.h"

#define		FG_UPDATER_AFI

#ifdef FG_UPDATER_AFI
#include "sh366003_fg.h"
struct sh_decoder
{
	u8 addr;
	u8 reg;
	u8 length;
	u8 buf_first_val;
};
#endif

#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../misc/mediatek/prize/hardware_info/hardware_info.h"
extern struct hardware_info current_coulo_info;
#endif



int g_sh366003_uisoc = 0;
EXPORT_SYMBOL(g_sh366003_uisoc);
int g_sh366003_tbat = 0;
EXPORT_SYMBOL(g_sh366003_tbat);
/*drv add by zhaopengge 20230823---start*/
bool sh366003_init_done = false;
EXPORT_SYMBOL(sh366003_init_done);
/*drv add by zhaopengge 20230823---end*/


static u32 fw_version = 0;
static u16 afi_ver = 0;
//s32 version_ret = CHECK_VERSION_ERR;
//static int test_temp = 5;
static u8 state_of_health = 100;

#define sh_info	pr_info
#define sh_dbg	pr_debug
#define sh_err	pr_err
#define sh_log	pr_err

#define queue_delayed_work_time  4000

#define	INVALID_REG_ADDR	0xFF
#define SHFS_UPDATE_KEY		0xF1F2

#define SH_FG_I2C_DEV_ADDR    (0xAA >> 1)

#define FG_FLAGS_FD				BIT(4)
#define	FG_FLAGS_FC				BIT(5)
#define	FG_FLAGS_DSG				BIT(6)
#define FG_FLAGS_RCA				BIT(9)

enum {
	UPDATE_REASON_FG_RESET = 1,
	UPDATE_REASON_NEW_VERSION,
	UPDATE_REASON_FORCED,
};


enum sh_fg_reg_idx {
	SH_FG_REG_CTRL = 0,
	SH_FG_REG_TEMP,		/* Battery Temperature */
	SH_FG_REG_VOLT,		/* Battery Voltage */
	SH_FG_REG_CURR,		/* Battery Current */
	SH_FG_REG_AI,		/* Average Current */
	SH_FG_REG_BATT_STATUS,	/* BatteryStatus */
	SH_FG_REG_TTE,		/* Time to Empty */
	SH_FG_REG_TTF,		/* Time to Full */
	SH_FG_REG_FCC,		/* Full Charge Capacity */
	SH_FG_REG_RM,		/* Remaining Capacity */
	SH_FG_REG_CC,		/* Cycle Count */
	SH_FG_REG_SOC,		/* Relative State of Charge */
	SH_FG_REG_SOH,		/* State of Health */
	SH_FG_REG_DC,		/* Design Capacity */
	SH_FG_REG_ALT_MAC,	/* AltManufactureAccess*/
	SH_FG_REG_MAC_CHKSUM,	/* MACChecksum */
	NUM_REGS,
};

enum sh_fg_mac_cmd {
	FG_MAC_CMD_CTRL_STATUS	= 0x0000,
	FG_MAC_CMD_DEV_TYPE	= 0x0001,
	FG_MAC_CMD_FW_VER	= 0x0002,
	FG_MAC_CMD_HW_VER	= 0x0003,
	FG_MAC_CMD_IF_SIG	= 0x0004,
	FG_MAC_CMD_CHEM_ID	= 0x0006,
	FG_MAC_CMD_GAUGING	= 0x0021,
	FG_MAC_CMD_SEAL		= 0x0030,
	FG_MAC_CMD_DEV_RESET	= 0x0041,
	FG_MAC_CMD_GAUGEINFO_BLOCK1	= 0x0060,
	FG_MAC_CMD_GAUGEINFO_BLOCK2	= 0x0066,
	FG_MAC_CMD_GAUGEINFO_BLOCK3	= 0x0067,
	FG_MAC_CMD_GAUGESTATUS = 0x0056,
	FG_MAC_CMD_DASTATUS1 = 0x0071,
};


enum {
	SEAL_STATE_RSVED,
	SEAL_STATE_UNSEALED,
	SEAL_STATE_SEALED,
	SEAL_STATE_FA,
};


enum sh_fg_device {
	SH366003,
};

static const unsigned char *device2str[] = {
	"sh366003",
};

static u8 sh366003_regs[NUM_REGS] = {
	0x00,	/* CONTROL */
	0x06,	/* TEMP */
	0x08,	/* VOLT */
	0x0C,	/* CURR */
	0x14,	/* AVG CURRENT */
	0x0A,	/* FLAGS */
	0x16,	/* Time to empty */
	0x18,	/* Time to full */
	0x12,	/* Full charge capacity */
	0x10,	/* Remaining Capacity */
	0x2A,	/* CycleCount */
	0x2C,	/* State of Charge */
	0x2E,	/* State of Health */
	0x3C,	/* Design Capacity */
	0x3E,	/* AltManufacturerAccess*/
	0x60,	/* MACChecksum */
};

struct sh_fg_chip {
	struct device *dev;
	struct i2c_client *client;
	
	struct workqueue_struct *cwfg_workqueue;
	struct delayed_work battery_delay_work;

	struct mutex i2c_rw_lock;
	struct mutex data_lock;
	struct mutex update_lock;
	struct mutex irq_complete;

	bool irq_waiting;
	bool irq_disabled;
	bool resume_completed;

	int fw_ver;
	int df_ver;

	u8 chip;
	u8 regs[NUM_REGS];

	/* status tracking */

	bool batt_fc;
	bool batt_fd;

	bool batt_dsg;
	bool batt_rca;	/* remaining capacity alarm */

	int seal_state;
	int batt_tte;
	int batt_soc_last;
	int batt_soc;
	int batt_fcc;	/* Full charge capacity */
	int batt_rm;	/* Remaining capacity */
	int batt_dc;	/* Design Capacity */
	int batt_volt;
	int batt_temp;
	int batt_curr;
	int av_batt_curr;
	int batt_soh;

	int batt_cyclecnt;	/* cycle count */
	int force_update;	
	int batt_id;

	/* debug */
	int skip_reads;
	int skip_writes;

	int fake_soc;
	int fake_temp;

	struct power_supply *fg_psy;
	struct power_supply_desc fg_psy_d;
#ifdef FG_UPDATER_AFI
	bool need_update;
	bool updating;
	struct delayed_work afi_update_work;
	struct wakeup_source *wake_lock;
	spinlock_t slock;
	struct power_supply *chg_psy;
#endif
};

static void fg_dump_registers(struct sh_fg_chip *sh);
#ifdef FG_UPDATER_AFI
static s32 fg_gauge_unseal(struct sh_fg_chip *sm);
static s32 fg_gauge_seal(struct sh_fg_chip *sm);
static void sh366003_afi_update(struct sh_fg_chip *sh);
#endif

static int __fg_read_word(struct i2c_client *client, u8 reg, u16 *val)
{
	s32 ret;

	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0) {
		pr_err("i2c read word fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*val = (u16)ret;

	return 0;
}

#ifdef FG_UPDATER_AFI
static int __fg_write_word(struct i2c_client *client, u8 reg, u16 val)
{
	s32 ret;

	ret = i2c_smbus_write_word_data(client, reg, val);
	if (ret < 0) {
		pr_err("i2c write word fail: can't write 0x%02X to reg 0x%02X\n",
				val, reg);
		return ret;
	}

	return 0;
}
#endif
static int __fg_read_block(struct i2c_client *client, u8 reg, u8 *buf, u8 len)
{

	int ret = 0;
	
	//pr_err("%s,%d\n",__func__,__LINE__);

	ret = i2c_smbus_read_i2c_block_data(client, reg, len, buf);

	return ret;
}

static int __fg_write_block(struct i2c_client *client, u8 reg, u8 *buf, u8 len)
{
	int ret;

	ret = i2c_smbus_write_i2c_block_data(client, reg, len, buf);

	return ret;
}

static int fg_read_word(struct sh_fg_chip *sh, u8 reg, u16 *val)
{
	int ret;

	if (sh->skip_reads) {
		*val = 0;
		return 0;
	}

	mutex_lock(&sh->i2c_rw_lock);
	ret = __fg_read_word(sh->client, reg, val);
	mutex_unlock(&sh->i2c_rw_lock);

	return ret;
}

static int fg_read_block(struct sh_fg_chip *sh, u8 reg, u8 *buf, u8 len)
{
	int ret;

	if (sh->skip_reads)
		return 0;
	mutex_lock(&sh->i2c_rw_lock);
	ret = __fg_read_block(sh->client, reg, buf, len);
	mutex_unlock(&sh->i2c_rw_lock);

	return ret;

}

static int fg_write_block(struct sh_fg_chip *sh, u8 reg, u8 *data, u8 len)
{
	int ret;

	if (sh->skip_writes)
		return 0;

	mutex_lock(&sh->i2c_rw_lock);
	ret = __fg_write_block(sh->client, reg, data, len);
	mutex_unlock(&sh->i2c_rw_lock);

	return ret;
}

static u8 checksum(u8 *data, u8 len)
{
	u8 i;
	u16 sum = 0;
	
	//pr_err("%s,%d,len:%d\n",__func__,__LINE__,len);

	for (i = 0; i < len; i++){
		sum += data[i];
	}
	
	//pr_err("%s,%d\n",__func__,__LINE__);
	
	sum &= 0xFF;

	return 0xFF - sum;
}

#if 0
static void fg_print_buf(const char *msg, u8 *buf, u8 len)
{
	int i;
	int idx = 0;
	int num;
	u8 strbuf[128];

	pr_err("%s buf: ", msg);
	for (i = 0; i < len; i++) {
		num = sprintf(&strbuf[idx], "%02X ", buf[i]);
		idx += num;
	}
	pr_err("%s\n", strbuf);
}
#else
static void fg_print_buf(const char *msg, u8 *buf, u8 len)
{}
#endif

#if 0
#define TIMEOUT_INIT_COMPLETED	100
static int fg_check_init_completed(struct sh_fg_chip *sh)
{
	int ret;
	int i = 0;
	u16 status;

	while (i++ < TIMEOUT_INIT_COMPLETED) {
		ret = fg_read_word(sh, ah->regs[SH_FG_REG_BATT_STATUS],
				&status);
		if (ret >= 0 && (status & 0x0080))
			return 0;
		msleep(100);
	}
	pr_err("wait for FG INITCOMP timeout\n");
	return ret;
}
#endif


static int fg_mac_read_block(struct sh_fg_chip *sh, u16 cmd, u8 *buf, u8 len)
{
	int ret;
	u8 cksum_calc, cksum;
	u8 t_buf[400] = {0};
	u8 t_len;
	int i;
	
	t_buf[0] = (u8)cmd;
	t_buf[1] = (u8)(cmd>> 8);
	
	ret = fg_write_block(sh, sh->regs[SH_FG_REG_ALT_MAC], t_buf, 2);
	if (ret < 0)
		return ret;
	
	msleep(100);

	ret = fg_read_block(sh, sh->regs[SH_FG_REG_ALT_MAC], buf, len);
	if (ret < 0)
		return ret;
	
	//pr_err("%s,%d,len:%d\n",__func__,__LINE__,len);

	fg_print_buf("mac_read_block", t_buf, len);

	cksum = t_buf[34];
	t_len = t_buf[35];
	
	//pr_err("%s,%d,t_len:%d\n",__func__,__LINE__,t_len);

	cksum_calc = checksum(t_buf, t_len - 2);
	if (cksum_calc != cksum)
		return 1;
	
	for (i = 0; i < len; i++)
		buf[i] = t_buf[i+2];
	
	return 0;
}


static int fg_read_fw_version(struct sh_fg_chip *sh)
{

	int ret;
	u8 buf[360] = {0};
	
	pr_err("%s,%d\n",__func__,__LINE__);

	ret = fg_mac_read_block(sh, FG_MAC_CMD_FW_VER , buf, 4);
	if (ret < 0) {
		pr_err("Failed to read firmware version:%d\n", ret);
		return -1;
	}
	fw_version = buf[3] << 8 | buf[2];

	sh_log("FW Ver:%04X\n", fw_version);
	return 0;
}


static int fg_read_status(struct sh_fg_chip *sh)
{
	int ret;
	u16 flags;

	ret = fg_read_word(sh, sh->regs[SH_FG_REG_BATT_STATUS], &flags);
	if (ret < 0)
		return ret;

	mutex_lock(&sh->data_lock);
	sh->batt_fc		= !!(flags & FG_FLAGS_FC);
	sh->batt_fd		= !!(flags & FG_FLAGS_FD);
	sh->batt_rca		= !!(flags & FG_FLAGS_RCA);
	sh->batt_dsg		= !!(flags & FG_FLAGS_DSG);
	mutex_unlock(&sh->data_lock);

	return 0;
}

#define UI_SOC	96

static int fg_rmc_fcc_convert(struct sh_fg_chip *sh,int real_soc)
{
	u32 temp = 0;
	u32 rmc = sh->batt_rm;
	u32 fcc = sh->batt_fcc;
	
	pr_err("[%s],rmc:%d,fcc:%d,real_soc:%d\n",__func__,rmc,fcc,real_soc);
	
	if(fcc <= 0){
		return real_soc;
	}
	
	if(rmc == 0 && real_soc == 0){
		return 0;
	}

	temp = rmc * 10000 / fcc;
	
	pr_err("[%s],start---temp:%d\n",__func__,temp);
	
	if(temp < 100){
		temp = 1;
	}
	else{
		temp = temp * 100 / UI_SOC / 100;
	}
	pr_err("[%s],end---temp:%d\n",__func__,temp);
	return temp;
}


static int fg_rsoc_convert(struct sh_fg_chip *sh,int soc_real)
{
	int ui_soc = 0;
	if(soc_real < 0 || sh->batt_soc_last < 0){
		pr_err("[%s],soc  err!!soc_real:%d,soc_last:%d\n",__func__,soc_real,sh->batt_soc_last);
		return soc_real;
	}

	//fg_read_status(sh);
	//ui_soc = soc_real * 100 / UI_SOC;
	ui_soc = fg_rmc_fcc_convert(sh,soc_real);
	if(ui_soc > 100){
		ui_soc = 100;
	}
	pr_err("[enter:%s]soc_real:%d,ui_soc:%d,soc_last:%d,dsg:%d\n",__func__, \
			soc_real,ui_soc,sh->batt_soc_last,sh->batt_dsg);
	if(sh->batt_dsg){
		if(ui_soc > sh->batt_soc_last){
			ui_soc = sh->batt_soc_last;
		}
		else{
			/*if(soc_real == 96 && ui_soc == 100 && sh->batt_soc_last == 100){
				pr_err("[out:%s][%d]ui_soc force 99\n",__func__,__LINE__);
				return 99;
			}*/
			if(sh->batt_soc_last - ui_soc > 1){
				ui_soc = sh->batt_soc_last - 1;
			}
		}
	}
	else{/*充电状态*/
		if(sh->batt_soc_last > ui_soc){
			sh->batt_soc_last = ui_soc;
		}
		else{
			/*if(soc_real == 96 && ui_soc == 100 && sh->batt_soc_last == 98){
				pr_err("[out:%s][%d]ui_soc force 99\n",__func__,__LINE__);
				return 99;
			}*/
			if(ui_soc - sh->batt_soc_last > 1){
				ui_soc = sh->batt_soc_last + 1;
			}
		}
	}
	
	if(ui_soc > 100){
		ui_soc = 100;
	}
	
	sh->batt_soc_last = ui_soc;
	pr_err("[out:%s]soc_real:%d,ui_soc:%d,soc_last:%d,dsg:%d\n",__func__, \
			soc_real,ui_soc,sh->batt_soc_last,sh->batt_dsg);
	return ui_soc;
}


static int fg_read_rsoc(struct sh_fg_chip *sh)
{
	int ret;
	u16 soc = 0;

	ret = fg_read_word(sh, sh->regs[SH_FG_REG_SOC], &soc);
	if (ret < 0) {
		pr_err("could not read RSOC, ret = %d\n", ret);
		return ret;
	}
	//g_sh366003_uisoc = fg_rsoc_convert(sh,soc);

	return soc;

}

static int fg_read_temperature(struct sh_fg_chip *sh)
{
	int ret;
	u16 temp = 0;

	ret = fg_read_word(sh, sh->regs[SH_FG_REG_TEMP], &temp);
	if (ret < 0) {
		pr_err("could not read temperature, ret = %d\n", ret);
		return ret;
	}
	g_sh366003_tbat = (temp - 2730) / 10;

	return temp - 2730;

}

static int fg_read_volt(struct sh_fg_chip *sh)
{
	int ret;
	u16 volt = 0;

	ret = fg_read_word(sh, sh->regs[SH_FG_REG_VOLT], &volt);
	if (ret < 0) {
		pr_err("could not read voltage, ret = %d\n", ret);
		return ret;
	}

	return volt;

}

static int fg_read_av_current(struct sh_fg_chip *sh, int *curr)
{
	int ret;
	u16 avg_curr = 0;

	ret = fg_read_word(sh, sh->regs[SH_FG_REG_AI], &avg_curr);
	if (ret < 0) {
		pr_err("could not read current, ret = %d\n", ret);
		return ret;
	}
	*curr = (int)((s16)avg_curr);

	return ret;
}

static int fg_read_current(struct sh_fg_chip *sh, int *curr)
{
	int ret;
	u16 avg_curr = 0;

	ret = fg_read_word(sh, sh->regs[SH_FG_REG_CURR], &avg_curr);
	if (ret < 0) {
		pr_err("could not read current, ret = %d\n", ret);
		return ret;
	}
	*curr = (int)((s16)avg_curr);

	return ret;
}



static int fg_read_fcc(struct sh_fg_chip *sh)
{
	int ret;
	u16 fcc;

	if (sh->regs[SH_FG_REG_FCC] == INVALID_REG_ADDR) {
		pr_err("FCC command not supported!\n");
		return 0;
	}

	ret = fg_read_word(sh, sh->regs[SH_FG_REG_FCC], &fcc);

	if (ret < 0)
		pr_err("could not read FCC, ret=%d\n", ret);

	return fcc;
}

static int fg_read_dc(struct sh_fg_chip *sh)
{

	int ret;
	u16 dc;

	if (sh->regs[SH_FG_REG_DC] == INVALID_REG_ADDR) {
		pr_err("DesignCapacity command not supported!\n");
		return 0;
	}

	ret = fg_read_word(sh, sh->regs[SH_FG_REG_DC], &dc);

	if (ret < 0) {
		pr_err("could not read DC, ret=%d\n", ret);
		return ret;
	}

	return dc;
}


static int fg_read_rm(struct sh_fg_chip *sh)
{
	int ret;
	u16 rm;

	if (sh->regs[SH_FG_REG_RM] == INVALID_REG_ADDR) {
		pr_err("RemainingCapacity command not supported!\n");
		return 0;
	}

	ret = fg_read_word(sh, sh->regs[SH_FG_REG_RM], &rm);

	if (ret < 0) {
		pr_err("could not read rm, ret=%d\n", ret);
		return ret;
	}
	//g_sh366003_uisoc = rm;

	return rm;

}

static int fg_read_soh(struct sh_fg_chip *sh)
{
	int ret;
	u16 soh;

	if(sh->regs[SH_FG_REG_SOH] == INVALID_REG_ADDR){
		pr_err("State of Health command not supported!\n");
		return 0;
	}

	ret = fg_read_word(sh, sh->regs[SH_FG_REG_SOH], &soh);

	if (ret < 0) {
		pr_err("could not read soh, ret=%d\n", ret);
		return ret;
	}

	return soh;
}

static int fg_read_cyclecount(struct sh_fg_chip *sh)
{
	int ret;
	u16 cc;

	if (sh->regs[SH_FG_REG_CC] == INVALID_REG_ADDR) {
		pr_err("Cycle Count not supported!\n");
		return -1;
	}

	ret = fg_read_word(sh, sh->regs[SH_FG_REG_CC], &cc);

	if (ret < 0) {
		pr_err("could not read Cycle Count, ret=%d\n", ret);
		return ret;
	}

	return cc;
}

static int fg_read_tte(struct sh_fg_chip *sh)
{
	int ret;
	u16 tte;

	if (sh->regs[SH_FG_REG_TTE] == INVALID_REG_ADDR) {
		pr_err("Time To Empty not supported!\n");
		return -1;
	}

	ret = fg_read_word(sh, sh->regs[SH_FG_REG_TTE], &tte);

	if (ret < 0) {
		pr_err("could not read Time To Empty, ret=%d\n", ret);
		return ret;
	}

	if (ret == 0xFFFF)
		return -ENODATA;

	return tte;
}

static void fg_read_gaugeinfo_block(struct sh_fg_chip *sh)
{

	int ret;
	u8 buf1[36];
	u8 buf2[36];
	u8 buf3[36];

	ret = fg_mac_read_block(sh, FG_MAC_CMD_GAUGEINFO_BLOCK1 , buf1, 16);
	if (ret < 0) {
		pr_err("Failed to read gaugeinfo block1:%d\n", ret);
		return;
	}

	sh_log("Cell1 Max Voltage:%d,Cell2 Max Voltage:%d,Cell1 Min Voltage:%d,Cell2 Min Voltage:%d\n",
		buf1[3] << 8 | buf1[2], buf1[5] << 8 | buf1[4], buf1[7] << 8 | buf1[6], buf1[9] << 8 | buf1[8]);
	sh_log("Max Charge Current:%d,Max Discharge Current:%d,Max Cell Temperature:%d,Min Cell Temperature:%d\n",
		buf1[11] << 8 | buf1[10], buf1[13] << 8 | buf1[12], buf1[14], buf1[15]);


	ret = fg_mac_read_block(sh, FG_MAC_CMD_GAUGEINFO_BLOCK2 , buf2, 34);
	if (ret < 0) {
		pr_err("Failed to read gaugeinfo block2:%d\n", ret);
		return;
	}

	sh_log("No. Of COV Events:%d,Last COV Event:%d\n",
		buf2[3] << 8 | buf2[2], buf2[5] << 8 | buf2[4]);
	sh_log("No. Of CUV Events:%d,Last CUV Event:%d\n",
		buf2[7] << 8 | buf2[6], buf2[9] << 8 | buf2[8]);	
	sh_log("No. Of OCD Events:%d,Last OCD Event:%d\n",
		buf2[11] << 8 | buf2[10], buf2[13] << 8 | buf2[12]);
	sh_log("No. Of OCC Events:%d,Last OCC Event:%d\n",
		buf2[15] << 8 | buf2[14], buf2[17] << 8 | buf2[16]);
	sh_log("No. Of AOLD Events :%d,Last AOLD Event:%d\n",
		buf2[19] << 8 | buf2[18], buf2[21] << 8 | buf2[20]);
	sh_log("No. Of ASCD Events:%d,Last ASCD Event:%d\n",
		buf2[23] << 8 | buf2[22], buf2[25] << 8 | buf2[24]);
	sh_log("No. Of ASCC Events:%d,Last ASCC Event:%d\n",
		buf2[27] << 8 | buf2[26], buf2[29] << 8 | buf2[28]);
	sh_log("No. Of OTC Events:%d,Last OTC Event:%d\n",
		buf2[31] << 8 | buf2[30], buf2[33] << 8 | buf2[32]);

	ret = fg_mac_read_block(sh, FG_MAC_CMD_GAUGEINFO_BLOCK3 , buf3, 28);
	if (ret < 0) {
		pr_err("Failed to read gaugeinfo block3:%d\n", ret);
		return;
	}

	sh_log("No. Of OTD Events:%d,Last OTD Event:%d\n",
		buf3[3] << 8 | buf3[2], buf3[5] << 8 | buf3[4]);
	sh_log("No. Of UTC Events:%d,Last UTC Event:%d\n",
		buf3[7] << 8 | buf3[6], buf3[9] << 8 | buf3[8]);	
	sh_log("No. Of UTD Events:%d,Last UTD Event:%d\n",
		buf3[11] << 8 | buf3[10], buf3[13] << 8 | buf3[12]);
	sh_log("No. Of PTO Events:%d,Last PTO Event:%d\n",
		buf3[15] << 8 | buf3[14], buf3[17] << 8 | buf3[16]);
	sh_log("No. Of CTO Events :%d,Last CTO Event:%d\n",
		buf3[19] << 8 | buf3[18], buf3[21] << 8 | buf3[20]);
	sh_log("No. Of No. Of Shutdowns:%d,No. of Partial Resets :%d\n",
		buf3[23] << 8 | buf3[22], buf3[24]);
	sh_log("No. of Full Resets:%d,No. of WDT resets :%d\n",
		buf3[25], buf3[27] << 8 | buf3[26]);
}

static int fg_get_batt_status(struct sh_fg_chip *sh)
{

	fg_read_status(sh);

	if (sh->batt_fc)
		return POWER_SUPPLY_STATUS_FULL;
	else if (sh->batt_dsg)
		return POWER_SUPPLY_STATUS_DISCHARGING;
	else if (sh->batt_curr > 0)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;

}


static int fg_get_batt_capacity_level(struct sh_fg_chip *sh)
{

	if (sh->batt_fc)
		return POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (sh->batt_rca)
		return POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (sh->batt_fd)
		return POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else
		return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;

}

static int fg_get_current_convert(int currnet)
{

	//int vbus = battery_get_vbus();
	
	//pr_err("gezi-----%s-----%d\n",__func__,vbus);

	//if(vbus > 3900 && vbus < 5500){
	//	return currnet * 2;
	//}
	
	return currnet;

}



static enum power_supply_property fg_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	/*POWER_SUPPLY_PROP_HEALTH,*//*implement it in battery power_supply*/
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

static int sh_afi_updating_get_property(struct sh_fg_chip *sh,enum power_supply_property psp,union power_supply_propval *val)
{
	
	switch (psp) {
		
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		mutex_lock(&sh->data_lock);
		val->intval = sh->batt_volt * 1000;
		mutex_unlock(&sh->data_lock);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		mutex_lock(&sh->data_lock);
		val->intval = fg_get_current_convert(sh->batt_curr) * 1000;
		mutex_unlock(&sh->data_lock);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		mutex_lock(&sh->data_lock);
		val->intval = fg_get_current_convert(sh->batt_curr) * 1000;
		mutex_unlock(&sh->data_lock);
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		if (sh->fake_soc >= 0) {
			val->intval = sh->fake_soc;
			break;
		}
		mutex_lock(&sh->data_lock);
		val->intval = sh->batt_soc;
		mutex_unlock(&sh->data_lock);
		break;

	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = fg_get_batt_capacity_level(sh);
		break;

	case POWER_SUPPLY_PROP_TEMP:
		if (sh->fake_temp != -EINVAL) {
			val->intval = sh->fake_temp;
			break;
		}
		mutex_lock(&sh->data_lock);
		val->intval = sh->batt_temp;
		mutex_unlock(&sh->data_lock);
		break;

	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		mutex_lock(&sh->data_lock);
		val->intval = sh->batt_tte;
		mutex_unlock(&sh->data_lock);
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
		mutex_lock(&sh->data_lock);
		val->intval = sh->batt_fcc;
		mutex_unlock(&sh->data_lock);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		mutex_lock(&sh->data_lock);
		val->intval = sh->batt_rm;
		mutex_unlock(&sh->data_lock);	
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		mutex_lock(&sh->data_lock);
		val->intval = sh->batt_dc;
		mutex_unlock(&sh->data_lock);
		break;

	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		mutex_lock(&sh->data_lock);
		val->intval = sh->batt_cyclecnt;
		mutex_unlock(&sh->data_lock);
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	default:
		//mutex_unlock(&sh->update_lock);
		return -EINVAL;
	}
	return 0;
}

static int fg_get_property(struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	struct sh_fg_chip *sh = power_supply_get_drvdata(psy);
	int ret;

	//mutex_lock(&sh->update_lock);
	
	if(sh->updating){
		sh_afi_updating_get_property(sh,psp,val);
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = fg_get_batt_status(sh);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = fg_read_volt(sh);
		mutex_lock(&sh->data_lock);
		if (ret >= 0)
			sh->batt_volt = ret;
		val->intval = sh->batt_volt * 1000;
		mutex_unlock(&sh->data_lock);

		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		mutex_lock(&sh->data_lock);
		fg_read_current(sh, &sh->batt_curr);
		//val->intval = sh->batt_curr * 1000;
		val->intval = fg_get_current_convert(sh->batt_curr) * 1000;
		mutex_unlock(&sh->data_lock);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		mutex_lock(&sh->data_lock);
		fg_read_av_current(sh, &sh->av_batt_curr);
		//val->intval = sh->av_batt_curr * 1000;
		val->intval = fg_get_current_convert(sh->av_batt_curr) * 1000;
		mutex_unlock(&sh->data_lock);
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		if (sh->fake_soc >= 0) {
			val->intval = sh->fake_soc;
			break;
		}
/*drv add by zhaopengge 20230911 ---start*/		
//		ret = fg_read_rsoc(sh);
		mutex_lock(&sh->data_lock);
//		if (ret >= 0){
//			sh->batt_soc = ret;
//		}
/*drv add by zhaopengge 20230911 ---end*/
		val->intval = sh->batt_soc;
		mutex_unlock(&sh->data_lock);
		break;

	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = fg_get_batt_capacity_level(sh);
		break;

	case POWER_SUPPLY_PROP_TEMP:
		if (sh->fake_temp != -EINVAL) {
			val->intval = sh->fake_temp;
			break;
		}
		//ret = fg_read_temperature(sh);
		mutex_lock(&sh->data_lock);
		//if (ret > 0)
		//	sh->batt_temp = ret;
		val->intval = sh->batt_temp;
		mutex_unlock(&sh->data_lock);
		break;

	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		ret = fg_read_tte(sh);
		mutex_lock(&sh->data_lock);
		if (ret >= 0)
			sh->batt_tte = ret;

		val->intval = sh->batt_tte;
		mutex_unlock(&sh->data_lock);
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = fg_read_fcc(sh);
		mutex_lock(&sh->data_lock);
		if (ret > 0)
			sh->batt_fcc = ret;
		val->intval = sh->batt_fcc;
		mutex_unlock(&sh->data_lock);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		ret = fg_read_rm(sh);
		mutex_lock(&sh->data_lock);
		if (ret > 0)
			sh->batt_rm = ret;
		val->intval = sh->batt_rm;
		mutex_unlock(&sh->data_lock);	
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = fg_read_dc(sh);
		mutex_lock(&sh->data_lock);
		if (ret > 0)
			sh->batt_dc = ret;
		val->intval = sh->batt_dc * 1000;
		mutex_unlock(&sh->data_lock);
		break;

	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = fg_read_cyclecount(sh);
		mutex_lock(&sh->data_lock);
		if (ret >= 0)
			sh->batt_cyclecnt = ret;
		val->intval = sh->batt_cyclecnt;
		mutex_unlock(&sh->data_lock);
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	default:
		//mutex_unlock(&sh->update_lock);
		return -EINVAL;
	}

	//mutex_unlock(&sh->update_lock);

	return 0;
}

static int fg_set_property(struct power_supply *psy,
			       enum power_supply_property prop,
			       const union power_supply_propval *val)
{
	struct sh_fg_chip *sh = power_supply_get_drvdata(psy);

	mutex_lock(&sh->update_lock);

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
		sh->fake_temp = val->intval;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		sh->fake_soc = val->intval;
		power_supply_changed(sh->fg_psy);
		break;
	default:
		fg_dump_registers(sh);
		mutex_unlock(&sh->update_lock);
		return -EINVAL;
	}

	mutex_unlock(&sh->update_lock);

	return 0;
}


static int fg_prop_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}



static int fg_psy_register(struct sh_fg_chip *sh)
{
	struct power_supply_config fg_psy_cfg = {};

	sh->fg_psy_d.name = "cw-bat";
	sh->fg_psy_d.type = POWER_SUPPLY_TYPE_BATTERY;
	sh->fg_psy_d.properties = fg_props;
	sh->fg_psy_d.num_properties = ARRAY_SIZE(fg_props);
	sh->fg_psy_d.get_property = fg_get_property;
	sh->fg_psy_d.set_property = fg_set_property;
	sh->fg_psy_d.property_is_writeable = fg_prop_is_writeable;

	fg_psy_cfg.drv_data = sh;
	fg_psy_cfg.num_supplicants = 0;
	sh->fg_psy = devm_power_supply_register(sh->dev,
						&sh->fg_psy_d,
						&fg_psy_cfg);
	if (IS_ERR(sh->fg_psy)) {
		pr_err("Failed to register fg_psy");
		return PTR_ERR(sh->fg_psy);
	}
	return 0;
}


static void fg_psy_unregister(struct sh_fg_chip *sh)
{

	power_supply_unregister(sh->fg_psy);
}

static const u8 fg_dump_regs[] = {
	0x00, 0x02, 0x04, 0x06,
	0x08, 0x0A, 0x0C, 0x0E,
	0x10, 0x16, 0x18, 0x1A,
	0x1C, 0x1E, 0x20, 0x28,
	0x2A, 0x2C, 0x2E, 0x30,
	
};

static ssize_t fg_attr_show_Qmax(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sh_fg_chip *sh = i2c_get_clientdata(client);
	int ret;
	u8 t_buf[64];
	int len;

	memset(t_buf, 0, 64);

	mutex_lock(&sh->update_lock);

	ret = fg_mac_read_block(sh, 0x0075, t_buf, 6);
	if (ret < 0) {
		mutex_unlock(&sh->update_lock);
		return 0;
	}

	len =
	    sprintf(buf, "Qmax Cell 0 = %d\n", (t_buf[3] << 8) | t_buf[2]);

	mutex_unlock(&sh->update_lock);

	return len;
}


//static DEVICE_ATTR(RaTable, S_IRUGO, fg_attr_show_Ra_table, NULL);
static DEVICE_ATTR(Qmax, S_IRUGO, fg_attr_show_Qmax, NULL);
static struct attribute *fg_attributes[] = {
//	&dev_attr_RaTable.attr,
	&dev_attr_Qmax.attr,
	NULL,
};

static const struct attribute_group fg_attr_group = {
	.attrs = fg_attributes,
};


static void fg_dump_registers(struct sh_fg_chip *sh)
{
	int i;
	int ret;
	u16 val;

	for (i = 0; i < ARRAY_SIZE(fg_dump_regs); i++) {
		msleep(5);
		ret = fg_read_word(sh, fg_dump_regs[i], &val);
		if (!ret)
			pr_err("Reg[%02X] = 0x%04X\n", fg_dump_regs[i], val);
	}
}
/*
static int fg_read_bat_state(struct sh_fg_chip *sh)
{
	int ret = 0;
	u8 buf0[3] = {0};
	u8 buf1[32] = {0};
	
	ret = fg_mac_read_block(sh, FG_MAC_CMD_GAUGESTATUS, buf0, 3);
	if (ret < 0) {
		sh_log("Failed to read gaugeinfo GAUGESTATUS:%d\n", ret);
		return -1;
	}
	
	sh_log("GAUGESTATUS:0x%x,0x%x,0x%x",buf0[0],buf0[1],buf0[2]);
	
	ret = fg_mac_read_block(sh, FG_MAC_CMD_DASTATUS1, buf1, 32);
	if (ret < 0) {
		sh_log("Failed to read gaugeinfo DASTATUS1:%d\n", ret);
		return -1;
	}
	
//	sh_log("BATSTATUS0:0x%x,0x%x,0x%x,0x%x\n",buf1[30],buf1[31],buf1[28],buf1[29]);
	
//	sh_log("CELL1 VOL:%d,CELL2 VOL:%d\n",(buf1[30] << 8) + buf1[31],(buf1[28] << 8) + buf1[29]);
	
	sh_log("BATSTATUS1:0x%x,0x%x,0x%x,0x%x\n",buf1[0],buf1[1],buf1[2],buf1[3]);
	
	sh_log("CELL1 VOL:%d,CELL2 VOL:%d\n",(buf1[1] << 8) + buf1[0],(buf1[3] << 8) + buf1[2]);
	
	return 0;
	
}
*/
#ifdef FG_UPDATER_AFI

//struct power_supply *chg_psy

static int sh366003_get_charger_state(struct sh_fg_chip *sh)
{
	union power_supply_propval online = {0};
	int ret = 0;
	
	if (IS_ERR_OR_NULL(sh->chg_psy)) {
		sh->chg_psy = power_supply_get_by_name("primary_chg");
		if (IS_ERR_OR_NULL(sh->chg_psy)){
			chr_err("%s Couldn't get chg_psy\n", __func__);
			return 0;
		}
	}
	
	ret = power_supply_get_property(sh->chg_psy,POWER_SUPPLY_PROP_ONLINE, &online);	
	return online.intval;
}
static bool sh366003_upgrad_check(struct sh_fg_chip *sh)
{

	int online = sh366003_get_charger_state(sh);
	
	pr_err("[%s]nu:%d,soc:%d,dsg:%d,online:%d\n", \
			__func__,sh->need_update,sh->batt_soc,sh->batt_dsg,online);
			
	if(!sh->batt_dsg){
		return false;
	}
	
	if(online){
		return false;
	}

	if(sh->batt_soc < 10){
		return false;
	}
	pr_err("[%s] update start!!!\n",__func__);	
	return true;
}
#endif

static int sh_update_data(struct sh_fg_chip *sh_t)
{
	struct sh_fg_chip *sh = sh_t;
	
	if(IS_ERR_OR_NULL(sh)){
		return -1;
	}
	
	if(sh->updating){
		sh_log("[%s] afi updating...wait...\n",__func__);
		return 0;
	}
	
	fg_read_status(sh);
	
	//sh->batt_soc = fg_read_rsoc(sh);
	
	g_sh366003_uisoc = sh->batt_soc;
	sh->batt_volt = fg_read_volt(sh);
	fg_read_current(sh, &sh->batt_curr);
	sh->batt_temp = fg_read_temperature(sh);
	sh->batt_rm = fg_read_rm(sh);
	sh->batt_soh = fg_read_soh(sh);
	sh->batt_cyclecnt = fg_read_cyclecount(sh);
	
	sh->batt_fcc = fg_read_fcc(sh);
	
	sh->batt_soc = fg_rsoc_convert(sh,fg_read_rsoc(sh));
	//fg_read_bat_state(sh);

	//mutex_unlock(&sh->data_lock);

	//mutex_unlock(&sh->update_lock);

	sh_log("RSOC:%d,Volt:%d,Curr:%d, Temp:%d, SOH:%d,cycnt:%d,RC:%d,FCC:%d,dsg:%d\n", \
		sh->batt_soc, sh->batt_volt, sh->batt_curr, sh->batt_temp, \
		sh->batt_soh,sh->batt_cyclecnt,sh->batt_rm,sh->batt_fcc,sh->batt_dsg);
		
	
	
	return 0;
}


static void sh_bat_work(struct work_struct *work)
{
	struct delayed_work *delay_work;
	struct sh_fg_chip *sh;
	int ret;

	delay_work = container_of(work, struct delayed_work, work);
	sh = container_of(delay_work, struct sh_fg_chip, battery_delay_work);

	ret = sh_update_data(sh);
	if (ret < 0)
		printk(KERN_ERR "iic read error when update data");

	if(sh->updating){
		sh_log("[%s] afi updating...dont't need update psy...\n",__func__);
		return;
	}
	
	power_supply_changed(sh->fg_psy);

	queue_delayed_work(sh->cwfg_workqueue, &sh->battery_delay_work, msecs_to_jiffies(queue_delayed_work_time));
}

static void determine_initial_status(struct sh_fg_chip *sh)
{
	//fg_irq_thread(sh->client->irq, sh);
	/*drv add by zhaopengge 20230911 ---start*/
	queue_delayed_work(sh->cwfg_workqueue, &sh->battery_delay_work, msecs_to_jiffies(500));
	/*drv add by zhaopengge 20230911 ---end*/
}

static void fg_rsoc_init(struct sh_fg_chip *sh)
{
	int ret;
	u16 soc = 0;
	ret = fg_read_word(sh, sh->regs[SH_FG_REG_SOC], &soc);
	if (ret < 0) {
		pr_err("[%s]could not read RSOC, ret = %d\n",__func__,ret);
		return;
	}
	sh->batt_soc_last = soc * 100 / UI_SOC;
	pr_err("[%s]soc:%d,batt_soc_last:%d\n",__func__,soc,sh->batt_soc_last);
	return;
}


/*
static ssize_t show_fw_ver(struct device *dev, struct device_attribute *attr,char *buf)
{
	return sprintf(buf, "0x%02X\n", fw_version);
}
static DEVICE_ATTR(fw_ver, 0664, show_fw_ver, NULL);
*/

#ifdef FG_UPDATER_AFI



static s32 fg_decode_iic_read(struct sh_fg_chip *sm, struct sh_decoder *decoder, u8 *pBuf)
{
	static struct i2c_msg msg[2];
	u8 addr = IIC_ADDR_OF_2_KERNEL(decoder->addr);
	s32 ret;

	if (!sm->client->adapter)
		return -ENODEV;

	mutex_lock(&sm->i2c_rw_lock);

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].buf = &(decoder->reg);
	msg[0].len = sizeof(u8);
	msg[1].addr = addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = pBuf;
	msg[1].len = decoder->length;
	ret = (s32)i2c_transfer(sm->client->adapter, msg, ARRAY_SIZE(msg));

	mutex_unlock(&sm->i2c_rw_lock);
	return ret;
}

static s32 fg_decode_iic_write(struct sh_fg_chip *sm, struct sh_decoder *decoder)
{
	static struct i2c_msg msg[1];
	static u8 write_buf[WRITE_BUF_MAX_LEN];
	u8 addr = IIC_ADDR_OF_2_KERNEL(decoder->addr);
	u8 length = decoder->length;
	s32 ret;//i = 0;
	//u8 * p = &(decoder->buf_first_val);

	if (!sm->client->adapter)
		return -ENODEV;

	if ((length <= 0) || (length + 1 >= WRITE_BUF_MAX_LEN))
	{
		pr_err("i2c write buffer fail: length invalid!");
		return -1;
	}
	
	//pr_err("[%s]addr:0x%x,length:%d,reg:0x%x\n",__func__,addr,length,decoder->reg);
	
	/*
	for(i = 0; i < length;i++){
		pr_err("[%s]addr:0x%x,length:%d,reg:0x%x,data:%x\n",__func__,addr,length,decoder->reg,*(p+i));
	}
	*/
	mutex_lock(&sm->i2c_rw_lock);
	memset(write_buf, 0, WRITE_BUF_MAX_LEN * sizeof(u8));
	write_buf[0] = decoder->reg;
	memcpy(&write_buf[1], &(decoder->buf_first_val), length);

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].buf = write_buf;
	msg[0].len = sizeof(u8) * (length + 1);

	ret = i2c_transfer(sm->client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0)
	{
		pr_err("i2c write buffer fail: can't write reg 0x%02X\n", decoder->reg);
	}

	mutex_unlock(&sm->i2c_rw_lock);
	return (ret < 0) ? ret : 0;
}

int file_decode_process(struct sh_fg_chip *sm, char *profile_name)
{
	struct device *dev = &sm->client->dev;
	// struct device_node* np = dev->of_node;
	u8 *pBuf = NULL;
	u8 *pBuf_Read = NULL;
	char strDebug[FILEDECODE_STRLEN];
	int buflen;
	int wait_ms;
	int i, j,k;
	int line_length;
	int i_bak = 0;
	int result = -1;
	int retry;
	int retry2 = 0;
	int c_cnt = 1;
	int c_cnt_bak = 0;

	pr_err("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	pr_err("file decode start:\n");
	if (strcmp("sinofs_afi_data", profile_name) == 0)
	{
		buflen = sizeof(sinofs_afi_data);
	}
	else
	{
		goto main_process_error;
	}

	pr_err("ele_len=%d, key=%s\n", buflen, profile_name);
	pBuf = (u8 *)devm_kzalloc(dev, buflen, 0);
	pBuf_Read = (u8 *)devm_kzalloc(dev, BUF_MAX_LENGTH, 0);

	if ((pBuf == NULL) || (pBuf_Read == NULL))
	{
		result = ERRORTYPE_ALLOC;
		pr_err(" kzalloc error");
		goto main_process_error;
	}

	if (strcmp("sinofs_afi_data", profile_name) == 0)
	{
		memcpy(pBuf, sinofs_afi_data, buflen);
	}

	pr_err("start unseal, nop...\n");
	if (fg_gauge_unseal(sm) < 0)
	{
		pr_err("unseal error.\n");
	}
	
	for(i = 0;i < 10; i++){
		pr_err("pBuf[%d] = 0x%x\n",i,pBuf[i]);
	}

	//print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, pBuf, 32); // 作用，将pBuf的内容填充到strDebug中
	snprintf(strDebug,32,"%s",pBuf);
	pr_err("file_decode_process: first data=%s\n", strDebug);

	i = 0;
	j = 0;
WRITE_OPERA_RETRY:
	while (i < buflen)
	{
		/* delay: b0: operate, b1: 2, b2-b3: time, big-endian */
		/* other: b0: operate, b1: TWIADR, b2: reg, b3: data_length, b4...end: item */
		if (pBuf[i + INDEX_TYPE] == OPERATE_WAIT)
		{ // type==4 延时作用
			pr_err("WAIT\n");
			wait_ms = ((int)pBuf[i + INDEX_WAIT_HIGH] * 256) + pBuf[i + INDEX_WAIT_LOW];

			if (pBuf[i + INDEX_WAIT_LENGTH] == 2)
			{						 // p[i+1]表示延时数据的个数应该永远是2，否则错误
				pr_err("[%s]wait_ms:%d\n",__func__,wait_ms);
				HOST_DELAY(wait_ms); /* 20211029, Ethan */
				i += LINELEN_WAIT;	 // 修改i的值，加4
			}
			else
			{ // 否则出错的处理
				//print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				snprintf(strDebug,32,"%s",&pBuf[i + INDEX_TYPE]);
				pr_err("file_decode_process wait error! index=%d, str=%s\n", i, strDebug);
				result = ERRORTYPE_LINE;
				goto main_process_error;
			}
		}
		else if (pBuf[i + INDEX_TYPE] == OPERATE_READ)
		{ // type==1
			pr_err("READ\n");
			line_length = pBuf[i + INDEX_LENGTH];
			if (line_length <= 0)
			{
				result = ERRORTYPE_LINE;
				goto main_process_error;
			}

			/* 20211026, Ethan. IAP addr may differ from default addr */
			/*if (fg_read_block(sm, pBuf[i + INDEX_REG], line_length, pBuf_Read) < 0) { */
			if (fg_decode_iic_read(sm, (struct sh_decoder *)&pBuf[i + INDEX_ADDR], pBuf_Read) < 0)
			{
				//print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				snprintf(strDebug,32,"%s",&pBuf[i + INDEX_TYPE]);
				pr_err("file_decode_process read error! index=%d, str=%s\n", i, strDebug);
				result = ERRORTYPE_COMM;
				goto main_process_error;
			}

			i += LINELEN_READ;
		}
		else if (pBuf[i + INDEX_TYPE] == OPERATE_COMPARE)
		{ // type==3，表示比较
			pr_err("COMP--i=%d,%d\n", i_bak, c_cnt);

			line_length = pBuf[i + INDEX_LENGTH]; // 获取下标是3的数据，是发送的数据长度（个数）
			if (line_length <= 0)
			{
				result = ERRORTYPE_LINE;
				goto main_process_error;
			}

			for (retry = 0; retry < COMPARE_RETRY_CNT; retry++)
			{
				/* 20211026, Ethan. IAP addr may differ from default addr */
				/*if (fg_read_block(sm, pBuf[i + INDEX_REG], line_length, pBuf_Read) < 0) { */
				if (fg_decode_iic_read(sm, (struct sh_decoder *)&pBuf[i + INDEX_ADDR], pBuf_Read) < 0) // 返回值存入 pBuf_Read
				{																					   // 返回值小于0出错
					//print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
					snprintf(strDebug,32,"%s",&pBuf[i + INDEX_TYPE]);
					pr_err("file_decode_process compare_read error! index=%d, str=%s\n", i, strDebug);
					result = ERRORTYPE_COMM;
					goto file_decode_process_compare_loop_end;
				}
				// else 正确读取
				//print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, pBuf_Read, line_length); // 将 pBuf_Read 内容存入strDebug打印
				snprintf(strDebug,line_length,"%s",pBuf_Read);
				pr_debug("file_decode_process loop compare successfully: IC read=%s\n\n", strDebug);

				result = 0;
				for (j = 0; j < line_length; j++)
				{
					if (pBuf[INDEX_DATA + i + j] != pBuf_Read[j])
					{
						//pr_err("file_decode_process compare,hfile:0x%x,read:0x%x\n",pBuf[INDEX_DATA + i + j],pBuf_Read[j]);
						result = ERRORTYPE_COMPARE;
						break;
					}
				}

				if (result == 0)
					break; //

				/* compare fail */
				//print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				snprintf(strDebug,32,"%s",&pBuf[i + INDEX_TYPE]);
				pr_err("file_decode_process compare error! index=%d, retry=%d, host=%s\n", i, retry, strDebug);
				for(k = 0;k < line_length;k++){
					pr_err("file_decode_process compare error,index=%d,hfile:0x%x,read data=0x%x\n",k,pBuf[i + INDEX_DATA + k],pBuf_Read[k]);
				}
				//print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, pBuf_Read, 32);
				snprintf(strDebug,32,"%s",pBuf_Read);
				pr_err("ic=%s\n\n", strDebug);

			file_decode_process_compare_loop_end:
				HOST_DELAY(COMPARE_RETRY_WAIT); /* 20211029, Ethan */
			}									// 重复读取两次后，满足 retry >= COMPARE_RETRY_CNT条件

			if (retry >= COMPARE_RETRY_CNT)
			{
				result = ERRORTYPE_COMPARE; /* 20211125, Ethan */
				pr_err("COMP--re,i=%d\n", i_bak);
				i = i_bak;
				c_cnt = c_cnt_bak;
				if (retry2 > 10)
				{
					goto main_process_error;
				}
				retry2++;
				pr_err("retry2 = %d\n", retry2);
				goto WRITE_OPERA_RETRY;
			}

			i += LINELEN_COMPARE + line_length;
			i_bak = i;

			retry2 = 0; // 比较成功后
			c_cnt++;
			c_cnt_bak = c_cnt;
		}
		else if (pBuf[i + INDEX_TYPE] == OPERATE_WRITE)
		{ // p[i+0] // type==2表示写
			pr_err("WRITE\n");
			line_length = pBuf[i + INDEX_LENGTH]; // p[i+3]，获得长度值
			if (line_length <= 0)
			{
				result = ERRORTYPE_LINE;
				goto main_process_error;
			} // 判断长度数据是否正确，若出错，则跳出
			/*
			struct sh_decoder
			{
				u8 addr;
				u8 reg;
				u8 length;
				u8 buf_first_val;
			};
			*/
			
			if(line_length == 2)
			{
				if((pBuf[i + INDEX_DATA] == 0x41) && (pBuf[i + INDEX_DATA1] == 0x00))
				{
					pr_err("gezi:sh366003-------_______------start reset fg\n");
				}
			}
			
			if (fg_decode_iic_write(sm, (struct sh_decoder *)&pBuf[i + INDEX_ADDR]) != 0) // 写成功返回0
			{
				//print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				snprintf(strDebug,32,"%s",&pBuf[i + INDEX_TYPE]);
				pr_err("file_decode_process write error! index=%d, str=%s\n", i, strDebug);
				result = ERRORTYPE_COMM;
				goto main_process_error;
			}

			i += LINELEN_WRITE + line_length; // 修改i的值，固定是4+len，表示整个数组下标
		}
		else
		{
			pr_err("OPERATE_ no active\n");
			result = ERRORTYPE_LINE;
			goto main_process_error;
		}
		
		//pr_err("[%s]i=%d,buflen=%d\n",__func__,i,buflen);
	}
	result = ERRORTYPE_NONE;

main_process_error:
	pr_err("file_decode_process end: result=%d\n", result);
	pr_err("end: result=%d\n", result);
	fg_gauge_seal(sm);

	return result;
}

static int fg_read_sbs_word(struct sh_fg_chip *sm, u32 reg, u16 *val)
{
	int ret = -1;

	pr_info("fg_read_sbs_word start, reg=%08X\n", reg);
	/* 20211029, Ethan. */
	/*
	if (sm->skip_reads) {
		*val = 0;
		return 0;
	}
	*/

	mutex_lock(&sm->i2c_rw_lock);
	if ((reg & CMDMASK_ALTMAC_R) == CMDMASK_ALTMAC_R)
	{ /* 20211116, Ethan */
		ret = __fg_write_word(sm->client, CMD_ALTMAC, (u16)reg);
		if (ret < 0)
			goto fg_read_sbs_word_end;

		HOST_DELAY(CMD_SBS_DELAY); /* 20211029, Ethan */
								   //		HOST_DELAY(500); /* 20220816, zhaorangao  */

		ret = __fg_read_word(sm->client, CMD_ALTBLOCK, val);
	}
	else
	{
		ret = __fg_read_word(sm->client, (u8)reg, val);
	}
fg_read_sbs_word_end:
	mutex_unlock(&sm->i2c_rw_lock);

	return ret;
}

static int fg_write_sbs_word(struct sh_fg_chip *sm, u32 reg, u16 val)
{
	int ret;

	/* 20211029, Ethan. */
	/*
	if (sm->skip_writes)
		return 0;
	*/

	mutex_lock(&sm->i2c_rw_lock);
	ret = __fg_write_word(sm->client, (u8)reg, val);
	mutex_unlock(&sm->i2c_rw_lock);

	return ret;
}

static s32 fg_gauge_unseal(struct sh_fg_chip *sm)
{
	s32 ret;
	u16 temp;
	
	pr_err("SH366003 unseal start,CMD_UNSEALKEY:0x%x\n",CMD_UNSEALKEY);

	ret = fg_write_sbs_word(sm, CMD_MAC, (u16)CMD_UNSEALKEY);
	if (ret < 0)
		goto fg_gauge_unseal_End;
	HOST_DELAY(CMD_SBS_DELAY);

	ret = fg_write_sbs_word(sm, CMD_MAC, (u16)(CMD_UNSEALKEY >> 16));
	if (ret < 0)
		goto fg_gauge_unseal_End;
	HOST_DELAY(CMD_SBS_DELAY);

	ret = fg_write_sbs_word(sm, CMD_MAC, (u16)CMD_UNSEALKEY_FULL);
	if (ret < 0)
		goto fg_gauge_unseal_End;
	HOST_DELAY(CMD_SBS_DELAY);

	ret = fg_write_sbs_word(sm, CMD_MAC, (u16)(CMD_UNSEALKEY_FULL >> 16));
	if (ret < 0)
		goto fg_gauge_unseal_End;
	HOST_DELAY(CMD_SBS_DELAY);

	/**读取确认解密正确*/
	if (fg_read_sbs_word(sm, CMD_OPERATION_STATUS, &temp) < 0)
	{
		pr_err("read CMD_OPERATION_STATUS(0x0054) error.\n");
		ret = CHECK_VERSION_ERR;
		goto fg_gauge_unseal_End;
	}
	pr_err(" Chip Operation Status=0x%04X\n", temp); // 20220422, Ethan
	if (!(temp & 0x00000300))
	{
		pr_err("SH366003 unseal ok.\n");
	}
	else
	{
		pr_err("SH366003 unseal error.\n");
	}
	ret = 0;
fg_gauge_unseal_End:
	return ret;
}

static s32 fg_gauge_seal(struct sh_fg_chip *sm)
{
	return fg_write_sbs_word(sm, CMD_ALTMAC, CMD_SEAL);
}

static s32 Check_Chip_Version(struct sh_fg_chip *sm)
{

	s32 ret = CHECK_VERSION_ERR;
	u16 temp = 0,last_ver = 0,retry_cnt = 20,same_cnt = 0;

	pr_err("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	pr_err("Check_Chip_Version: new afi=0x%04X\n", version_afi);

	//pr_err("start unseal, send 0x12345678:\n");
	if (fg_gauge_unseal(sm) < 0)
	{
		pr_err("unseal error.\n");
	}

	pr_err("start read CMD_AFI_STATIC_SUM(0x0005):\n");
	/* read afi */
	/*if (fg_read_sbs_word(sm, CMD_AFI_STATIC_SUM, &temp) < 0)
	{
		pr_err("read CMD_AFI_STATIC_SUM(0x0005) error.\n");
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	} */
	/**发两次，第一次数据容易丢失. _2023.11.21 acereng*/
	while(retry_cnt > 0)
	{
		if (fg_read_sbs_word(sm, CMD_AFI_STATIC_SUM, &temp) < 0)
		{
			pr_err("read CMD_AFI_STATIC_SUM(0x0005) error.\n");
			ret = CHECK_VERSION_ERR;
			goto Check_Chip_Version_End;
		}
		
		pr_err("Chip_Version:ic afi=0x%04X,last afi=0x%04X,retry:%d,same_cnt:%d\n",temp,last_ver,retry_cnt,same_cnt);

		if(temp == 0){
		//if(1){
			retry_cnt--;
			msleep(20);
		}
		else{
			//break;
			if(last_ver == temp){
				same_cnt++;
				if(same_cnt >= 3){
					afi_ver = temp;
					pr_err("[%s],get real afi ver:0x%04X\n",__func__,temp);
					break;
				}
			}
			else{
				same_cnt = 0;
			}
			last_ver = temp;
		}
	}

	pr_err("[0.0]Chip_Version: ic afi=0x%04X\n", temp); // 20220422, Ethan
	
	if(retry_cnt <= 0){
		sm->need_update = false;
		goto Check_Chip_Version_End;	
	}
	
	if(temp == 0){
		sm->need_update = false;
		goto Check_Chip_Version_End;
	}
	
	if (temp != version_afi)
	{
		sm->need_update = true;
		pr_err(" Chip_Version: ic afi=0x%04X(0x%04X),need update AFI!\n", temp,version_afi); // 20220422, Ethan
	}
	else if (temp == version_afi)
	{
		sm->need_update = false;
		pr_err(" Chip_Version: ic afi=0x%04X(0x%04X),AFI is up-to-date!\n", temp,version_afi); // 20220422, Ethan
	}

Check_Chip_Version_End:
	fg_gauge_seal(sm);
	return ret;
}

/*
static void sh366003_boot_complete_work_func(struct work_struct* work)
{
	
	struct sh_fg_chip *sh;
	struct delayed_work *delay_work;
	
	delay_work = container_of(work, struct delayed_work, work);
	sh = container_of(delay_work, struct sh_fg_chip, sh366003_boot_complete_work);

	if(!sh){
		sh->updating = false;
		pr_err("[%s]sh NULL...return\n",__func__);
		return;
	}
	
	sh->boot_complete = true;
}
*/

static void sh366003_afi_update_work_func(struct work_struct* work)
{
	//int ret = 0,retry = 0;
		
	//delay_work = container_of(work, struct work_struct, work);
//	struct sh_fg_chip *sh = (struct sh_fg_chip *)container_of(work, struct sh_fg_chip, afi_update_work);
	
	struct delayed_work *delay_work;
	struct sh_fg_chip *sh;
	//int ret;

	delay_work = container_of(work, struct delayed_work, work);
	sh = container_of(delay_work, struct sh_fg_chip, afi_update_work);	
	

	if(!sh){
		sh->updating = false;
		//queue_delayed_work(sh->cwfg_workqueue, &sh->battery_delay_work, msecs_to_jiffies(queue_delayed_work_time));
		pr_err("[%s]sh NULL...return\n",__func__);
		return;
	}
	
	if(!sh->need_update){
		pr_err("[%s]need't update..\n",__func__);
		return;
	}
	
	if(sh366003_upgrad_check(sh)){
		sh->need_update = false;
		sh366003_afi_update(sh);
	}
	else{
		schedule_delayed_work(&sh->afi_update_work,msecs_to_jiffies(10000));
	}

}

static void sh366003_afi_update(struct sh_fg_chip *sh)
{
	int ret = 0,retry = 0;
	unsigned long flags = 0;
	//int write_ok = 0;
	sh->updating = true;
	
	spin_lock_irqsave(&sh->slock, flags);
		if (!sh->wake_lock->active)
			__pm_stay_awake(sh->wake_lock);
	spin_unlock_irqrestore(&sh->slock, flags);


	pr_err("Probe: AFI Update start...\n");
	
	for(retry = 0; retry < FILE_DECODE_RETRY; retry++)
	{
		ret = file_decode_process(sh, "sinofs_afi_data");
		if (ret == ERRORTYPE_NONE)
			break;
		HOST_DELAY(FILE_DECODE_DELAY);
	}
	
	if (0 == ret){
		afi_ver = version_afi;
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
		sprintf(current_coulo_info.more," AFI: 0x%x",afi_ver);
#endif
		pr_err("AFI Update successfully, ret=%d\n", ret);
	}
	else{
		pr_err("AFI Update fail, ret=%d\n", ret);
	}
	sh->updating = false;
	
	queue_delayed_work(sh->cwfg_workqueue, &sh->battery_delay_work, msecs_to_jiffies(queue_delayed_work_time));
	
	spin_lock_irqsave(&sh->slock, flags);
	__pm_relax(sh->wake_lock);
	spin_unlock_irqrestore(&sh->slock, flags);
	
}
#endif

static int sh_fg_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{

	int ret,retry_cnt = 5;
	struct sh_fg_chip *sh;
	u8 *regs;
	
	pr_err("----%s-----start!\n",__func__);

	sh = devm_kzalloc(&client->dev, sizeof(*sh), GFP_KERNEL);

	if (!sh)
		return -ENOMEM;

	sh->dev = &client->dev;
	sh->client = client;
	sh->chip = id->driver_data;

	sh->batt_soc	= -ENODATA;
	sh->batt_soc_last = -ENODATA;
	sh->batt_fcc	= -ENODATA;
	sh->batt_rm	= -ENODATA;
	sh->batt_dc	= -ENODATA;
	sh->batt_volt	= -ENODATA;
	sh->batt_temp	= -ENODATA;
	sh->batt_curr	= -ENODATA;
	sh->batt_soh	= -ENODATA;
	sh->batt_cyclecnt = -ENODATA;
	
	sh->fake_soc	= -EINVAL;
	sh->fake_temp	= -EINVAL;
#ifdef FG_UPDATER_AFI	
	sh->need_update = false;
	//sh->boot_complete = false;
	sh->updating = false;
#endif	
	
	if (sh->chip == SH366003) {
		regs = sh366003_regs;
	} else {
		pr_err("unexpected fuel gauge: %d\n", sh->chip);
		regs = sh366003_regs;
	}

	memcpy(sh->regs, regs, NUM_REGS);

	i2c_set_clientdata(client, sh);

	mutex_init(&sh->i2c_rw_lock);
	mutex_init(&sh->data_lock);
	mutex_init(&sh->update_lock);
	mutex_init(&sh->irq_complete);
#ifdef FG_UPDATER_AFI
	sh->wake_lock = wakeup_source_register(&client->dev, "afi_update_wake_lock");
	spin_lock_init(&sh->slock);
#endif
	sh->resume_completed = true;
	sh->irq_waiting = false;
	
err0:
	pr_err("%s [00]fg_read_fw_version\n",__func__);
	ret = fg_read_fw_version(sh);
	pr_err("%s [11]fg_read_fw_version\n",__func__);
	if(ret < 0){
		retry_cnt--;
		pr_err("%s fg_read_fw_version retry:%d\n",__func__,retry_cnt);
		if(retry_cnt > 0){
			msleep(50);
			goto err0;
		}
		else{
			pr_err("%s fg_read_fw_version i2c err!!!\n",__func__);
/*drv add by zhaopengge 20230823---start*/		
			sh366003_init_done = false;
/*drv add by zhaopengge 20230823---end*/
			return 0;
		}
	}
	

	fg_read_gaugeinfo_block(sh);
	
	state_of_health = fg_read_soh(sh);
	fg_rsoc_init(sh);
	fg_psy_register(sh);
	
	Check_Chip_Version(sh);

	ret = sysfs_create_group(&sh->dev->kobj, &fg_attr_group);
	if (ret)
		pr_err("Failed to register sysfs, err:%d\n", ret);
	
#ifdef FG_UPDATER_AFI
	INIT_DELAYED_WORK(&sh->afi_update_work, sh366003_afi_update_work_func);
#endif	
	//ret = device_create_file(&(client->dev), &dev_attr_fw_ver);
	sh->cwfg_workqueue = create_singlethread_workqueue("cwfg_gauge");
	INIT_DELAYED_WORK(&sh->battery_delay_work, sh_bat_work);

	determine_initial_status(sh);
/*drv add by zhaopengge 20230823---start*/	
	sh366003_init_done = true;
/*drv add by zhaopengge 20230823---end*/
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
		sprintf(current_coulo_info.chip,"sh366003,fw:0x%04X",fw_version);
		sprintf(current_coulo_info.id,"0x%02x,SOH:%d",client->addr,state_of_health);
		strcpy(current_coulo_info.vendor,"SinoWealth");
		sprintf(current_coulo_info.more," AFI: 0x%x",afi_ver);
#endif


#ifdef FG_UPDATER_AFI
	schedule_delayed_work(&sh->afi_update_work,msecs_to_jiffies(10000));	
#endif

	sh_log("sh fuel gauge probe successfully, %s\n", device2str[sh->chip]);

	return 0;

//err_1:
	fg_psy_unregister(sh);

	return ret;
}


static inline bool is_device_suspended(struct sh_fg_chip *sh)
{
	return !sh->resume_completed;
}


static int sh_fg_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sh_fg_chip *sh = i2c_get_clientdata(client);
/*drv add by zhaopengge 20230823---start*/	
	if(!sh366003_init_done){
		return 0;
	}
/*drv add by zhaopengge 20230823---end*/
/*
	mutex_lock(&sh->irq_complete);
	sh->resume_completed = false;
	mutex_unlock(&sh->irq_complete);
*/
	cancel_delayed_work(&sh->battery_delay_work);
	
	if(sh->need_update){
		cancel_delayed_work(&sh->afi_update_work);
	}

	return 0;
}

/*
static int sh_fg_suspend_noirq(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sh_fg_chip *sh = i2c_get_clientdata(client);

	if (sh->irq_waiting) {
		pr_err_ratelimited("Aborting suspend, an interrupt was detected while suspending\n");
		return -EBUSY;
	}
	return 0;

}
*/

static int sh_fg_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sh_fg_chip *sh = i2c_get_clientdata(client);

	//mutex_lock(&sh->irq_complete);
	//sh->resume_completed = true;
	
	/*if (sh->irq_waiting) {
		sh->irq_disabled = false;
		//enable_irq(client->irq);
		mutex_unlock(&sh->irq_complete);
		//fg_irq_thread(client->irq, sh);
	} else {*/
	//}
	
	//mutex_unlock(&sh->irq_complete);
	
	//power_supply_changed(sh->fg_psy);
/*drv add by zhaopengge 20230823---start*/	
	if(!sh366003_init_done){
		return 0;
	}
/*drv add by zhaopengge 20230823---end*/	
	queue_delayed_work(sh->cwfg_workqueue, &sh->battery_delay_work, msecs_to_jiffies(20));
	if(sh->need_update){
		schedule_delayed_work(&sh->afi_update_work,msecs_to_jiffies(1000));
	}
	return 0;


}

static int sh_fg_remove(struct i2c_client *client)
{
	struct sh_fg_chip *sh = i2c_get_clientdata(client);

	fg_psy_unregister(sh);

	mutex_destroy(&sh->data_lock);
	mutex_destroy(&sh->i2c_rw_lock);
	mutex_destroy(&sh->update_lock);
	mutex_destroy(&sh->irq_complete);

	sysfs_remove_group(&sh->dev->kobj, &fg_attr_group);

	return 0;

}

static void sh_fg_shutdown(struct i2c_client *client)
{
	pr_info("sh fuel gauge driver shutdown!\n");
}

static const struct of_device_id sh_fg_match_table[] = {
	{.compatible = "sino,sh366003",},
	{},
};
MODULE_DEVICE_TABLE(of, sh_fg_match_table);

static const struct i2c_device_id sh_fg_id[] = {
	{ "sh366003", SH366003 },
	{},
};
MODULE_DEVICE_TABLE(i2c, sh_fg_id);

static const struct dev_pm_ops sh_fg_pm_ops = {
	.resume		= sh_fg_resume,
	.suspend	= sh_fg_suspend,
};

static struct i2c_driver sh_fg_driver = {
	.driver	= {
		.name   = "sh_fg",
		.owner  = THIS_MODULE,
		.of_match_table = sh_fg_match_table,
		.pm     = &sh_fg_pm_ops,
	},
	.id_table       = sh_fg_id,

	.probe          = sh_fg_probe,
	.remove		= sh_fg_remove,
	.shutdown	= sh_fg_shutdown,

};

module_i2c_driver(sh_fg_driver);

MODULE_DESCRIPTION("SH SH366003 Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Sinowealth");



