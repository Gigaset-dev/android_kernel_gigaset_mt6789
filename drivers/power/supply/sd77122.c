/*
 * Copyright(c) Bigmtech, 2021. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/io.h>
//#include <mach/gpio.h>
#include <linux/gpio.h>

#include "sd77122.h"
int32_t sd77122_enable_qnqp(void);
int32_t sd77122_disable_qnqp(void);
int32_t sd77122_enter_prep_to_parrell_mode(void);
int32_t sd77122_enter_parrell_to_prep_mode(void);
int32_t sd77122_judge_protect_or_prep_mode(void);
int32_t sd77122_parallel_ilimit(int32_t timeout, int32_t curr);
int32_t sd77122_prep_ilimit(int32_t timeout, int32_t curr);

const char *sys_mode[]=
{
  "power_on_startup",
  "Reserved",
  "active_protect",
  "Reserved",
  "active_serial",  
  "ini_parallel",
  "active_pre_p",  
  "active_parallel",
  "sleep_parallel",
  "Reserved",  
};

struct sd77122_chip 
{
	struct device *dev;
	struct i2c_client * client;
	struct delayed_work sd77122_work;
	struct delayed_work sd77122_prep_work;
	struct mutex i2c_rw_lock;
	int32_t	component_id;
};

struct sd77122_chip *g_sd77122_chip = NULL;

int32_t sd77122_enter_parallel_mode(void);

static int32_t sd77122_dump_msg(struct sd77122_chip *chip);
/*----------------------------------------------------------------------i2c module start----------------------------------------------------------------------------*/
static int32_t sd77122_read_data(struct sd77122_chip *chip, uint8_t addr, uint8_t *buf,uint8_t len)
{
	int32_t ret = 0;
	int32_t i = 0;
	usleep_range(1000,2000);
	mutex_lock(&chip->i2c_rw_lock);
	for(i=0; i<4; i++)
	{
		ret = i2c_smbus_read_i2c_block_data(chip->client, addr, len, buf);
		if(ret >= 0)
			break;
		else
			dev_err(chip->dev, "%s: reg[0x%02X] err %d, %d times\n",__func__, addr, ret, i);
	}
	
	mutex_unlock(&chip->i2c_rw_lock);

 return (i>=4) ? -1 : 0;
}

static int32_t sd77122_write_data(struct sd77122_chip *chip, uint8_t addr, uint8_t *buf ,uint8_t len)
{
	int32_t ret = 0;
	int32_t i = 0;
	usleep_range(1000,2000);
	mutex_lock(&chip->i2c_rw_lock);
	for(i=0; i<4; i++)
	{
		ret = i2c_smbus_write_i2c_block_data(chip->client, addr, len, buf);
		if(ret >= 0)
			break;
		else
			dev_err(chip->dev, "%s: reg[0x%02X] err %d, %d times\n",__func__, addr, ret, i);
	}
	mutex_unlock(&chip->i2c_rw_lock);
	return (i>=4) ? -1 : 0;
}

#if (1 == PEC_ENABLE)
static uint8_t i2cif_calc_pec(uint8_t data, uint8_t crcin)  
{
	//Calculate CRC-8 value; uses polynomial input
	uint8_t crc8  = data ^ crcin;
	uint8_t index = 0;
	for (index = 0; index < 8; index++) {
		if (crc8 & 0x80)
			crc8 = (crc8 << 1) ^ I2C_PEC_POLY;
		else
			crc8 = (crc8 << 1);
	}
	return crc8;
}

static int32_t pec_verification(uint8_t init_pec,uint8_t* pdata,uint8_t len, uint8_t pec_receive)
{
	uint8_t i = 0;
	uint8_t pec = init_pec;

	for(i=0;i<len;i++)
		pec = i2cif_calc_pec(pdata[i],pec);
		
	if(pec_receive != pec)
	{
		// for(i=0;i<len;i++)
		// 	pr_err("data[%d] = 0x%02X\n",i,pdata[i]);

		pr_err("correct pec value 0x%02X, receive pec value 0x%02X\n",pec,pec_receive);

		return -1;
	}
	return 0;
}
#endif

