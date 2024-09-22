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
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
#include <uapi/linux/sched/types.h>
#endif

#include "ist-input-core.h"

#define POLL_INTERVAL_DEFAULT          (20)
//#define IST_HERE dev_info(&ist->i2c->dev, "%s %d", __func__, __LINE__)

static int orientations[8][9] = {
	{ 1,  0,  0,  0,  1,  0,  0,  0,  1},
	{ 0, -1,  0,  1,  0,  0,  0,  0,  1},
	{-1,  0,  0,  0, -1,  0,  0,  0,  1},
	{ 0,  1,  0, -1,  0,  0,  0,  0,  1},
	{ 1,  0,  0,  0, -1,  0,  0,  0, -1},
	{ 0, -1,  0, -1,  0,  0,  0,  0, -1},
	{-1,  0,  0,  0,  1,  0,  0,  0, -1},
	{ 0,  1,  0,  1,  0,  0,  0,  0, -1},
};

static DEFINE_MUTEX(i2c_mutex);

static int ist_parse_interrupts_string(struct ist_base *ist, const char *buf,
				int* intr_type, int16_t intr[MAX_AXIS_COUNT], int* intr_count);
static int ist_parse_thresholds_string(struct ist_base *ist, const char *buf,
				int16_t low[MAX_AXIS_COUNT], int16_t high[MAX_AXIS_COUNT], int* thres_count);

int* ist_get_orientation(struct ist_base *ist)
{
	if (ist->layout >= 0 && ist->layout < 8) {
		return orientations[ist->layout];
	} else {
		dev_warn(&ist->i2c->dev, "Wrong layout = %d", ist->layout);
		return orientations[0];
	}
}

void ist_remap_data(struct ist_base* ist, int16_t raw[3], int16_t converted[3])
{
	int* orientation = ist_get_orientation(ist);

	converted[0] = (orientation[0])*raw[0] + (orientation[1])*raw[1] + (orientation[2])*raw[2];
	converted[1] = (orientation[3])*raw[0] + (orientation[4])*raw[1] + (orientation[5])*raw[2];
	converted[2] = (orientation[6])*raw[0] + (orientation[7])*raw[1] + (orientation[8])*raw[2];
}

void ist_remap_transposed_data(struct ist_base* ist, int16_t raw[3], int16_t converted[3])
{
	int* orientation = ist_get_orientation(ist);

	converted[0] = (orientation[0])*raw[0] + (orientation[3])*raw[1] + (orientation[6])*raw[2];
	converted[1] = (orientation[1])*raw[0] + (orientation[4])*raw[1] + (orientation[7])*raw[2];
	converted[2] = (orientation[2])*raw[0] + (orientation[5])*raw[1] + (orientation[8])*raw[2];
}

int ist_i2c_rxdata(struct i2c_client *i2c, unsigned char *rxData, int length)
{
	struct i2c_msg msgs[] = {
		{
			.addr = i2c->addr,
			.flags = 0,
			.len = 1,
			.buf = rxData,
		},
		{
			.addr = i2c->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = rxData,
		}, };
	unsigned char addr = rxData[0];

	mutex_lock(&i2c_mutex);
	if (i2c_transfer(i2c->adapter, msgs, 2) < 0) {
		dev_err(&i2c->dev, "%s: transfer failed.", __func__);
		mutex_unlock(&i2c_mutex);
		return -EIO;
	}
	mutex_unlock(&i2c_mutex);
	dev_vdbg(&i2c->dev, "RxData: len=%02x, addr=%02x, data=%02x",
			length, addr, rxData[0]);
	return 0;
}

int ist_i2c_txdata(struct i2c_client *i2c, unsigned char *txData, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = i2c->addr,
			.flags = 0,
			.len = length,
			.buf = txData,
		}, };

	mutex_lock(&i2c_mutex);
	if (i2c_transfer(i2c->adapter, msg, 1) < 0) {
		dev_err(&i2c->dev, "%s: transfer failed.", __func__);
		mutex_unlock(&i2c_mutex);
		return -EIO;
	}
	mutex_unlock(&i2c_mutex);
	dev_vdbg(&i2c->dev, "TxData: len=%02x, addr=%02x data=%02x",
			length, txData[0], txData[1]);
	return 0;
}

int ist_set_reg(struct ist_base* ist, unsigned char reg, unsigned char val)
{
	unsigned char buffer[2];
	
	buffer[0] = reg;
	buffer[1] = val;
	if (ist_i2c_txdata(ist->i2c, buffer, 2))
	{
		dev_err(&ist->i2c->dev, "reg %x write %x failed at %d\n", buffer[0], buffer[1], __LINE__);
	}
	dev_info(&ist->i2c->dev, "ist_set_reg - reg 0x%x set 0x%x \n", buffer[0], buffer[1]);
	return 0;
}

