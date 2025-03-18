#include "../../../../drv_common.h"

#if PRI_AW87XXX_DUAL_CONFIG_1

#define pa_1_i2c_bus_x i2c6
#define pa_1_i2c_idx 58
#define pa_1_i2c_addr 0x58
#if PRI_AW87XXX_NEED_RESET_PIN
#define pa_1_reset_gpio  156
#endif

#define pa_2_i2c_bus_x i2c6
#define pa_2_i2c_idx 5b
#define pa_2_i2c_addr 0x5b
#if PRI_AW87XXX_NEED_RESET_PIN
#define pa_2_reset_gpio  151
#endif

#endif

#if PRI_AW87XXX_DUAL_CONFIG_2
#define pa_1_i2c_bus_x i2c6
#define pa_1_i2c_idx 58
#define pa_1_i2c_addr 0x58
#if PRI_AW87XXX_NEED_RESET_PIN
#define pa_1_reset_gpio  156
#endif

#define pa_2_i2c_bus_x i2c6
#define pa_2_i2c_idx 59
#define pa_2_i2c_addr 0x59
#if PRI_AW87XXX_NEED_RESET_PIN
#define pa_2_reset_gpio  151
#endif
#endif