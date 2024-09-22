#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/workqueue.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>

#include "gc9a01_i2c.h"

#define DEVICE_NAME  "gc9a01_iic"

#ifndef GC9A01_SYS_TEST
#define GC9A01_SYS_TEST
#endif

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

static int gc9a01_i2c_is_probe_ok = false;
struct gc9a01_data * g_gc9a01_data = NULL;

static const struct of_device_id gc9a01_match_table[] = {
    {.compatible = "mediatek,gc9a01_touch",},
    { },
};

/*Define gc9a01 iic read function*/
int gc9a01_read(struct i2c_client *client, unsigned char reg, unsigned char *data)
{
	int ret = 0;
	//msleep(10);	
	ret = i2c_smbus_read_i2c_block_data(client, reg, 1, data);
	printk("gc9a01 id %2x = %2x\n", reg, data[0]);
	return ret;
}

int gc9a01_read_block(struct i2c_client *client, unsigned char reg, unsigned char *data,int len)
{
	int ret = 0;
	//msleep(10);	
	ret = i2c_smbus_read_i2c_block_data(client, reg, len, data);
	//cw_printk(0,"%2x = %2x\n", reg, buf[0]);
	return ret;
}

/*Define gc9a01 iic write function*/		
int gc9a01_write(struct i2c_client *client, unsigned char reg, unsigned char *buf)
{
	int ret = 0;
	//msleep(10);	
	ret = i2c_smbus_write_i2c_block_data(client, reg, 1, buf);
	//cw_printk(0,"%2x = %2x\n", reg, buf[0]);
	return ret;
}


void gc9a01_irq_disable(struct gc9a01_data * gc_data)
{
    unsigned long irqflags;

    spin_lock_irqsave(&gc_data->irq_lock, irqflags);
	
	pr_err("------%s--------irq_disabled = %d\n",__func__,gc_data->irq_disabled);

    if (!gc_data->irq_disabled) {
        disable_irq_nosync(gc_data->tp_irq);
        gc_data->irq_disabled = true;
    }

    spin_unlock_irqrestore(&gc_data->irq_lock, irqflags);
    
}

void gc9a01_irq_enable(struct gc9a01_data * gc_data)
{
    unsigned long irqflags = 0;
	pr_err("------%s--------irq_disabled = %d\n",__func__,gc_data->irq_disabled);
    spin_lock_irqsave(&gc_data->irq_lock, irqflags);

    if (gc_data->irq_disabled) {
        enable_irq(gc_data->tp_irq);
        gc_data->irq_disabled = false;
    }
    spin_unlock_irqrestore(&gc_data->irq_lock, irqflags);

}


static void gc9a01_reset(struct gc9a01_data * gc_data)
{
	/*
	if(value){
		gpio_direction_output(gc9a01_tp_reset,1);
	}
	else{
		gpio_direction_output(gc9a01_tp_reset,0);
	}
	*/
	if(!gc_data){
		pr_err("gc_data was null,return...\n");
	}
	gpio_direction_output(gc_data->gc9a01_tp_reset,0);
	msleep(10);
	gpio_direction_output(gc_data->gc9a01_tp_reset,1);
	msleep(50);	
}


 
static int gc9a01_get_gpio(struct device *dev,struct gc9a01_data * gc_data)
{
	int ret = 0;
	const struct of_device_id *match;
	
	if (dev->of_node){
		match = of_match_device(of_match_ptr(gc9a01_match_table), dev);
		if (!match) {
			printk("Error: No device match found\n");
			return -ENODEV;
		}
	}

	gc_data->gc9a01_tp_reset = of_get_named_gpio(dev->of_node, "reset_gpio", 0);
	
	printk("%s------[gezi] gc9a01_tp_reset =  %d\n", __func__, gc_data->gc9a01_tp_reset);
	if(gc_data->gc9a01_tp_reset != 0) {
		ret = gpio_request(gc_data->gc9a01_tp_reset, "gc9a01_tp_reset");
		if (ret) 
			printk("%s------[gezi] gpio request gc9a01_tp_reset = 0x%x fail with %d\n", __func__, gc_data->gc9a01_tp_reset, ret);
	}
	
	gc_data->gc9a01_tp_int = of_get_named_gpio(dev->of_node, "irq_gpio", 0);
	
	printk("%s------[gezi] gc9a01_tp_int =  %d\n", __func__, gc_data->gc9a01_tp_int);
	if(gc_data->gc9a01_tp_int != 0) {
		ret = gpio_request(gc_data->gc9a01_tp_int, "gc9a01_tp_int");
		if (ret) 
			printk("%s------[gezi] gpio request gc9a01_tp_int = 0x%x fail with %d\n", __func__,gc_data->gc9a01_tp_int, ret);
	}
/*	
	gpio_direction_output(gc9a01_tp_reset,0);
	msleep(10);
	gpio_direction_output(gc9a01_tp_reset,1);
	msleep(50);	
*/
	//gpio_direction_output(gc9a01_tp_reset,0);
	return ret;
}


