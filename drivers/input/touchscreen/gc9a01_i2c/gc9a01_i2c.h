
#ifndef __GC9A01_I2C__H__
#define __GC9A01_I2C__H__


/*register address*/
#define FTS_REG_START             0x00
#define FTS_REG_CHIP_ID           0xA7     //chip ID .Lucifer.gu change
#define FTS_REG_LOW_POWER         0xE5     //low power
#define FTS_REG_FW_VER            0xA6     //FW  version 
#define FTS_REG_VENDOR_ID         0xA8     //TP vendor ID 
#define FTS_REG_POINT_RATE        0x88     //report rate

#define CHIP_59_ENTER_SLEEP       0x03
#define CHIP_OTHER_ENTER_SLEEP    0x03

#define TOUCH_SLEEP_TIMEOUT_TIME	(3*60*60) // defalut 12hours = 12*60*60

/*
static unsigned int gc9a01_tp_reset = 0;
static unsigned int gc9a01_tp_int = 0;
static int tp_irq = 0;
static int tpd_flag = 0;
static struct input_dev *gc9a01_dev;
struct i2c_client *g_client = NULL;
//static struct delayed_work delay_work;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DEFINE_MUTEX(i2c_access);
*/

struct gc9a01_data {
	unsigned int gc9a01_tp_reset;
	unsigned int gc9a01_tp_int;
	int tp_irq;
	int tpd_flag;
	unsigned char irq_disabled;
	struct input_dev *gc9a01_dev;
	struct i2c_client *client;
	wait_queue_head_t  waiter;
	struct mutex i2c_access;
	spinlock_t irq_lock;
	struct regulator *vdd;
};

extern void gc9a01_tp_power_ctrl(int value);

#endif