void ist_report_data(struct ist_base *ist, int16_t *xyz, int count)
{
	struct input_dev* idev = ist->idev;
	int16_t remapped[3] = {0};
	int16_t temp[MAX_AXIS_COUNT] = {0};
	int i;

	for (i=0; i<count && i<MAX_AXIS_COUNT;i++) {
		temp[i] = xyz[i];
	}

	// remap by layout
	if (count >= 3) {
		ist_remap_data(ist, temp, remapped);
		temp[0] = remapped[0];
		temp[1] = remapped[1];
		temp[2] = remapped[2];
	}
	dev_info(&ist->i2c->dev,
		"DATA: [%d, %d, %d]", temp[0], temp[1], temp[2]);

	if (count >= 1)
		input_report_abs(idev, ABS_X, temp[0]);
	if (count >= 2)
		input_report_abs(idev, ABS_Y, temp[1]);
	if (count >= 3)
		input_report_abs(idev, ABS_Z, temp[2]);
	if (count >= 4)
		input_report_abs(idev, ABS_RX, xyz[3]);
	input_sync(idev);
}

static enum hrtimer_restart ist_work_timer(struct hrtimer *timer)
{
	struct ist_base *ist;
	ktime_t poll_delay;
	ist = container_of((struct hrtimer *)timer, struct ist_base, work_timer);

	complete(&ist->report_complete);
	if (ist->poll_delay_ms > 0) {
		poll_delay = ktime_set(0, ist->poll_delay_ms * NSEC_PER_MSEC);
	} else {
		poll_delay = ktime_set(0, POLL_INTERVAL_DEFAULT * NSEC_PER_MSEC);
	}
	ist->hrtimer_running = true;
	hrtimer_forward_now(&ist->work_timer, poll_delay);
	return HRTIMER_RESTART;
}

static int ist_threaded_poll_func(void *data)
{
	struct ist_base *ist = data;
	while (!kthread_should_stop()) {
		/* wait for report event */
		wait_for_completion(&ist->report_complete);
		if (atomic_read(&ist->enabled) <= 0) {
			continue;
		}
		if (ist->timed_poll_func) {
			ist->timed_poll_func(ist);
		}
	}
	return 0;
}

struct input_dev *ist_init_input(struct ist_base *ist)
{
	int status;
	int str_size;
	char *str;
	struct input_dev *input = NULL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,9,0)
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };
#endif

	hrtimer_init(&ist->work_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ist->work_timer.function = ist_work_timer;
	ist->hrtimer_running = false;
	str_size = snprintf(NULL, 0, "%s_report_event", ist->name);
	str = devm_kzalloc(&ist->i2c->dev, str_size+1, GFP_KERNEL);
	ist->thread = kthread_run(ist_threaded_poll_func, ist, str);
	if (IS_ERR(ist->thread)) {
		printk("unable to create %s thread\n", str);
		return NULL;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,9,0)
	sched_setscheduler_nocheck(ist->thread, SCHED_FIFO, &param);
#else
	sched_set_fifo(ist->thread);
#endif

	input = input_allocate_device();
	if (!input)
		return NULL;

	input->name = ist->name;
	input->phys = ist->name;
	input->id.bustype = BUS_I2C;

	__set_bit(EV_ABS, input->evbit);
	input_set_events_per_packet(input, 100);
	input_set_abs_params(input, ABS_X, -32768, 32767, 0, 0);
	input_set_abs_params(input, ABS_Y, -32768, 32767, 0, 0);
	input_set_abs_params(input, ABS_Z, -32768, 32767, 0, 0);
	input_set_abs_params(input, ABS_RX, -32768, 32767, 0, 0);
	input_set_abs_params(input, ABS_THROTTLE, -32768, 32767, 0, 0);
	/*input_set_abs_params(input, ABS_RUDDER, 0, 3, 0, 0);*/

	/* Report the dummy value */
	input_set_abs_params(input, ABS_MISC, INT_MIN, INT_MAX, 0, 0);

	input_set_capability(input, EV_ABS, ABS_X);
	input_set_capability(input, EV_ABS, ABS_Y);
	input_set_capability(input, EV_ABS, ABS_Z);
	input_set_capability(input, EV_ABS, ABS_RX);

	// for interrupt event
	input_set_capability(input, EV_KEY, BTN_0);
	input_set_capability(input, EV_KEY, BTN_1);
	input_set_capability(input, EV_KEY, BTN_2);

	status = input_register_device(input);
	if (status) {
		dev_err(&ist->i2c->dev, "error registering input device\n");
		return NULL;
	}
	input_set_drvdata(input, (void*)ist);
	return input;
}