static int32_t sd77122_write_word(struct sd77122_chip *chip,uint8_t index,uint16_t data)
{
	uint8_t w_buf[5] = {0};
	uint8_t pec = 0;
	uint8_t i = 0;
	pr_err("data write,index:0x%x \n", data, index);

	w_buf[0] = (uint8_t)((data & 0xFF00) >> 8);
	w_buf[1] = (uint8_t)(data & 0x00FF);

	pec = i2cif_calc_pec(SD77122_SLAVE_ADDRESS,0);
	pec = i2cif_calc_pec(index,pec);
	
	for(i=0;i<2;i++)
		pec = i2cif_calc_pec(pec,w_buf[i]);
	
	w_buf[2] = pec;

	if(sd77122_write_data(chip,index, w_buf, 3) >= 0)
		return 1;

	return -1; 
}

static int32_t sd77122_read_word(struct sd77122_chip *chip,uint8_t index,uint16_t *data_buf)
{
	int32_t ret = 0;
	uint8_t r_buf[5] = {0};
	uint8_t pec = 0;

	if(NULL == data_buf)
		return -1;

	ret = sd77122_read_data(chip,index, r_buf, 3);
	if (ret) 
	{
		pr_err("%s:i2c_read error\n",__func__);
		return -1;
	}

	pec = i2cif_calc_pec(SD77122_SLAVE_ADDRESS,pec);
	pec = i2cif_calc_pec(index,pec);
	pec = i2cif_calc_pec(SD77122_SLAVE_ADDRESS | 1,pec);

	ret = pec_verification(pec,r_buf,2,r_buf[2]);
	if(0 == ret)
		*data_buf = r_buf[0] << 8 | r_buf[1];
	else
	{
		pr_err("addr:0x%x, reg:0x%x, PEC Error\n", SD77122_SLAVE_ADDRESS, index);
		return -1;
	}

	return 1;
}

static int32_t sd77122_update(struct sd77122_chip *chip,uint8_t RegNum, uint16_t val, uint16_t MASK,uint16_t SHIFT)
{
	uint16_t data = 0;
	int32_t ret = 0;

	ret = sd77122_read_word(chip,RegNum, &data);
	pr_err("data read:0x%x\n", data);

	if(ret < 0)
		return ret;

	if(((data & MASK) >> SHIFT) != val)
	{
		data &= ~(MASK);
		data |= (val << SHIFT);
		return sd77122_write_word(chip,RegNum, data);
	}
	return 0;
}

#if 0
static int32_t sd77122_update_reg0c(struct sd77122_chip *chip,uint8_t RegNum, uint16_t val, uint16_t MASK,uint16_t SHIFT)
{
	uint16_t data = 0;
	int32_t ret = 0;

	ret = sd77122_read_word(chip,RegNum, &data);
	pr_err("data read:0x%x\n", data);

	if(ret < 0)
		return ret;

	//if(((data & MASK) >> SHIFT) != val)
	{
		data &= ~(MASK);
		data |= (val << SHIFT);
		return sd77122_write_word(chip,RegNum, data);
	}
	return 0;
}
#endif
/*----------------------------------------------------------------------i2c module end ----------------------------------------------------------------------------*/

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
int32_t sd77122_enbale_adc(void)
{
	return sd77122_update(g_sd77122_chip,SD77122_R14,0x7F,ADC_SEL_EN_MASK,ADC_SEL_EN_SHIFT);
}
EXPORT_SYMBOL(sd77122_enbale_adc);

int32_t sd77122_disbale_adc(void)
{
	return sd77122_update(g_sd77122_chip,SD77122_R14,0,ADC_SEL_EN_MASK,ADC_SEL_EN_SHIFT);
}
EXPORT_SYMBOL(sd77122_disbale_adc);


