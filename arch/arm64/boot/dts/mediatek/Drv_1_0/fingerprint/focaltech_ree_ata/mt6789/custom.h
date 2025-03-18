#include "../../../drv_common.h"


#if PRI_FOCALTECH_REE_ATA_CONFIG_1
#define SPI_INDEX spi5
#define FP_IRQ_PIN 10
#define FP_RESET_PIN 105

#define FP_CS_AS_SPI    PINMUX_GPIO47__FUNC_SPI5_CSB
#define FP_CK_AS_SPI    PINMUX_GPIO46__FUNC_SPI5_CLK
#define FP_MI_AS_SPI    PINMUX_GPIO49__FUNC_SPI5_MI
#define FP_MO_AS_SPI    PINMUX_GPIO48__FUNC_SPI5_MO

#endif