#ifdef CONFIG_OF
static int ist_parse_dt(struct i2c_client *client, struct ist_base *ist)
{
	struct device_node *np = client->dev.of_node;
	uint32_t layout=0, poll_delay_ms=30;
	int32_t index;
	const char* str;
	if (!of_property_read_u32(np, "layout", &layout)) {
		if (layout >= 0 && layout <= 7) {
			ist->layout = layout;
			dev_info(&client->dev, "layout set to %d", layout);
		} else {
			dev_warn(&client->dev, "ignore invalid layout (%d)", layout);
		}
	}
	if (!of_property_read_u32(np, "poll_delay_ms", &poll_delay_ms)) {
		ist->poll_delay_ms = poll_delay_ms;
		dev_info(&client->dev, "poll_delay_ms set to %d", poll_delay_ms);
	}
	if (!of_property_read_s32(np, "index", &index)) {
		if (index >= 0 && index < 4) { // max 4 devices
			ist->index = index;
			dev_info(&client->dev, "index set to %d", index);
		} else {
			ist->index = -1;
			dev_warn(&client->dev, "ignore invalid index (%d)", index);
		}
	} else {
		ist->index = -1;
		dev_warn(&client->dev, "device not indexed");
	}
	ist->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (ist->irq_gpio >= 0) {
		dev_info(&client->dev, "irq_gpio is %d", ist->irq_gpio);
	} else {
		dev_warn(&client->dev, "no irq_gpio");
	}

	if (!of_property_read_u32(np, "use_irq", &ist->use_irq)) {
		dev_info(&client->dev, "use_irq is %d", ist->use_irq);
	} else {
		dev_warn(&client->dev, "not use_irq");
	}
	if (!of_property_read_string(np, "ist,interrupts", &str)) {
		dev_info(&client->dev, "ist,interrupts is \"%s\"", str);
		ist_parse_interrupts_string(ist, str, &ist->intr_type, ist->intr_value, &ist->intr_count);
	}
	if (!of_property_read_string(np, "ist,thresholds", &str)) {
		dev_info(&client->dev, "ist,thresholds is \"%s\"", str);
		ist_parse_thresholds_string(ist, str, ist->thres_low, ist->thres_high, &ist->thres_count);
	}
	return 0;
}
#else
static int ist_parse_dt(struct i2c_client *client, struct ist_base *ist)
{
	UNUSED(ist);
	dev_info(&client->dev, "CONFIG_OF not configured");
	return 0;
}
#endif

void ist_short_to_2byte(short x, u8 *hbyte, u8 *lbyte)
{
	unsigned short temp;

	if (x >= 0) {
		temp  = x;
	} else {
		temp  = 65536 + x;
	}
	*lbyte = temp & 0x00ff;
	*hbyte = (temp & 0xff00) >> 8;
}

int ist_setup_eint(struct ist_base *ist)
{
	int ret = 0;
	if (ist->use_irq) {
		if (gpio_is_valid(ist->irq_gpio)) {
			int	str_size = snprintf(NULL, 0, "%s_up_irq", ist->name);
			char* str = devm_kzalloc(&ist->i2c->dev, str_size+1, GFP_KERNEL);
			ret = gpio_request(ist->irq_gpio, str);
			if (ret) {
				dev_err(&ist->i2c->dev, "unable to request gpio [%d] by name %s", ist->irq_gpio, str);
				return -EINVAL;
			}  else {
				ret = gpio_direction_input(ist->irq_gpio);
				msleep(50);
				ist->irq = gpio_to_irq(ist->irq_gpio);
			}
			dev_info(&ist->i2c->dev, "GPIO=%d irq=%d ret=%d",ist->irq_gpio, ist->irq, ret);
			return 0;
		} else {
			dev_err(&ist->i2c->dev, "gpio[%d] is invalid", ist->irq_gpio);
			return -EINVAL;
		}
	} else {
		dev_warn(&ist->i2c->dev, "irq not configured, check dts");
		return 0;
	}
}

static int ist_enable_irq(struct ist_base *ist, bool enable)
{
	if (enable) {
		dev_info(&ist->i2c->dev, "enable irq");
		synchronize_irq(ist->irq);
		enable_irq(ist->irq);
	} else {
		dev_info(&ist->i2c->dev, "disable irq");
		disable_irq_nosync(ist->irq);
	}
	return 0;
}

static irqreturn_t ist_irq_handler(int irq, void *data)
{
	struct ist_base *ist;;
	ist = (struct ist_base *)data;
	disable_irq_nosync(irq);
	schedule_delayed_work(&ist->delayed_work, 0);
	dev_info(&ist->i2c->dev, "IRQ handled");
	return IRQ_HANDLED;
}

static int ist_clear_interrupt(struct ist_base *ist)
{
	int err;
	if (ist->clear_interrupt) {
		err = ist->clear_interrupt(ist);
		if (err < 0) {
			return err;
		}
	}
    return 0;
}

static void ist_irq_delayedwork_func(struct work_struct *work)
{
	struct delayed_work *delayed_work = container_of(work, struct delayed_work, work);
	struct ist_base *ist = container_of(delayed_work, struct ist_base, delayed_work);

	dev_info(&ist->i2c->dev, "ist_irq_delayedwork_func enter");

	if (ist->irq_delayedwork) {
		ist->irq_delayedwork(ist);
	}
	mutex_lock(&ist->lock);
	if (ist->irq_enabled) {
		ist_clear_interrupt(ist);
		ist_enable_irq(ist, 1);
	}
	mutex_unlock(&ist->lock);

	dev_info(&ist->i2c->dev, "ist_irq_delayedwork_func leave");
}

