/*
 * Copyright (C) 2015 MediaTek Inc.
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


#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/ctype.h>

#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/input.h>
#include <linux/time.h>

#include <linux/sysfs.h>


//camera_enable1  拉高控制3.3V

//camera_enable2 拉低

//camera_enable3 拉高

#define SERIAL_CAMERA_DEVNAME "serial_camera_dev"


static int serial_camera_remove(struct platform_device *dev);
static int serial_camera_probe(struct platform_device *pdev);
static void serial_camera_shutdown(struct platform_device *dev);
//void serial_camera_rfid_pwr(u8 enable);

static const struct of_device_id serial_camera_of_match[] = {
	{.compatible = "mediatek,serial_camera"},
	{},
};
MODULE_DEVICE_TABLE(of, serial_camera_of_match);

//prize add by lipengpeng 20210330 start 
static int air_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	printk("lpp serial_camera enter suspend\n");
	//serial_camera_enter_suspend(0);
        return 0;
}

static int air_resume(struct platform_device *pdev)
{
	printk("lpp serial_camera enter resume\n");
	//serial_camera_enter_resume(0);
        return 0;
}
//prize add by lipengpeng 20210330 end

static struct platform_driver serial_camera_platform_driver = {
	.probe = serial_camera_probe,
	.remove = serial_camera_remove,
	.shutdown = serial_camera_shutdown,
//prize add by lipengpeng 20210330 start 
	.suspend = air_suspend,
    .resume = air_resume,
//prize add by lipengpeng 20210330 end 
	.driver = {
		   .name = SERIAL_CAMERA_DEVNAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = serial_camera_of_match,
#endif
	},
};

/*----------------------------------------------------------------------------*/

#ifdef CONFIG_PINCTRL
static struct pinctrl *serial_camera_gpio;
static struct pinctrl_state *serial_camera_enable1_gpio0;
static struct pinctrl_state *serial_camera_enable1_gpio1;
static struct pinctrl_state *serial_camera_enable2_gpio0;
static struct pinctrl_state *serial_camera_enable2_gpio1;
static struct pinctrl_state *serial_camera_enable3_gpio0;
static struct pinctrl_state *serial_camera_enable3_gpio1;
static struct pinctrl_state *serial_camera_rst_gpio0;
static struct pinctrl_state *serial_camera_rst_gpio1;
#if 1
static unsigned int gpio_enable1_status;
static unsigned int gpio_enable2_status;
static unsigned int gpio_enable3_status;
static unsigned int gpio_rst_status;
#endif
#endif

