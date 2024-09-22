/*
Z6 Digital Lcd Driver
gezi
2021-06-30
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sysfs.h>
#include <linux/power_supply.h>

#include "dlcd.h"

#include <mt-plat/mtk_pwm.h>
#include <mt-plat/mtk_pwm_hal.h>

int DLCD_DEBUG_ENABLE = 1;

#ifdef DLCD_CTIMER
static spinlock_t wake_dlcd_lock;

static struct hrtimer dlcd_timer;

static DECLARE_WAIT_QUEUE_HEAD(dlcd_thread_wq);

static unsigned int dlcd_thread_wq_flag = false;
#endif

static DEFINE_MUTEX(dlcd_pinctrl_mutex);
static DEFINE_MUTEX(dlcd_convert_mutex);
static DEFINE_MUTEX(dlcd_show_mutex);

//CHARGE_STATE g_curent_charge_state = STATE_HOST_BATTERY_DISCHARGE;

//static ktime_t g_ktime;


static struct pinctrl *dlcdpin_pinctrl;
//static struct pinctrl_state *dlcdpin_default;
static struct pinctrl_state *dlcdpin_smp_i2cc_en_l;
static struct pinctrl_state *dlcdpin_smp_i2cc_en_h;
static struct pinctrl_state *dlcdpin_smp_scl_l;
static struct pinctrl_state *dlcdpin_smp_scl_h;
static struct pinctrl_state *dlcdpin_smp_sda_l;
static struct pinctrl_state *dlcdpin_smp_sda_h;
static struct pinctrl_state *dlcdpin_smpbl_pwm_l;
static struct pinctrl_state *dlcdpin_smpbl_pwm_h;
static struct pinctrl_state *dlcdpin_smpbl_pwm_pwm;
static struct pinctrl_state *dlcdpin_smp_v33_en_l;
static struct pinctrl_state *dlcdpin_smp_v33_en_h;

const struct of_device_id dlcd_id[] = {
	{.compatible = "chipnorth,cn91c4s48"},
	{},
};
MODULE_DEVICE_TABLE(of, dlcd_id);

/*===================================================================
	wait for some time to get proper I2C timing:至少4.7us
=====================================================================*/
void I2C_wait(void)
{
	udelay(5);
}
/*===================================================================
	I2C acknowledge
=====================================================================*/
void I2C_ack(void)
{
	//int	i;
	mutex_lock(&dlcd_pinctrl_mutex); 
	pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_sda_h);//SDA=1;
	I2C_wait();
	pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_scl_h);//SCL=1;
	I2C_wait();
	//bflag_Write_erro=1;
	//for(i=0;i<22;i++)
	//{
	//	if(SDA==0)
	//	{
	//		bflag_Write_erro=0;
	//		break;
	//	}
	//}
	pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_scl_l);//SCL=0;
	mutex_unlock(&dlcd_pinctrl_mutex); 
}
/*===================================================================
	I2C start condition:SDA,SCL初始都为H,SDA先由H->L，SCL再由H->L
=====================================================================*/
void I2C_start(void)
{
	mutex_lock(&dlcd_pinctrl_mutex); 
	pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_sda_h);//SDA=1;
	I2C_wait();
	pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_scl_h);//SCL=1;
	I2C_wait();
	pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_sda_l);//SDA=0;
	I2C_wait();
	pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_scl_l);//SCL=0;
	I2C_wait();
	mutex_unlock(&dlcd_pinctrl_mutex); 
}
/*===================================================================
	I2C stop condition:SDA初始为L,SCL初始为H,SDA由L->H
=====================================================================*/
void I2C_stop(void)
{	
	mutex_lock(&dlcd_pinctrl_mutex); 
	pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_scl_l);//SCL=0;
	I2C_wait();
	pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_sda_l);//SDA=0;
	I2C_wait();
	pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_scl_h);//SCL=1;
	I2C_wait();
	pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_sda_h);//SDA=1;
	I2C_wait();
	mutex_unlock(&dlcd_pinctrl_mutex); 
}