int ist_enable_interrupt(struct ist_base *ist, int enable)
{
	int err = 0;

	dev_info(&ist->i2c->dev, "ist_enable_interrupt, enable=%d", enable);
	if (enable) {
		if (!ist->irq_enabled) {
			if (ist->enable_interrupt) {
				err = ist->enable_interrupt(ist, 1);
				if (err < 0) {
					return err;
				}
			}

			/* requst irq */
			if ((err = request_threaded_irq(ist->irq, NULL,
					&ist_irq_handler, IRQ_TYPE_EDGE_FALLING | IRQF_ONESHOT,
					ist->name, (void *)ist)) < 0) {
					dev_err(&ist->i2c->dev, "IRQ LINE (%s) NOT AVAILABLE!!", ist->name);
					return -EINVAL;
			}
			irq_set_irq_wake(ist->irq, 1);
			dev_info(&ist->i2c->dev, "irq enabled");
			ist->irq_enabled = 1;
		}
	} else {
		if (ist->irq_enabled) {
			if (ist->enable_interrupt) {
				err = ist->enable_interrupt(ist, 0);
				if (err < 0) {
					return err;
				}
			}
			disable_irq(ist->irq);
			free_irq(ist->irq, (void *)ist);
			dev_info(&ist->i2c->dev, "irq freed");
			ist->irq_enabled = 0;
		}
	}
	return 0;
}

void ist_report_interrupt(struct ist_base* ist, unsigned value, int count)
{
	int i;
	int16_t intr[MAX_AXIS_COUNT], mapped[3];
	struct input_dev *input_dev = ist->idev;
	dev_info(&input_dev->dev, "report interrupts value=0x%X, count=%d", value, count);

	for (i=0; i < MAX_AXIS_COUNT; i++) { // test bit 0, 1, 2
		if (i<count) {
			intr[i] = ((value >> i) & 0x01) ? 1 : 0;
		} else {
			intr[i] = 0;
		}
	}
	dev_info(&ist->i2c->dev,
		"interrupts: [%d,%d,%d,%d]", intr[0], intr[1], intr[2], intr[3]);
	if (count >= 3) {
		ist_remap_data(ist, intr, mapped);
		for (i=0; i<3; i++) {
			intr[i] = !!mapped[i]; // no negative, either 0 or 1
		}
	}
	dev_info(&ist->i2c->dev,
		"mapped interrupts: [%d,%d,%d,%d]", intr[0], intr[1], intr[2], intr[3]);

	for (i=0; i < MAX_AXIS_COUNT && i < count; i++) { // test bit 0, 1, 2
		if (intr[i]) {
			unsigned key = BTN_0 + (unsigned)i;
			dev_info(&input_dev->dev, "report interrupt from %d", i);
			input_report_key(input_dev, key, 1); // down
			input_sync(input_dev);
			input_report_key(input_dev, key, 0); // up
			input_sync(input_dev);
		}
	}
}

static ssize_t ist_chip_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ist_base *ist = dev_get_drvdata(dev);
	return sprintf(buf, "%s\n", ist->name);
}

static ssize_t ist_layout_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ist_base *ist = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", ist->layout);
}

static ssize_t ist_layout_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct ist_base *ist = dev_get_drvdata(dev);
	int result;
	int value;

	if (!ist) {
		dev_err(dev, "ist data not set!");
		return -1;
	}

	result = sscanf(buf, "%d", &value);
	if (result != 1) {
		dev_err(dev, "%s invalid value \n", __func__);
		return -EINVAL;
	}

	if (value >=0 && value <= 7) {
		dev_info(dev, "%s layout set to %d \n", __func__, value);
		mutex_lock(&ist->lock);
		ist->layout = value;
		mutex_unlock(&ist->lock);
		return count;
	} else {
		dev_err(dev, "%s invalid layout=%d. should be 0 to 7.\n", __func__, value);
		return -EINVAL;
	}
}

static ssize_t ist_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int16_t vec[4];
	struct ist_base *ist = dev_get_drvdata(dev);

	if (atomic_read(&ist->enabled)) {
		return -EBUSY;
	}
	if (ist->read_once) {
		ret = ist->read_once(ist, vec);
	}
	if (ret <= 0) {
		dev_warn(dev, "read_value failed\n");
		return -1;
	}

	if (ret == 1) 
		ret = sprintf(buf, "%hd\n", vec[0]);
	else if (ret == 2)
		ret = sprintf(buf, "%hd %hd\n", vec[0], vec[1]);
	else if (ret == 3)
		ret = sprintf(buf, "%hd %hd %hd\n", vec[0], vec[1], vec[2]);
	else if (ret == 4)
		ret = sprintf(buf, "%hd %hd %hd %hd\n", vec[0], vec[1], vec[2], vec[3]);
	return ret;
}

static ssize_t ist_scale_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ist_base *ist = dev_get_drvdata(dev);
	return sprintf(buf, "%u.%06u\n", ist->scale_val_int, ist->scale_val_micro % 1000000);
}

static ssize_t ist_delay_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ist_base *ist = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", ist->poll_delay_ms);
}

