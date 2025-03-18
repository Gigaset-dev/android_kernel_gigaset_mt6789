/************************************************
	于2023.11.22创建
	描述：SPI驱动W25q32程序
	作者：-小猫巫-
************************************************/

/************************************************
	头文件
************************************************/
#include "w25qxx.h"



/************************************************
	主函数
************************************************/
#if SPI_SORF_OR_HARD

#else
void Delay(vu32 nCount)
{
  for(; nCount != 0; nCount--);
}
#endif
/*******************************************************************************
 函数: SPI_UserInit
 功能: SPI初始化函数
 参数: 无
 返回: 无
*******************************************************************************/
void SPI_UserInit(void)
{
	#if	SPI_SORF_OR_HARD
	GPIO_InitTypeDef GPIO_InitStructure;
	SPI_InitTypeDef SPI_InitStructure;
	
	RCC_APB2PeriphClockCmd(SPI_RCC_GPIO, ENABLE);
	if(SPI_RCC_SPIx == RCC_APB2Periph_SPI1){
		RCC_APB2PeriphClockCmd(SPI_RCC_SPIx, ENABLE);
	}
	else{
		RCC_APB1PeriphClockCmd(SPI_RCC_SPIx ,ENABLE);
	}
	
	GPIO_InitStructure.GPIO_Pin = SPI_PIN;  //数据和时钟管脚
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;//复用推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(SPI_GPIO,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = SPI_CS_PIN;	//CS管脚
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(SPI_CS_GPIO,&GPIO_InitStructure);
	
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex; //设置SPI单向或者双向的数据模式:SPI设置为双线双向全双工
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;					//SPI主机模式
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;			//设置SPI的数据大小:SPI发送接收8位帧结构
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;						//串行同步时钟的空闲状态为高电平
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;					//串行同步时钟的第二个跳变沿（上升或下降）数据被采样		
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;							//NSS信号由硬件（NSS管脚）还是软件（使用SSI位）管理:内部NSS信号有SSI位控制
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;//fpclk 2分频
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;		//高位先行
	SPI_InitStructure.SPI_CRCPolynomial = 7;
	
	SPI_Init(SPIx ,&SPI_InitStructure);
	SPI_Cmd(SPIx, ENABLE);//使能SPI
	
	SPI_FLASH_CS(Low);
	SPI_FLASH_CS(High);
	#else
	GPIO_InitTypeDef  GPIO_InitStructure;
 	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOx, ENABLE);	 //使能端口时钟

	GPIO_InitStructure.GPIO_Pin = SPI_GPIO_CSN|SPI_GPIO_SCK|SPI_GPIO_MOSI;				 //端口配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 //推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 //IO口速度为50MHz
	GPIO_Init(SPI_GPIOx, &GPIO_InitStructure);					 //根据设定参数初始化
	
	SPI_FLASH_CS(High);      //片选不选中
    
	GPIO_InitStructure.GPIO_Pin = SPI_GPIO_MOSO;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;     //上拉输入
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(SPI_GPIOx, &GPIO_InitStructure);
	#endif
}


///*******************************************************************************
// 函数: w25q32_Init
// 功能: 初始化w25q32
// 参数: 无
// 返回: 无
//*******************************************************************************/
//void w25q32_Init(void)
//{
//	SPI_FLASH_SectorErase(Block0 & Sector0);
//	SPI_FLASH_SectorErase(Block0 & Sector1);
//	SPI_FLASH_SectorErase(Block0 & Sector2);
//	SPI_FLASH_SectorErase(Block0 & Sector3);
//	SPI_FLASH_SectorErase(Block0 & Sector4);
//}


