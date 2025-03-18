
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
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/spi/spi.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_PM_SLEEP
#include <linux/pm_wakeup.h>
#endif
#include <linux/firmware.h>

#include "w25qxx.h"
#define DPS_DEV_NAME  "mediatek,gc9a01"

#define H3_FW_SIZE                     (512 * 1024)
#define H3_FW_ID						0x55AA
//#define H3_FW_BIN_NAME "H3_55AA1202.bin"
#define H3_FW_BIN_NAME "H3_55AA1217.bin"

int miniscreen_width = 0;
int miniscreen_height = 0;

struct gc9a01_struct {
	struct delayed_work dwork;
	struct spi_device *spi;
	struct regulator *vdd;
	u8 *fw_mem;
};

struct gc9a01_struct gc9a01_data;

static struct pinctrl *gc9a01_pinctrl;

//static struct pinctrl_state *gc9a01_reset_active;
//static struct pinctrl_state *gc9a01_reset_suspend;
//static struct pinctrl_state *gc9a01_bl_active;
//static struct pinctrl_state *gc9a01_bl_suspend;
//static struct pinctrl_state *gc9a01_dc_active;
//static struct pinctrl_state *gc9a01_dc_suspend;
static struct pinctrl_state *spi1_as_cs;
static struct pinctrl_state *spi1_as_ck;
static struct pinctrl_state *spi1_as_mi;
static struct pinctrl_state *spi1_as_mo;

static struct pinctrl_state *spi1_as_cs_hi;
static struct pinctrl_state *spi1_as_ck_hi;
static struct pinctrl_state *spi1_as_mi_hi;
static struct pinctrl_state *spi1_as_mo_hi;
//static struct pinctrl_state *gc9a01_bl_pwm;

