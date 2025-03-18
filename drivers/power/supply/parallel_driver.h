#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
//#include <linux/wakelock.h>
#include <linux/suspend.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>
//#include <mt-plat/mtk_boot.h>
//
#define  sd77428_data  fg_bms_chip
#define  sd77122_chip  fg_bms_chip

typedef struct batt_data {
    uint16_t    batt_rc;        //Relative State Of Charged, present percentage of battery capacity
    uint16_t    batt_rsoc;      //Relative State Of Charged, present percentage of battery capacity
    uint16_t    batt_soh;       //0~100， SOH
    uint16_t    batt_cyclecnt;  //cycle cnt
    uint16_t    batt_voltage;   //Voltage of battery, in mV
    int16_t     batt_current;   //Current of battery, in mA; plus value means charging, minus value means discharging
    int16_t     batt_temp;      //Temperature of battery
    uint16_t    batt_capacity;  //adjusted residual capacity
    uint16_t    batt_fcc;
    uint16_t    batt_dfcc;
    uint16_t    batt_cc;
    
    int16_t    ext_charger;
    
    // int16_t  discharge_current_th;
    // uint8_t  charge_end;
}batt_data_t;

typedef struct pack_fuse_data {
    uint16_t    pack_fuse_rsoc;      // fuse rsoc now
    uint16_t    pack_fuse_voltage;   //fuse Voltage of battery, in mV
    int16_t     pack_fuse_current;   //fuse Current of battery, in mA; plus value means charging, minus value means discharging
    int16_t     pack_fuse_temp;      //fuse Temperature of battery
}pack_fuse_data_t;

struct fg_bms_chip
{
    struct device *dev;
    struct i2c_client * client;
    struct mutex i2c_rw_lock;
    struct mutex irq_lock;

    struct delayed_work sd77122_work;
    struct delayed_work sd77122_prep_work;
    struct delayed_work sd77428_work;
    // struct delayed_work sd77428_mainwork;
    // struct delayed_work sd77428_rntwork;
    struct delayed_work sd77428_download_work;
    struct delayed_work chip_fuse_work;

    struct power_supply *bat;
    struct power_supply_desc bat_desc;
    struct power_supply_config bat_cfg;
    struct power_supply *ac_psy;
    struct power_supply *usb_psy;

    uint8_t adapter_status;
    bool chg_full;
    uint8_t err_times;
    batt_data_t batt_info;
    pack_fuse_data_t pack_fuse_info;
    
    //sd77122
    int32_t  irq_gpio;
    int32_t  irq;
    
    uint8_t porn_flag;
    uint8_t ohtp_flag;
    uint8_t pre_timeout_flag;
    uint8_t trigend_flag;
    uint8_t ocp_state;
    uint8_t uvt_state;
    uint8_t cp_fail_state;
    uint8_t bat_in_det;
};

//------------------------------------extern interface-------------------------------------------//
extern int32_t sd77122_parse_dt(struct sd77122_chip *chip,struct device *dev);

extern int32_t sd77122_register_irq(struct sd77122_chip *chip);

extern int32_t sd77122_ic_init(struct sd77122_chip *chip);
extern int32_t sd77122_resume(struct device *dev_chip);
extern int32_t sd77122_suspend(struct device *dev_chip);
extern int32_t sd77122_remove(struct i2c_client *client);
extern void sd77122_shutdown(struct i2c_client *client);

extern int32_t sd77428main_ic_init(struct sd77428_data *chip);
extern int32_t sd77428main_suspend(struct device *dev);
extern int32_t sd77428main_resume(struct device *dev);
extern int32_t sd77428main_remove(struct i2c_client *client);
extern void sd77428main_shutdown(struct i2c_client *client);

extern int32_t sd77428rnt_ic_init(struct sd77428_data *chip);
extern int32_t sd77428rnt_suspend(struct device *dev);
extern int32_t sd77428rnt_resume(struct device *dev);
extern int32_t sd77428rnt_remove(struct i2c_client *client);
extern void sd77428rnt_shutdown(struct i2c_client *client);

extern int32_t sd77122_enable_qnqp(void);
extern int32_t sd77122_disable_qnqp(void);
extern int32_t sd77122_enter_prep_to_parrell_mode(void);
extern int32_t sd77122_enter_parrell_to_prep_mode(void);
extern int32_t sd77122_judge_protect_or_prep_mode(void);
extern int32_t sd77122_parallel_ilimit(int32_t timeout, int32_t curr);
extern int32_t sd77122_prep_ilimit(int32_t timeout, int32_t curr);

//----------------------------------------------------------------------//
extern int32_t sd77428main_chip_ok(void);
extern int32_t sd77428rnt_chip_ok(void);
extern int32_t sd77428main_get_battery_voltage(void);
extern int32_t sd77428rnt_get_battery_voltage(void);
extern int32_t sd77428main_get_battery_current(void);
extern int32_t sd77428rnt_get_battery_current(void);
extern int32_t sd77428main_get_battery_temp(void);
extern int32_t sd77428rnt_get_battery_temp(void);
extern int32_t sd77428main_get_soc(void);
extern int32_t sd77428rnt_get_soc(void);

extern int32_t sd77428main_get_ext_charger(void);
// extern int32_t sd77428main_set_ext_charger(int16_t charge_state);

extern int32_t fg_fuse_voltage(void);
extern int32_t fg_fuse_current(void);
extern int32_t fg_fuse_thm(void);
extern int32_t fg_fuse_rsoc(void);