/*===================================================================
	Initialize I2C interface:默认上拉
=====================================================================*/
void I2C_init(void)
{	
	mutex_lock(&dlcd_pinctrl_mutex); 
	pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_sda_h);//SDA=1;
	I2C_wait();
	pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_scl_h);//SCL=1;
	I2C_wait();
	mutex_unlock(&dlcd_pinctrl_mutex); 
	
}
/*===================================================================
	I2C sent byte data
=====================================================================*/
void I2Csent_dat(int dat)
{
	int i;
	mutex_lock(&dlcd_pinctrl_mutex); 
	for(i=0;i<8;i++)
	{
		if(dat&0x80)
		{
			pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_sda_h);//SDA=1;
		}
		else
		{
			pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_sda_l);//SDA=0;
		}
		dat = dat << 1;
		I2C_wait();

 	  pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_scl_h);//SCL=1;		//generate clock pluse
	  I2C_wait();	//delay

	  pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_scl_l);//SCL=0;
	  I2C_wait();
	}
	mutex_unlock(&dlcd_pinctrl_mutex); 
	I2C_ack();

}
/*===================================================================
	initializing  IC
=====================================================================*/
void CN91C4S48_init(void)
{
//	CSA_SEL();         //Chip 1 initialization

	I2C_start();
	I2Csent_dat(0x7C); //Slave Address
	I2Csent_dat(0xA2); //DISCTL
	I2C_stop();

	I2C_start();
	I2Csent_dat(0x7C); //Slave Address
	I2Csent_dat(0xC8); //MODSET
	I2C_stop();

	I2C_start();
	I2Csent_dat(0x7C); //Slave Address
	I2Csent_dat(0xE0); //EVRSET
	I2C_stop();

    I2C_start();
	I2Csent_dat(0x7C); //Slave Address
	I2Csent_dat(0xE8); //ICSET
	I2C_stop();

	I2C_start();
	I2Csent_dat(0x7C); //Slave Address
	I2Csent_dat(0xF0); //BLKCTL
	I2C_stop();

	I2C_start();
	I2Csent_dat(0x7C); //Slave Address
	I2Csent_dat(0xF8); //APCTL
	I2C_stop();

}

void Display_On(int dat)
{
	int i;

//	CSA_SEL();         //Chip 1 initialization
  I2C_start();
  I2Csent_dat(0x7C);
 // I2Csent_dat(0x80);
  I2Csent_dat(0x00);

	for(i=0;i<24;i++)
	{
		I2Csent_dat(dat);
	}
	I2C_stop();
	mdelay(200);
	
}

/*===================================================================
	set character display
=====================================================================*/
void Show_Pic(u8 *pos)
{
	int i;
	int j=0;
	mutex_lock(&dlcd_show_mutex); 
//	CSA_SEL();
	I2C_start();
	I2Csent_dat(0x7C);
	I2Csent_dat(0x00);

	for(i=0;i<15;i++)
	{
		I2Csent_dat(pos[j]);
		j+=1;
	}
	I2C_stop();
	//delay(2000);
	mutex_unlock(&dlcd_show_mutex); 
}

void dump_data(void)
{
	char pTemp[45];
	int len=0,i=0;
	for(i=0;i<15;i++)
		len += snprintf(pTemp+i*3,4,"%02x ",disp_array_all[i]);
	DLCD_DBG("%s|len=%d",pTemp,len);
}

static int dlcd_baccklight_enable(int pwm_num,int enable)
{
	struct pwm_spec_config pwm_setting;

	memset(&pwm_setting, 0, sizeof(struct pwm_spec_config));
	pwm_setting.pwm_no = pwm_num;
	pwm_setting.mode = PWM_MODE_OLD;
	DLCD_DBG("enable=%d,pwm_no=%d\n",enable,pwm_num);
	if(enable>0){
		enable = enable>160?160:enable;
		enable = enable<1?80:enable;
		/* We won't choose 32K to be the clock src of old mode because of system performance. */
		/* The setting here will be clock src = 26MHz, CLKSEL = 26M/1625 (i.e. 16K) */
		pwm_setting.clk_src = PWM_CLK_OLD_MODE_32K;  //prize  PWM_CLK_OLD_MODE_32K PWM_CLK_OLD_MODE_BLOCK 26M PWM_CLK_NEW_MODE_BLOCK PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625
		pwm_setting.clk_div = CLK_DIV16;
		pwm_setting.pmic_pad = 0;
		mutex_lock(&dlcd_pinctrl_mutex); 
		pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smpbl_pwm_pwm);   //pwm
		mutex_unlock(&dlcd_pinctrl_mutex); 
		pwm_setting.PWM_MODE_OLD_REGS.THRESH = enable; //3   , 2不亮 1不亮
		//pwm_setting.clk_div = CLK_DIV1;
		pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH = 160; //8
		pwm_setting.PWM_MODE_OLD_REGS.IDLE_VALUE = 0;
		pwm_setting.PWM_MODE_OLD_REGS.GUARD_VALUE = 0;
		pwm_setting.PWM_MODE_OLD_REGS.GDURATION = 0;
		pwm_setting.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
		pwm_set_spec_config(&pwm_setting);
	}else{
		pr_info("[DLCD] disable pwm\n");
		mt_pwm_disable(pwm_setting.pwm_no, pwm_setting.pmic_pad);
		mutex_lock(&dlcd_pinctrl_mutex); 
		pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smpbl_pwm_l);
		mutex_unlock(&dlcd_pinctrl_mutex); 
		
	}
	return 0;
}