static int serial_camera_gpio_init(struct device	*dev)
{    
	int ret=0;
//	unsigned int mode;
	//const struct of_device_id *match;

	pr_debug("[serial_camera][GPIO] enter %s, %d\n", __func__, __LINE__);

	serial_camera_gpio = devm_pinctrl_get(dev);
	if (IS_ERR(serial_camera_gpio)) {
		ret = PTR_ERR(serial_camera_gpio);
		pr_info("[serial_camera][ERROR] Cannot find serial_camera_gpio!\n");
		return ret;
	}
//enable1
	serial_camera_enable1_gpio0 = pinctrl_lookup_state(serial_camera_gpio, "serial_camera_enable1_gpio0");
	if (IS_ERR(serial_camera_enable1_gpio0)) {
		ret = PTR_ERR(serial_camera_enable1_gpio0);
		pr_info("[serial_camera][ERROR] Cannot find serial_camera_enable1_gpio0 %d!\n",
			ret);
	}
	serial_camera_enable1_gpio1= pinctrl_lookup_state(serial_camera_gpio, "serial_camera_enable1_gpio1");
	if (IS_ERR(serial_camera_enable1_gpio1)) {
		ret = PTR_ERR(serial_camera_enable1_gpio1);
		pr_info("[serial_camera][ERROR] Cannot find serial_camera_enable1_gpio1 %d!\n",
			ret);
	}
//enable2	
	serial_camera_enable2_gpio0 = pinctrl_lookup_state(serial_camera_gpio, "serial_camera_enable2_gpio0");
	if (IS_ERR(serial_camera_enable2_gpio0)) {
		ret = PTR_ERR(serial_camera_enable2_gpio0);
		pr_info("[serial_camera][ERROR] Cannot find serial_camera_enable2_gpio0 %d!\n",
			ret);
	}
	serial_camera_enable2_gpio1= pinctrl_lookup_state(serial_camera_gpio, "serial_camera_enable2_gpio1");
	if (IS_ERR(serial_camera_enable2_gpio1)) {
		ret = PTR_ERR(serial_camera_enable2_gpio1);
		pr_info("[serial_camera][ERROR] Cannot find serial_camera_enable2_gpio1 %d!\n",
			ret);
	}
//enable3	
 	serial_camera_enable3_gpio0 = pinctrl_lookup_state(serial_camera_gpio, "serial_camera_enable3_gpio0");
	if (IS_ERR(serial_camera_enable3_gpio0)) {
		ret = PTR_ERR(serial_camera_enable3_gpio0);
		pr_info("[serial_camera][ERROR] Cannot find serial_camera_enable3_gpio0 %d!\n",
			ret);
	}
	serial_camera_enable3_gpio1= pinctrl_lookup_state(serial_camera_gpio, "serial_camera_enable3_gpio1");
	if (IS_ERR(serial_camera_enable3_gpio1)) {
		ret = PTR_ERR(serial_camera_enable3_gpio1);
		pr_info("[serial_camera][ERROR] Cannot find serial_camera_enable3_gpio1 %d!\n",
			ret);
	}
//rst	
	serial_camera_rst_gpio0 = pinctrl_lookup_state(serial_camera_gpio, "serial_camera_rst_gpio0");
	if (IS_ERR(serial_camera_rst_gpio0)) {
		ret = PTR_ERR(serial_camera_rst_gpio0);
		pr_info("[serial_camera][ERROR] Cannot find serial_camera_rst_gpio0 %d!\n",
			ret);
	}
	serial_camera_rst_gpio1= pinctrl_lookup_state(serial_camera_gpio, "serial_camera_rst_gpio1");
	if (IS_ERR(serial_camera_rst_gpio1)) {
		ret = PTR_ERR(serial_camera_rst_gpio1);
		pr_info("[serial_camera][ERROR] Cannot find serial_camera_rst_gpio1 %d!\n",
			ret);
	}
	
	gpio_enable1_status =of_get_named_gpio(dev->of_node, "gpio_enable1_status", 0);
	gpio_enable2_status =of_get_named_gpio(dev->of_node, "gpio_enable2_status", 0);
	gpio_enable3_status =of_get_named_gpio(dev->of_node, "gpio_enable3_status", 0);
	gpio_rst_status =of_get_named_gpio(dev->of_node, "gpio_rst_status", 0);


	printk("[serial_camera][GPIO] serial_camera_gpio_get_info end!\n");

    return ret;

}
//prize add by lipengpeng 20220302 start 
void serial_camera_enable3(u8 enable)
{
    pr_debug("%s enable =%d\n", __func__);

    if(enable)
        pinctrl_select_state(serial_camera_gpio,serial_camera_enable3_gpio1);
    else
        pinctrl_select_state(serial_camera_gpio,serial_camera_enable3_gpio0);
        
}
//prize add by lipengpeng 20220302 start 
int get_serial_camera_enable3_status(void)
{

   int enable3_status = 0;
   return enable3_status = __gpio_get_value(gpio_enable3_status);
   printk("%s enable3_status =%d\n", enable3_status); 
   return enable3_status;
}
//prize add by lipengpeng 20220302 start 
void serial_camera_enable2(u8 enable)
{
    pr_debug("%s enable =%d\n", __func__);

    if(enable)
        pinctrl_select_state(serial_camera_gpio,serial_camera_enable2_gpio1);
    else
        pinctrl_select_state(serial_camera_gpio,serial_camera_enable2_gpio0);
        
}
//prize add by lipengpeng 20220302 start 
int get_serial_camera_enable2_status(void)
{

   int enable2_status = 0;
   return enable2_status = __gpio_get_value(gpio_enable2_status);
   printk("%s enable2_status =%d\n", enable2_status); 
   return enable2_status;
}
//prize add by lipengpeng 20220302 start 
void serial_camera_enable1(u8 enable)
{
    pr_debug("%s enable =%d\n", __func__);

    if(enable)
        pinctrl_select_state(serial_camera_gpio,serial_camera_enable1_gpio1);
    else
        pinctrl_select_state(serial_camera_gpio,serial_camera_enable1_gpio0);

}
//prize add by lipengpeng 20220302 start 
int get_serial_camera_enable1_status(void)
{

   int enable1_status = 0;
   return enable1_status = __gpio_get_value(gpio_enable1_status);
   printk("%s enable1_status =%d\n", enable1_status); 
   return enable1_status;
}
//prize add by lipengpeng 20220302 start 
void serial_camera_rst(u8 enable)
{
    pr_debug("%s enable =%d\n", __func__);

    if(enable)
        pinctrl_select_state(serial_camera_gpio,serial_camera_rst_gpio1);
    else
        pinctrl_select_state(serial_camera_gpio,serial_camera_rst_gpio0);

} 