static void ist_set_delay(struct ist_base *ist, unsigned int delay_msec)
{
	int rc = 0;
	ktime_t poll_delay;

	dev_info(&ist->i2c->dev, "delay=%d => %d", ist->poll_delay_ms, delay_msec);

	if (delay_msec < IST_MIN_DELAY) {
		delay_msec = IST_MIN_DELAY;
	}

	if (delay_msec == ist->poll_delay_ms)
		return;

	if (ist->set_delay) {
		rc = ist->set_delay(ist, delay_msec);
	}
	if (!rc) {
		mutex_lock(&ist->lock);
		ist->poll_delay_ms = delay_msec;
		if (atomic_read(&ist->enabled)) {
			if (ist->poll_delay_ms > 0) {
				poll_delay = ktime_set(0, ist->poll_delay_ms * NSEC_PER_MSEC);
			} else {
				poll_delay = ktime_set(0, POLL_INTERVAL_DEFAULT * NSEC_PER_MSEC);
			}
			hrtimer_start(&ist->work_timer, poll_delay, HRTIMER_MODE_REL);
		}
		mutex_unlock(&ist->lock);
	}
}

static ssize_t ist_delay_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct ist_base *ist = dev_get_drvdata(dev);
	int result;
	unsigned value;

	if (!ist) {
		dev_err(dev, "ist data not set!");
		return -1;
	}

	result = sscanf(buf, "%u", &value);
	if (result != 1) {
		return -1;
	}
	dev_info(dev, "%s value=%d \n", __func__, value);
	ist_set_delay(ist, value);
	return count;
}

int ist_set_enable(struct ist_base *ist, int enable)
{
	int rc = 0;

	dev_info(&ist->i2c->dev, "ist_set_enable enable=%d", enable);
	if (ist->set_enable) {
		rc = ist->set_enable(ist, enable);
	}

	if (ist->use_irq) {
		if (!enable) {
			/* stop irq delaywork task, if any */
			cancel_delayed_work_sync(&ist->delayed_work);
		}
		ist_enable_interrupt(ist, enable);
	}
	if (!rc) {
		mutex_lock(&ist->lock);
		if (enable) {
			ktime_t poll_delay;
			ist->hrtimer_running = true;
			if (ist->poll_delay_ms > 0) {
				poll_delay = ktime_set(0, ist->poll_delay_ms * NSEC_PER_MSEC);
			} else {
				poll_delay = ktime_set(0, POLL_INTERVAL_DEFAULT * NSEC_PER_MSEC);
			}
			hrtimer_start(&ist->work_timer, poll_delay, HRTIMER_MODE_REL);
			atomic_set(&ist->enabled, 1);
			dev_info(&ist->i2c->dev, "timer started");
		} else {
			if (ist->hrtimer_running) {
				ist->hrtimer_running = false;
				hrtimer_cancel(&ist->work_timer);
				dev_info(&ist->i2c->dev, "timer cancelled");
			}
			atomic_set(&ist->enabled, 0);
		}
		mutex_unlock(&ist->lock);
	}
	dev_info(&ist->i2c->dev, "ist_set_enable done");
	return rc;
}

static ssize_t ist_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ist_base *ist = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", atomic_read(&ist->enabled));
}

static ssize_t ist_enable_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct ist_base *ist = dev_get_drvdata(dev);
	int result;
	unsigned value;

	if (!ist) {
		dev_err(dev, "ist data not set!");
		return -1;
	}
	result = sscanf(buf, "%u", &value);
	if (result != 1) {
		return -1;
	}
	dev_info(dev, "%s value=%d \n", __func__, !!value);
	ist_set_enable(ist, !!value);
	return count;
}

static uint8_t ist_read_register(struct ist_base *ist, int reg)
{
	unsigned char reg_value = (unsigned char)reg;
	int rc = 0;

	rc = ist_i2c_rxdata(ist->i2c, &reg_value, 1);
	if (rc) {
			dev_err(&ist->i2c->dev, "ist_read_register failed.(%d)\n", rc);
			return rc;
	}
	dev_info(&ist->i2c->dev, "read reg 0x%x = 0x%x\n",reg ,reg_value);
    return reg_value;
}

static ssize_t ist_dump_regs_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct ist_base *ist = dev_get_drvdata(dev);
	// int reg = 0;
	int i,j;
	int bytes_written = 0;
    
    char temp_buffer[512]; 
    ssize_t count = 0; 
	uint8_t reg_value = 0;

	for( i = 0 ; i < 16 ; i++ )
	{
		for( j = 0 ; j < 16 ; j++ )
		{
			reg_value = ist_read_register(ist, i*16+j); // Function to read register
			snprintf(temp_buffer, sizeof(temp_buffer), " 0x%02X", reg_value);
			if(j == 15)
			{
				strcat(temp_buffer, "\n");//	snprintf(temp_buffer, sizeof(temp_buffer), "\n");
			}
			// Copy temp_buffer to buf
			bytes_written = snprintf(buf + count, PAGE_SIZE - count, "%s", temp_buffer);
			if (bytes_written < 0) {
				return bytes_written; // Return error code if snprintf fails
			}
			count += bytes_written;
			
			if (count >= PAGE_SIZE) {
				break; // Stop if buf is full
			}
		}
	}
    return count;
}