static irqreturn_t cst3xx_ts_irq_handler(int irq, void *data)
{

    //printk("-------enter cst3xx_ts_irq_handler-------%d-----\n",__gpio_get_value(gc9a01_tp_int));
	struct gc9a01_data * gc_data = data;

	//disable_irq_nosync(gc_data->tp_irq);//use in interrupt,disable_irq will make dead lock.

	gc_data->tpd_flag = 1;
	wake_up_interruptible(&gc_data->waiter);

	return IRQ_HANDLED;
}

static int gc9a01_input_init(struct gc9a01_data * gc_data)
{
	int ret = 0;
	//struct input_dev *gc9a01_dev = gc_data->gc_dev;
	
	gc_data->gc9a01_dev = input_allocate_device();
	if (gc_data->gc9a01_dev == NULL) {
		pr_err("Failed to allocate input device for gc9a01_dev\n");
		return -1;
	}
	
	gc_data->gc9a01_dev->name = "gc9a01_touch";
	//gc_data->gc9a01_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	//gc_data->gc9a01_dev->keybit[BIT_WORD(BTN_TOOL_FINGER)] = BIT_MASK(BTN_TOOL_FINGER);
	
	__set_bit(EV_REL,gc_data->gc9a01_dev->evbit);
    __set_bit(REL_X, gc_data->gc9a01_dev->relbit);
    __set_bit(REL_Y, gc_data->gc9a01_dev->relbit);
    //__set_bit(REL_Z, gc_data->gc9a01_dev->relbit);
	
    __set_bit(EV_SYN, gc_data->gc9a01_dev->evbit);
    __set_bit(EV_ABS, gc_data->gc9a01_dev->evbit);
    __set_bit(EV_KEY, gc_data->gc9a01_dev->evbit);
    __set_bit(BTN_TOOL_FINGER,gc_data->gc9a01_dev->keybit);
    __set_bit(INPUT_PROP_BUTTONPAD,gc_data->gc9a01_dev->propbit);

	input_set_abs_params(gc_data->gc9a01_dev, ABS_MT_POSITION_X, 0, 240, 0, 0);
	input_set_abs_params(gc_data->gc9a01_dev, ABS_MT_POSITION_Y, 0, 240, 0, 0);
	
	//input_set_abs_params(gc_data->gc9a01_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
	
	//input_set_abs_params(gc_data->gc9a01_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
	//input_set_abs_params(gc_data->gc9a01_dev, ABS_MT_TRACKING_ID, 0, 0x0F, 0, 0);
/*
	input_set_abs_params(gc9a01_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(gc9a01_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);
*/

	ret = input_register_device(gc_data->gc9a01_dev);
	if (ret) {
		pr_err("Register %s input device failed", gc_data->gc9a01_dev->name);
		return -1;
	}
	
	return 0;
}

static int gc9a01_eint_setup(struct gc9a01_data * gc_data)
{
	int ret;
	//struct device_node *node;

	//usb_eint_type = IRQ_TYPE_EDGE_RISING;
	
	gpio_direction_input(gc_data->gc9a01_tp_int);	
	
	gc_data->tp_irq = gpio_to_irq(gc_data->gc9a01_tp_int);

	pr_err("tp_irq=%d", gc_data->tp_irq);
	
	ret = request_irq(gc_data->tp_irq, cst3xx_ts_irq_handler,IRQF_TRIGGER_FALLING, "gc9a01_tp_eint_func", gc_data);
	//ret = request_irq(charge_state_irq, charge_state_eint_func,IRQ_TYPE_LEVEL_LOW, "battery_exist_eint_default", NULL);	
	if (ret > 0){
		pr_err("usb EINT IRQ LINE NOT AVAILABLE\n");
	}
	else {
		pr_err("usb eint set EINT finished, usb_irq=%d\n",gc_data->tp_irq);
	}		
	
	enable_irq_wake(gc_data->tp_irq);	

	return ret;
}

static int gc9a01_get_chip_id(struct i2c_client *client)
{
	unsigned char chip_id = 0;

	gc9a01_read(client,FTS_REG_CHIP_ID,&chip_id);
	
	printk("[Touch sensor(gc9a01_get_chip_id) get chip id succ] = %x\n", chip_id);
	
	if(chip_id == 0xB5) {
		return 0;
	}
	
	return -1;
	
}