/***************************************
node:/sys/bus/platform/drivers/serial_camera_dev/serial_camera_enable3
1:enable
0:disable
***********************************/
static ssize_t serial_camera_enable3_show(struct device_driver *ddri, char *buf)
{
	int serial_camera_enable3_value = 0;
    ssize_t res;


    serial_camera_enable3_value = __gpio_get_value(gpio_enable3_status);
    
	res = snprintf(buf, PAGE_SIZE, "serial_camera_enable3_value = %d\n",serial_camera_enable3_value);

	return res;
}

static ssize_t serial_camera_enable3_store(struct device_driver *ddri,
				      const char *buf, size_t tCount)
{

    int serial_camera_enable3_flag;
	int ret = 0;

	if (strlen(buf) < 1) {
		pr_notice("%s() Invalid input!!\n", __func__);
		return -EINVAL;
	}

    ret = sscanf(buf, "%d", &serial_camera_enable3_flag);


	if (serial_camera_enable3_flag == 1){
		serial_camera_enable3(1);
     }
	else{
		serial_camera_enable3(0);
    }
   
	return tCount;
}  
/***************************************
node:/sys/bus/platform/drivers/serial_camera_dev/serial_camera_enable2
1:enable
0:disable
***********************************/                      
static ssize_t serial_camera_enable2_show(struct device_driver *ddri, char *buf)
{
  int serial_camera_enable2_value = 0;
  ssize_t res;

  serial_camera_enable2_value = __gpio_get_value(gpio_enable2_status);

  res = snprintf(buf, PAGE_SIZE, "serial_camera_enable2_value = %d\n",serial_camera_enable2_value);

  return res;
}

static ssize_t serial_camera_enable2_store(struct device_driver *ddri,
                    const char *buf, size_t tCount)
{
  int serial_camera_enable2_flag;
  int ret = 0;

  if (strlen(buf) < 1) {
      pr_notice("%s() Invalid input!!\n", __func__);
      return -EINVAL;
  }

  ret = sscanf(buf, "%d", &serial_camera_enable2_flag);


  if (serial_camera_enable2_flag == 1){
      serial_camera_enable2(1);

   }else{
      serial_camera_enable2(0);
  }
 
  return tCount;
}   
/***************************************
node:/sys/bus/platform/drivers/serial_camera_dev/serial_camera_enable1
1:enable
0:disable
***********************************/ 
static ssize_t serial_camera_enable1_show(struct device_driver *ddri, char *buf)
{
  int serial_camera_enable1_value = 0;
  ssize_t res;

  serial_camera_enable1_value = __gpio_get_value(gpio_enable1_status);

  res = snprintf(buf, PAGE_SIZE, "serial_camera_enable1_value = %d\n",serial_camera_enable1_value);

  return res;
}

static ssize_t serial_camera_enable1_store(struct device_driver *ddri,
                    const char *buf, size_t tCount)
{

  int serial_camera_enable1_flag;
  int ret = 0;

  if (strlen(buf) < 1) {
      pr_notice("%s() Invalid input!!\n", __func__);
      return -EINVAL;
  }

  ret = sscanf(buf, "%d", &serial_camera_enable1_flag);


  if (serial_camera_enable1_flag == 1){
      serial_camera_enable1(1);

   }else{
      serial_camera_enable1(0);
  }
 
  return tCount;
}   