static ssize_t ist_set_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct ist_base *ist = dev_get_drvdata(dev);

    int reg, result = 0;
    uint8_t value;

    // Parse the input from the user space
    if (sscanf(buf, "%x %hhx", &reg, &value) != 2) {
        return -EINVAL; // Invalid input
    }

	result = ist_set_reg(ist, reg, value);

    if (result < 0) {
        return result;
    }

    return count; 
}

static ssize_t ist_thresholds_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ist_base *ist = dev_get_drvdata(dev);
	int rc = 0;
	int i, count = 0;
	int16_t low[MAX_AXIS_COUNT] = {0}, high[MAX_AXIS_COUNT] = {0};
	int16_t min[3], max[3];
	char* p = buf;

	if (ist->get_thresholds) {
		count = ist->get_thresholds(ist, low, high, sizeof(low)/sizeof(low[0]));
	}

	// remap if multiple axis
	if (count>=3) {
		ist_remap_data(ist, low, min);
		ist_remap_data(ist, high, max);
		for (i=0; i<3; i++) {
			low[i] = (max[i]>=min[i]) ? min[i] : max[i];
			high[i] = (max[i]>=min[i]) ? max[i] : min[i];
		}
	}

	if (count > 0) {
		for (i = 0; i < count; i++) {
			rc = sprintf(p, "%d,%d", low[i], high[i]);
			p += rc;
			if (i+1 < count) { // not last one
				*(p++) = ',';
			}
		}
	}

	*(p++) = '\n';
	*(p++) = 0;
	return strlen(buf);
}

static int ist_parse_thresholds_string(struct ist_base *ist, const char *buf,
					int16_t low[MAX_AXIS_COUNT], int16_t high[MAX_AXIS_COUNT], int* thres_count)
{
	int result, i;
	short min[3] = {0}, max[3] = {0};

	result = sscanf(buf, "%hd,%hd,%hd,%hd,%hd,%hd",
		&low[0], &high[0], &low[1], &high[1], &low[2], &high[2]);
	if (result != 2 && result != 4 && result != 6 ) {
		dev_err(&ist->i2c->dev, "%s Wrong format. should be \"low0,high0[,low1,high1[,low2,high2]]\" and lowX < highX. buf=%s", ist->name, buf);
		return -EINVAL;
	}

	for (i = 0; i < 3 && (i*2) < result; i++) {
		if (low[i] >= high[i]) {
			dev_err(&ist->i2c->dev, "%s Wrong threshold value[%d]. should be low < high. buf=%s", ist->name, i, buf);
			return -EINVAL;
		}
	}

	*thres_count = result / 2;

	// remap if multiple axis; layout must be determined
	if (*thres_count==3) {
		ist_remap_transposed_data(ist, low, min);
		ist_remap_transposed_data(ist, high, max);
		for (i=0; i<3; i++) {
			low[i] = (max[i]>=min[i]) ? min[i] : max[i];
			high[i] = (max[i]>=min[i]) ? max[i] : min[i];
		}
	}
	return 0;
}

static ssize_t ist_thresholds_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct ist_base *ist = dev_get_drvdata(dev);
	int rc;
	short low[MAX_AXIS_COUNT] = {0}, high[MAX_AXIS_COUNT] = {0};
	int thres_count;

	rc = ist_parse_thresholds_string(ist, buf, low, high, &thres_count);
	if (!rc) {
		if (ist->set_thresholds) {
			rc = ist->set_thresholds(ist, low, high, thres_count);
			if (rc < 0) {
				dev_err(dev, "%s Unable to set thresholds", ist->name);
				return -1;
			}
		}
	}

	return count;
}

static ssize_t ist_interrupts_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ist_base *ist = dev_get_drvdata(dev);
	int intr_type = 0;
	int16_t intr[MAX_AXIS_COUNT], mapped[3];
	int int_value[MAX_AXIS_COUNT];
	int count = 0;
	int i;
	const char* type_str;
	int len = 0;
	char* p = buf;

	if (ist->get_interrupts) {
		count = ist->get_interrupts(ist, &intr_type, int_value, sizeof(int_value)/sizeof(int_value[0]));

		// from int16_t to int
		for (i=0; i<count; i++) {
			intr[i] = int_value[i];
		}

		if (count >= 3) {
			ist_remap_data(ist, intr, mapped);
			for (i=0; i<3;i++) {
				intr[i] = !!mapped[i]; // no negative, either 0 or 1
			}
		}
	}

	type_str = (intr_type == INTERRUPT_BESIDE) ? "beside" : "within";

	strcpy(p, type_str);
	len = strlen(type_str);
	p[len++] = ' ';

	for (i = 0; i < count; i++) {
		buf[len++] = (!!intr[i]) ? '1' : '0';
		if (i+1 < count) { // not last one
			buf[len++] = ',';
		}
	}
	buf[len++] = '\n';
	buf[len++] = 0;
	return len;
}

