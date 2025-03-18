#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
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
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/kthread.h>
#include <linux/input.h>
#if defined(CONFIG_PM_WAKELOCKS)
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif
#include <linux/time.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#define ftm_node_DEVNAME    "ftm_node_dev"

static int ftm_finger_exist = 0;
static ssize_t finger_show(struct device *dev,struct device_attribute*attr, char *buf)
{
    int len = 0;
    len += sprintf((char*)buf, "%d\n", ftm_finger_exist);
	printk("[%d] =len !!!\n",len);
    return len;
}
 
static DEVICE_ATTR(exist, S_IRUGO|S_IWUSR, finger_show, NULL);


static const struct attribute *ftm_node_event_attr[] = {
		&dev_attr_exist.attr,

        NULL,
};
/*************************************************************************************************************************************************/
static const struct attribute_group ftm_node_event_attr_group = {
        .attrs = (struct attribute **) ftm_node_event_attr,
};

void pri_ftm_node_exist(int exist)
{
	if(exist){
		ftm_finger_exist = 1;
    printk(KERN_ERR "[%d] =ftm_finger_exist !!!\n",ftm_finger_exist);}
	else{
		ftm_finger_exist = 0;
		printk(KERN_ERR "[%d] =ftm_finger_exist !!!\n",ftm_finger_exist);}
}
EXPORT_SYMBOL_GPL(pri_ftm_node_exist);


static int ftm_node_probe(struct platform_device *pdev){

    int ret = 0;

	ret = sysfs_create_group(&pdev->dev.kobj, &ftm_node_event_attr_group);
	if(ret < 0) {
		printk(KERN_ERR "ftm_node:sysfs_create_group fail\r\n");
		return ret;
	}

	printk("[%s] ok!!!\n",__func__);
    return 0;
}

static int ftm_node_remove(struct platform_device *pdev){
	printk("[ftm_node_dev]:ftm_node_remove begin!\n");
	printk("[ftm_node_dev]:ftm_node_remove Done!\n");
    
	return 0;
}

const struct of_device_id ftm_node_of_match[] = {
	{ .compatible = "coosea,ftm_node", },
	{},
};

struct platform_device ftm_node_device = {
	.name		= ftm_node_DEVNAME,
	.id			= -1,
};

static struct platform_driver ftm_node_driver = {
	.remove = ftm_node_remove,
	.probe = ftm_node_probe,
	.driver = {
			.name = ftm_node_DEVNAME,
			.owner = THIS_MODULE,
			.of_match_table = ftm_node_of_match,
	},
};

static int __init ftm_node_init(void)
{
	int ret = 0;
	
	printk("COOSEA ftm_node ftm_node_init\n");
	
	ret = platform_driver_register(&ftm_node_driver);
	if (ret < 0)
		printk("ftm_node : ftm_node_init failed ret:%d\n", ret);
	return 0;
}

static void __exit ftm_node_exit(void)
{
	printk("COOSEA ftm_node ftm_node_exit \n");
	platform_driver_unregister(&ftm_node_driver);

}

module_init(ftm_node_init);
module_exit(ftm_node_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("COOSEA ftm_node driver");
MODULE_AUTHOR("Liao Jie<liaojie@cooseagroup.com>");