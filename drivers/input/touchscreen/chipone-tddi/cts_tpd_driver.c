#define TAG "TPD"

#include "tpd.h"
#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_driver.h"

//prize-lixuefeng-20150512-start
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/mediatek/prize/hardware_info/hardware_info.h"

extern struct hardware_info current_tp_info;
#endif
//prize-lixuefeng-20150512-end

struct chipone_ts_data *chipone_ts_data = NULL;

static int chipone_tpd_local_init(void)
{
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
    static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
    static int tpd_wb_end_local[TPD_WARP_CNT] = TPD_WARP_END;
#endif

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
    static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

    int ret;
	pr_err("gezi %s------------------------%d\n", __func__,__LINE__);
    if ((ret = cts_driver_init()) != 0) {
        cts_err("Init driver failed %d", ret);
        return ret;
    }

    if (tpd_load_status == 0) {
        cts_err("Driver not load successfully, exit.");
        cts_driver_exit();
        return -1;
    }

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
#endif

#if (defined(CONFIG_TPD_HAVE_CALIBRATION) && !defined(CONFIG_TPD_CUSTOM_CALIBRATION))
    memcpy(tpd_calmat, tpd_def_calmat_local, 8 * 4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local, 8 * 4);
#endif

    tpd_type_cap = 1;

/*prize-20190323-penggy for TP info start */
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
	if(!chipone_ts_data){
		pr_err("gezi %s------------------------%d\n", __func__,__LINE__);
		return 0;
	}
	else{
	//	pr_err("gezi %s------------------------%d\n", __func__,__LINE__);
	}
	sprintf(current_tp_info.chip,"ICNL9911, Firmware version:%04x", chipone_ts_data->cts_dev.fwdata.version);
	sprintf(current_tp_info.id,"hwid: %06x fwid: %04x", chipone_ts_data->cts_dev.hwdata->hwid, chipone_ts_data->cts_dev.hwdata->fwid);
	strcpy(current_tp_info.vendor,"ICNL");
	sprintf(current_tp_info.more,"%d*%d", chipone_ts_data->cts_dev.fwdata.res_x+1, chipone_ts_data->cts_dev.fwdata.res_y+1);
#else
//	pr_err("gezi %s------------------------%d\n", __func__,__LINE__);
#endif	
/*prize-20190323-penggy for TP info end */	
	//pr_err("gezi %s------------------------%d\n", __func__,__LINE__);
    return 0;
}

/* TPD pass dev = NULL */
static void chipone_tpd_suspend(struct device *dev)
{
    cts_driver_suspend(chipone_ts_data);
}

/* TPD pass dev = NULL */
static void chipone_tpd_resume(struct device *dev)
{
    cts_driver_resume(chipone_ts_data);
}

static struct tpd_driver_t chipone_tpd_driver = {
    .tpd_device_name = CFG_CTS_DRIVER_NAME,
    .tpd_local_init = chipone_tpd_local_init,
    .suspend = chipone_tpd_suspend,
    .resume = chipone_tpd_resume,
    .tpd_have_button = 0,
    .attrs = {
        .attr = NULL,
        .num  = 0,
    }
};

int chipone_tpd_driver_init(void)
{
    int ret;
	
	pr_err("gezi %s------------------------%d\n", __func__,__LINE__);
	
    cts_info("Chipone TDDI TPD driver %s", CFG_CTS_DRIVER_VERSION);

    tpd_get_dts_info();
	
    if (tpd_dts_data.touch_max_num < 2) {
        tpd_dts_data.touch_max_num = 2;
    } else if (tpd_dts_data.touch_max_num > CFG_CTS_MAX_TOUCH_NUM) {
        tpd_dts_data.touch_max_num = CFG_CTS_MAX_TOUCH_NUM;
    }

    if((ret = tpd_driver_add(&chipone_tpd_driver)) < 0) {
        cts_err("Add TPD driver failed %d", ret);
        return ret;
    }

    return 0;
}
/*
static void __exit chipone_tpd_driver_exit(void)
{
    cts_info("Chipone TPD driver exit");

    tpd_driver_remove(&chipone_tpd_driver);
}
*/
//module_init(chipone_tpd_driver_init);
//module_exit(chipone_tpd_driver_exit);

#ifdef CFG_CTS_KERNEL_BUILTIN_FIRMWARE
MODULE_FIRMWARE(CFG_CTS_FIRMWARE_FILENAME);
#endif /* CFG_CTS_KERNEL_BUILTIN_FIRMWARE */

MODULE_DESCRIPTION("Chipone TDDI TPD Driver for MTK platform");
MODULE_VERSION(CFG_CTS_DRIVER_VERSION);
MODULE_AUTHOR("Miao Defang <dfmiao@chiponeic.com>");
MODULE_LICENSE("GPL");