void array_merge(void)
{
	int i=0;
	u8 temp1,temp2;
	for(i=0;i<4;i++)
	{
		disp_array_all[i+3]=disp_array_time[i];
		if(i<3)
		disp_array_all[i]=disp_array_date[i];
	}

	disp_array_all[7]= (0xF1 & disp_array_bat[0])|((0xE0 & disp_array_temp[0])>>4);
	
	for(i=0;i<4;i++)
	{
		temp1= (disp_array_temp[i]&0x0F)<<4;temp2=(disp_array_temp[i+1]&0xF0)>>4;
		DLCD_DBG("[%s] i=%d temp1:%02x temp2:%02x\n",__func__,i,temp1,temp2);
		disp_array_all[i+8]=temp1 | temp2;
	}
	disp_array_all[12]=((disp_array_temp[4]&0x0F)<<4) | ((disp_array_humidity[0]&0xF0)>>4);
	disp_array_all[13]=((disp_array_humidity[0]&0x0F)<<4) | ((disp_array_humidity[1]&0xF0)>>4);
	disp_array_all[14]=(disp_array_humidity[1]&0x0F)<<4;
	
	dump_data();
	Show_Pic(disp_array_all);
}
#ifdef DLCD_CTIMER
void dlcd_test(void)
{
	
	TM_TRACE_FUNC

	Show_Pic(disp_array_all);
	mdelay(200);
}


void wake_up_dlcd_thread_wq(void)
{
	
	DLCD_DBG("----[gezi][%s]\n",__func__);
	TM_TRACE_FUNC
	spin_lock(&wake_dlcd_lock);
	
	dlcd_thread_wq_flag = true;

	wake_up(&dlcd_thread_wq);
	
	spin_unlock(&wake_dlcd_lock);
}

//EXPORT_SYMBOL(wake_up_dlcd_thread_wq);

int dlcd_thread_routine(void *x)
{
	TM_TRACE_FUNC
	while (true) 
	{
		wait_event(dlcd_thread_wq, (dlcd_thread_wq_flag == true));
		
		dlcd_thread_wq_flag = false;
		
		DLCD_DBG("dlcd_thread_routine......wake up...........\n");
		
		dlcd_test();
	}
}
#endif
	
static ssize_t dlcd_time_show(struct device *dev, struct device_attribute *attr,char *buf)
{

	TM_TRACE_FUNC
	return sprintf(buf,"%x %x %x %x\n",disp_array_time[0],disp_array_time[1],disp_array_time[2],disp_array_time[3]);
}