static int gc9a01_pinctrl_init(struct device *dev)
{
	int ret = 0;

	/* get pinctrl */
	gc9a01_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(gc9a01_pinctrl)) {
		pr_err("Failed to get devm_pinctrl_get\n");
		ret = PTR_ERR(gc9a01_pinctrl);
	}

	/* get reset pinctrl */
	//gc9a01_reset_active = pinctrl_lookup_state(gc9a01_pinctrl, "reset_active");
	//if (IS_ERR(gc9a01_reset_active)) {
	//	pr_err("Failed to init gc9a01_reset_active\n");
	//	ret = PTR_ERR(gc9a01_reset_active);
	//}
	//gc9a01_reset_suspend = pinctrl_lookup_state(gc9a01_pinctrl, "reset_suspend");
	//if (IS_ERR(gc9a01_reset_suspend)) {
	//	pr_err("Failed to init gc9a01_reset_suspend\n");
	//	ret = PTR_ERR(gc9a01_reset_suspend);
	//}

	/* get bl pinctrl */
	//gc9a01_bl_active = pinctrl_lookup_state(gc9a01_pinctrl, "bl_active");
	//if (IS_ERR(gc9a01_bl_active)) {
	//	pr_err("Failed to init gc9a01_bl_active\n");
	//	ret = PTR_ERR(gc9a01_bl_active);
	//}
	//gc9a01_bl_suspend = pinctrl_lookup_state(gc9a01_pinctrl, "bl_suspend");
	//if (IS_ERR(gc9a01_bl_suspend)) {
	//	pr_err("Failed to init gc9a01_bl_suspend\n");
	//	ret = PTR_ERR(gc9a01_bl_suspend);
	//}

	/* get dc pinctrl */
	//gc9a01_dc_active = pinctrl_lookup_state(gc9a01_pinctrl, "dc_active");
	//if (IS_ERR(gc9a01_dc_active)) {
	//	pr_err("Failed to init gc9a01_dc_active\n");
	//	ret = PTR_ERR(gc9a01_dc_active);
	//}
	//gc9a01_dc_suspend = pinctrl_lookup_state(gc9a01_pinctrl, "dc_suspend");
	//if (IS_ERR(gc9a01_dc_suspend)) {
	//	pr_err("Failed to init gc9a01_dc_suspend\n");
	//	ret = PTR_ERR(gc9a01_dc_suspend);
	//}

	/* get spi pinctrl */
	spi1_as_cs = pinctrl_lookup_state(gc9a01_pinctrl, "spi1_as_cs_t");
	if (IS_ERR(spi1_as_cs)) {
		pr_err("Failed to init spi1_as_cs\n");
		ret = PTR_ERR(spi1_as_cs);
	}
	spi1_as_ck = pinctrl_lookup_state(gc9a01_pinctrl, "spi1_as_ck_t");
	if (IS_ERR(spi1_as_ck)) {
		pr_err("Failed to init spi1_as_ck\n");
		ret = PTR_ERR(spi1_as_ck);
	}
	spi1_as_mi = pinctrl_lookup_state(gc9a01_pinctrl, "spi1_as_mi_t");
	if (IS_ERR(spi1_as_mi)) {
		pr_err("Failed to init spi1_as_mi\n");
		ret = PTR_ERR(spi1_as_mi);
	}
	spi1_as_mo = pinctrl_lookup_state(gc9a01_pinctrl, "spi1_as_mo_t");
	if (IS_ERR(spi1_as_mo)) {
		pr_err("Failed to init spi1_as_mo\n");
		ret = PTR_ERR(spi1_as_mo);
	}

	spi1_as_cs_hi = pinctrl_lookup_state(gc9a01_pinctrl, "spi_cs_hi");
	if (IS_ERR(spi1_as_cs_hi)) {
		pr_err("Failed to init spi1_as_cs_hi\n");
		ret = PTR_ERR(spi1_as_cs_hi);
	}
	spi1_as_ck_hi = pinctrl_lookup_state(gc9a01_pinctrl, "spi_ck_hi");
	if (IS_ERR(spi1_as_ck_hi)) {
		pr_err("Failed to init spi1_as_ck_hi\n");
		ret = PTR_ERR(spi1_as_ck_hi);
	}
	spi1_as_mi_hi = pinctrl_lookup_state(gc9a01_pinctrl, "spi_mi_hi");
	if (IS_ERR(spi1_as_mi_hi)) {
		pr_err("Failed to init spi1_as_mi_hi\n");
		ret = PTR_ERR(spi1_as_mi_hi);
	}
	spi1_as_mo_hi = pinctrl_lookup_state(gc9a01_pinctrl, "spi1_mo_hi");
	if (IS_ERR(spi1_as_mo_hi)) {
		pr_err("Failed to init spi1_as_mo_hi\n");
		ret = PTR_ERR(spi1_as_mo_hi);
	}

	/* get bl pwm pinctrl */
	//gc9a01_bl_pwm = pinctrl_lookup_state(gc9a01_pinctrl, "bl_pwm");
	//if (IS_ERR(gc9a01_bl_pwm)) {
	//	pr_err("Failed to init gc9a01_bl_pwm\n");
	//	ret = PTR_ERR(gc9a01_bl_pwm);
	//}

	//pinctrl_select_state(gc9a01_pinctrl, spi1_as_cs);
	//pinctrl_select_state(gc9a01_pinctrl, spi1_as_ck);
	//pinctrl_select_state(gc9a01_pinctrl, spi1_as_mi);
	//pinctrl_select_state(gc9a01_pinctrl, spi1_as_mo);

	pinctrl_select_state(gc9a01_pinctrl, spi1_as_cs_hi);
	pinctrl_select_state(gc9a01_pinctrl, spi1_as_ck_hi);
	pinctrl_select_state(gc9a01_pinctrl, spi1_as_mi_hi);
	pinctrl_select_state(gc9a01_pinctrl, spi1_as_mo_hi);
	//pinctrl_select_state(gc9a01_pinctrl, gc9a01_bl_suspend);

	return ret;
}

void gc9a01_bl_ctl(int level)
{
	pr_err("%s %d level=%d\n", __func__, __LINE__, level);

	//if (level)
	//	pinctrl_select_state(gc9a01_pinctrl, gc9a01_bl_active);
	//else
	//	pinctrl_select_state(gc9a01_pinctrl, gc9a01_bl_suspend);
}

void gc9a01_reset_ctl(int on)
{
	pr_err("%s %d on=%d\n", __func__, __LINE__, on);

	//if (on)
	//	pinctrl_select_state(gc9a01_pinctrl, gc9a01_reset_active);
	//else
	//	pinctrl_select_state(gc9a01_pinctrl, gc9a01_reset_suspend);
}

void gc9a01_dc_ctl(int on)
{
	// pr_err("%s %d on=%d\n", __func__, __LINE__, on);

	//if (on)
	//	pinctrl_select_state(gc9a01_pinctrl, gc9a01_dc_active);
	//else
	//	pinctrl_select_state(gc9a01_pinctrl, gc9a01_dc_suspend);
}

static int gc9a01_sync_write(uint8_t *tx, uint32_t len)
{
	int ret = 0;
	struct spi_message m;
	struct spi_transfer t = {
		.tx_buf = tx,
		.len = len,
		//.speed_hz	= gc9a01_data.spi->speed_hz,
	};

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	ret = spi_sync(gc9a01_data.spi, &m);
	if (ret == 0)
		return m.actual_length;
	
	return ret;
}

int gc9a01_write_cmd(uint8_t cmd)
{
	uint8_t tx_buf[2] = {0};
	int ret = 0;
	
	gc9a01_dc_ctl(0);
	tx_buf[0] = cmd;
	ret = gc9a01_sync_write(tx_buf, 1);
	gc9a01_dc_ctl(1);
	
	return ret;
}