int32_t sd77122_enable_qnqp(void)
{
	int32_t ret = 0;
	
	ret = sd77122_update(g_sd77122_chip,SD77122_R0A, 1, BAT_DET_SW_CTRL_MASK,BAT_DET_SW_CTRL_SHIFT);
	if(ret < 0)return -1;
	
	return sd77122_update(g_sd77122_chip,SD77122_R0A, 1, BAT_DET_MODE_SEL_MASK,BAT_DET_MODE_SEL_SHIFT);
}
EXPORT_SYMBOL(sd77122_enable_qnqp);

int32_t sd77122_disable_qnqp(void)
{
	int32_t ret = 0;
	
	ret = sd77122_update(g_sd77122_chip,SD77122_R0A, 0, BAT_DET_SW_CTRL_MASK,BAT_DET_SW_CTRL_SHIFT);
	if(ret < 0)return -1;
	
	return sd77122_update(g_sd77122_chip,SD77122_R0A, 1, BAT_DET_MODE_SEL_MASK,BAT_DET_MODE_SEL_SHIFT);
}
EXPORT_SYMBOL(sd77122_disable_qnqp);


int32_t sd77122_enter_prep_to_parrell_mode(void)
{
	int32_t ret = 0;
	
	ret = sd77122_update(g_sd77122_chip,SD77122_R0B,0x1,PRE_CURR_SET_OPT_MASK,PRE_CURR_SET_OPT_SHIFT);
	if(ret < 0)return -1;

	return sd77122_update(g_sd77122_chip,SD77122_R0C,31,PRE_CUR_SET_SW_MASK,PRE_CUR_SET_SW_SHIFT);//31=0x1f	
}
EXPORT_SYMBOL(sd77122_enter_prep_to_parrell_mode);

int32_t sd77122_enter_parrell_to_prep_mode(void)
{
	int32_t ret = 0;

	ret = sd77122_disable_qnqp();
	if(ret < 0)
		{	pr_info("error sd77122_disable_qnqp\n");
		return -1;
		}
	return sd77122_enable_qnqp();
}
EXPORT_SYMBOL(sd77122_enter_parrell_to_prep_mode);

//ver A1 BUG, B0 not
int32_t sd77122_judge_protect_or_prep_mode(void)
{
	int32_t ret = 0;
	uint16_t reg_val = 0;
	
	ret = sd77122_read_word(g_sd77122_chip,SD77122_R06,&reg_val);
    if(ret<0)return -1;

	reg_val = reg_val & 0xf;
	if(0x2 == reg_val)
	{
		pr_info("enter protect mode\n");

		ret = sd77122_read_word(g_sd77122_chip,SD77122_R08,&reg_val);
    	if(ret<0)return -1;

		reg_val = reg_val & 0x1;
		if(1 == reg_val)
		{
			ret = sd77122_enter_parallel_mode();
			if(ret < 0)return -1;
			pr_info("enter parallel mode\n");
		}
	}
	return 0;

}
EXPORT_SYMBOL(sd77122_judge_protect_or_prep_mode);

