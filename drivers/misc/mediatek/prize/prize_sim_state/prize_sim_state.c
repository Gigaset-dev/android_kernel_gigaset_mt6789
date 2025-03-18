#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

static unsigned int GPIO_SIM_STATE;

int get_lk_boot_mode(void)
{
	struct device_node *np_chosen;
	struct tag_bootmode *tag = NULL;

    np_chosen = of_find_node_by_path("/chosen");
    if (!np_chosen)
        np_chosen = of_find_node_by_path("/chosen@0");

    tag = (struct tag_bootmode *)of_get_property(np_chosen, "atag,boot", NULL);
    if (!tag) {
        printk("%s: fail to get atag,boot\n", __func__);
        return -1;
    } else {
        printk("%s---bootmode: 0x%x\n",__func__,tag->bootmode);
        return tag->bootmode;
    }

	return 0;
}

/*Prize: add for X9LAVA-1524 begin*/
static ssize_t cd_gpio_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int cd_gpio_state_sim1;

	cd_gpio_state_sim1 = gpio_get_value(GPIO_SIM_STATE);
	return sprintf(buf, "%d\n", cd_gpio_state_sim1);
}

static DEVICE_ATTR(cd_gpio, S_IRUGO, cd_gpio_show, NULL);
/*Prize: add for X9LAVA-1524 end*/

static int sim_probe(struct platform_device *pdev)
{
	int ret = 0;
    printk(KERN_INFO "SIM state probe function called\n");

    if (get_lk_boot_mode() == 1) {

		GPIO_SIM_STATE = of_get_named_gpio(pdev->dev.of_node, "gpio_state", 0);
		ret = gpio_request(GPIO_SIM_STATE, "my_gpio");
		if (ret) {
			printk(KERN_INFO "Failed to request GPIO %d\n", GPIO_SIM_STATE);
			return ret;
		}

		ret = gpio_direction_input(GPIO_SIM_STATE);
		if (ret) {
			printk(KERN_INFO "Failed to set GPIO %d as input\n", GPIO_SIM_STATE);
			gpio_free(GPIO_SIM_STATE);
			return ret;
		}

		/*Prize: add for X9LAVA-1524 begin*/
		// Create sysfs file for cd_gpio
		ret = device_create_file(&pdev->dev, &dev_attr_cd_gpio);
		if (ret) {
			dev_err(&pdev->dev, "Failed to create sysfs file for cd_gpio\n");
			return ret;
		}
		/*Prize: add for X9LAVA-1524 end*/
		printk("[%s %d]device_create_file  OK!!!\n", __func__, __LINE__);
	}

    return 0;
}

static int sim_remove(struct platform_device *pdev)
{
    return 0;
}

static const struct of_device_id sim_of_match[] = {
    { .compatible = "mediatek,simstate", },
    {},
};

static struct platform_driver sim_platform_driver = {
    .probe = sim_probe,
    .remove = sim_remove,
    .driver = {
        .name = "simstate",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(sim_of_match),
    },
};

MODULE_DEVICE_TABLE(of, sim_of_match);

static int __init sim_driver_init(void)
{
    int ret = 0;

    printk(KERN_INFO "SIM state driver initializing\n");

    ret = platform_driver_register(&sim_platform_driver);
    if (ret < 0) {
        printk(KERN_ERR "failed to register platform driver\n");
        return ret;
    }

    return 0;
}

static void __exit sim_driver_exit(void)
{
    printk(KERN_INFO "SIM state driver exiting\n");

    platform_driver_unregister(&sim_platform_driver);
}

module_init(sim_driver_init);
module_exit(sim_driver_exit);

MODULE_AUTHOR("prize");
MODULE_LICENSE("GPL");