/*******************************************************************************
 函数: SPI_FlashSendByte
 功能: 通过SPI总线发送一个字节的数据
 参数: 
		byt：需要发送的单个字节
 返回: 读到的数据
*******************************************************************************/
uint8_t SPI_FlashSendByte(uint8_t byte)
{
	#if	SPI_SORF_OR_HARD
  while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_TXE) == RESET);
  SPI_I2S_SendData(SPIx, byte);
  while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_RXNE) == RESET);
  return SPI_I2S_ReceiveData(SPIx);
	#else
	for(uint8_t bit_ctr=0;bit_ctr<8;bit_ctr++) 
	{
		if(byte & 0x80) MOSI_H;
		else MOSI_L;
		byte = (byte << 1);
		SCK_H; 
		Delay(0xff);
		if(READ_MISO)   byte |= 0x01; 
		SCK_L; 
		Delay(0xff);
	}
	return(byte); 
	#endif
}




/*******************************************************************************
 函数: SPI_FlashPowerdown
 功能: SPI_FLASH进入掉电模式
 参数: 无
 返回: 无
*******************************************************************************/
void SPI_FlashPowerdown(void)   
{ 
  SPI_FLASH_CS(Low);
  SPI_FlashSendByte(W25X_PowerDown);//发送掉电命令
  SPI_FLASH_CS(High);
}   



/*******************************************************************************
 函数: SPI_FlashWakeup
 功能: SPI_FLASH从掉电模式唤醒
 参数: 无
 返回: 无
*******************************************************************************/
void SPI_FlashWakeup(void)   
{
  SPI_FLASH_CS(Low);
  SPI_FlashSendByte(W25X_ReleasePowerDown);//发送唤醒命令
  SPI_FLASH_CS(High);      
}


/*******************************************************************************
 函数: SPI_FlashReadDeviceid
 功能: 读取FLASH器件ID
 参数: 无
 返回: 无
*******************************************************************************/
uint32_t SPI_FlashReadDeviceid(void)
{
  uint32_t Temp = 0;

  SPI_FLASH_CS(Low);
  SPI_FlashSendByte(W25X_DeviceID);
  SPI_FlashSendByte(Dummy_Byte);
  SPI_FlashSendByte(Dummy_Byte);
  SPI_FlashSendByte(Dummy_Byte);
  
  Temp = SPI_FlashSendByte(Dummy_Byte);

  SPI_FLASH_CS(High);
  return Temp;
}



/*******************************************************************************
 函数: SPI_FlashReadJedecid
 功能: 读取jedecid
 参数: 无
 返回: 无
*******************************************************************************/
uint32_t SPI_FlashReadJedecid(void)
{
  uint32_t temp = 0, temp0 = 0, temp1 = 0, temp2 = 0;

  SPI_FLASH_CS(Low);
  SPI_FlashSendByte(W25X_JedecDeviceID);
  temp0 = SPI_FlashSendByte(Dummy_Byte);
  temp1 = SPI_FlashSendByte(Dummy_Byte);
  temp2 = SPI_FlashSendByte(Dummy_Byte);
  SPI_FLASH_CS(High);

  temp = (temp0 << 16) | (temp1 << 8) | temp2;

  return temp;
}



/*******************************************************************************
 函数: SPI_FlashWriteEnable
 功能: 写使能函数
 参数: 无
 返回: 无
*******************************************************************************/
void SPI_FlashWriteEnable(void)
{
  SPI_FLASH_CS(Low);
  SPI_FlashSendByte(W25X_WriteDisable);//写使能命令
  SPI_FLASH_CS(High);
}




/*******************************************************************************
 函数: SPI_FlashWaitForWriteEnd
 功能: flash判忙函数(忙等待形式)
 参数: 无
 返回: 无
*******************************************************************************/
void SPI_FlashWaitForWriteEnd(void)
{
  uint8_t FLASH_Status=0;
  SPI_FLASH_CS(Low);
  SPI_FlashSendByte(W25X_ReadStatusReg);
  do
  {
    FLASH_Status=SPI_FlashSendByte(Dummy_Byte);
  }
  while((FLASH_Status & WIP_Flag) == SET);
  SPI_FLASH_CS(High);
}