int gc9a01_write_data(uint8_t data)
{
	uint8_t tx_buf[2] = {0};
	int ret = 0;
	
	gc9a01_dc_ctl(1);
	tx_buf[0] = data;
	ret = gc9a01_sync_write(tx_buf, 1);
	gc9a01_dc_ctl(1);
	
	return ret;
}

int gc9a01_read_data(u8 *tx_buf, u8 *rx_buf, int len)
{
    int status;
    struct spi_transfer t = {
        .tx_buf = tx_buf,
        .rx_buf = rx_buf,
        .len = len,
    };

    struct spi_device *spi = gc9a01_data.spi;

    // 使用spi_message传递transfer
    struct spi_message m;
    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
 
	pr_err("%s %d\n", __func__, __LINE__);
 
    // 执行SPI传输
    status = spi_sync(spi, &m);
    if (status) {
        // 处理错误
		pr_err("spi syna error %s %d\n", __func__, __LINE__);
        return status;
    }

    return 0;
}

/*******************************************************************************
 函数: SPI_FlashPowerdown
 功能: SPI_FLASH进入掉电模式
 参数: 无
 返回: 无
*******************************************************************************/
/*
static int H3_SPI_FlashPowerdown(void)
{
	unsigned char tx_buf[2] = {0};
	int ret = 0;

	tx_buf[0] = W25X_PowerDown;

	pr_err("%s %d\n", __func__, __LINE__);
	ret = gc9a01_sync_write(tx_buf, 1);

	return ret;
}   
*/
/*******************************************************************************
 函数: SPI_FlashWakeup
 功能: SPI_FLASH从掉电模式唤醒
 参数: 无
 返回: 无
*******************************************************************************/
static int H3_SPI_FlashWakeup(void)   
{
	unsigned char tx_buf[2] = {0};
	int ret = 0;

	tx_buf[0] = W25X_ReleasePowerDown;

	pr_err("%s %d\n", __func__, __LINE__);
	ret = gc9a01_sync_write(tx_buf, 1);

	return ret;
}

/*******************************************************************************
 函数: SPI_FlashReadDeviceid
 功能: 读取FLASH器件ID
 参数: 无
 返回: 无
*******************************************************************************/
static uint32_t H3_SPI_FlashReadDeviceid(void)
{
	unsigned char h3_read_buf[6];
	unsigned char h3_write_buf[6];

	//h3_write_buf[0] = W25X_DeviceID;

	h3_write_buf[0] = W25X_DeviceID;
	h3_write_buf[1] = 0xFF;
	h3_write_buf[2] = 0xFF;
	h3_write_buf[3] = 0xFF;
	gc9a01_read_data(h3_write_buf, h3_read_buf, 5);
	pr_err("%s %d read_buf[0] = 0x%x h3_read_buf[1] = 0x%x\n", __func__, __LINE__, h3_read_buf[0], h3_read_buf[1]);
	pr_err("%s %d read_buf[2] = 0x%x h3_read_buf[3] = 0x%x\n", __func__, __LINE__, h3_read_buf[2], h3_read_buf[3]);
	pr_err("%s %d read_buf[4] = 0x%x\n", __func__, __LINE__, h3_read_buf[4]);

	msleep(2);

	h3_write_buf[0] = W25X_ManufactDeviceID;
	h3_write_buf[1] = 0x00;
	h3_write_buf[2] = 0x00;
	h3_write_buf[3] = 0x00;

	gc9a01_read_data(h3_write_buf, h3_read_buf,6);
	pr_err("%s %d read_buf[0] = 0x%x h3_read_buf[1] = 0x%x\n", __func__, __LINE__, h3_read_buf[0], h3_read_buf[1]);
	pr_err("%s %d read_buf[2] = 0x%x h3_read_buf[3] = 0x%x\n", __func__, __LINE__, h3_read_buf[2], h3_read_buf[3]);
	pr_err("%s %d read_buf[4] = 0x%x h3_read_buf[5] = 0x%x\n", __func__, __LINE__, h3_read_buf[4], h3_read_buf[5]);

	return h3_read_buf[4];
}

/*******************************************************************************
 函数: SPI_FlashReadJedecid
 功能: 读取jedecid
 参数: 无
 返回: 无
*******************************************************************************/
static uint32_t H3_SPI_FlashReadJedecid(void)
{

	int temp = 0;
	unsigned char h3_read_buf[4];
	unsigned char h3_write_buf[4];

	h3_write_buf[0] = W25X_JedecDeviceID;

	gc9a01_read_data(h3_write_buf, h3_read_buf, 4);

	temp = (h3_read_buf[0] << 24)|(h3_read_buf[1] << 16) | (h3_read_buf[2] << 8) | h3_read_buf[3];

	pr_err("%s %d temp = 0x%x\n", __func__, __LINE__, temp);

  return temp;
}

