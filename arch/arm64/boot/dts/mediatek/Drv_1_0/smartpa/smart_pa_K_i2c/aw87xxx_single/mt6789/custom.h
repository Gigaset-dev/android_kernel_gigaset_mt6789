#include "../../../../drv_common.h"

#if PRI_AW87XXX_SINGLE_CONFIG_1
#define pa_1_i2c_bus_x i2c6
#define pa_1_i2c_idx 58
#define pa_1_i2c_addr 0x58
#if PRI_AW87XXX_NEED_RESET_PIN
#define pa_1_reset_gpio  43
#endif
#endif