static ssize_t dlcd_time_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t size)
{

	int ret=0;
	TM_TRACE_FUNC
	
	DLCD_DBG("buf:%s,size:%d",buf,(int)size);
	ret = sscanf(buf,"%x %x %x %x",&disp_array_time[0],&disp_array_time[1],&disp_array_time[2],&disp_array_time[3]);
	array_merge();
	return size;
	
}
static ssize_t dlcd_date_show(struct device *dev, struct device_attribute *attr,char *buf)
{

	TM_TRACE_FUNC
	return sprintf(buf,"%x %x %x\n",disp_array_date[0],disp_array_date[1],disp_array_date[2]);
}
static ssize_t dlcd_date_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t size)
{
	int ret = 0;
	TM_TRACE_FUNC
	
	DLCD_DBG("buf:%s,size:%d",buf,(int)size);
	ret = sscanf(buf,"%x %x %x",&disp_array_date[0],&disp_array_date[1],&disp_array_date[2]);
	array_merge();
	return size;
}
static ssize_t dlcd_bat_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	TM_TRACE_FUNC
	return sprintf(buf,"%x\n",disp_array_bat[0]);
}
static ssize_t dlcd_bat_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t size)
{
	int ret = 0;
	TM_TRACE_FUNC
	
	DLCD_DBG("buf:%s,size:%d",buf,(int)size);
	ret = sscanf(buf,"%x",&disp_array_bat[0]);
	array_merge();
	return size;
}
static ssize_t dlcd_temp_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	TM_TRACE_FUNC
	return sprintf(buf,"%x %x %x %x %x\n",disp_array_temp[0],disp_array_temp[1],disp_array_temp[2],disp_array_temp[3],disp_array_temp[4]);
}
static ssize_t dlcd_temp_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t size)
{
	int ret = 0;
	TM_TRACE_FUNC
	
	DLCD_DBG("buf:%s,size:%d",buf,(int)size);
	ret = sscanf(buf,"%x %x %x %x %x",&disp_array_temp[0],&disp_array_temp[1],&disp_array_temp[2],&disp_array_temp[3],&disp_array_temp[4]);
	DLCD_DBG("%x %x %x %x %x",disp_array_temp[0],disp_array_temp[1],disp_array_temp[2],disp_array_temp[3],disp_array_temp[4]);
	array_merge();
	return size;
}
static ssize_t dlcd_humidity_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	TM_TRACE_FUNC
	return sprintf(buf,"%x %x\n",disp_array_humidity[0],disp_array_humidity[1]);
}
static ssize_t dlcd_humidity_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t size)
{
	int ret = 0;
	TM_TRACE_FUNC
	
	DLCD_DBG("buf:%s,size:%d",buf,(int)size);
	ret = sscanf(buf,"%x %x",&disp_array_humidity[0],&disp_array_humidity[1]);
	array_merge();
	return size;
}
static ssize_t dlcd_backlight_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	TM_TRACE_FUNC
	return sprintf(buf,"%x\n",0);
}
static ssize_t dlcd_backlight_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t size)
{
	int ret = 0,enable=0;
	TM_TRACE_FUNC
	
	DLCD_DBG("buf:%s,size:%d",buf,(int)size);
	ret = sscanf(buf,"%d",&enable);
	
	dlcd_baccklight_enable(0,enable);
	return size;
}
static ssize_t dlcd_enable_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	int ret=0;
	TM_TRACE_FUNC
	ret +=  gpio_get_value(290+154);
	ret +=  gpio_get_value(290+144);
	ret +=  gpio_get_value(290+145);
	ret +=  gpio_get_value(290+95);
	ret +=  gpio_get_value(290+42);
	return sprintf(buf,"%x\n",ret>0?1:0);
}
static ssize_t dlcd_enable_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t size)
{
	int ret = 0,enable=0;
	TM_TRACE_FUNC
	
	DLCD_DBG("buf:%s,size:%d",buf,(int)size);
	ret = sscanf(buf,"%d",&enable);
	if(!enable){
		mutex_lock(&dlcd_pinctrl_mutex); 
		pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_i2cc_en_l);
		pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_scl_l);
		pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_sda_l);
		pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_v33_en_l);
		pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smpbl_pwm_l);
		mutex_unlock(&dlcd_pinctrl_mutex); 
	}else{
		mutex_lock(&dlcd_pinctrl_mutex); 
		pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_v33_en_h);
		pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_i2cc_en_h);
		mutex_unlock(&dlcd_pinctrl_mutex); 
		I2C_init();
		I2C_wait();
		CN91C4S48_init();
		memset(disp_array_all,0,sizeof(disp_array_all));
		Show_Pic(disp_array_all);
	}
	return size;
}
u8 char2hex(char pchar)
{
	u8 tohex=0;
	if (pchar >= 'A'&&pchar <= 'F')
		tohex= pchar-55;
	else if (pchar >= 'a'&&pchar <= 'f')
		tohex= pchar-87;
	else if (pchar >= '0'&&pchar <= '9')
		tohex= pchar-'0';
	return tohex;
}
static ssize_t dlcd_regs_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	char *p=buf; 
	int i=0;
	for(i=0;i<15;i++)
	{
		//DLCD_DBG("%s %x",p,strlen(p),disp_array_all[i]);
		sprintf(p+strlen(p),"%x ", disp_array_all[i]);
	}
	DLCD_DBG("%s",p);
	return sprintf(buf, "%s\n",p);
}
static ssize_t dlcd_regs_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t size)
{
	char temp_reg[31];
	int i;
	TM_TRACE_FUNC
	memset(temp_reg,0,sizeof(temp_reg));
	if(size>30)
		strncpy(temp_reg,buf,30);
	else
		strncpy(temp_reg,buf,size);
	temp_reg[30]='\0';
	DLCD_DBG("%s",temp_reg);
    for(i = 0; i < 15; i++)
    {
		//DLCD_DBG("%c(%x) %c(%x)",temp_reg[2*i],temp_reg[2*i],temp_reg[2*i+1],temp_reg[2*i+1]);
		disp_array_all[i] = (char2hex(temp_reg[2*i])<<4)|char2hex(temp_reg[2*i+1]);
		//DLCD_DBG("disp_array_all[%d]:%x======= %x %x",i,disp_array_all[i],char2hex(temp_reg[2*i]),char2hex(temp_reg[2*i+1]));
    }
	dump_data();
	Show_Pic(disp_array_all);

	DLCD_DBG("buf:%s",buf);
	return size;
}
static DEVICE_ATTR(dlcd_time, 0664, dlcd_time_show, dlcd_time_store);
static DEVICE_ATTR(dlcd_date, 0664, dlcd_date_show, dlcd_date_store);
static DEVICE_ATTR(dlcd_bat,  0664, dlcd_bat_show, dlcd_bat_store);
static DEVICE_ATTR(dlcd_temp, 0664, dlcd_temp_show, dlcd_temp_store);
static DEVICE_ATTR(dlcd_humidity,  0664, dlcd_humidity_show, dlcd_humidity_store);
static DEVICE_ATTR(dlcd_backlight,  0664, dlcd_backlight_show, dlcd_backlight_store);
static DEVICE_ATTR(dlcd_enable,  0664, dlcd_enable_show, dlcd_enable_store);
static DEVICE_ATTR(dlcd_regs,  0664, dlcd_regs_show, dlcd_regs_store);