/*******************************************************************************
 函数: SPI_FlashWriteEnable
 功能: 写使能函数
 参数: 无
 返回: 无
*******************************************************************************/
static int H3_SPI_FlashWriteEnable(void)
{
	unsigned char tx_buf[2] = {0};
	int ret = 0;

	tx_buf[0] = W25X_WriteEnable;

	pr_err("%s %d\n", __func__, __LINE__);
	ret = gc9a01_sync_write(tx_buf, 1);

	return ret;
}

#if 0
/*******************************************************************************
 函数: SPI_FlashWriteDisable
 功能: 写使能函数
 参数: 无
 返回: 无
*******************************************************************************/
static int H3_SPI_FlashWriteDisable(void)
{
	unsigned char tx_buf[2] = {0};
	int ret = 0;

	tx_buf[0] = W25X_WriteDisable;

	pr_err("%s %d\n", __func__, __LINE__);
	ret = gc9a01_sync_write(tx_buf, 1);

	return ret;
}
#endif

/*******************************************************************************
 函数: SPI_FlashWaitForWriteEnd
 功能: flash判忙函数(忙等待形式)
 参数: 无
 返回: 无
*******************************************************************************/
static void H3_SPI_FlashWaitForWriteEnd(void)
{
	//int temp = 0;
	unsigned char h3_read_buf[3];
	unsigned char h3_write_buf[3];

	h3_write_buf[0] = W25X_ReadStatusReg;

	pr_err("%s %d\n", __func__, __LINE__);
	//mdelay(1);

	//return;

	while (1) {
		gc9a01_read_data(h3_write_buf, h3_read_buf, 2);
		pr_err("%s %d W25X_ReadStatusReg = 0x%x  0x%x\n", __func__, __LINE__, h3_read_buf[1],h3_read_buf[2]);
		if ((h3_read_buf[1] & WIP_Flag) == 0x01) {
			mdelay(1);
		}
		else {
			break;
		}
	}
}

#if 0
/*******************************************************************************
 函数: SPI_FlashSendAddr
功能: 向Flash发送需要操作的地址
 参数: 
		Addr：地址
 返回: 无
*******************************************************************************/
static void H3_SPI_FlashSendAddr(uint32_t Addr)
{
	unsigned char tx_buf[3] = {0};
	int ret = 0;

	tx_buf[0] = (Addr & 0xFF0000) >> 16;
	tx_buf[1] = (Addr& 0xFF00) >> 8;
	tx_buf[2] = Addr & 0xFF;

	pr_err("%s %d Addr = 0x%x\n", __func__, __LINE__, Addr);

	ret = gc9a01_sync_write(tx_buf, 3);

	return;
}

/*******************************************************************************
 函数: SPI_FlashBufferRead
 功能: flash读取
 参数: 
		ReadAddr：读取的flash地址
 返回: 
		pBuffer：读取的数据缓冲区
*******************************************************************************/
static uint32_t H3_SPI_FlashBufferRead(uint32_t ReadAddr)
{
	uint32_t temp = 0;
	u8 h3_read_buf[3];
	u8 h3_write_buf[3];
	unsigned char tx_buf[2] = {0};

	tx_buf[0] = W25X_ReadData;

	h3_write_buf[0] = (ReadAddr & 0xFF0000) >> 16;
	h3_write_buf[1] = (ReadAddr& 0xFF00) >> 8;
	h3_write_buf[2] = ReadAddr & 0xFF;

	gc9a01_sync_write(tx_buf, 1);

	gc9a01_read_data(h3_write_buf, h3_read_buf, 3);

	temp = (h3_read_buf[0] << 16) | (h3_read_buf[1] << 8) | h3_read_buf[2];

	pr_err("%s %d temp = 0x%x\n", __func__, __LINE__, temp);

	return temp;
}