/*
*timeout: 
*#define WDT_SETTING_4S               0x00
*#define WDT_SETTING_8S               0x01
*#define WDT_SETTING_16S              0x10
*#define WDT_SETTING_32S              0x11
*/
int32_t sd77122_parallel_ilimit(int32_t timeout, int32_t curr)
{
	int32_t ret = 0;
	
#if 0//ver A1	
	ret = sd77122_update(g_sd77122_chip,SD77122_R0A,0x1,BAT_CURR_LMT_EN_MASK,BAT_CURR_LMT_EN_SHIFT);
	if(ret < 0)return -1;

	ret = sd77122_update(g_sd77122_chip,SD77122_R0B,0x1,PRE_CURR_SET_OPT_MASK,PRE_CURR_SET_OPT_SHIFT);
	if(ret < 0)return -1;

	ret = sd77122_update(g_sd77122_chip,SD77122_R0B,timeout,PRE_CUR_WD_SET_MASK,PRE_CUR_WD_SET_SHIFT);
	if(ret < 0)return -1;

	ret = sd77122_update_reg0c(g_sd77122_chip,SD77122_R0C,curr,PRE_CUR_SET_SW_MASK,PRE_CUR_SET_SW_SHIFT);	
	if(ret < 0)return -1;
#else//ver B0
	ret = sd77122_update(g_sd77122_chip,SD77122_R0A,0x1,BAT_CURR_LMT_EN_MASK,BAT_CURR_LMT_EN_SHIFT);
	if(ret < 0)return -1;

	ret = sd77122_update(g_sd77122_chip,SD77122_R13,curr,MAX_CURR_SET_MASK,MAX_CURR_SET_SHIFT);
	if(ret < 0)return -1;

#endif

	return 0;
}
EXPORT_SYMBOL(sd77122_parallel_ilimit);

/*
*timeout: 
*#define WDT_SETTING_4S               0x00
*#define WDT_SETTING_8S               0x01
*#define WDT_SETTING_16S              0x10
*#define WDT_SETTING_32S              0x11
*/
int32_t sd77122_prep_ilimit(int32_t timeout, int32_t curr)
{
	int32_t ret = 0;

#if 0//ver A1	
	ret = sd77122_update(g_sd77122_chip,SD77122_R0B,0x1,PRE_CURR_SET_OPT_MASK,PRE_CURR_SET_OPT_SHIFT);
	if(ret < 0)return -1;

	ret = sd77122_update(g_sd77122_chip,SD77122_R0B,timeout,PRE_CUR_WD_SET_MASK,PRE_CUR_WD_SET_SHIFT);
	if(ret < 0)return -1;

	ret = sd77122_update_reg0c(g_sd77122_chip,SD77122_R0C,curr,PRE_CUR_SET_SW_MASK,PRE_CUR_SET_SW_SHIFT);	
	if(ret < 0)
	{
		pr_info("test 0x0C write error=%d\n");
		return -1;
	}
#else//ver B0
	ret = sd77122_update(g_sd77122_chip,SD77122_R0B,0x1,PRE_CURR_SET_OPT_MASK,PRE_CURR_SET_OPT_SHIFT);
	if(ret < 0)return -1;

	ret = sd77122_update(g_sd77122_chip,SD77122_R0C,0x1,PRE_CURR_WD_DIS_MASK,PRE_CURR_WD_DIS_SHIFT);
	if(ret < 0)return -1;

	ret = sd77122_update(g_sd77122_chip,SD77122_R0C,curr,PRE_CUR_SET_SW_MASK,PRE_CUR_SET_SW_SHIFT);
	if(ret < 0)return -1;

#endif
	
	return 0;
}
EXPORT_SYMBOL(sd77122_prep_ilimit);

#if 1
static void sd77122_prep_work_func(struct work_struct *work)
{
	struct sd77122_chip *chip = container_of(work, struct sd77122_chip,sd77122_prep_work.work);
	uint16_t reg_val;

	sd77122_read_word(g_sd77122_chip,SD77122_R06,&reg_val);
    		
	//printk("sd77122 enter sd77122_prep_work_func\n");

	reg_val = reg_val & 0xf;
	if(0x6 == reg_val)
	{
		printk("sd77122 enter Prep parallel mode\n");
		sd77122_prep_ilimit(3,3);	
	}
	else if (0x7 == reg_val){
		printk("sd77122 enter active parallel mode\n");
		sd77122_parallel_ilimit(3,1);
	}
	
	schedule_delayed_work(&chip->sd77122_prep_work, msecs_to_jiffies(3000));
	
}
#endif
int32_t sd77122_set_profile(void)//charge open qnpn
{
	sd77122_enable_qnqp();
	usleep_range(1000,2000);
	printk("sd77122 enter sd77122_set_profile\n");
	
	schedule_delayed_work(&g_sd77122_chip->sd77122_prep_work, msecs_to_jiffies(1000));

	return 0;
}
EXPORT_SYMBOL(sd77122_set_profile);