/*******************************************************************************
 函数: SPI_FlashSendAddr
功能: 向Flash发送需要操作的地址
 参数: 
		Addr：地址
 返回: 无
*******************************************************************************/
void	SPI_FlashSendAddr(uint32_t	Addr)
{
	#ifdef	ADDR3
  SPI_FlashSendByte((Addr & 0xFF0000) >> 16);
  SPI_FlashSendByte((Addr& 0xFF00) >> 8);
  SPI_FlashSendByte(Addr & 0xFF);
	#else
	SPI_FlashSendByte((Addr & 0xFF000000) >> 24);
	SPI_FlashSendByte((Addr & 0xFF0000) >> 16);
  SPI_FlashSendByte((Addr& 0xFF00) >> 8);
  SPI_FlashSendByte(Addr & 0xFF);
	#endif
}


/*******************************************************************************
 函数: SPI_FlashBufferRead
 功能: flash读取
 参数: 
		ReadAddr：读取的flash地址
		NumByteToRead：读取多少个字节
 返回: 
		pBuffer：读取的数据缓冲区
*******************************************************************************/
void SPI_FlashBufferRead(uint8_t* pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead)
{
  SPI_FLASH_CS(Low);
  SPI_FlashSendByte(W25X_ReadData);
	SPI_FlashSendAddr(ReadAddr);
  while (NumByteToRead--) 
  {
    *pBuffer = SPI_FlashSendByte(Dummy_Byte);//读出
    pBuffer++;
  }
  SPI_FLASH_CS(High);
}



/*******************************************************************************
 函数: SPI_FLASH_SectorErase
 功能: 扇区擦除
 参数: 
		SectorAddr：需要擦除的扇区起始地址
 返回: 无
*******************************************************************************/
void SPI_FLASH_SectorErase(uint32_t SectorAddr)
{
	SPI_FlashWriteEnable();
	SPI_FlashWaitForWriteEnd();
	SPI_FLASH_CS(Low);
	//扇区擦除,其中第一个字节为扇区擦除命令编码（20h），紧跟其后的为要进行擦除的24位起始地址(扇区起始地址)。
	SPI_FlashSendByte(W25X_SectorErase);
	SPI_FlashSendAddr(SectorAddr);
	SPI_FLASH_CS(High);
	SPI_FlashWaitForWriteEnd();
}



/*******************************************************************************
 函数: SPI_FLASH_BlockErase
 功能: 块区擦除
 参数: 
		SectorAddr：需要擦除的块区起始地址
 返回: 无
*******************************************************************************/
void SPI_FLASH_BlockErase(uint32_t BlockAddr)
{
	SPI_FlashWriteEnable();
	SPI_FlashWaitForWriteEnd();
	SPI_FLASH_CS(Low);
	//扇区擦除,其中第一个字节为块区擦除命令编码（D8h），紧跟其后的为要进行擦除的24位起始地址(块区起始地址)。
	SPI_FlashSendByte(W25X_BlockErase);
	SPI_FlashSendAddr(BlockAddr);
	SPI_FLASH_CS(High);
	SPI_FlashWaitForWriteEnd();
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
void SPI_FlashPageWrite(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)
{
	SPI_FlashWriteEnable();
	SPI_FlashWaitForWriteEnd();
	SPI_FLASH_CS(Low);
	SPI_FlashSendByte(W25X_PageProgram);	//页编程指令
	SPI_FlashSendAddr(WriteAddr);
	if(NumByteToWrite>SPI_FLASH_PerWritePageSize)
		NumByteToWrite=SPI_FLASH_PerWritePageSize;
	while(NumByteToWrite--)
	{
		SPI_FlashSendByte(*pBuffer);
		pBuffer++;
	}
	SPI_FLASH_CS(High);
	SPI_FlashWaitForWriteEnd();
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
void SPI_FlashWriteNoCheck(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite)   
{ 			 		 
	uint16_t pageremain = 0;	  
	pageremain = 256 - WriteAddr % 256; //单页剩余的字节数		 	    
	if(NumByteToWrite <= pageremain)	pageremain = NumByteToWrite;//写入的字节数小于一页  pageremain就等于要写入的字节数
	while(1)
	{	   
		SPI_FlashPageWrite(pBuffer,WriteAddr,pageremain);//写入单页剩余的字节数
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