/*******************************************************************************
 函数: SPI_FLASH_SectorErase
 功能: 扇区擦除
 参数: 
		SectorAddr：需要擦除的扇区起始地址
 返回: 无
*******************************************************************************/
static void H3_SPI_FLASH_SectorErase(uint32_t SectorAddr)
{

	unsigned char tx_buf[4] = {0};
	int ret = 0;

	tx_buf[0] = W25X_SectorErase;
	tx_buf[1] = (SectorAddr & 0xFF0000) >> 16;
	tx_buf[2] = (SectorAddr& 0xFF00) >> 8;
	tx_buf[3] = SectorAddr & 0xFF;

	pr_err("%s %d\n", __func__, __LINE__);

	H3_SPI_FlashWriteEnable();
	H3_SPI_FlashWaitForWriteEnd();
	//扇区擦除,其中第一个字节为扇区擦除命令编码（20h），紧跟其后的为要进行擦除的24位起始地址(扇区起始地址)。
	ret = gc9a01_sync_write(tx_buf, 4);
	H3_SPI_FlashWaitForWriteEnd();
}
#endif
/*******************************************************************************
 函数: SPI_FLASH_BlockErase
 功能: 块区擦除
 参数: 
		SectorAddr：需要擦除的块区起始地址
 返回: 无
*******************************************************************************/
static void H3_SPI_FLASH_BlockErase(uint32_t BlockAddr)
{

	unsigned char tx_buf[4] = {0};
	int ret = 0;

	tx_buf[0] = W25X_BlockErase;
	tx_buf[1] = (BlockAddr & 0xFF0000) >> 16;
	tx_buf[2] = (BlockAddr& 0xFF00) >> 8;
	tx_buf[3] = BlockAddr & 0xFF;

	pr_err("%s %d\n", __func__, __LINE__);

	H3_SPI_FlashWriteEnable();
	H3_SPI_FlashWaitForWriteEnd();

	//块区擦除,其中第一个字节为块区擦除命令编码（D8h），紧跟其后的为要进行擦除的24位起始地址(块区起始地址)。
	ret = gc9a01_sync_write(tx_buf, 4);

	mdelay(50);

	H3_SPI_FlashWaitForWriteEnd();
}

/*******************************************************************************
 函数: SPI_FlashPageWrite
 功能: 页面写操作
 参数: 
		pBuffer：需要写入的字节缓冲数组，一次写入不能超过一页（如果是从头开始写，则不能超过256）
		WriteAddr：写入的起始地址
		NumByteToWrite：写入多少个字节
 返回: 无
*******************************************************************************/
static void H3_SPI_FlashPageWrite(unsigned char* pBuffer,u32 WriteAddr,u16 NumByteToWrite)
{
	//u8 h3_write_buf[3];
	//unsigned char tx_buf[2] = {0};
	unsigned char write_buf[256 + 4] = {0};
	int len = 0;
	int i = 0;

	write_buf[0] = W25X_PageProgram;
	write_buf[1] = (WriteAddr & 0xFF0000) >> 16;
	write_buf[2] = (WriteAddr& 0xFF00) >> 8;
	write_buf[3] = WriteAddr & 0xFF;

	pr_err("%s %d len = %d\n", __func__, __LINE__,NumByteToWrite);

	H3_SPI_FlashWriteEnable();
	H3_SPI_FlashWaitForWriteEnd();
	//gc9a01_sync_write(tx_buf,1);//页编程指令
	//gc9a01_sync_write(h3_write_buf,3);//写入页地址
	if(NumByteToWrite>SPI_FLASH_PerWritePageSize) {
		NumByteToWrite=SPI_FLASH_PerWritePageSize;
		pr_err("%s %d\n", __func__, __LINE__);
	}
		pr_err("%s %d len = %d\n", __func__, __LINE__, NumByteToWrite);
	len = NumByteToWrite;

	for(i = 0; i < NumByteToWrite; i++)
	{
		//SPI_FlashSendByte(*pBuffer);
		write_buf[i + 4] = pBuffer[i];
		//pBuffer++;
	}
	gc9a01_sync_write(write_buf, len + 4);

	H3_SPI_FlashWaitForWriteEnd();
}

/*******************************************************************************
 函数: SPI_FlashWriteNoCheck
 功能: flash写入函数(要保证flash是被擦除过的)(自动换页)
 参数: 
		pBuffer：需要写入的字节缓冲数组
		WriteAddr：写入的起始地址
		NumByteToWrite：写入多少个字节
 返回: 无
*******************************************************************************/
void H3_SPI_FlashWriteNoCheck(unsigned char* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite)   
{ 			 		 
	uint16_t pageremain = 0;	  
	pageremain = 256 - WriteAddr % 256; //单页剩余的字节数
	
	if(NumByteToWrite <= pageremain)
		pageremain = NumByteToWrite;//写入的字节数小于一页  pageremain就等于要写入的字节数

	pr_err("%s %d\n", __func__, __LINE__);

	while(1)
	{	   
		H3_SPI_FlashPageWrite(pBuffer,WriteAddr,pageremain);//写入单页剩余的字节数
		if(NumByteToWrite==pageremain)	break;//写入结束了
	 	else //需要写入的字节大于单页剩余数
		{
			pBuffer+=pageremain;
			WriteAddr+=pageremain;
			NumByteToWrite-=pageremain;			  //减去已经写入了的字节数
			if(NumByteToWrite>256)	pageremain=256; //一次可以写入256个字节
			else	pageremain=NumByteToWrite; 	  //不够256个字节了
		}
	}	    
}

