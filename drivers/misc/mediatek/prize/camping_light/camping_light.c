#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/proc_fs.h>
#include <media/rc-core.h>

#include <mt-plat/mtk_pwm.h>
#include <mt-plat/mtk_pwm_hal.h>

#define camping_light_DEBUG
#ifdef camping_light_DEBUG
#define camping_light_log(fmt,arg...) \
	do{\
		printk("<<camping_light-drv>>[%d]"fmt"", __LINE__, ##arg);\
    }while(0)
#else
#define camera_als_dbg(fmt,arg...)
#endif
static struct pinctrl *camping_light_pinctrl;
static struct pinctrl_state *led_default;
static struct pinctrl_state *led_led_l;
static struct pinctrl_state *led_led_set;
static struct pinctrl_state *led_en_h;
static struct pinctrl_state *led_en_l;
struct class *camping_light_class;
static int camping_light_status=0;
static int g_pwm_num=0;//PWM2
//
//    Freq=clk_src/clk_div/(2*（DATA_WIDTH+1）)
//    占空比=(THRESH+1)/(2*（DATA_WIDTH+1）)，when GUARD_VALUE=0

 static void prize_camping_light_set_disable(u32 pwm_no, u8 pmic_pad)
 {
	mt_pwm_disable(pwm_no, pmic_pad);
	pinctrl_select_state(camping_light_pinctrl,led_led_l);   //set  gpio low
	pinctrl_select_state(camping_light_pinctrl,led_en_l);
 }
#define CAMPING_LIGHT_PWM_DATA_WIDTH_VALUE 260 //25khz
#define CAMPING_LIGHT_PWM_THRESH_MODE_1 240 //97%
#define CAMPING_LIGHT_PWM_THRESH_MODE_2 130 //50%
#define CAMPING_LIGHT_PWM_THRESH_MODE_3 32//38%

static struct pwm_spec_config pwm_setting;
//prize huangjiwu for   set  pwm  enable
static int prize_camping_light_set_enable(int pwm_num,int led_mode)
{
	pwm_setting.pwm_no = pwm_num;
	pwm_setting.mode = PWM_MODE_OLD;

	printk("prize_camping_light_set_enable: led_mode=%d,pwm_no=%d\n", led_mode,
		   pwm_num);
	/* We won't choose 32K to be the clock src of old mode because of system performance. */
	/* The setting here will be clock src = 26MHz, CLKSEL = 26M/1625 (i.e. 16K) */
	pwm_setting.clk_src = PWM_CLK_OLD_MODE_BLOCK;  //prize  PWM_CLK_OLD_MODE_32K PWM_CLK_OLD_MODE_BLOCK 26M PWM_CLK_NEW_MODE_BLOCK PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625
	pwm_setting.clk_div = CLK_DIV4;
	pwm_setting.pmic_pad = 0;

	switch (led_mode) {
	/* Actually, the setting still can not to turn off NLED. We should disable PWM to turn off NLED. */
	case 0:
		prize_camping_light_set_disable(pwm_num,0);
		break;
	case 1:
		pinctrl_select_state(camping_light_pinctrl,led_en_h);
		pinctrl_select_state(camping_light_pinctrl,led_led_set);   //pwm
		pwm_setting.PWM_MODE_OLD_REGS.THRESH = CAMPING_LIGHT_PWM_THRESH_MODE_1;
		//pwm_setting.clk_div = CLK_DIV1;
		pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH = CAMPING_LIGHT_PWM_DATA_WIDTH_VALUE;
		pwm_setting.PWM_MODE_OLD_REGS.IDLE_VALUE = 0;
		pwm_setting.PWM_MODE_OLD_REGS.GUARD_VALUE = 0;
		pwm_setting.PWM_MODE_OLD_REGS.GDURATION = 0;
		pwm_setting.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
		pwm_set_spec_config(&pwm_setting);
		break;
		
	case 2:
		pinctrl_select_state(camping_light_pinctrl,led_en_h);
		pinctrl_select_state(camping_light_pinctrl,led_led_set);   //pwm
		pwm_setting.PWM_MODE_OLD_REGS.THRESH = CAMPING_LIGHT_PWM_THRESH_MODE_2;
		//pwm_setting.clk_div = CLK_DIV1;
		pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH = CAMPING_LIGHT_PWM_DATA_WIDTH_VALUE;
		pwm_setting.PWM_MODE_OLD_REGS.IDLE_VALUE = 0;
		pwm_setting.PWM_MODE_OLD_REGS.GUARD_VALUE = 0;
		pwm_setting.PWM_MODE_OLD_REGS.GDURATION = 0;
		pwm_setting.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
		pwm_set_spec_config(&pwm_setting);
		break;
	case 3:
		pinctrl_select_state(camping_light_pinctrl,led_en_h);
		pinctrl_select_state(camping_light_pinctrl,led_led_set);   //pwm
		pwm_setting.PWM_MODE_OLD_REGS.THRESH = CAMPING_LIGHT_PWM_THRESH_MODE_3;
		//pwm_setting.clk_div = CLK_DIV1;
		pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH = CAMPING_LIGHT_PWM_DATA_WIDTH_VALUE;
		pwm_setting.PWM_MODE_OLD_REGS.IDLE_VALUE = 0;
		pwm_setting.PWM_MODE_OLD_REGS.GUARD_VALUE = 0;
		pwm_setting.PWM_MODE_OLD_REGS.GDURATION = 0;
		pwm_setting.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
		pwm_set_spec_config(&pwm_setting);
		break;
	default:
		printk("%s Unexpected mode!!\n",__func__);
		pinctrl_select_state(camping_light_pinctrl,led_en_h);
		pinctrl_select_state(camping_light_pinctrl,led_led_set);   //pwm
		if (led_mode%2 == 0)
			pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH = led_mode; //8
		else
			pwm_setting.PWM_MODE_OLD_REGS.THRESH = led_mode; //4
		//pwm_setting.clk_div = CLK_DIV1;
		pwm_setting.PWM_MODE_OLD_REGS.IDLE_VALUE = 0;
		pwm_setting.PWM_MODE_OLD_REGS.GUARD_VALUE = 0;
		pwm_setting.PWM_MODE_OLD_REGS.GDURATION = 0;
		pwm_setting.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
		pwm_set_spec_config(&pwm_setting);
		break;
	}
	printk("%s %d : pwm_setting.PWM_MODE_OLD_REGS.THRESH=%d,pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH=%d\n", __func__,__LINE__,pwm_setting.PWM_MODE_OLD_REGS.THRESH,pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH);
	return 0;
}
static ssize_t camping_light_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d\n", camping_light_status);
}

