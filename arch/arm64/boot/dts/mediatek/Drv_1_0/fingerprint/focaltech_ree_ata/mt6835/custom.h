#include "../../../drv_common.h"


#if PRI_FOCALTECH_REE_ATA_CONFIG_1
#define SPI_INDEX spi1
#define FP_IRQ_PIN 15
#define FP_RESET_PIN 7

#define FP_CS_AS_SPI    PINMUX_GPIO61__FUNC_SPI1_CSB
#define FP_CK_AS_SPI    PINMUX_GPIO60__FUNC_SPI1_CLK
#define FP_MI_AS_SPI    PINMUX_GPIO63__FUNC_SPI1_MI
#define FP_MO_AS_SPI    PINMUX_GPIO62__FUNC_SPI1_MO


#endif