#define H3_SECTOR_SIZE                     (4 * 1024)
static u8 *g_flash_buffer = NULL;
/**
 * @brief FLASH写数据函数
 * 
 * @param address 待写入数据的内存地址
 * @param data 待写入的数据
 * @param length 待写入数据的个数
 */
static void H3_FLASH_WriteData(uint32_t address, unsigned char *data, uint32_t length)
{
    uint32_t sectorPosition = address / 4096;                                   // 扇区地址
    uint32_t sectorOffset = address % 4096;                                     // 在扇区中的偏移地址
    uint32_t sectorRemain = 4096 - sectorOffset;                                // 扇区剩余空间大小
    uint32_t i = 0;
    unsigned char *pBuff;
    //unsigned char sectoraddr[3];

	pr_err("%s %d  sectorPosition = %d sectorOffset = %d sectorRemain = %d\n", __func__, __LINE__, sectorPosition, sectorOffset, sectorRemain);
	mdelay(10);
	if (IS_ERR_OR_NULL(g_flash_buffer)) {
		g_flash_buffer = kzalloc(H3_SECTOR_SIZE, GFP_KERNEL);
	}
	pr_err("%s %d\n", __func__, __LINE__);
	mdelay(10);
	if (IS_ERR_OR_NULL(g_flash_buffer)) {
		pr_err("sector buf mem allocate fail\n");
		return;
	}
	pr_err("%s %d\n", __func__, __LINE__);
	mdelay(10);
	pBuff = g_flash_buffer;
    // 当写入的数据小扇区剩余空间大小，将写入的字节数赋值给扇区剩余空间大小
    sectorRemain = (length <= sectorRemain ? length : sectorRemain);

	pr_err("%s %d\n", __func__, __LINE__);

	//扇区擦除
	for(i = 0; i < 8; i++) {
		pr_err("%s %d\n", __func__, __LINE__);
		H3_SPI_FLASH_BlockErase(i * 65536);	
	}

    while (1)
    {
		pr_err("%s %d\n", __func__, __LINE__);
        //直接写入扇区剩余空间
        H3_SPI_FlashWriteNoCheck(data, address, sectorRemain);
  
        // 写入完成
        if (length == sectorRemain)
        {
            break;
        }
        // 写入未完成
        else
        {
            sectorPosition++;                                                   // 扇区加1，使用下一个扇区
            sectorOffset = 0;                                                   // 扇区偏移地址为0

            address += sectorRemain;                                            // 写地址偏移
            data += sectorRemain;                                               // 写数据指针偏移
            length -= sectorRemain;                                             // 写入总长度减去已经写入的个数

            // 当剩余长度大于扇区长度4096时，可以一次写一整个扇区
            // 当剩余长度小于扇区长度4096时，下一个扇区可以写完
            sectorRemain = (length > 4096 ? 4096 : length);
        }
    }
}

static uint32_t H3_SPI_FW_Read(void)
{
	unsigned char h3_read_buf[6];
	unsigned char h3_write_buf[6];
	int FW_VER;

	//h3_write_buf[0] = W25X_DeviceID;

	h3_write_buf[0] = W25X_ReadData;
	h3_write_buf[1] = 0x07;
	h3_write_buf[2] = 0xFF;
	h3_write_buf[3] = 0xF0;
	gc9a01_read_data(h3_write_buf, h3_read_buf, 6);
	pr_err("%s %d read_buf[0] = 0x%x h3_read_buf[1] = 0x%x\n", __func__, __LINE__, h3_read_buf[0], h3_read_buf[1]);
	pr_err("%s %d read_buf[2] = 0x%x h3_read_buf[3] = 0x%x\n", __func__, __LINE__, h3_read_buf[2], h3_read_buf[3]);
	pr_err("%s %d read_buf[4] = 0x%x h3_read_buf[5] = 0x%x\n", __func__, __LINE__, h3_read_buf[4], h3_read_buf[5]);

	msleep(2);

	h3_write_buf[0] = W25X_ReadData;
	h3_write_buf[1] = 0x07;
	h3_write_buf[2] = 0xFF;
	h3_write_buf[3] = 0xF2;

	gc9a01_read_data(h3_write_buf, h3_read_buf,6);
	pr_err("%s %d read_buf[0] = 0x%x h3_read_buf[1] = 0x%x\n", __func__, __LINE__, h3_read_buf[0], h3_read_buf[1]);
	pr_err("%s %d read_buf[2] = 0x%x h3_read_buf[3] = 0x%x\n", __func__, __LINE__, h3_read_buf[2], h3_read_buf[3]);
	pr_err("%s %d read_buf[4] = 0x%x h3_read_buf[5] = 0x%x\n", __func__, __LINE__, h3_read_buf[4], h3_read_buf[5]);

	FW_VER = (h3_read_buf[4] << 8) | h3_read_buf[5];

	return FW_VER;
}