int32_t sd77122_set_standby(void)//discharge close qnpn
{
	printk("sd77122 enter sd77122_set_standby\n");
	return sd77122_disable_qnqp();

}
EXPORT_SYMBOL(sd77122_set_standby);

int32_t sd77122_get_pack2_vol(uint16_t *volt_val)
{
	uint16_t reg_val = 0;
    int32_t ret = 0 ;
	
	ret = sd77122_read_word(g_sd77122_chip,SD77122_R16,&reg_val);
    if(ret<0)
    {
        return ret;
    }
	
	*volt_val =(reg_val-2048)*5 /2;
	 return 0;
}
EXPORT_SYMBOL(sd77122_get_pack2_vol);


int32_t sd77122_get_pack1_vol(uint16_t *volt_val)
{
	uint16_t reg_val = 0;
    int32_t ret =0 ;
	
	ret= sd77122_read_word(g_sd77122_chip,SD77122_R17,&reg_val);
    if(ret<0)
    {
        return ret;
    }
	
	*volt_val =(reg_val-2048)*5 /2;//mv
	 return 0;
}
EXPORT_SYMBOL(sd77122_get_pack1_vol);




int32_t sd77122_disable_event_mask(void)
{
	return sd77122_update(g_sd77122_chip,SD77122_R07,0x7FFF,ALL_EVENT_MASK,0);
}

int32_t sd77122_enter_ship_mode(void)
{
	return sd77122_write_word(g_sd77122_chip,SD77122_R06,ENTER_SHIP_MODE);
}

int32_t sd77122_exit_ship_mode(void)
{
	return sd77122_write_word(g_sd77122_chip,SD77122_R06,EXIT_SHIP_MODE);
}

int32_t sd77122_get_sys_mode_status(uint16_t *status_val)
{
    uint16_t reg_val = 0;
    int32_t ret =0 ;
	 
	ret = sd77122_read_word(g_sd77122_chip,SD77122_R06,&reg_val);
    if(ret<0)
    {
        return ret;	
    }
	
	*status_val = reg_val&0xf;
	 
	return 0;
}
EXPORT_SYMBOL(sd77122_get_sys_mode_status);


static int32_t temp_val =0 ;
//static int32_t bat_on_status =1 ;

int32_t sd77122_check_bat_on(uint16_t pack1_volt,uint16_t pack2_volt)
{
	uint16_t reg_val = 0;
    int32_t ret =0 ;
	
	uint16_t status_val = 0;
	
	sd77122_get_sys_mode_status(&status_val);
	//0A
	if((status_val == 6)||(status_val == 7))
	{
		sd77122_read_word(g_sd77122_chip,SD77122_R0A,&reg_val);
		temp_val = reg_val;
		sd77122_write_word(g_sd77122_chip,SD77122_R0A,(reg_val | 0x3000));
		//bat_on_status =  0;
	}
	
	ret= sd77122_read_word(g_sd77122_chip,SD77122_R0A,&reg_val);
    if(ret<0)
    {
        return ret;
    }
	ret = (reg_val & 0xc000) >> 14;
	
    if((status_val == 2)&&(ret == 1))
	{
		//bat_on_status = 0;
		sd77122_write_word(g_sd77122_chip,SD77122_R0A,temp_val);
	}
	sd77122_dump_msg(g_sd77122_chip);
	return ret;
}