static write_allow=1;
static ssize_t dlcd_debug_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	int ret[5];
	TM_TRACE_FUNC
	ret[0] =  gpio_get_value(290+154);
	ret[1] =  gpio_get_value(290+144);
	ret[2] =  gpio_get_value(290+145);
	ret[3] =  gpio_get_value(290+95);
	ret[4] =  gpio_get_value(290+42);
	DLCD_DBG("gpio: 154 144 145 95 42 =%d %d %d %d %d\n",ret[0],ret[1],ret[2],ret[3],ret[4]);
	return sprintf(buf, "%d\n",write_allow);
}
static ssize_t dlcd_debug_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t size)
{
	int temp = 0,value=0;
	int ret=0;

	TM_TRACE_FUNC
	//ret=strncmp(buf, "bat", 3);
	//DLCD_DBG("dlcd_debug_store ret:%d=======",ret);
	//ret=sscanf(buf+3, "%d", &temp);
	//DLCD_DBG("dlcd_debug_store ret:%d temp:%d=======",ret,temp);
	write_allow=0;
	mutex_lock(&dlcd_convert_mutex); 

	if (!strncmp(buf, "bat", 3) && (sscanf(buf+3, "%d", &temp) == 1)) {
		switch(temp){
			case 0:
				disp_array_all[7]= (disp_array_all[7]&0x0E)|0x01;
				break;
			case 1:
				disp_array_all[7]= (disp_array_all[7]&0x0E)|0x11;
				break;
			case 2:
				disp_array_all[7]= (disp_array_all[7]&0x0E)|0x31;
				break;
			case 3:
				disp_array_all[7]= (disp_array_all[7]&0x0E)|0x71;
				break;
			case 4:
				disp_array_all[7]= (disp_array_all[7]&0x0E)|0xF1;
				break;			
			case 5:
				disp_array_all[7]= (disp_array_all[7]&0x0E)|0x00;
				break;		
		}
    }else if (!strncmp(buf, "chg", 3) && (sscanf(buf+3, "%d", &temp) == 1)) {
		if(temp)
			disp_array_all[8]= (disp_array_all[8]&0xFE)|0x01;
		else
			disp_array_all[8]= (disp_array_all[8]&0xFE)|0x00;
    }else if(!strncmp(buf, "icon", 4) && (sscanf(buf+4, "%d %d", &temp,&value) == 2)){
		switch(temp){
			case 1://S5
				disp_array_all[1]= (disp_array_all[1]&0xFE)|value;
				break;
			case 2://12BC
				disp_array_all[2]= (disp_array_all[2]&0xFE)|value;
				break;
			case 3://COL
				disp_array_all[4]= (disp_array_all[4]&0xFE)|value;
				break;
			case 4://S1
				disp_array_all[8]= (disp_array_all[8]&0xFE)|value;
				break;			
			case 5://S4
				disp_array_all[9]= (disp_array_all[9]&0xFE)|value;
				break;
			case 6://P1
				disp_array_all[10]= (disp_array_all[10]&0xFE)|value;
				break;
			case 7://S2
				if(value==1){
					disp_array_all[11]= (disp_array_all[11]&0xF0)|0x0F;
					disp_array_all[12]= (disp_array_all[12]&0x0F)|0x80;
				}else if(value==2){
					disp_array_all[11]= (disp_array_all[11]&0xF0)|0x0d;
					disp_array_all[12]= (disp_array_all[12]&0x0F)|0x90;
				}else
				{
					disp_array_all[11]= (disp_array_all[11]&0xF0)|0x00;
					disp_array_all[12]= (disp_array_all[12]&0x0F)|0x00;
				}
				break;			
			case 8://S3
				disp_array_all[13]= (disp_array_all[13]&0xFE)|value;
				break;					
		}
	}else if (!strncmp(buf, "time", 4)) {
		ret= sscanf(buf+4, "%d", &temp);
		if(ret!=1)
		{
			disp_array_all[6] = (disp_array_all[6]&0x01);
			disp_array_all[5] = (disp_array_all[5]&0x01);
			disp_array_all[4] = (disp_array_all[4]&0x01);
			disp_array_all[3] = (disp_array_all[3]&0x01);
		}else{
			disp_array_all[6] = (disp_array_all[6]&0x01) | disp_time_code[temp%10000/1000];
			disp_array_all[5] = (disp_array_all[5]&0x01) | disp_time_code[temp%1000/100];
			disp_array_all[4] = (disp_array_all[4]&0x01) | disp_time_code[temp%100/10];
			disp_array_all[3] = (disp_array_all[3]&0x01) | disp_time_code[temp%10];
		}
    }else if (!strncmp(buf, "date", 4)) {
		ret= sscanf(buf+4, "%d", &temp);
		if(ret!=1){
			disp_array_all[2] = (disp_array_all[2]&0xFE);
			disp_array_all[2] = (disp_array_all[2]&0x01);
			disp_array_all[1] = (disp_array_all[1]&0x01);
			disp_array_all[0] = (disp_array_all[0]&0x01);
		}else{
			disp_array_all[2] = (disp_array_all[2]&0xFE) | ((temp%10000/1000)&0x01);
			disp_array_all[2] = (disp_array_all[2]&0x01) | disp_time_code[temp%1000/100];
			disp_array_all[1] = (disp_array_all[1]&0x01) | disp_time_code[temp%100/10];
			disp_array_all[0] = (disp_array_all[0]&0x01) | disp_time_code[temp%10];
		}
    }else if (!strncmp(buf, "temp", 4)) {
		ret= sscanf(buf+4, "%d", &temp);
		if(ret!=1){
			disp_array_all[7] = (disp_array_all[7]&0xF1);
			disp_array_all[8] = (disp_array_all[8]&0x01);
			disp_array_all[9] = (disp_array_all[9]&0x01);
			disp_array_all[10] = (disp_array_all[10]&0x01);
			disp_array_all[11] = (disp_array_all[11]&0x0F) ;
		}else{
			if(temp<0){
				temp *= -1;
				disp_array_all[7]  = (disp_array_all[7]&0xF1)  | 0x4;
				disp_array_all[8]  = (disp_array_all[8]&0x01)  |((disp_temp_code[temp%1000/100]&0xe0)>>4);
				disp_array_all[9]  = (disp_array_all[9]&0x01)  | ((disp_temp_code[temp%1000/100]&0x0f)<<4)|((disp_temp_code[temp%100/10]&0xe0)>>4);
				disp_array_all[10] = (disp_array_all[10]&0x01) | ((disp_temp_code[temp%100/10]&0x0f)<<4)|((disp_temp_code[temp%10]&0xe0)>>4);
				disp_array_all[11] = (disp_array_all[11]&0x0F) | ((disp_temp_code[temp%10]&0x0f)<<4);
			}else{
				if(temp<10000){
					disp_array_all[7]  = (disp_array_all[7]&0xF1)  | ((disp_temp_code[temp%10000/1000]&0xe0)>>4);
					disp_array_all[8]  = (disp_array_all[8]&0x01)  | ((disp_temp_code[temp%10000/1000]&0x0f)<<4)|((disp_temp_code[temp%1000/100]&0xe0)>>4);
					disp_array_all[9]  = (disp_array_all[9]&0x01)  | ((disp_temp_code[temp%1000/100]&0x0f)<<4)|((disp_temp_code[temp%100/10]&0xe0)>>4);
					disp_array_all[10] = (disp_array_all[10]&0x01) | ((disp_temp_code[temp%100/10]&0x0f)<<4)|((disp_temp_code[temp%10]&0xe0)>>4);
					disp_array_all[11] = (disp_array_all[11]&0x0F) | ((disp_temp_code[temp%10]&0x0f)<<4);
				}else{
					if(temp==10001){
						disp_array_all[7]  = (disp_array_all[7]&0xF1)  | 0x4;
						disp_array_all[8]  = (disp_array_all[8]&0x01)  | 0x4;
						disp_array_all[9]  = (disp_array_all[9]&0x01)  | 0x4;
						disp_array_all[10] = (disp_array_all[10]&0x00) ;
						disp_array_all[11] = (disp_array_all[11]&0x00) ;
						disp_array_all[12] = (disp_array_all[12]&0x0F) ;
					}
				}
			}
		}
    }else if (!strncmp(buf, "humi", 4)) {
		ret= sscanf(buf+4, "%d", &temp);
		if(ret!=1){
			disp_array_all[12] = (disp_array_all[12]&0xF1);
			disp_array_all[13] = (disp_array_all[13]&0x01);
			disp_array_all[14] = (disp_array_all[14]&0x01);
		}else if(temp<10000){
			disp_array_all[12] = (disp_array_all[12]&0xF1) | ((disp_temp_code[temp%100/10]&0xe0)>>4);
			disp_array_all[13] = (disp_array_all[13]&0x01) | ((disp_temp_code[temp%100/10]&0x0f)<<4)|((disp_temp_code[temp%10]&0xe0)>>4);
			disp_array_all[14] = (disp_array_all[14]&0x01) | ((disp_temp_code[temp%10]&0x0f)<<4);
		}else{
			if(temp==10001){
				disp_array_all[12] = (disp_array_all[12]&0xF1) | 0x4;
				disp_array_all[13] = (disp_array_all[13]&0x00) | 0x4;
				disp_array_all[14] = (disp_array_all[14]&0x01) ;
			}
		}
    }else if (!strncmp(buf, "refresh", 7)) {
		DLCD_DBG("refresh disp");
		Show_Pic(disp_array_all);
    }else{
		DLCD_DBG("invalid input");
	}
	dump_data();
	mutex_unlock(&dlcd_convert_mutex); 
	
	write_allow=1;
	DLCD_DBG("buf:%s,size:%d,ret:%d,temp:%d,write_allow:%d",buf,(int)size,ret,temp,write_allow);

	return size;
}
static DEVICE_ATTR(dlcd_debug,  0664, dlcd_debug_show, dlcd_debug_store);