static ssize_t camping_light_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t size)
{
	if(sscanf(buf, "%u", &camping_light_status) != 1)
	{
		camping_light_log("[camping_dev]: Invalid values\n");
		return -EINVAL;
	}

	camping_light_log("[camping_dev] %s camping_light_status value = %d [0:OFF ; mode 1~3 ; other:Singular set thresh, Even numbers set data_width ]\n ", __func__, camping_light_status);

	if(camping_light_status >= 0)
	{
		prize_camping_light_set_enable(g_pwm_num,camping_light_status);//PWM1
	}
	else
	{
		camping_light_log("[camping_dev]: Invalid values\n");
		return -EINVAL;
	}

	return size;
}
static DEVICE_ATTR(camping_light, 0664, camping_light_show, camping_light_store);


int camping_light_dts(struct platform_device *pdev)
{

	camping_light_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (!IS_ERR(camping_light_pinctrl)){
		led_default = pinctrl_lookup_state(camping_light_pinctrl,"led_default");
		if (IS_ERR(led_default)){
			printk(KERN_ERR"camping_light get pinctrl state led_default fail %d\n",PTR_ERR(led_default));
		}
		led_led_l = pinctrl_lookup_state(camping_light_pinctrl,"led_led_l");
		if (IS_ERR(led_led_l)){
			printk(KERN_ERR"camping_light get pinctrl state led_led_l fail %d\n",PTR_ERR(led_led_l));
		}
		led_led_set = pinctrl_lookup_state(camping_light_pinctrl,"led_led_set");
		if (IS_ERR(led_led_set)){
			printk(KERN_ERR"camping_light get pinctrl state led_led_set fail %d\n",PTR_ERR(led_led_set));
		}
		
		led_en_h = pinctrl_lookup_state(camping_light_pinctrl,"led_en_h");
		if (IS_ERR(led_en_h)){
			printk(KERN_ERR"camping_light get pinctrl state led_en_h fail %d\n",PTR_ERR(led_en_h));
		}
		led_en_l = pinctrl_lookup_state(camping_light_pinctrl,"led_en_l");
		if (IS_ERR(led_en_l)){
			printk(KERN_ERR"camping_light get pinctrl state led_en_l fail %d\n",PTR_ERR(led_en_l));
		}
	}else{
		printk(KERN_ERR"camping_light get pinctrl fail %d\n",PTR_ERR(camping_light_pinctrl));
		return -EINVAL;
	}
	
	pinctrl_select_state(camping_light_pinctrl,led_led_l);
	pinctrl_select_state(camping_light_pinctrl,led_en_l);

	return 0;
}

static int camping_light_probe(struct platform_device *pdev)
{
    int ret;
	struct device *camping_light_dev;
    camping_light_log("%s\n", __func__);
    
    ret = camping_light_dts(pdev);
    if (ret != 0) {
        camping_light_log("camping_light_dts failed!\n");
        return -1;
    }
    
    camping_light_class = class_create(THIS_MODULE, "camping_light");
	if (IS_ERR(camping_light_class)) {
		camping_light_log("Failed to create class(camping_light_class)!");
		return PTR_ERR(camping_light_class);
	}
	camping_light_dev = device_create(camping_light_class, NULL, 0, NULL, "camping_light_data");
	if (IS_ERR(camping_light_dev))
		camping_light_log("Failed to create camping_light_dev device");
	//test  8804 使用 节点
	if (device_create_file(camping_light_dev, &dev_attr_camping_light) < 0)
		camping_light_log("Failed to create device file(%s)!",dev_attr_camping_light.attr.name);	

	camping_light_log("%s OK\n", __func__);
    return 0;
}

static int camping_light_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id camping_light_of_match[] = {
	{ .compatible = "prize,camping_light" },
	{ }
};
MODULE_DEVICE_TABLE(of, camping_light_of_match);

static struct platform_driver camping_light_driver = {
	.probe = camping_light_probe,
	.remove = camping_light_remove,
	.driver = {
		.name = "camping_light",
		.of_match_table = of_match_ptr(camping_light_of_match),
	},
};

static int __init camping_light_init(void) {
	int ret;
    
	camping_light_log("%s\n", __func__);
    
	ret = platform_driver_register(&camping_light_driver);
	if (ret) {
		camping_light_log("****[%s] Unable to register driver (%d)\n", __func__, ret);
		return ret;
	}
	
	return 0;
}

static void __exit camping_light_exit(void) {
	camping_light_log("%s\n", __func__);
	platform_driver_unregister(&camping_light_driver);
}

late_initcall(camping_light_init);
module_exit(camping_light_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<yaozhipeng@szprize.com >");
MODULE_DESCRIPTION("CAMPING LIGHT Driver");