int32_t sd77122_is_in_ship_mode(void)
{
     uint16_t reg_val = 0;
     int32_t ret =0 ;

     ret= sd77122_read_word(g_sd77122_chip,SD77122_R06,&reg_val);

     if(ret <0)
        return ret;
      
     if((reg_val& SHIP_MODE_MASK)>>SHIP_MODE_SHIFT)
       return 1;

     return 0;     
}

int32_t sd77122_enter_parallel_mode(void)
{
	return sd77122_write_word(g_sd77122_chip,SD77122_R06,PARALLEL_MODE);
}
EXPORT_SYMBOL(sd77122_enter_parallel_mode);


static int32_t sd77122_dump_msg(struct sd77122_chip *chip)
{
	uint8_t i = 0;
	uint16_t data[SD77122_REG_NUM] = { 0 };
	int32_t ret = 0;
	uint16_t pack1_volt = 0,pack2_volt = 0;
	uint16_t sts_val= 0;

	for (i = 0; i < SD77122_REG_NUM; i++) 
	{
		ret = sd77122_read_word(chip,i, &data[i]);
		if (ret < 0) 
		{
			pr_err("i2c transfor error\n");
			return -1;
		}
	}

	pr_info("[0x00]=0x%04X, [0x01]=0x%04X, [0x02]=0x%04X, [0x03]=0x%04X, [0x04]=0x%04X, [0x05]=0x%04X, [0x06]=0x%04X, [0x07]=0x%04X \n",
			  data[0x00],    data[0x01],    data[0x02],    data[0x03],    data[0x04],    data[0x05],    data[0x06],    data[0x07]);

	pr_info("[0x08]=0x%04X, [0x09]=0x%04X, [0x0A]=0x%04X, [0x0B]=0x%04X, [0x0C]=0x%04X, [0x0D]=0x%04X, [0x0E]=0x%04X, [0x0F]=0x%04X \n",
			 data[0x08],	 data[0x09],   data[0x0A],    data[0x0B],	  data[0x0C],	  data[0x0D],	 data[0x0E],	data[0x0F]);

	pr_info("[0x10]=0x%04X, [0x11]=0x%04X, [0x12]=0x%04X,  [0x14]=0x%04X  ,[0x15]=0x%04X, [0x16]=0x%04X, [0x17]=0x%04X, [0x1B]=0x%04X, [0x1C]=0x%04X \n", 
	         data[0x10],    data[0x11],    data[0x12],    data[0x14],     data[0x15],    data[0x16],    data[0x17],		data[0x1B],    data[0x1C]);
    
	sd77122_get_pack1_vol(&pack1_volt);
	sd77122_get_pack2_vol(&pack2_volt);
	sd77122_get_sys_mode_status(&sts_val);
	pr_info("PACK1=%dmv,PACK2=%dmv,mode_code=%d,serial_parallel=%s mode.\n",pack1_volt, pack2_volt, sts_val, sys_mode[sts_val]);
	
	return 0;
}

static ssize_t show_pack1_voltage(struct device *dev,struct device_attribute *attr, char *buf)
{
	uint16_t pack1_volt = 0;
	sd77122_get_pack1_vol(&pack1_volt);

	return sprintf(buf,"%d\n", pack1_volt);
}

static ssize_t show_pack2_voltage(struct device *dev,struct device_attribute *attr, char *buf)
{
    uint16_t pack2_volt = 0;
	sd77122_get_pack2_vol(&pack2_volt);

	return sprintf(buf,"%d\n", pack2_volt);
}

static ssize_t show_sys_mode(struct device *dev,struct device_attribute *attr, char *buf)
{
	uint16_t sts_val= 0;
	sd77122_get_sys_mode_status(&sts_val);

	return sprintf(buf,"%s\n", sys_mode[sts_val]);
}