static u8 *fw_update_buff = NULL;
int gc9a01_update_bin(void) {
	const struct firmware *fw = NULL;
	uint32_t fw_addr = 0x00;
	int size = 0;
	int FW_VER_IC = 0;
	int FW_VER_BIN = 0;
	int fw_add = 0x07FFF0;
	int FW_check = 0;

	if (IS_ERR_OR_NULL(fw_update_buff)) {
		fw_update_buff = kzalloc(H3_FW_SIZE, GFP_KERNEL);
	}

	if (IS_ERR_OR_NULL(fw_update_buff)) {
		pr_err("fw buf mem allocate fail\n");
		//goto retry;
		return 0;
	}

	gc9a01_data.fw_mem = fw_update_buff;

	if (request_firmware(&fw, H3_FW_BIN_NAME, &gc9a01_data.spi->dev)) {
		pr_err("request firmware fail\n");
		//g_ret_update = 1;
		return 0;
	}

	memcpy(fw_update_buff, fw->data, fw->size);
	size = fw->size;
	pr_err("gc9a01_update_bin request firmware success\n");
	//pr_err("gc9a01_update_bin fw_update_buff[0]=0x%x fw_update_buff[1]=0x%x fw size = %d\n", fw_update_buff[0], fw_update_buff[1], fw->size);
	if (fw) {
		release_firmware(fw);
	}

	FW_check = ((fw_update_buff[fw_add] << 8) & 0xFF00) + (fw_update_buff[fw_add + 1] & 0xFF);
	FW_VER_BIN = ((fw_update_buff[fw_add + 2] << 8) & 0xFF00) + (fw_update_buff[fw_add + 3] & 0xFF);
	FW_VER_IC = H3_SPI_FW_Read();

	pr_err("gc9a01_update_bin FW_check =0x%x FW_VER_IC=0x%x FW_VER_BIN=0x%x\n", FW_check, FW_VER_IC, FW_VER_BIN);

	if (FW_check != H3_FW_ID){
		pr_err("gc9a01_update_bin FW_check 0x%x != 0x55AA\n", FW_check);		
		return 0;
	}

	if (FW_VER_IC != FW_VER_BIN){
		H3_FLASH_WriteData(fw_addr, gc9a01_data.fw_mem, size);
	} else {
		pr_err("gc9a01_update_bin FW_VER_BIN is the same with FW_VER_IC\n");
	}

	return 1;

}

void lcd_gc9a01_set_window(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t xOffset, uint16_t yOffset)
{
	yStart = yStart + yOffset;
	yEnd = yEnd + yOffset;
	xStart = xStart + xOffset;
	xEnd = xEnd + xOffset;

	gc9a01_write_cmd(0x2A);
	gc9a01_write_data(xStart>>8);
	gc9a01_write_data(xStart&0xff);
	gc9a01_write_data(xEnd>>8);
	gc9a01_write_data(xEnd&0xff);
	
	gc9a01_write_cmd(0x2B);
	gc9a01_write_data(yStart>>8);
	gc9a01_write_data(yStart&0xff);
	gc9a01_write_data(yEnd>>8);
	gc9a01_write_data(yEnd&0xff);

	gc9a01_write_cmd(0x2c);
}

int gc9a01_scr_on(void)
{
	pr_err("%s %d\n", __func__, __LINE__);
	schedule_delayed_work(&gc9a01_data.dwork, msecs_to_jiffies(30000));
	return 0;
}
EXPORT_SYMBOL(gc9a01_scr_on);

#define SPI_MAX_SPEED_HZ     (12000000)

#define MINISCREEN_WIDTH     (240)
#define MINISCREEN_HEIGHT    (240)