static void gc9a01_report(unsigned short x,unsigned short y,unsigned char mode,unsigned char finger_num)
{
	static unsigned char pressed = 0;
	//static unsigned int tracking_id = 0;
	
	if(unlikely(!g_gc9a01_data)){
		return;
	}
	//if(finger_num){
	//	input_mt_report_slot_state(g_gc9a01_data->gc9a01_dev, MT_TOOL_FINGER, true);
	//}
	//input_report_abs(g_gc9a01_data->gc9a01_dev, ABS_MT_TOUCH_MAJOR, mode);
		//input_report_abs(g_gc9a01_data->gc9a01_dev, ABS_MT_PRESSURE,mode);
	input_report_abs(g_gc9a01_data->gc9a01_dev, ABS_MT_POSITION_X, x);
	input_report_abs(g_gc9a01_data->gc9a01_dev, ABS_MT_POSITION_Y, y);

	if((!pressed) && finger_num){
		//tracking_id++;
		//input_mt_report_slot_state(g_gc9a01_data->gc9a01_dev, MT_TOOL_FINGER, true);
		//input_report_abs(g_gc9a01_data->gc9a01_dev, ABS_MT_TOUCH_MAJOR, mode);
		//input_report_abs(g_gc9a01_data->gc9a01_dev, ABS_MT_TRACKING_ID, tracking_id);
		input_report_key(g_gc9a01_data->gc9a01_dev, BTN_TOOL_FINGER, 1);
		pressed = 1;
	}
	


	
	if(!finger_num){
		//input_mt_report_slot_state(g_gc9a01_data->gc9a01_dev, MT_TOOL_FINGER, false);
		//input_report_abs(g_gc9a01_data->gc9a01_dev, ABS_MT_TRACKING_ID, 0xffffffff);
		input_report_key(g_gc9a01_data->gc9a01_dev, BTN_TOOL_FINGER, 0);
		pressed = 0;
	}
	
    input_sync(g_gc9a01_data->gc9a01_dev);
	
}

static int touch_event_handler(void *arg)
{
	struct gc9a01_data *gc_data = arg;
	int ret = 0;
	unsigned short pdwSampleX, pdwSampleY;
	unsigned char tp_temp[10];
	unsigned char finger_num;
	do{
		wait_event_interruptible(gc_data->waiter, gc_data->tpd_flag != 0);
		gc_data->tpd_flag = 0;
		mutex_lock(&gc_data->i2c_access);
		
		//pr_err("gezi---------------%s----------%d\n",__func__,__LINE__);
		
		ret = gc9a01_read_block(gc_data->client, FTS_REG_START,tp_temp, 7);
		
		//for(i = 0; i < 7; i++){
		//	
		//}
		if(ret < 0){
			pr_err("gezi-------- err-------%s----------%d\n",__func__,__LINE__);
			goto exit_unlock;
		}
		finger_num = tp_temp[2]; //手指个数
		pdwSampleX = ((tp_temp[3] & 0x0F) << 8) + tp_temp[4];
		pdwSampleY = ((tp_temp[5] & 0x0F) << 8) + tp_temp[6];
		
	//	pr_err("reg[0x03]= %x,reg[0x05] = %x\n",tp_temp[3],tp_temp[5]);

		pr_err("mode = %d,finger_num = %d,x = %x,y = %x\n",tp_temp[1],finger_num,pdwSampleX,pdwSampleY);
		
		gc9a01_report(pdwSampleX,pdwSampleY,tp_temp[1],finger_num);
		
		//if(tp_temp[0] = 0x00) //扱点模式.要求FAE將 手勢碍清零.很多吋候手勢碍没有被清除

exit_unlock:
		//enable_irq(gc_data->tp_irq);
		mutex_unlock(&gc_data->i2c_access);
		
	} while (!kthread_should_stop());

	return ret;
}

static void gc9a01_suspend(void)
{
	int ret = 0;
	unsigned char enterSleep = 0x03;

	if(unlikely(!g_gc9a01_data)){
		return;
	}
	
	gc9a01_reset(g_gc9a01_data);
    ret = gc9a01_write(g_gc9a01_data->client,FTS_REG_LOW_POWER,&enterSleep);
	if(ret < 0){
		pr_err("gc9a01 enter suspend failed\n");
	}
	gc9a01_irq_disable(g_gc9a01_data);
	pr_err("gc9a01 enter suspend \n");
	
}

static void gc9a01_resume(void)
{
	//int ret = 0;
	//unsigned char QuitSleep = 0x01;
	
	if(unlikely(!g_gc9a01_data)){
		return;
	}
	
	gc9a01_reset(g_gc9a01_data);
/*	
    ret = gc9a01_write(g_gc9a01_data->client,0xfe,&QuitSleep);
	if(ret < 0){
		pr_err("gc9a01 quit suspend failed\n");
	}
*/
	gc9a01_irq_enable(g_gc9a01_data);
	pr_err("gc9a01 enter resume \n");
}


