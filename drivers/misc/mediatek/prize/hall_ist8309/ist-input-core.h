#ifndef __IST_INPUT_CORE_H__
#define __IST_INPUT_CORE_H__

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/hrtimer.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>

#define UNUSED(x) ( (void)(x) )
#define IST_MIN_DELAY	(5) // milliseconds
#define MAX_AXIS_COUNT  (4) // X, Y, Z, TEMP

struct ist_base;
typedef void (*timed_work_func)(struct ist_base *ist);
typedef int (*read_value_func)(struct ist_base* ist, int16_t vec[4]);
typedef int (*set_enable_func)(struct ist_base* ist, int);
typedef int (*set_delay_func)(struct ist_base* ist, int);
typedef int (*set_thresholds_func)(struct ist_base *ist, short* lowthd, short* highthd, size_t size); // return<0 on failure, or return count of threshold
typedef int (*get_thresholds_func)(struct ist_base *ist, short* lowthd, short* highthd, size_t size); // return count of thresholds
typedef int (*set_interrupts_func)(struct ist_base *ist, int intr_type, int* intr, size_t size); // return<0 on failure, or return count of interrupts
typedef int (*get_interrupts_func)(struct ist_base *ist, int* intr_type, int* intr, size_t size); // return count of interrupts
typedef int (*enable_interrupt_func)(struct ist_base* ist, int);
typedef int (*clear_interrupt_func)(struct ist_base* ist);
typedef void (*irq_delayedwork_func)(struct ist_base* ist);

#define INTERRUPT_BESIDE 0
#define INTERRUPT_WITHIN 1

struct ist_base {
	char name[20];
	struct i2c_client *i2c;
	struct input_dev *idev;
	struct hrtimer work_timer;
	struct completion report_complete;
	struct task_struct *thread;
	bool hrtimer_running;
	struct mutex lock;
	atomic_t enabled;
	unsigned int on_before_suspend;
	struct {
		unsigned scale_val_int;
		unsigned scale_val_micro;
	};
	// dts values
	uint32_t layout;
	int poll_delay_ms;
	int index;
	int use_irq;
	int irq_gpio;
	int intr_type;
	int16_t intr_value[MAX_AXIS_COUNT];
	int intr_count;
	int16_t thres_low[MAX_AXIS_COUNT];
	int16_t thres_high[MAX_AXIS_COUNT];
	int thres_count;

	// callback
	timed_work_func timed_poll_func;
	read_value_func read_once; /* read data while not enabled */
	set_enable_func set_enable;
	set_delay_func set_delay;

	// irq and threshold variables
	get_interrupts_func get_interrupts;
	set_interrupts_func set_interrupts;
	set_thresholds_func set_thresholds;
	get_thresholds_func get_thresholds;
	enable_interrupt_func enable_interrupt;
	clear_interrupt_func clear_interrupt;
	irq_delayedwork_func irq_delayedwork;
	struct delayed_work delayed_work;
	int irq;
	int irq_enabled;
};

// utils
void ist_short_to_2byte(short x, u8 *hbyte, u8 *lbyte);

// i2c access
int ist_i2c_rxdata(struct i2c_client *i2c, unsigned char *rxData, int length);
int ist_i2c_txdata(struct i2c_client *i2c, unsigned char *txData, int length);
int ist_remove(struct i2c_client *client);
int ist_suspend(struct device *dev);
int ist_resume(struct device *dev);

// ist functions
struct ist_base *ist_alloc(struct i2c_client *client, ssize_t extra_size, const char* name);
int ist_add_device_attr(struct ist_base *ist, struct attribute * attr);
int ist_remove_device_attr(struct ist_base *ist, const char* attr_name);
int ist_common_init(struct ist_base *ist);
int ist_common_uninit(struct ist_base *ist);
int ist_setup_eint(struct ist_base *ist);
int ist_set_enable(struct ist_base *ist, int enable);
void ist_report_data(struct ist_base* ist, int16_t *xyz, int data_count);
void ist_report_interrupt(struct ist_base* ist, unsigned value, int intr_count);
void ist_remap_data(struct ist_base* ist, int16_t raw[3], int16_t converted[3]);
void ist_remap_transposed_data(struct ist_base* ist, int16_t raw[3], int16_t converted[3]);
int ist_set_reg(struct ist_base* ist, unsigned char reg, unsigned char val);
int ist_enable_interrupt(struct ist_base *ist, int enable);

#endif // __IST_INPUT_CORE_H__