
#ifndef _HYNITRON_CFG_H
#define _HYNITRON_CFG_H


#define I2C_PORT
#ifdef I2C_PORT
    #define I2C_USE_DMA      (1)
#else
    #define SPI_MODE         (0)
    #define SPI_DELAY_CS     (10) //us
    #define SPI_CLOCK_FREQ   (8000000)
#endif

#define HYN_TRANSFER_LIMIT_LEN   (2048) //need >= 8

#define HYN_POWER_ON_UPDATA     (1)

#define HYN_GKI_VER           (1) //GKI ver enable
#define HYN_APK_DEBUG_EN      (1)

#define HYN_GESTURE_EN        (1)
#define HYN_PROX_TYEP         (0) //0:disable 1:default 2:mtk_sensor 3:Spread misc

#define MAX_POINTS_REPORT     (5)

#define KEY_USED_POS_REPORT   (0)

#define ESD_CHECK_EN          (0)

#define HYN_MT_PROTOCOL_B_EN  (1)

//selftest cfg
#define HYN_TP0_TEST_LOG_SAVE  (1)


#endif