void gc9a01_init(void)
{
	pr_err("%s %d\n", __func__, __LINE__);
	gc9a01_data.spi->mode = 0;//SPI_CPOL;
	gc9a01_data.spi->bits_per_word = 8;
	gc9a01_data.spi->max_speed_hz = SPI_MAX_SPEED_HZ;
	spi_setup(gc9a01_data.spi);

	gc9a01_write_cmd(0xFE);
	gc9a01_write_cmd(0xEF);

	gc9a01_write_cmd(0xEB);
	gc9a01_write_data(0x14);

	gc9a01_write_cmd(0x84);
	gc9a01_write_data(0x60); //40->60 0xb5 en  20210529  james

	gc9a01_write_cmd(0x88);
	gc9a01_write_data(0x0A);

	gc9a01_write_cmd(0x89);
	gc9a01_write_data(0x23);  ///gc9a01_data.spi 2data reg en  20210529

	gc9a01_write_cmd(0x8A);
	gc9a01_write_data(0x00);

}
#if 0
static void gc9a01_fill(void)
{
	uint16_t r[MINISCREEN_WIDTH] = {0};
	uint16_t g[MINISCREEN_WIDTH] = {0};
	uint16_t b[MINISCREEN_WIDTH] = {0};
	//int ret = 0;
	int height = MINISCREEN_HEIGHT;
	int strip_w = 30;
	int i = 0;

	for(i=0;i<MINISCREEN_WIDTH;i++)
		*(r+i) = 0x00f8;
	for(i=0;i<MINISCREEN_WIDTH;i++)
		*(g+i) = 0xe007;
	for(i=0;i<MINISCREEN_WIDTH;i++)
		*(b+i) = 0x1f00;

#if 0
	printk("%s %x %x %x %x %d\n",__func__, *((uint8_t *)r),*((uint8_t *)r+1), *((uint8_t *)r+2),*((uint8_t *)r+3), sizeof(r));
	for(i=0;i<MINISCREEN_HEIGHT;i++){
		ret = gc9a01_sync_write((uint8_t *)r, sizeof(r));
		printk("%s %d %d\n",__func__,i, ret);
	}
#endif
	do {
		if (height < strip_w) {
			strip_w = height;
			if (height <= 0)
				break;
		}
		for (i = 0; i < strip_w; i++) {
			gc9a01_sync_write((uint8_t *)r, sizeof(r));
			height -= 1;
		}
		if (height < strip_w){
			strip_w = height;
			if (height <= 0)
				break;
		}
		for (i = 0; i < strip_w; i++) {
			gc9a01_sync_write((uint8_t *)g, sizeof(r));
			height -= 1;
		}
		if (height < strip_w){
			strip_w = height;
			if (height <= 0)
				break;
		}
		for (i = 0; i < strip_w; i++) {
			gc9a01_sync_write((uint8_t *)b, sizeof(r));
			height -= 1;
		}
	} while(height > 0);
}
#endif

extern void h3_reset_control(int val);
static void gc9a01_dwork(struct work_struct *work)
{
	int err = 0;

	pr_err("%s %d\n", __func__, __LINE__);

	//gc9a01_init();

	pinctrl_select_state(gc9a01_pinctrl, spi1_as_cs);
	pinctrl_select_state(gc9a01_pinctrl, spi1_as_ck);
	pinctrl_select_state(gc9a01_pinctrl, spi1_as_mi);
	pinctrl_select_state(gc9a01_pinctrl, spi1_as_mo);

	h3_reset_control(0);

	if (err != 0) {
		//gc9a01_fill();
		gc9a01_bl_ctl(85);
	}

	//if ( err < 0){
		//H3_SPI_FlashPowerdown();
		//mdelay(10);
		H3_SPI_FlashWakeup();
	//}

	H3_SPI_FlashReadDeviceid();
	mdelay(1);
	H3_SPI_FlashReadJedecid();

	gc9a01_update_bin();

	H3_SPI_FW_Read();

	pinctrl_select_state(gc9a01_pinctrl, spi1_as_cs_hi);
	pinctrl_select_state(gc9a01_pinctrl, spi1_as_ck_hi);
	pinctrl_select_state(gc9a01_pinctrl, spi1_as_mi_hi);
	pinctrl_select_state(gc9a01_pinctrl, spi1_as_mo_hi);
	h3_reset_control(1);

}

int gc9a01_probe(struct spi_device *spi)
{
	int err = 0;

	pr_err("%s %d\n", __func__, __LINE__);

	miniscreen_width = MINISCREEN_WIDTH;
	miniscreen_height = MINISCREEN_HEIGHT;

	/* init pinctrl */
	if (gc9a01_pinctrl_init(&spi->dev)) {
		pr_err("gc9a01_pinctrl_init err\n");
		err = -EFAULT;
		goto error;
	} else {
		pr_err("gc9a01_pinctrl_init success\n");
	}

	//gc9a01_reset_ctl(0);

	gc9a01_data.spi = spi;
	INIT_DELAYED_WORK(&gc9a01_data.dwork, gc9a01_dwork);

	return 0;
error:
	return err;
}

EXPORT_SYMBOL(miniscreen_width);
EXPORT_SYMBOL(miniscreen_height);
EXPORT_SYMBOL(gc9a01_init);
EXPORT_SYMBOL(gc9a01_bl_ctl);
EXPORT_SYMBOL(gc9a01_reset_ctl);
EXPORT_SYMBOL(gc9a01_dc_ctl);
EXPORT_SYMBOL(gc9a01_probe);