struct class *dlcd_class;
#ifdef DLCD_CTIMER
enum hrtimer_restart dlcd_hrtimer_func(struct hrtimer *timer)
{
	ktime_t ktime;
	TM_TRACE_FUNC
	DLCD_DBG("gezi----- charge_state_hrtimer_func....\n ");
	
	ktime = ktime_set(3, 0);
	
	wake_up_dlcd_thread_wq();
	
	hrtimer_start(&dlcd_timer, ktime, HRTIMER_MODE_REL);
	
	return HRTIMER_NORESTART;
}

void dlcd_hrtimer_init(void)
{
	ktime_t ktime;
	TM_TRACE_FUNC
	ktime = ktime_set(5, 0); 
	
	DLCD_DBG("dlcd_timer init..\n");

	hrtimer_init(&dlcd_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dlcd_timer.function = dlcd_hrtimer_func;
	hrtimer_start(&dlcd_timer, ktime, HRTIMER_MODE_REL);

}
#endif
int dlc_pinctrl_parse_dts(struct platform_device *pdev)
{

	dlcdpin_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (!IS_ERR(dlcdpin_pinctrl)){
		//dlcdpin_default = pinctrl_lookup_state(dlcdpin_pinctrl,"dlcdpin_default");
		//if (IS_ERR(dlcdpin_default)){
		//	printk(KERN_ERR"typec_accdet get pinctrl state dlcdpin_default fail %d\n",PTR_ERR(dlcdpin_default));
		//}
		dlcdpin_smp_i2cc_en_l = pinctrl_lookup_state(dlcdpin_pinctrl,"dlcdpin_smp_i2cc_en_l");
		if (IS_ERR(dlcdpin_smp_i2cc_en_l)){
			printk(KERN_ERR"typec_accdet get pinctrl state dlcdpin_smp_i2cc_en_l fail %d\n",PTR_ERR(dlcdpin_smp_i2cc_en_l));
		}
		dlcdpin_smp_i2cc_en_h = pinctrl_lookup_state(dlcdpin_pinctrl,"dlcdpin_smp_i2cc_en_h");
		if (IS_ERR(dlcdpin_smp_i2cc_en_h)){
			printk(KERN_ERR"typec_accdet get pinctrl state dlcdpin_smp_i2cc_en_h fail %d\n",PTR_ERR(dlcdpin_smp_i2cc_en_h));
		}
		dlcdpin_smp_scl_l = pinctrl_lookup_state(dlcdpin_pinctrl,"dlcdpin_smp_scl_l");
		if (IS_ERR(dlcdpin_smp_scl_l)){
			printk(KERN_ERR"typec_accdet get pinctrl state dlcdpin_smp_scl_l fail %d\n",PTR_ERR(dlcdpin_smp_scl_l));
		}
		dlcdpin_smp_scl_h = pinctrl_lookup_state(dlcdpin_pinctrl,"dlcdpin_smp_scl_h");
		if (IS_ERR(dlcdpin_smp_scl_h)){
			printk(KERN_ERR"typec_accdet get pinctrl state dlcdpin_smp_scl_h fail %d\n",PTR_ERR(dlcdpin_smp_scl_h));
		}
		dlcdpin_smp_sda_l = pinctrl_lookup_state(dlcdpin_pinctrl,"dlcdpin_smp_sda_l");
		if (IS_ERR(dlcdpin_smp_sda_l)){
			printk(KERN_ERR"typec_accdet get pinctrl state dlcdpin_smp_sda_l fail %d\n",PTR_ERR(dlcdpin_smp_sda_l));
		}
		dlcdpin_smp_sda_h = pinctrl_lookup_state(dlcdpin_pinctrl,"dlcdpin_smp_sda_h");
		if (IS_ERR(dlcdpin_smp_sda_h)){
			printk(KERN_ERR"typec_accdet get pinctrl state dlcdpin_smp_sda_h fail %d\n",PTR_ERR(dlcdpin_smp_sda_h));
		}
		dlcdpin_smpbl_pwm_l = pinctrl_lookup_state(dlcdpin_pinctrl,"dlcdpin_smpbl_pwm_l");
		if (IS_ERR(dlcdpin_smpbl_pwm_l)){
			printk(KERN_ERR"typec_accdet get pinctrl state dlcdpin_smpbl_pwm_l fail %d\n",PTR_ERR(dlcdpin_smpbl_pwm_l));
		}
		dlcdpin_smpbl_pwm_h = pinctrl_lookup_state(dlcdpin_pinctrl,"dlcdpin_smpbl_pwm_h");
		if (IS_ERR(dlcdpin_smpbl_pwm_h)){
			printk(KERN_ERR"typec_accdet get pinctrl state dlcdpin_smpbl_pwm_h fail %d\n",PTR_ERR(dlcdpin_smpbl_pwm_h));
		}
		dlcdpin_smpbl_pwm_pwm = pinctrl_lookup_state(dlcdpin_pinctrl,"dlcdpin_smpbl_pwm_pwm");
		if (IS_ERR(dlcdpin_smpbl_pwm_pwm)){
			printk(KERN_ERR"typec_accdet get pinctrl state dlcdpin_smpbl_pwm_pwm fail %d\n",PTR_ERR(dlcdpin_smpbl_pwm_pwm));
		}
		dlcdpin_smp_v33_en_l = pinctrl_lookup_state(dlcdpin_pinctrl,"dlcdpin_smp_v33_en_l");
		if (IS_ERR(dlcdpin_smp_v33_en_l)){
			printk(KERN_ERR"typec_accdet get pinctrl state dlcdpin_smp_v33_en_l fail %d\n",PTR_ERR(dlcdpin_smp_v33_en_l));
		}
		dlcdpin_smp_v33_en_h = pinctrl_lookup_state(dlcdpin_pinctrl,"dlcdpin_smp_v33_en_h");
		if (IS_ERR(dlcdpin_smp_v33_en_h)){
			printk(KERN_ERR"typec_accdet get pinctrl state dlcdpin_smp_v33_en_h fail %d\n",PTR_ERR(dlcdpin_smp_v33_en_h));
		}		
		
	}else{
		printk(KERN_ERR"dlc get pinctrl fail %d\n",PTR_ERR(dlcdpin_pinctrl));
		return -EINVAL;
	}
	//pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_i2cc_en_h);
	//pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_scl_h);
	//pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_sda_h);
	//pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smpbl_pwm_h);
	//pinctrl_select_state(dlcdpin_pinctrl,dlcdpin_smp_v33_en_h);
	return 0;
}
static int dlcd_probe(struct platform_device *pdev)
{
	int ret=0;
	//struct device *dev = &pdev->dev;
	//const struct of_device_id *match;
	struct device *dlcd_dev;
	TM_TRACE_FUNC
	ret = dlc_pinctrl_parse_dts(pdev);
	if(ret < 0)
	{
		DLCD_DBG("----%s-----error...\n",__func__);
		return -1;
	}
	dlcd_class = class_create(THIS_MODULE, "dlcd_node");
	
	if (IS_ERR(dlcd_class)) {
		DLCD_DBG("Failed to create class(dlcd_class)!");
		return PTR_ERR(dlcd_class);
	}
	dlcd_dev = device_create(dlcd_class, NULL, 0, NULL, "dlcd_fun");
	if (IS_ERR(dlcd_dev))
		DLCD_DBG("Failed to create dlcd_dev device");
	
	if (device_create_file(dlcd_dev, &dev_attr_dlcd_time) < 0)
		DLCD_DBG("Failed to create device file(%s)!",dev_attr_dlcd_time.attr.name);	
	if (device_create_file(dlcd_dev, &dev_attr_dlcd_date) < 0)
		DLCD_DBG("Failed to create device file(%s)!",dev_attr_dlcd_date.attr.name);	
	if (device_create_file(dlcd_dev, &dev_attr_dlcd_bat) < 0)
		DLCD_DBG("Failed to create device file(%s)!",dev_attr_dlcd_bat.attr.name);	
	if (device_create_file(dlcd_dev, &dev_attr_dlcd_temp) < 0)
		DLCD_DBG("Failed to create device file(%s)!",dev_attr_dlcd_temp.attr.name);	
	if (device_create_file(dlcd_dev, &dev_attr_dlcd_humidity) < 0)
		DLCD_DBG("Failed to create device file(%s)!",dev_attr_dlcd_humidity.attr.name);
	if (device_create_file(dlcd_dev, &dev_attr_dlcd_debug) < 0)
		DLCD_DBG("Failed to create device file(%s)!",dev_attr_dlcd_debug.attr.name);
	if (device_create_file(dlcd_dev, &dev_attr_dlcd_backlight) < 0)
		DLCD_DBG("Failed to create device file(%s)!",dev_attr_dlcd_backlight.attr.name);
	if (device_create_file(dlcd_dev, &dev_attr_dlcd_enable) < 0)
		DLCD_DBG("Failed to create device file(%s)!",dev_attr_dlcd_enable.attr.name);
	if (device_create_file(dlcd_dev, &dev_attr_dlcd_regs) < 0)
		DLCD_DBG("Failed to create device file(%s)!",dev_attr_dlcd_regs.attr.name);
#ifdef DLCD_CTIMER
	spin_lock_init(&wake_dlcd_lock);
#endif

	I2C_init();
	I2C_wait();

	CN91C4S48_init();
	Show_Pic(disp_array_all);
#ifdef DLCD_CTIMER
	kthread_run(dlcd_thread_routine, NULL, "dlcd_thread_routine");
	
	dlcd_hrtimer_init();
#endif
	DLCD_DBG("----%s-----success...\n",__func__);
	
	return 0;
}


static int dlcd_remove(struct platform_device *pdev)
{
	TM_TRACE_FUNC
	return 0;
}

static struct platform_driver dlcd_driver = {
	.probe = dlcd_probe,
	//.suspend = dlcd_suspend, 
	//.resume = dlcd_resume,
	.remove = dlcd_remove,
	.driver = {
		   .name = "dlcd",
		   .of_match_table = of_match_ptr(dlcd_id),	
	},
};


static int __init dlcd_init(void)
{
	int ret = 0;
	TM_TRACE_FUNC
	ret = platform_driver_register(&dlcd_driver);
	if (ret) {
		DLCD_DBG("---- dlcd driver register error:(%d)\n", ret);
		return ret;
	} else {
		DLCD_DBG("----dlcd platform driver register done!\n");
	}

	return 0;

}

static void dlcd_exit(void)
{
	TM_TRACE_FUNC
	DLCD_DBG("----dlcd_exit\n");
	platform_driver_unregister(&dlcd_driver);

}

module_init(dlcd_init);
module_exit(dlcd_exit);

MODULE_DESCRIPTION("Prize Digital LCD Driver");
MODULE_AUTHOR("zhaopengge <zhaopengge@szprize.com>");
MODULE_LICENSE("GPL");