/***************************************
node:/sys/bus/platform/drivers/serial_camera_dev/serial_camera_rst
1:enable
0:disable
***********************************/ 
static ssize_t serial_camera_rst_show(struct device_driver *ddri, char *buf)
{
  int serial_camera_rst_value = 0;
  ssize_t res;

  serial_camera_rst_value = __gpio_get_value(gpio_rst_status);

  res = snprintf(buf, PAGE_SIZE, "serial_camera_rst_value = %d\n",serial_camera_rst_value);

  return res;
}

static ssize_t serial_camera_rst_store(struct device_driver *ddri,
                    const char *buf, size_t tCount)
{

  int serial_camera_rst_flag;
  int ret = 0;

  if (strlen(buf) < 1) {
      pr_notice("%s() Invalid input!!\n", __func__);
      return -EINVAL;
  }

  ret = sscanf(buf, "%d", &serial_camera_rst_flag);


  if (serial_camera_rst_flag == 1){
      serial_camera_rst(1);

   }else{
      serial_camera_rst(0);
  }
 
  return tCount;
} 


/*----------------------------------------------------------------------------*/
#if 0                   
static DRIVER_ATTR(serial_camera_enable1, 0644, serial_camera_3V_funen_show,serial_camera_3V_funen_store);
static DRIVER_ATTR(serial_camera_enable2, 0644, serial_camera_5V_boost_show,serial_camera_5V_boost_store);
static DRIVER_ATTR(serial_camera_enable3, 0644, serial_camera_rfid_tof_show,serial_camera_tof_pwr_store);
#else
static DRIVER_ATTR_RW(serial_camera_enable1);
static DRIVER_ATTR_RW(serial_camera_enable2);
static DRIVER_ATTR_RW(serial_camera_enable3);
static DRIVER_ATTR_RW(serial_camera_rst);

#endif

/*----------------------------------------------------------------------------*/
static struct driver_attribute *serial_camera_attr_list[] = {    
	&driver_attr_serial_camera_enable1,
	&driver_attr_serial_camera_enable2, 
	&driver_attr_serial_camera_enable3,
	&driver_attr_serial_camera_rst,
};


/*----------------------------------------------------------------------------*/
static int serial_camera_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(serial_camera_attr_list);

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, serial_camera_attr_list[idx]);
		if (err) {
			pr_err("driver_create_file (%s) = %d\n",
				   serial_camera_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int serial_camera_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(serial_camera_attr_list);

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, serial_camera_attr_list[idx]);

	return err;
}

static int serial_camera_probe(struct platform_device *pdev)
{

    const struct of_device_id *id;
	struct device	*dev = &pdev->dev;
    int err =0 ;
  	printk("serial_camera_probe start\n");
        
	id = of_match_node(serial_camera_of_match, pdev->dev.of_node);
	if (!id)
		return -ENODEV;
    serial_camera_gpio_init(dev);

    /* Register sysfs attribute */
	err = serial_camera_create_attr(&serial_camera_platform_driver.driver);
	if (err) {
		pr_err("create attribute err = %d\n", err);
		goto exit_sysfs_create_group_failed;
	}
    
    //serial_cameraprt_enable(1);

	printk("serial_camera_probe done\n");

	return 0;
exit_sysfs_create_group_failed:
    return -1;

}

static int serial_camera_remove(struct platform_device *dev)
{
    int err = 0;
	err = serial_camera_delete_attr(&serial_camera_platform_driver.driver);
	if (err)
		pr_err("serial_camera_delete_attr fail: %d\n", err);

	return err;
}

static void serial_camera_shutdown(struct platform_device *dev)
{

}


static int __init serial_camera_init(void)
{
	int ret;

	pr_debug("Init start\n");    

	ret = platform_driver_register(&serial_camera_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}

	pr_debug("Init done\n");

	return 0;
}

static void __exit serial_camera_exit(void)
{
	pr_debug("Exit start\n");

	platform_driver_unregister(&serial_camera_platform_driver);

	pr_debug("Exit done\n");
}

module_init(serial_camera_init);
module_exit(serial_camera_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("pengpeng li <lipengpeng@szprize.com>");
MODULE_DESCRIPTION("MTK serial_camera Core Driver");