void gc9a01_tp_power_ctrl(int value)
{
	if (gc9a01_i2c_is_probe_ok) {
		if(value){
			gc9a01_resume();
		}else{
			gc9a01_suspend();
		}
	} else {
		pr_err("gc9a01 i2c probe err\n");
	}
}
EXPORT_SYMBOL(gc9a01_tp_power_ctrl);

#ifdef GC9A01_SYS_TEST
static struct class * gc9a01_class;

static ssize_t gc9a01_test_store(struct class *class, struct class_attribute *attr,	const char *buf, size_t count)
{
	if(buf[0] == '0')
	{
		gc9a01_suspend();
	}
	else if(buf[0] == '1')
	{
		gc9a01_resume();
	}
	return count;
}

static struct class_attribute gc9a01_class_attrs[] = {
	__ATTR(test, S_IRUGO | S_IWUSR, NULL, gc9a01_test_store),
	__ATTR_NULL,
};


static int gc9a01_sysfs_create(void)
{
	int i = 0,ret = 0;
	
	gc9a01_class = class_create(THIS_MODULE, "gc9a01_tp");
	if (IS_ERR(gc9a01_class))
		return PTR_ERR(gc9a01_class);
	for (i = 0; gc9a01_class_attrs[i].attr.name; i++) {
		ret = class_create_file(gc9a01_class,&gc9a01_class_attrs[i]);
		if (ret < 0)
		{
			pr_err("gc9a01_sysfs_create error !!\n");
			return ret;
		}
	}
	return ret;
	//gc9a01_class->dev_groups = rt5509_cal_groups;
}
#endif

static int gc9a01_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct task_struct *thread;
	struct gc9a01_data * gc_data = NULL;
	int err = 0;
	
	pr_err("gezi-------------%s-----------------%d\n",__func__,__LINE__);
	
	gc_data = devm_kzalloc(&client->dev, sizeof(*gc_data), GFP_KERNEL);
	if (!gc_data){
		pr_err("gezi------ENOMEM-------%s-----------------%d\n",__func__,__LINE__);
		return -ENOMEM;
	}
	
	gc9a01_get_gpio(&client->dev,gc_data);
	init_waitqueue_head(&gc_data->waiter);
	mutex_init(&gc_data->i2c_access);
	spin_lock_init(&gc_data->irq_lock);
	gc_data->client = client;
	gc9a01_reset(gc_data);
	g_gc9a01_data = gc_data;
/*
	gc_data->vdd = devm_regulator_get(&client->dev, "vdd");
	if (IS_ERR_OR_NULL(gc_data->vdd))
		dev_err(&client->dev, "get regulator fail %d\n",PTR_ERR(gc_data->vdd));
	else
		regulator_enable(gc_data->vdd);
*/
	msleep(150);
	
	err = gc9a01_get_chip_id(client);
	
	if(err < 0){
		pr_err("gezi---gc9a01 get sensor id failed--%s---%d\n",__func__,__LINE__);
		return -1;
	}
	
	gc9a01_input_init(gc_data);
	
	thread = kthread_run(touch_event_handler, gc_data, "gc9a01_thread");
	
	
	if (IS_ERR(thread)) {
		err = PTR_ERR(thread);
		pr_err(" failed to create kernel thread: %d\n",err);
	}
	
	gc9a01_eint_setup(gc_data);
#ifdef GC9A01_SYS_TEST	
	gc9a01_sysfs_create();
#endif	
	gc9a01_i2c_is_probe_ok = true;
	return 0;
}

static int gc9a01_remove(struct i2c_client *client) 
{
	return 0;
}



static const struct i2c_device_id gc9a01_dev_id[] = {
    {"gc9a01_touch", 0},
    {},
};
MODULE_DEVICE_TABLE(i2c,gc9a01_dev_id);

static struct i2c_driver gc9a01_driver = {
    .driver   = {
        .name           = DEVICE_NAME,
        .owner          = THIS_MODULE,
        .of_match_table = gc9a01_match_table,
    },
    .probe    = gc9a01_probe,
    .remove   = gc9a01_remove,
    .id_table = gc9a01_dev_id,
};
//module_i2c_driver(gc9a01_driver);
static int __init gc9a01_touch_init(void)
{
	int ret = 0;
	pr_err("gezi--------%s---\n",__func__);
	
	ret = i2c_add_driver(&gc9a01_driver);
	
	return ret;
}
static void __exit gc9a01_touch_exit(void)
{
	i2c_del_driver(&gc9a01_driver);
}
late_initcall_sync(gc9a01_touch_init);
module_exit(gc9a01_touch_exit);

MODULE_AUTHOR("zhaopengge@cooseagroup.com");
MODULE_DESCRIPTION("GC9A01 TP DRIVER");
MODULE_LICENSE("GPL v2");