static ssize_t show_77121_msg(struct device *dev,struct device_attribute *attr, char *buf)
{
	uint16_t sts_val= 0;
    uint16_t pack1_volt = 0;
    uint16_t pack2_volt = 0;
	uint16_t bat_on= 0;
	sd77122_get_pack1_vol(&pack1_volt);
	sd77122_get_pack2_vol(&pack2_volt);
	bat_on = sd77122_check_bat_on(pack1_volt,pack2_volt);
	if (sd77122_get_sys_mode_status(&sts_val) < 0)
		return sprintf(buf,"pack1_volt=%d\r\npack2_volt=%d\r\nsys_mode=-1\r\n", pack1_volt,pack2_volt);

	return sprintf(buf,"pack2_volt=%d\r\npack1_volt=%d\r\n,pack2_volt=%d\r\n,bat_on=%d\r\n", pack2_volt,pack1_volt,sts_val,bat_on);  //pack2_volt=%d    sys_mode=%d  pack2_volt   sts_val
}

//static DEVICE_ATTR(check_pre_bat_on, 0444, show_check_pre_bat_on,show_check_pre_bat_on);	/* 664 */

static DEVICE_ATTR(pack1_voltage, 0444, show_pack1_voltage,NULL);	/* 664 */
static DEVICE_ATTR(pack2_voltage, 0444, show_pack2_voltage,NULL);	/* 664 */
static DEVICE_ATTR(sys_mode, 0444, show_sys_mode, NULL);
static struct attribute *sd77122_attributes[] = {
	&dev_attr_pack1_voltage.attr,
	&dev_attr_pack2_voltage.attr,
	&dev_attr_sys_mode.attr,
	NULL,
};

static DEVICE_ATTR(77121_msg, 0444, show_77121_msg,NULL);	/* 664 */


static struct attribute_group sd77122_attribute_group = {
	.attrs = sd77122_attributes,
};

int sd77122_create_sysfs(struct sd77122_chip *chip)
{
	int ret = 0;

	ret = sysfs_create_group(&chip->client->dev.kobj, &sd77122_attribute_group);
	if (ret) {
		printk("[EX]: sysfs_create_group() failed!!");
		sysfs_remove_group(&chip->client->dev.kobj, &sd77122_attribute_group);
		return -ENOMEM;
	} else {
		printk("[EX]: sysfs_create_group() succeeded!!");
	}

	return ret;
}

int sd77122_remove_sysfs(struct sd77122_chip *chip)
{
	sysfs_remove_group(&chip->client->dev.kobj, &sd77122_attribute_group);
	return 0;
}

static int32_t sd77122_detect_ic(struct sd77122_chip *chip)
{
	uint16_t hw_id = 0;
	int32_t ret = 0;

	ret = sd77122_read_word(chip,SD77122_R02,&hw_id);
	if(ret < 0)
		return ret;

	pr_info("hw_id:0x%02X\n",hw_id);
	
	if(SD77122_ICVERSION != hw_id)
		return -1;

	return 0;
}

static int32_t sd77122_get_component_id(struct sd77122_chip *chip)
{
	uint16_t vendor_id = 0, device_id = 0;
	uint16_t component_id =0 ;
	int32_t ret = 0;

	ret = sd77122_read_word(chip,SD77122_R00,&vendor_id);
	if(ret < 0)
		return ret;

	ret = sd77122_read_word(chip,SD77122_R01,&device_id);
	if(ret < 0)
		return ret;

	component_id = vendor_id << 8 | device_id;

	pr_info("vendor_id:0x%02X,device_id:0x%02X,component_id:0x%04X\n",vendor_id,device_id,component_id);
	
	return 0;
}
static void sd77122_hw_init(struct sd77122_chip *chip)
{
	sd77122_enbale_adc();
	sd77122_enter_parallel_mode();
	sd77122_disable_event_mask();
}

static void sd77122_work_func(struct work_struct *work)
{
	struct sd77122_chip *chip = container_of(work, struct sd77122_chip,sd77122_work.work);

	//sd77122_judge_protect_or_prep_mode();//ver A1

	sd77122_dump_msg(chip);
	
	schedule_delayed_work(&chip->sd77122_work, msecs_to_jiffies(3000));
}