static int ist_parse_interrupts_string(struct ist_base *ist,
				  const char *buf, int* intr_type, int16_t intr[MAX_AXIS_COUNT], int* intr_count)
{
	int result;
	int i;
	char type_str[20];
	int16_t mapped[3] = {0};

	result = sscanf(buf, "%19s %hd,%hd,%hd", type_str, &intr[0], &intr[1], &intr[2]);
	if (result < 2) {
		dev_err(&ist->i2c->dev, "%s Wrong format. should be \"\'beside\'|\'within\' x,y,z\", where x and y = '0' or '1'. buf=%s", ist->name, buf);
		return -EINVAL;
	}

	if (strcmp(type_str, "within") && strcmp(type_str, "beside")) {
		dev_err(&ist->i2c->dev, "%s Wrong interrupt type. should be \'beside\' or \'within\'. buf=%s", ist->name, buf);
		return -EINVAL;
	}

	*intr_count = result - 1;
	for (i=0; i < *intr_count; i++) {
		if (intr[i] != 0 && intr[i] != 1) {
			dev_err(&ist->i2c->dev, "Wrong values. x,y,z should 0 or 1. buf=%s", buf);
			return -EINVAL;
		}
	}

	if (*intr_count >= 3) {
		ist_remap_transposed_data(ist, intr, mapped);
		for (i=0; i<3;i++) {
			intr[i] = !!mapped[i]; // no negative, either 0 or 1
		}
	}

	*intr_type = !strcmp(type_str, "beside") ? INTERRUPT_BESIDE : INTERRUPT_WITHIN;
	return 0;
}

static ssize_t ist_interrupts_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct ist_base *ist = dev_get_drvdata(dev);
	int result;
	int intr_count;
	int intr_type = 0;
	int16_t intr[MAX_AXIS_COUNT] = {0};

	dev_info(dev, "ist_set_interrupts_store enter");

	result = ist_parse_interrupts_string(ist, buf, &intr_type, intr, &intr_count);
	if (result == 0) {
		if (ist->set_interrupts) {
			int int_value[MAX_AXIS_COUNT];
			int rc, i;

			// from int16_t to int
			for (i=0; i<intr_count; i++) {
				int_value[i] = (int)intr[i];
			}
			rc = ist->set_interrupts(ist, intr_type, int_value, intr_count);
			if (rc < 0) {
				dev_err(dev, "%s set_interrupts failed rc=%d", ist->name, rc);
				return rc;
			}
		}
		dev_info(dev, "ist_set_interrupts_store done");
		return count;
	} else {
		dev_info(dev, "ist_set_interrupts_store fail");
		return result;
	}
}

static DEVICE_ATTR(chipinfo, S_IRUSR|S_IRGRP, ist_chip_info, NULL);
static DEVICE_ATTR(layout, S_IRUSR|S_IRGRP|S_IWUSR, ist_layout_show, ist_layout_store);
static DEVICE_ATTR(value, S_IRUSR|S_IRGRP, ist_value_show, NULL);
static DEVICE_ATTR(delay, S_IRUSR|S_IWUSR|S_IWGRP, ist_delay_show, ist_delay_store);
static DEVICE_ATTR(scale, S_IRUSR|S_IRGRP, ist_scale_show, NULL);
static DEVICE_ATTR(enable, S_IRUSR|S_IWUSR|S_IWGRP, ist_enable_show, ist_enable_store);
static DEVICE_ATTR(dump_regs, S_IRUGO|S_IRGRP, ist_dump_regs_show, NULL);
static DEVICE_ATTR(set_reg, S_IRUGO|S_IWUSR|S_IWGRP, NULL, ist_set_reg_store);
static DEVICE_ATTR(thresholds, S_IRUGO|S_IWUSR|S_IWGRP, ist_thresholds_show, ist_thresholds_store);
static DEVICE_ATTR(interrupts, S_IRUGO|S_IWUSR|S_IWGRP, ist_interrupts_show, ist_interrupts_store);

#define MAX_ATTRIBUTES	14
static struct attribute *ist_attributes[MAX_ATTRIBUTES] = {
	&dev_attr_chipinfo.attr,
	&dev_attr_layout.attr,
	&dev_attr_value.attr,
	&dev_attr_delay.attr,
	&dev_attr_scale.attr,
	&dev_attr_enable.attr,
	&dev_attr_dump_regs.attr,
	&dev_attr_set_reg.attr,
	NULL,
};

static const struct attribute_group ist_attr_group = {
	.attrs = ist_attributes,
};

int ist_remove_device_attr(struct ist_base *ist, const char* attr_name)
{
	struct attribute **pattr;

	if (ist->idev) {
		dev_err(&ist->i2c->dev, "idev already inited");
		return -1;
	}

	pattr = ist_attributes;
	while (*pattr != NULL) {
		if (!strncmp((*pattr)->name, attr_name, strlen((*pattr)->name))) {
			while (*pattr != NULL) {
				*pattr = *(pattr+1);
				pattr++;
			}
			dev_info(&ist->i2c->dev, "attribute(%s) removed", attr_name);
			return 0;
		}
		pattr++;
	}
	return 0;
}

int ist_add_device_attr(struct ist_base *ist, struct attribute * attr)
{
	int i;

	if (ist->idev) {
		dev_err(&ist->i2c->dev, "idev already inited");
		return -1;
	}

	i = 0;
	while (ist_attributes[i] != NULL) {
		if (!strncmp(ist_attributes[i]->name, attr->name, strlen(ist_attributes[i]->name))) {
			dev_info(&ist->i2c->dev, "attribute(%s) already exists at %d", attr->name, i);
			return 0;
		}
		i++;
	}
	if (i < MAX_ATTRIBUTES-1) {
		dev_info(&ist->i2c->dev, "add attributes for %s at %d", attr->name, i);
		ist_attributes[i] = attr,
		ist_attributes[i+1] = NULL;
		return 0;
	} else {
		dev_err(&ist->i2c->dev, "Try adding attribute(%s) - too many attributes max=%d", attr->name, MAX_ATTRIBUTES-1);
		return -1;
	}
}

struct ist_base *ist_alloc(struct i2c_client *client, ssize_t extra_size, const char* name)
{
	int res = 0;
	struct ist_base *ist = NULL;

	dev_info(&client->dev, "ist_alloc - %s\n", name);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c functionality check failed.\n");
		res = -ENODEV;
		goto out_alloc;
	}

	ist = devm_kzalloc(&client->dev, sizeof(struct ist_base) + extra_size,
			GFP_KERNEL);
	if (!ist) {
		dev_err(&client->dev, "memory allocation failed.\n");
		res = -ENOMEM;
		goto out_alloc;
	}

	ist->poll_delay_ms = 50; // 50ms
	ist->index = -1;
	atomic_set(&ist->enabled, 0);
	ist->on_before_suspend = 0;
	ist->intr_type = INTERRUPT_BESIDE;
	ist->intr_count = 0;
	ist->thres_count = 0;
	init_completion(&ist->report_complete);

	res = ist_parse_dt(client, ist);
	if (res) {
		dev_err(&client->dev, "Unable to parse platform data.(%d)", res);
		goto out_alloc_free;
	}

	ist->i2c = client;
	if (ist->index >= 0) {
		snprintf(ist->name, sizeof(ist->name), "%s-%d", name, ist->index);
	} else {
		strncpy(ist->name, name, sizeof(ist->name));
	}
	dev_set_drvdata(&client->dev, ist);
	i2c_set_clientdata(ist->i2c, ist);
	ist->scale_val_int = 1;
	ist->scale_val_micro = 0;

	mutex_init(&ist->lock);
	INIT_DELAYED_WORK(&ist->delayed_work, ist_irq_delayedwork_func);
	return ist;

out_alloc_free:
	if (ist)
		devm_kfree(&client->dev, (void*)ist);
out_alloc:
	return NULL;
}

int ist_common_init(struct ist_base *ist)
{
	int res = 0;

	// if use_irq, add attributes for interrupts and threshold
	if (ist->use_irq) {
		res = ist_add_device_attr(ist, &dev_attr_interrupts.attr);
		if (res < 0)
			goto out_init;
		res = ist_add_device_attr(ist, &dev_attr_thresholds.attr);
		if (res < 0)
			goto out_init;
	}

	ist->idev = ist_init_input(ist);
	if (!ist->idev) {
		dev_err(&ist->i2c->dev, "init input device failed\n");
		res = -ENODEV;
		goto out_init;
	}

	/* create sysfs group */
	res = sysfs_create_group(&ist->idev->dev.kobj, &ist_attr_group);
	if (res) {
		res = -EROFS;
		dev_err(&ist->i2c->dev, "Unable to creat sysfs group\n");
		goto out_init_free_input;
	}

	dev_info(&ist->i2c->dev, "common init ok\n");
	return 0;

out_init_free_input:
	input_unregister_device(ist->idev);
out_init:
	return -1;
}

int ist_common_uninit(struct ist_base *ist)
{
	if (ist->idev) {
		input_unregister_device(ist->idev);
	}
	hrtimer_cancel(&ist->work_timer);
	if (!IS_ERR(ist->thread)) {
		complete(&ist->report_complete);
		kthread_stop(ist->thread);
	}
	return 0;
}

int ist_remove(struct i2c_client *client)
{
	struct ist_base *ist = dev_get_drvdata(&client->dev);

	if (ist->idev)
		input_unregister_device(ist->idev);

	hrtimer_cancel(&ist->work_timer);
	if (!IS_ERR(ist->thread)) {
		complete(&ist->report_complete);
		kthread_stop(ist->thread);
	}
	return 0;
}

int ist_suspend(struct device *dev)
{
	struct ist_base *ist = dev_get_drvdata(dev);
	dev_info(dev, "suspended\n");
	ist->on_before_suspend = atomic_read(&ist->enabled);
	if (ist->on_before_suspend) {
		ist_set_enable(ist, false);
	}

	return 0;
}

int ist_resume(struct device *dev)
{
	struct ist_base *ist = dev_get_drvdata(dev);

	dev_info(dev, "resumed\n");

	if (ist->on_before_suspend) {
		ist_set_enable(ist, true);
	}

	return 0;
}