static int32_t sd77122_driver_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	int32_t ret = 0;
	struct sd77122_chip *chip = NULL;

	chip = devm_kzalloc(&client->dev, sizeof(struct sd77122_chip),GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->dev = &client->dev;
	//init mutex asap,before iic operate
	mutex_init(&chip->i2c_rw_lock);

    g_sd77122_chip = chip;
	
	i2c_set_clientdata(client, chip);

	ret = sd77122_detect_ic(chip);
	if(ret < 0)
	{
		pr_err("do not detect ic, exit\n");
		return -ENODEV;
	}	
	
	sd77122_get_component_id(chip);
		
	sd77122_hw_init(chip);

	sd77122_dump_msg(chip);

	ret = device_create_file(&client->dev, &dev_attr_77121_msg);
	if (ret < 0) 
		pr_err("create sysfs failed %d\n",ret);

	INIT_DELAYED_WORK(&chip->sd77122_work, sd77122_work_func);
	INIT_DELAYED_WORK(&g_sd77122_chip->sd77122_prep_work, sd77122_prep_work_func);
	schedule_delayed_work(&chip->sd77122_work, msecs_to_jiffies(1000));

	ret = sd77122_create_sysfs(chip);
	if (ret)
		printk("create sysfs node fail");

	ret = sysfs_create_link(NULL, &client->dev.kobj, "sd77122");
	if (ret)
		printk("link flash sd77122 fail");

    return 0;		
}

static int sd77122_resume(struct device *dev_chip)
{
	struct sd7601_chip *chip = i2c_get_clientdata(to_i2c_client(dev_chip));

	if(chip == NULL)
		return 0;

	return 0;
}

static int sd77122_suspend(struct device *dev_chip)
{
	struct sd7601_chip *chip = i2c_get_clientdata(to_i2c_client(dev_chip));
	if(chip == NULL)
		return 0;

	return 0;
}
static int32_t sd77122_driver_remove(struct i2c_client *client)
{
	struct sd77122_chip *chip = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&chip->sd77122_work);

	mutex_destroy(&chip->i2c_rw_lock);

	return 0;
}
static void sd77122_driver_shutdown(struct i2c_client *client)
{
	// struct sd77122_chip *chip = i2c_get_clientdata(client);
	// sd77122_set_charger_en(chip,0);
}

static const struct dev_pm_ops sd77122_pm_ops = {
	.resume			= sd77122_resume,
	.suspend		= sd77122_suspend,
};

static const struct of_device_id sd77122_of_match[] = {
	{.compatible = "bigmtech,sd77122"},
	{},
};

static const struct i2c_device_id sd77122_i2c_id[] = { 
	{"sd77122",   0}, 
	{ },

};

static struct i2c_driver sd77122_driver = {
	.driver = {
		.name 			 = "sd77122",
		.owner 			 = THIS_MODULE,
		.pm				 = &sd77122_pm_ops,
		.of_match_table = sd77122_of_match,
	},
	.id_table 	= sd77122_i2c_id,
	.probe 		= sd77122_driver_probe,
	.remove		= sd77122_driver_remove,
	.shutdown	= sd77122_driver_shutdown,
};

static int32_t __init sd77122_init(void)
{
	if (0 != i2c_add_driver(&sd77122_driver)) 
		pr_info("failed to register sd77122 i2c driver.\n");
	else 
		pr_info("Success to register sd77122 i2c driver.\n");
	
	return 0;
}

static void __exit sd77122_exit(void)
{
	i2c_del_driver(&sd77122_driver);
}

module_init(sd77122_init);
module_exit(sd77122_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("sd77122 Driver");
MODULE_AUTHOR("cheng.huang <cheng.huang@bigmtech.com>");
