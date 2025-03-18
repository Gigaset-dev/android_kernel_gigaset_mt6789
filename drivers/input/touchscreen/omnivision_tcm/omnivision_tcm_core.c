/*
 * Omnivision TCM touchscreen driver
 *
 * Copyright (C) 2017-2018 Omnivision Incorporated. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND Omnivision
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL Omnivision BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF Omnivision WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, Omnivision'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include "omnivision_tcm_core.h"
#include <linux/platform_device.h>

/*
#include <linux/sched.h>
#include <linux/fb.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include "../tpd.h"
*/

//drv-modify by shenwenbin for TP charger mode 20240304 start
#if OMNIVISION_TCM_CHARGER_MODE_EN
#include <linux/power_supply.h>
#include <linux/property.h>
#endif
//drv-modify by shenwenbin for TP charger mode 20240304 end

/* #define RESET_ON_RESUME */

/* #define RESUME_EARLY_UNBLANK */

#define RESET_ON_RESUME_DELAY_MS 50

#define PREDICTIVE_READING

#define MIN_READ_LENGTH 9

/* #define FORCE_RUN_APPLICATION_FIRMWARE */

#define NOTIFIER_PRIORITY 2

#define RESPONSE_TIMEOUT_MS 3000

#define APP_STATUS_POLL_TIMEOUT_MS 1000

#define APP_STATUS_POLL_MS 100

#define ENABLE_IRQ_DELAY_MS 20

//#define FALL_BACK_ON_POLLING

#define POLLING_DELAY_MS 5

#define MODE_SWITCH_DELAY_MS 100

#define READ_RETRY_US_MIN 5000

#define READ_RETRY_US_MAX 10000

#define WRITE_DELAY_US_MIN 500

#define WRITE_DELAY_US_MAX 1000

#define DYNAMIC_CONFIG_SYSFS_DIR_NAME "dynamic_config"

#define ROMBOOT_DOWNLOAD_UNIT 16

#define PDT_END_ADDR 0x00ee

#define RMI_UBL_FN_NUMBER 0x35

/*prize add by lvyuanchuan 20220214 start */

#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/mediatek/prize/hardware_info/hardware_info.h"

extern struct hardware_info current_tp_info;
#endif

/*prize add by lvyuanchuan 20220214 end */

struct ovt_tcm_hcd *g_tcm_hcd;
#ifdef SPEED_UP_RESUME
static void speedup_resume(struct work_struct *work);
#endif

//drv add by shenwenbin for headset detect 20240806 start
#ifdef CONFIG_TOUCHSCREEN_OMNIVISION_TCM_EAR_PHONE
bool ovt_tcm_probe_initialized = false;
void ovt_tcm_switch_ear_mode(unsigned short value);
#endif
//drv add by shenwenbin for headset detect 20240806 end
struct ovt_tcm_host_dynamic_setting {
	bool host_setting_flag;    // if this  setting come from host
	unsigned short setting_value;
};

static struct ovt_tcm_host_dynamic_setting g_ovt_host_dynamic_set_value[256];

int ovt_tcm_set_dynamic_config_host_interface(unsigned char id, unsigned short value)
{
	int retval = 0;
	struct ovt_tcm_hcd *tcm_hcd = g_tcm_hcd;
	if ((!tcm_hcd) || (tcm_hcd->ovt_tcm_driver_removing)) {
		printk("tcm shutdown, return\n");
		return 0;
	}

	g_ovt_host_dynamic_set_value[id].host_setting_flag = true;
	g_ovt_host_dynamic_set_value[id].setting_value = value;

	if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode) || tcm_hcd->in_suspend || atomic_read(&tcm_hcd->host_downloading)) {

	} else {
		retval = tcm_hcd->set_dynamic_config(tcm_hcd, id, value);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,"Failed to set dynamic %02x : %d command\n", id, value);
		}
	}
	return retval;
}

EXPORT_SYMBOL(ovt_tcm_set_dynamic_config_host_interface);

#define dynamic_config_sysfs(c_name, id) \
static ssize_t ovt_tcm_sysfs_##c_name##_show(struct device *dev, \
		struct device_attribute *attr, char *buf) \
{ \
	int retval; \
	unsigned short value; \
	struct ovt_tcm_hcd *tcm_hcd; \
\
	tcm_hcd = g_tcm_hcd; \
\
	mutex_lock(&tcm_hcd->extif_mutex); \
\
	retval = tcm_hcd->get_dynamic_config(tcm_hcd, id, &value); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
				"Failed to get dynamic config\n"); \
		goto exit; \
	} \
\
	retval = snprintf(buf, PAGE_SIZE, "%u\n", value); \
\
exit: \
	mutex_unlock(&tcm_hcd->extif_mutex); \
\
	return retval; \
} \
\
static ssize_t ovt_tcm_sysfs_##c_name##_store(struct device *dev, \
		struct device_attribute *attr, const char *buf, size_t count) \
{ \
	int retval; \
	unsigned int input; \
	struct ovt_tcm_hcd *tcm_hcd; \
\
	tcm_hcd = g_tcm_hcd; \
\
	if (sscanf(buf, "%u", &input) != 1) \
		return -EINVAL; \
\
	mutex_lock(&tcm_hcd->extif_mutex); \
\
	retval = tcm_hcd->set_dynamic_config(tcm_hcd, id, input); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
				"Failed to set dynamic config\n"); \
		goto exit; \
	} \
\
	retval = count; \
\
exit: \
	mutex_unlock(&tcm_hcd->extif_mutex); \
\
	return retval; \
}

#define dynamic_config_advance_sysfs(c_name, id) \
static ssize_t ovt_tcm_sysfs_##c_name##_show(struct device *dev, \
		struct device_attribute *attr, char *buf) \
{ \
	int retval; \
	unsigned short value; \
	struct ovt_tcm_hcd *tcm_hcd; \
\
	tcm_hcd = g_tcm_hcd; \
\
	mutex_lock(&tcm_hcd->extif_mutex); \
\
	retval = tcm_hcd->get_dynamic_config(tcm_hcd, id, &value); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
				"Failed to get dynamic config\n"); \
		goto exit; \
	} \
\
	retval = snprintf(buf, PAGE_SIZE, "%u\n", value); \
\
exit: \
	mutex_unlock(&tcm_hcd->extif_mutex); \
\
	return retval; \
} \
\
static ssize_t ovt_tcm_sysfs_##c_name##_store(struct device *dev, \
		struct device_attribute *attr, const char *buf, size_t count) \
{ \
	int retval; \
	unsigned int input; \
	struct ovt_tcm_hcd *tcm_hcd; \
\
	tcm_hcd = g_tcm_hcd; \
\
	if (sscanf(buf, "%u", &input) != 1) \
		return -EINVAL; \
\
	mutex_lock(&tcm_hcd->extif_mutex); \
\
	retval = ovt_tcm_set_dynamic_config_host_interface(id, input); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
				"Failed to set dynamic config  #c_name\n"); \
		goto exit; \
	} \
\
	retval = count; \
\
exit: \
	mutex_unlock(&tcm_hcd->extif_mutex); \
\
	return retval; \
}


DECLARE_COMPLETION(response_complete);
DECLARE_COMPLETION(helper_complete);

static struct kobject *sysfs_dir;

static struct ovt_tcm_module_pool mod_pool;

SHOW_PROTOTYPE(ovt_tcm, info)
SHOW_PROTOTYPE(ovt_tcm, info_appfw)
STORE_PROTOTYPE(ovt_tcm, irq_en)
STORE_PROTOTYPE(ovt_tcm, reset)
SHOW_STORE_PROTOTYPE(ovt_tcm, ts_suspend)
#ifdef WATCHDOG_SW
STORE_PROTOTYPE(ovt_tcm, watchdog)
#endif
SHOW_STORE_PROTOTYPE(ovt_tcm, no_doze)
SHOW_STORE_PROTOTYPE(ovt_tcm, disable_noise_mitigation)
SHOW_STORE_PROTOTYPE(ovt_tcm, inhibit_frequency_shift)
SHOW_STORE_PROTOTYPE(ovt_tcm, requested_frequency)
SHOW_STORE_PROTOTYPE(ovt_tcm, disable_hsync)
SHOW_STORE_PROTOTYPE(ovt_tcm, rezero_on_exit_deep_sleep)
SHOW_STORE_PROTOTYPE(ovt_tcm, charger_connected)
SHOW_STORE_PROTOTYPE(ovt_tcm, roate_horizontal_level)
SHOW_STORE_PROTOTYPE(ovt_tcm, ear_phone_connected)
SHOW_STORE_PROTOTYPE(ovt_tcm, no_baseline_relaxation)
SHOW_STORE_PROTOTYPE(ovt_tcm, in_wakeup_gesture_mode)
SHOW_STORE_PROTOTYPE(ovt_tcm, stimulus_fingers)
SHOW_STORE_PROTOTYPE(ovt_tcm, grip_suppression_enabled)
SHOW_STORE_PROTOTYPE(ovt_tcm, enable_thick_glove)
SHOW_STORE_PROTOTYPE(ovt_tcm, enable_glove)


static struct device_attribute *attrs[] = {
	ATTRIFY(info),
	ATTRIFY(info_appfw),
	ATTRIFY(irq_en),
	ATTRIFY(reset),
	ATTRIFY(ts_suspend),
#ifdef WATCHDOG_SW
	ATTRIFY(watchdog),
#endif
};

static struct device_attribute *dynamic_config_attrs[] = {
	ATTRIFY(no_doze),
	ATTRIFY(disable_noise_mitigation),
	ATTRIFY(inhibit_frequency_shift),
	ATTRIFY(requested_frequency),
	ATTRIFY(disable_hsync),
	ATTRIFY(rezero_on_exit_deep_sleep),
	ATTRIFY(charger_connected),
	ATTRIFY(roate_horizontal_level),
	ATTRIFY(ear_phone_connected),
	ATTRIFY(no_baseline_relaxation),
	ATTRIFY(in_wakeup_gesture_mode),
	ATTRIFY(stimulus_fingers),
	ATTRIFY(grip_suppression_enabled),
	ATTRIFY(enable_thick_glove),
	ATTRIFY(enable_glove),
};

static int ovt_tcm_get_app_info(struct ovt_tcm_hcd *tcm_hcd);
static int ovt_tcm_sensor_detection(struct ovt_tcm_hcd *tcm_hcd);
static void ovt_tcm_check_hdl(struct ovt_tcm_hcd *tcm_hcd,
							unsigned char id);


static int ovt_tcm_suspend(struct device *dev);
static int ovt_tcm_resume(struct device *dev);

static ssize_t ovt_tcm_sysfs_ts_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct ovt_tcm_hcd *tcm_hcd;

	tcm_hcd = g_tcm_hcd;

	if (kstrtouint(buf, 10, &input))
		return -EINVAL;

	if (input == 1)
		ovt_tcm_suspend(&tcm_hcd->pdev->dev);
	else if (input == 0)
		ovt_tcm_resume(&tcm_hcd->pdev->dev);
	else
		return -EINVAL;

	return count;
}

static ssize_t ovt_tcm_sysfs_ts_suspend_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ovt_tcm_hcd *tcm_hcd;

	tcm_hcd = g_tcm_hcd;

	return sprintf(buf, "%s\n",
		tcm_hcd->in_suspend ? "true" : "false");
}


static ssize_t ovt_tcm_sysfs_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned int count;
	struct ovt_tcm_hcd *tcm_hcd;

	tcm_hcd = g_tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = tcm_hcd->identify(tcm_hcd, true);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do identification\n");
		goto exit;
	}

	count = 0;

	retval = snprintf(buf, PAGE_SIZE - count,
			"TouchComm version:  %d\n",
			tcm_hcd->id_info.version);
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	if (OMNIVISION_TCM_ID_SUBVERSION == 0) {
		retval = snprintf(buf, PAGE_SIZE - count,
				"Driver version:     %d.%d\n",
				(unsigned char)(OMNIVISION_TCM_ID_VERSION >> 8),
				(unsigned char)OMNIVISION_TCM_ID_VERSION);
	} else {
		retval = snprintf(buf, PAGE_SIZE - count,
				"Driver version:     %d.%d.%d\n",
				(unsigned char)(OMNIVISION_TCM_ID_VERSION >> 8),
				(unsigned char)OMNIVISION_TCM_ID_VERSION,
				OMNIVISION_TCM_ID_SUBVERSION);
	}
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	switch (tcm_hcd->id_info.mode) {
	case MODE_APPLICATION_FIRMWARE:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      Application Firmware\n");
		if (retval < 0)
			goto exit;
		break;
	case MODE_HOSTDOWNLOAD_FIRMWARE:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      Host Download Firmware\n");
		if (retval < 0)
			goto exit;
		break;
	case MODE_BOOTLOADER:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      Bootloader\n");
		if (retval < 0)
			goto exit;
		break;
	case MODE_TDDI_BOOTLOADER:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      TDDI Bootloader\n");
		if (retval < 0)
			goto exit;
		break;
	case MODE_TDDI_HOSTDOWNLOAD_BOOTLOADER:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      TDDI Host Download Bootloader\n");
		if (retval < 0)
			goto exit;
		break;
	case MODE_ROMBOOTLOADER:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      Rom Bootloader\n");
		if (retval < 0)
			goto exit;
		break;
	default:
		retval = snprintf(buf, PAGE_SIZE - count,
				"Firmware mode:      Unknown (%d)\n",
				tcm_hcd->id_info.mode);
		if (retval < 0)
			goto exit;
		break;
	}
	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
			"Part number:        ");
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = secure_memcpy(buf,
			PAGE_SIZE - count,
			tcm_hcd->id_info.part_number,
			sizeof(tcm_hcd->id_info.part_number),
			sizeof(tcm_hcd->id_info.part_number));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy part number string\n");
		goto exit;
	}
	buf += sizeof(tcm_hcd->id_info.part_number);
	count += sizeof(tcm_hcd->id_info.part_number);

	retval = snprintf(buf, PAGE_SIZE - count,
			"\n");
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
			"Packrat number:     %d\n",
			tcm_hcd->packrat_number);
	if (retval < 0)
		goto exit;

	count += retval;

	retval = count;

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t ovt_tcm_sysfs_info_appfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned int count;
	struct ovt_tcm_hcd *tcm_hcd;
	int i;

	tcm_hcd = g_tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = ovt_tcm_get_app_info(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get app info\n");
		goto exit;
	}

	count = 0;

	retval = snprintf(buf, PAGE_SIZE - count,
		"app info version:  %d\n",
		le2_to_uint(tcm_hcd->app_info.version));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"app info status:  %d\n",
		le2_to_uint(tcm_hcd->app_info.status));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"static config size:  %d\n",
		le2_to_uint(tcm_hcd->app_info.static_config_size));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"dynamic config size:  %d\n",
		le2_to_uint(tcm_hcd->app_info.dynamic_config_size));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"app config block:  %d\n",
		le2_to_uint(tcm_hcd->app_info.app_config_start_write_block));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"app config size:  %d\n",
		le2_to_uint(tcm_hcd->app_info.app_config_size));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"touch report config max size:  %d\n",
		le2_to_uint(tcm_hcd->app_info.max_touch_report_config_size));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"touch report payload max size:  %d\n",
		le2_to_uint(tcm_hcd->app_info.max_touch_report_payload_size));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count, "config id:  ");
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	for (i = 0; i < sizeof(tcm_hcd->app_info.customer_config_id); i++) {
		retval = snprintf(buf, PAGE_SIZE - count,
			"0x%2x ", tcm_hcd->app_info.customer_config_id[i]);
		buf += retval;
		count += retval;
	}

	retval = snprintf(buf, PAGE_SIZE - count, "\n");
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"max x:  %d\n",
		le2_to_uint(tcm_hcd->app_info.max_x));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"max y:  %d\n",
		le2_to_uint(tcm_hcd->app_info.max_y));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"max objects:  %d\n",
		le2_to_uint(tcm_hcd->app_info.max_objects));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"num cols:  %d\n",
		le2_to_uint(tcm_hcd->app_info.num_of_image_cols));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"num rows:  %d\n",
		le2_to_uint(tcm_hcd->app_info.num_of_image_rows));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"num buttons:  %d\n",
		le2_to_uint(tcm_hcd->app_info.num_of_buttons));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"has profile:  %d\n",
		le2_to_uint(tcm_hcd->app_info.has_hybrid_data));
	if (retval < 0)
		goto exit;

	buf += retval;
	count += retval;

	retval = snprintf(buf, PAGE_SIZE - count,
		"num force electrodes:  %d\n",
		le2_to_uint(tcm_hcd->app_info.num_of_force_elecs));
	if (retval < 0)
		goto exit;

	count += retval;

	retval = count;

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t ovt_tcm_sysfs_irq_en_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct ovt_tcm_hcd *tcm_hcd;

	tcm_hcd = g_tcm_hcd;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	mutex_lock(&tcm_hcd->extif_mutex);

	if (input == 0) {
		retval = tcm_hcd->enable_irq(tcm_hcd, false, true);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to disable interrupt\n");
			goto exit;
		}
	} else if (input == 1) {
		retval = tcm_hcd->enable_irq(tcm_hcd, true, NULL);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to enable interrupt\n");
			goto exit;
		}
	} else {
		retval = -EINVAL;
		goto exit;
	}

	retval = count;

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t ovt_tcm_sysfs_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	bool hw_reset;
	unsigned int input;
	struct ovt_tcm_hcd *tcm_hcd;

	tcm_hcd = g_tcm_hcd;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input == 1)
		hw_reset = false;
	else if (input == 2)
		hw_reset = true;
	else
		return -EINVAL;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = tcm_hcd->reset_n_reinit(tcm_hcd, hw_reset, true);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do reset and reinit\n");
		goto exit;
	}

	retval = count;

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

#ifdef WATCHDOG_SW
static ssize_t ovt_tcm_sysfs_watchdog_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct ovt_tcm_hcd *tcm_hcd;

	tcm_hcd = g_tcm_hcd;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input != 0 && input != 1)
		return -EINVAL;

	mutex_lock(&tcm_hcd->extif_mutex);

	tcm_hcd->watchdog.run = input;
	tcm_hcd->update_watchdog(tcm_hcd, input);

	mutex_unlock(&tcm_hcd->extif_mutex);

	return count;
}
#endif

dynamic_config_sysfs(no_doze, DC_NO_DOZE)

dynamic_config_sysfs(disable_noise_mitigation, DC_DISABLE_NOISE_MITIGATION)

dynamic_config_sysfs(inhibit_frequency_shift, DC_INHIBIT_FREQUENCY_SHIFT)

dynamic_config_sysfs(requested_frequency, DC_REQUESTED_FREQUENCY)

dynamic_config_sysfs(disable_hsync, DC_DISABLE_HSYNC)

dynamic_config_sysfs(rezero_on_exit_deep_sleep, DC_REZERO_ON_EXIT_DEEP_SLEEP)

dynamic_config_advance_sysfs(charger_connected, DC_CHARGER_CONNECTED)

dynamic_config_advance_sysfs(roate_horizontal_level, DC_ENABLE_ROATE_HORIZONTAL_LEVEL)

dynamic_config_advance_sysfs(ear_phone_connected, DC_ENABLE_EAR_PHONE)

dynamic_config_sysfs(no_baseline_relaxation, DC_NO_BASELINE_RELAXATION)

dynamic_config_sysfs(in_wakeup_gesture_mode, DC_IN_WAKEUP_GESTURE_MODE)

dynamic_config_sysfs(stimulus_fingers, DC_STIMULUS_FINGERS)

dynamic_config_sysfs(grip_suppression_enabled, DC_GRIP_SUPPRESSION_ENABLED)

dynamic_config_sysfs(enable_thick_glove, DC_ENABLE_THICK_GLOVE)

dynamic_config_sysfs(enable_glove, DC_ENABLE_GLOVE)


int ovt_tcm_add_module(struct ovt_tcm_module_cb *mod_cb, bool insert)
{
	struct ovt_tcm_module_handler *mod_handler;

	if (!mod_pool.initialized) {
		mutex_init(&mod_pool.mutex);
		INIT_LIST_HEAD(&mod_pool.list);
		mod_pool.initialized = true;
	}

	mutex_lock(&mod_pool.mutex);

	if (insert) {
		mod_handler = kzalloc(sizeof(*mod_handler), GFP_KERNEL);
		if (!mod_handler) {
			pr_err("%s: Failed to allocate memory for mod_handler\n",
					__func__);
			mutex_unlock(&mod_pool.mutex);
			return -ENOMEM;
		}
		mod_handler->mod_cb = mod_cb;
		mod_handler->insert = true;
		mod_handler->detach = false;
		list_add_tail(&mod_handler->link, &mod_pool.list);
	} else if (!list_empty(&mod_pool.list)) {
		list_for_each_entry(mod_handler, &mod_pool.list, link) {
			if (mod_handler->mod_cb->type == mod_cb->type) {
				mod_handler->insert = false;
				mod_handler->detach = true;
				goto exit;
			}
		}
	}

exit:
	mutex_unlock(&mod_pool.mutex);

	if (mod_pool.queue_work)
		queue_work(mod_pool.workqueue, &mod_pool.work);

	return 0;
}
EXPORT_SYMBOL(ovt_tcm_add_module);

static void ovt_tcm_module_work(struct work_struct *work)
{
	struct ovt_tcm_module_handler *mod_handler;
	struct ovt_tcm_module_handler *tmp_handler;
	struct ovt_tcm_hcd *tcm_hcd = mod_pool.tcm_hcd;

	mutex_lock(&mod_pool.mutex);

	if (!list_empty(&mod_pool.list)) {
		list_for_each_entry_safe(mod_handler,
				tmp_handler,
				&mod_pool.list,
				link) {
			if (mod_handler->insert) {
				if (mod_handler->mod_cb->init)
					mod_handler->mod_cb->init(tcm_hcd);
				mod_handler->insert = false;
			}
			if (mod_handler->detach) {
				if (mod_handler->mod_cb->remove)
					mod_handler->mod_cb->remove(tcm_hcd);
				list_del(&mod_handler->link);
				kfree(mod_handler);
			}
		}
	}

	mutex_unlock(&mod_pool.mutex);

	return;
}


#ifdef REPORT_NOTIFIER
/**
 * ovt_tcm_report_notifier() - notify occurrence of report received from device
 *
 * @data: handle of core module
 *
 * The occurrence of the report generated by the device is forwarded to the
 * asynchronous inbox of each registered application module.
 */
static int ovt_tcm_report_notifier(void *data)
{
	struct sched_param param = { .sched_priority = NOTIFIER_PRIORITY };
	struct ovt_tcm_module_handler *mod_handler;
	struct ovt_tcm_hcd *tcm_hcd = data;

	sched_setscheduler(current, SCHED_RR, &param);

	set_current_state(TASK_INTERRUPTIBLE);

	while (!kthread_should_stop()) {
		schedule();

		if (kthread_should_stop())
			break;

		set_current_state(TASK_RUNNING);

		mutex_lock(&mod_pool.mutex);

		if (!list_empty(&mod_pool.list)) {
			list_for_each_entry(mod_handler, &mod_pool.list, link) {
				if (!mod_handler->insert &&
						!mod_handler->detach &&
						(mod_handler->mod_cb->asyncbox))
					mod_handler->mod_cb->asyncbox(tcm_hcd);
			}
		}

		mutex_unlock(&mod_pool.mutex);

		set_current_state(TASK_INTERRUPTIBLE);
	};

	return 0;
}
#endif

/**
 * ovt_tcm_dispatch_report() - dispatch report received from device
 *
 * @tcm_hcd: handle of core module
 *
 * The report generated by the device is forwarded to the synchronous inbox of
 * each registered application module for further processing. In addition, the
 * report notifier thread is woken up for asynchronous notification of the
 * report occurrence.
 */

#define FW_LOG_BUFFER_SIZE 2048
static unsigned char fw_log[FW_LOG_BUFFER_SIZE];

static void ovt_tcm_dispatch_report(struct ovt_tcm_hcd *tcm_hcd)
{
	struct ovt_tcm_module_handler *mod_handler;

	LOCK_BUFFER(tcm_hcd->in);
	LOCK_BUFFER(tcm_hcd->report.buffer);

	tcm_hcd->report.buffer.buf = &tcm_hcd->in.buf[MESSAGE_HEADER_SIZE];

	tcm_hcd->report.buffer.buf_size = tcm_hcd->in.buf_size;
	tcm_hcd->report.buffer.buf_size -= MESSAGE_HEADER_SIZE;

	tcm_hcd->report.buffer.data_length = tcm_hcd->payload_length;

	tcm_hcd->report.id = tcm_hcd->status_report_code;

	/* report directly if touch report is received */
	if (tcm_hcd->report.id == REPORT_TOUCH) {
		if (tcm_hcd->report_touch)
			tcm_hcd->report_touch();

	} else if (tcm_hcd->report.id == REPORT_FW_PRINTF) {
        int cpy_length;
		if (tcm_hcd->report.buffer.data_length >= FW_LOG_BUFFER_SIZE - 1) {
			cpy_length = FW_LOG_BUFFER_SIZE - 1;
		} else {
			cpy_length = tcm_hcd->report.buffer.data_length;
		}
		memset(fw_log, 0, sizeof(fw_log));
        secure_memcpy(fw_log, FW_LOG_BUFFER_SIZE - 1, tcm_hcd->report.buffer.buf, tcm_hcd->report.buffer.buf_size, cpy_length);
        LOGE(tcm_hcd->pdev->dev.parent,
				"TouchFWLog: %s\n", fw_log);
    } else {

		/* once an identify report is received, */
		/* reinitialize touch in case any changes */
		if ((tcm_hcd->report.id == REPORT_IDENTIFY) &&
				IS_FW_MODE(tcm_hcd->id_info.mode)) {

			if (atomic_read(&tcm_hcd->helper.task) == HELP_NONE) {
				atomic_set(&tcm_hcd->helper.task,
						HELP_TOUCH_REINIT);
				queue_work(tcm_hcd->helper.workqueue,
						&tcm_hcd->helper.work);
			}
		}

		/* dispatch received report to the other modules */
		mutex_lock(&mod_pool.mutex);

		if (!list_empty(&mod_pool.list)) {
			list_for_each_entry(mod_handler, &mod_pool.list, link) {
				if (!mod_handler->insert &&
						!mod_handler->detach &&
						(mod_handler->mod_cb->syncbox))
					mod_handler->mod_cb->syncbox(tcm_hcd);
			}
		}

		tcm_hcd->async_report_id = tcm_hcd->status_report_code;

		mutex_unlock(&mod_pool.mutex);
	}

	UNLOCK_BUFFER(tcm_hcd->report.buffer);
	UNLOCK_BUFFER(tcm_hcd->in);

#ifdef REPORT_NOTIFIER
	wake_up_process(tcm_hcd->notifier_thread);
#endif

	return;
}

/**
 * ovt_tcm_dispatch_response() - dispatch response received from device
 *
 * @tcm_hcd: handle of core module
 *
 * The response to a command is forwarded to the sender of the command.
 */
static void ovt_tcm_dispatch_response(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;

	if (atomic_read(&tcm_hcd->command_status) != CMD_BUSY)
		return;

	tcm_hcd->response_code = tcm_hcd->status_report_code;

	if (tcm_hcd->payload_length == 0) {
		atomic_set(&tcm_hcd->command_status, CMD_IDLE);
		goto exit;
	}

	LOCK_BUFFER(tcm_hcd->resp);

	retval = ovt_tcm_alloc_mem(tcm_hcd,
			&tcm_hcd->resp,
			tcm_hcd->payload_length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for tcm_hcd->resp.buf\n");
		UNLOCK_BUFFER(tcm_hcd->resp);
		atomic_set(&tcm_hcd->command_status, CMD_ERROR);
		goto exit;
	}

	LOCK_BUFFER(tcm_hcd->in);

	retval = secure_memcpy(tcm_hcd->resp.buf,
			tcm_hcd->resp.buf_size,
			&tcm_hcd->in.buf[MESSAGE_HEADER_SIZE],
			tcm_hcd->in.buf_size - MESSAGE_HEADER_SIZE,
			tcm_hcd->payload_length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy payload\n");
		UNLOCK_BUFFER(tcm_hcd->in);
		UNLOCK_BUFFER(tcm_hcd->resp);
		atomic_set(&tcm_hcd->command_status, CMD_ERROR);
		goto exit;
	}

	tcm_hcd->resp.data_length = tcm_hcd->payload_length;

	UNLOCK_BUFFER(tcm_hcd->in);
	UNLOCK_BUFFER(tcm_hcd->resp);

	atomic_set(&tcm_hcd->command_status, CMD_IDLE);

exit:
	complete(&response_complete);

	return;
}

/**
 * ovt_tcm_dispatch_message() - dispatch message received from device
 *
 * @tcm_hcd: handle of core module
 *
 * The information received in the message read in from the device is dispatched
 * to the appropriate destination based on whether the information represents a
 * report or a response to a command.
 */
static void ovt_tcm_dispatch_message(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *build_id;
	unsigned int payload_length;
	unsigned int max_write_size;

	if (tcm_hcd->status_report_code == REPORT_IDENTIFY) {
		payload_length = tcm_hcd->payload_length;

		LOCK_BUFFER(tcm_hcd->in);

		retval = secure_memcpy((unsigned char *)&tcm_hcd->id_info,
				sizeof(tcm_hcd->id_info),
				&tcm_hcd->in.buf[MESSAGE_HEADER_SIZE],
				tcm_hcd->in.buf_size - MESSAGE_HEADER_SIZE,
				MIN(sizeof(tcm_hcd->id_info), payload_length));
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to copy identification info\n");
			UNLOCK_BUFFER(tcm_hcd->in);
			return;
		}

		UNLOCK_BUFFER(tcm_hcd->in);

		build_id = tcm_hcd->id_info.build_id;
		tcm_hcd->packrat_number = le4_to_uint(build_id);

		max_write_size = le2_to_uint(tcm_hcd->id_info.max_write_size);
		tcm_hcd->wr_chunk_size = MIN(max_write_size, WR_CHUNK_SIZE);
		if (tcm_hcd->wr_chunk_size == 0)
			tcm_hcd->wr_chunk_size = max_write_size;

		LOGN(tcm_hcd->pdev->dev.parent,
				"Received identify report (firmware mode = 0x%02x)\n",
				tcm_hcd->id_info.mode);

		if (atomic_read(&tcm_hcd->command_status) == CMD_BUSY) {
			switch (tcm_hcd->command) {
			case CMD_RESET:
			case CMD_RUN_BOOTLOADER_FIRMWARE:
			case CMD_RUN_APPLICATION_FIRMWARE:
			case CMD_ENTER_PRODUCTION_TEST_MODE:
			case CMD_ROMBOOT_RUN_BOOTLOADER_FIRMWARE:
			case CMD_REBOOT_TO_ROM_BOOTLOADER:
				tcm_hcd->response_code = STATUS_OK;
				atomic_set(&tcm_hcd->command_status, CMD_IDLE);
				complete(&response_complete);
				break;
			default:
				LOGN(tcm_hcd->pdev->dev.parent,
						"Device has been reset\n");
				atomic_set(&tcm_hcd->command_status, CMD_ERROR);
				complete(&response_complete);
				break;
			}
		}

		if ((tcm_hcd->id_info.mode == MODE_ROMBOOTLOADER) &&
				tcm_hcd->in_hdl_mode) {

			retval = wait_for_completion_timeout(tcm_hcd->helper.helper_completion,
				msecs_to_jiffies(500));
			if (retval == 0) {
				LOGE(tcm_hcd->pdev->dev.parent, "timeout to wait for helper completion\n");
				return;
			}
			if (atomic_read(&tcm_hcd->helper.task) ==
					HELP_NONE) {
				atomic_set(&tcm_hcd->helper.task,
						HELP_SEND_ROMBOOT_HDL);
				queue_work(tcm_hcd->helper.workqueue,
						&tcm_hcd->helper.work);
			} else {
				LOGN(tcm_hcd->pdev->dev.parent,
						"Helper thread is busy\n");
			}
			return;
		}

#ifdef FORCE_RUN_APPLICATION_FIRMWARE
		if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode) &&
				!mutex_is_locked(&tcm_hcd->reset_mutex)) {

			if (atomic_read(&tcm_hcd->helper.task) == HELP_NONE) {
				atomic_set(&tcm_hcd->helper.task,
						HELP_RUN_APPLICATION_FIRMWARE);
				queue_work(tcm_hcd->helper.workqueue,
						&tcm_hcd->helper.work);
				return;
			}
		}
#endif

		/* To avoid the identify report dispatching during the HDL. */
		if (atomic_read(&tcm_hcd->host_downloading)) {
			LOGN(tcm_hcd->pdev->dev.parent,
					"Switched to TCM mode and going to download the configs\n");
			return;
		}
	}


	if (tcm_hcd->status_report_code >= REPORT_IDENTIFY)
		ovt_tcm_dispatch_report(tcm_hcd);
	else
		ovt_tcm_dispatch_response(tcm_hcd);

	return;
}

/**
 * ovt_tcm_continued_read() - retrieve entire payload from device
 *
 * @tcm_hcd: handle of core module
 *
 * Read transactions are carried out until the entire payload is retrieved from
 * the device and stored in the handle of the core module.
 */
static int ovt_tcm_continued_read(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char marker;
	unsigned char code;
	unsigned int idx;
	unsigned int offset;
	unsigned int chunks;
	unsigned int chunk_space;
	unsigned int xfer_length;
	unsigned int total_length;
	unsigned int remaining_length;

	total_length = MESSAGE_HEADER_SIZE + tcm_hcd->payload_length + 1;

	remaining_length = total_length - tcm_hcd->read_length;

	LOCK_BUFFER(tcm_hcd->in);

	retval = ovt_tcm_realloc_mem(tcm_hcd,
			&tcm_hcd->in,
			total_length + 1);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to reallocate memory for tcm_hcd->in.buf\n");
		UNLOCK_BUFFER(tcm_hcd->in);
		return retval;
	}

	/* available chunk space for payload = total chunk size minus header
	 * marker byte and header code byte */
	if (tcm_hcd->rd_chunk_size == 0)
		chunk_space = remaining_length;
	else
		chunk_space = tcm_hcd->rd_chunk_size - 2;

	chunks = ceil_div(remaining_length, chunk_space);

	chunks = chunks == 0 ? 1 : chunks;

	offset = tcm_hcd->read_length;

	LOCK_BUFFER(tcm_hcd->temp);

	for (idx = 0; idx < chunks; idx++) {
		if (remaining_length > chunk_space)
			xfer_length = chunk_space;
		else
			xfer_length = remaining_length;

		if (xfer_length == 1) {
			tcm_hcd->in.buf[offset] = MESSAGE_PADDING;
			offset += xfer_length;
			remaining_length -= xfer_length;
			continue;
		}

		retval = ovt_tcm_alloc_mem(tcm_hcd,
				&tcm_hcd->temp,
				xfer_length + 2);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to allocate memory for tcm_hcd->temp.buf\n");
			UNLOCK_BUFFER(tcm_hcd->temp);
			UNLOCK_BUFFER(tcm_hcd->in);
			return retval;
		}

		retval = ovt_tcm_read(tcm_hcd,
				tcm_hcd->temp.buf,
				xfer_length + 2);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to read from device\n");
			UNLOCK_BUFFER(tcm_hcd->temp);
			UNLOCK_BUFFER(tcm_hcd->in);
			return retval;
		}

		marker = tcm_hcd->temp.buf[0];
		code = tcm_hcd->temp.buf[1];

		if (marker != MESSAGE_MARKER) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Incorrect header marker (0x%02x)\n",
					marker);
			UNLOCK_BUFFER(tcm_hcd->temp);
			UNLOCK_BUFFER(tcm_hcd->in);
			return -EIO;
		}

		if (code != STATUS_CONTINUED_READ) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Incorrect header code (0x%02x)\n",
					code);
			UNLOCK_BUFFER(tcm_hcd->temp);
			UNLOCK_BUFFER(tcm_hcd->in);
			return -EIO;
		}

		retval = secure_memcpy(&tcm_hcd->in.buf[offset],
				tcm_hcd->in.buf_size - offset,
				&tcm_hcd->temp.buf[2],
				tcm_hcd->temp.buf_size - 2,
				xfer_length);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to copy payload\n");
			UNLOCK_BUFFER(tcm_hcd->temp);
			UNLOCK_BUFFER(tcm_hcd->in);
			return retval;
		}

		offset += xfer_length;

		remaining_length -= xfer_length;
	}

	UNLOCK_BUFFER(tcm_hcd->temp);
	UNLOCK_BUFFER(tcm_hcd->in);

	return 0;
}

/**
 * ovt_tcm_raw_read() - retrieve specific number of data bytes from device
 *
 * @tcm_hcd: handle of core module
 * @in_buf: buffer for storing data retrieved from device
 * @length: number of bytes to retrieve from device
 *
 * Read transactions are carried out until the specific number of data bytes are
 * retrieved from the device and stored in in_buf.
 */
static int ovt_tcm_raw_read(struct ovt_tcm_hcd *tcm_hcd,
		unsigned char *in_buf, unsigned int length)
{
	int retval;
	unsigned char code;
	unsigned int idx;
	unsigned int offset;
	unsigned int chunks;
	unsigned int chunk_space;
	unsigned int xfer_length;
	unsigned int remaining_length;

	if (length < 2) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid length information\n");
		return -EINVAL;
	}

	/* minus header marker byte and header code byte */
	remaining_length = length - 2;

	/* available chunk space for data = total chunk size minus header marker
	 * byte and header code byte */
	if (tcm_hcd->rd_chunk_size == 0)
		chunk_space = remaining_length;
	else
		chunk_space = tcm_hcd->rd_chunk_size - 2;

	chunks = ceil_div(remaining_length, chunk_space);

	chunks = chunks == 0 ? 1 : chunks;

	offset = 0;

	LOCK_BUFFER(tcm_hcd->temp);

	for (idx = 0; idx < chunks; idx++) {
		if (remaining_length > chunk_space)
			xfer_length = chunk_space;
		else
			xfer_length = remaining_length;

		if (xfer_length == 1) {
			in_buf[offset] = MESSAGE_PADDING;
			offset += xfer_length;
			remaining_length -= xfer_length;
			continue;
		}

		retval = ovt_tcm_alloc_mem(tcm_hcd,
				&tcm_hcd->temp,
				xfer_length + 2);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to allocate memory for tcm_hcd->temp.buf\n");
			UNLOCK_BUFFER(tcm_hcd->temp);
			return retval;
		}

		retval = ovt_tcm_read(tcm_hcd,
				tcm_hcd->temp.buf,
				xfer_length + 2);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to read from device\n");
			UNLOCK_BUFFER(tcm_hcd->temp);
			return retval;
		}

		code = tcm_hcd->temp.buf[1];

		if (idx == 0) {
			retval = secure_memcpy(&in_buf[0],
					length,
					&tcm_hcd->temp.buf[0],
					tcm_hcd->temp.buf_size,
					xfer_length + 2);
		} else {
			if (code != STATUS_CONTINUED_READ) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Incorrect header code (0x%02x)\n",
						code);
				UNLOCK_BUFFER(tcm_hcd->temp);
				return -EIO;
			}

			retval = secure_memcpy(&in_buf[offset],
					length - offset,
					&tcm_hcd->temp.buf[2],
					tcm_hcd->temp.buf_size - 2,
					xfer_length);
		}
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to copy data\n");
			UNLOCK_BUFFER(tcm_hcd->temp);
			return retval;
		}

		if (idx == 0)
			offset += (xfer_length + 2);
		else
			offset += xfer_length;

		remaining_length -= xfer_length;
	}

	UNLOCK_BUFFER(tcm_hcd->temp);

	return 0;
}

/**
 * ovt_tcm_raw_write() - write command/data to device without receiving
 * response
 *
 * @tcm_hcd: handle of core module
 * @command: command to send to device
 * @data: data to send to device
 * @length: length of data in bytes
 *
 * A command and its data, if any, are sent to the device.
 */
static int ovt_tcm_raw_write(struct ovt_tcm_hcd *tcm_hcd,
		unsigned char command, unsigned char *data, unsigned int length)
{
	int retval;
	unsigned int idx;
	unsigned int chunks;
	unsigned int chunk_space;
	unsigned int xfer_length;
	unsigned int remaining_length;

	remaining_length = length;

	/* available chunk space for data = total chunk size minus command
	 * byte */
	if (tcm_hcd->wr_chunk_size == 0)
		chunk_space = remaining_length;
	else
		chunk_space = tcm_hcd->wr_chunk_size - 1;

	chunks = ceil_div(remaining_length, chunk_space);

	chunks = chunks == 0 ? 1 : chunks;

	LOCK_BUFFER(tcm_hcd->out);

	for (idx = 0; idx < chunks; idx++) {
		if (remaining_length > chunk_space)
			xfer_length = chunk_space;
		else
			xfer_length = remaining_length;

		retval = ovt_tcm_alloc_mem(tcm_hcd,
				&tcm_hcd->out,
				xfer_length + 1);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to allocate memory for tcm_hcd->out.buf\n");
			UNLOCK_BUFFER(tcm_hcd->out);
			return retval;
		}

		if (idx == 0)
			tcm_hcd->out.buf[0] = command;
		else
			tcm_hcd->out.buf[0] = CMD_CONTINUE_WRITE;

		if (xfer_length) {
			retval = secure_memcpy(&tcm_hcd->out.buf[1],
					tcm_hcd->out.buf_size - 1,
					&data[idx * chunk_space],
					remaining_length,
					xfer_length);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to copy data\n");
				UNLOCK_BUFFER(tcm_hcd->out);
				return retval;
			}
		}

		retval = ovt_tcm_write(tcm_hcd,
				tcm_hcd->out.buf,
				xfer_length + 1);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to write to device\n");
			UNLOCK_BUFFER(tcm_hcd->out);
			return retval;
		}

		remaining_length -= xfer_length;
	}

	UNLOCK_BUFFER(tcm_hcd->out);

	return 0;
}

/**
 * ovt_tcm_read_message() - read message from device
 *
 * @tcm_hcd: handle of core module
 * @in_buf: buffer for storing data in raw read mode
 * @length: length of data in bytes in raw read mode
 *
 * If in_buf is not NULL, raw read mode is used and ovt_tcm_raw_read() is
 * called. Otherwise, a message including its entire payload is retrieved from
 * the device and dispatched to the appropriate destination.
 */
static int ovt_tcm_read_message(struct ovt_tcm_hcd *tcm_hcd,
		unsigned char *in_buf, unsigned int length)
{
	int retval;
	bool retry;
	unsigned int total_length;
	struct ovt_tcm_message_header *header;

	mutex_lock(&tcm_hcd->rw_ctrl_mutex);

	if (in_buf != NULL) {
		retval = ovt_tcm_raw_read(tcm_hcd, in_buf, length);
		mutex_unlock(&tcm_hcd->rw_ctrl_mutex); // move unlock rw mutex to here
		goto exit;
	}

	retry = true;

retry:
	LOCK_BUFFER(tcm_hcd->in);

	retval = ovt_tcm_read(tcm_hcd,
			tcm_hcd->in.buf,
			tcm_hcd->read_length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read from device\n");
		UNLOCK_BUFFER(tcm_hcd->in);
		if (retry) {
			usleep_range(READ_RETRY_US_MIN, READ_RETRY_US_MAX);
			retry = false;
			goto retry;
		}
		mutex_unlock(&tcm_hcd->rw_ctrl_mutex); // move unlock rw mutex to here
		goto exit;
	}

	header = (struct ovt_tcm_message_header *)tcm_hcd->in.buf;


	if (header->marker != MESSAGE_MARKER) {
		if (!retry) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Incorrect header marker (0x%02x)\n",
					header->marker);
		}
		UNLOCK_BUFFER(tcm_hcd->in);
		retval = -ENXIO;
		if (retry) {
			usleep_range(READ_RETRY_US_MIN, READ_RETRY_US_MAX);
			retry = false;
			goto retry;
		}
		mutex_unlock(&tcm_hcd->rw_ctrl_mutex); // move unlock rw mutex to here
		goto exit;
	}

	tcm_hcd->status_report_code = header->code;

	tcm_hcd->payload_length = le2_to_uint(header->length);

/*
	LOGN(tcm_hcd->pdev->dev.parent,
			"Status report code = 0x%02x\n",
			tcm_hcd->status_report_code);

	LOGN(tcm_hcd->pdev->dev.parent,
			"Payload length = %d\n",
			tcm_hcd->payload_length);
*/

	if (tcm_hcd->status_report_code <= STATUS_ERROR ||
			tcm_hcd->status_report_code == STATUS_INVALID) {
		switch (tcm_hcd->status_report_code) {
		case STATUS_OK:
			break;
		case STATUS_CONTINUED_READ:
			LOGD(tcm_hcd->pdev->dev.parent,
					"Out-of-sync continued read\n");
		case STATUS_IDLE:
		case STATUS_BUSY:
			tcm_hcd->payload_length = 0;
			UNLOCK_BUFFER(tcm_hcd->in);
			mutex_unlock(&tcm_hcd->rw_ctrl_mutex); // move unlock rw mutex to here
			retval = 0;
			goto exit;
		default:
			LOGE(tcm_hcd->pdev->dev.parent,
					"Incorrect Status code (0x%02x)\n",
					tcm_hcd->status_report_code);
			if (tcm_hcd->status_report_code == STATUS_INVALID) {
				if (retry) {
					usleep_range(READ_RETRY_US_MIN,
							READ_RETRY_US_MAX);
					retry = false;
					UNLOCK_BUFFER(tcm_hcd->in);
					goto retry;
				} else {
					tcm_hcd->payload_length = 0;
				}
			}
		}
	}

	total_length = MESSAGE_HEADER_SIZE + tcm_hcd->payload_length + 1;

#ifdef PREDICTIVE_READING
	if (total_length <= tcm_hcd->read_length) {
		goto check_padding;
	} else if (total_length - 1 == tcm_hcd->read_length) {
		tcm_hcd->in.buf[total_length - 1] = MESSAGE_PADDING;
		goto check_padding;
	}
#else
	if (tcm_hcd->payload_length == 0) {
		tcm_hcd->in.buf[total_length - 1] = MESSAGE_PADDING;
		goto check_padding;
	}
#endif

	UNLOCK_BUFFER(tcm_hcd->in);

	retval = ovt_tcm_continued_read(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do continued read\n");
		mutex_unlock(&tcm_hcd->rw_ctrl_mutex); // move unlock rw mutex to here
		goto exit;
	};

	LOCK_BUFFER(tcm_hcd->in);

	tcm_hcd->in.buf[0] = MESSAGE_MARKER;
	tcm_hcd->in.buf[1] = tcm_hcd->status_report_code;
	tcm_hcd->in.buf[2] = (unsigned char)tcm_hcd->payload_length;
	tcm_hcd->in.buf[3] = (unsigned char)(tcm_hcd->payload_length >> 8);

check_padding:
	if (tcm_hcd->in.buf[total_length - 1] != MESSAGE_PADDING) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Incorrect message padding byte (0x%02x)\n",
				tcm_hcd->in.buf[total_length - 1]);
		UNLOCK_BUFFER(tcm_hcd->in);
		retval = -EIO;
		mutex_unlock(&tcm_hcd->rw_ctrl_mutex); // move unlock rw mutex to here
		goto exit;
	}

	UNLOCK_BUFFER(tcm_hcd->in);
	mutex_unlock(&tcm_hcd->rw_ctrl_mutex);

#ifdef PREDICTIVE_READING
	total_length = MAX(total_length, MIN_READ_LENGTH);
	tcm_hcd->read_length = MIN(total_length, tcm_hcd->rd_chunk_size);
	if (tcm_hcd->rd_chunk_size == 0)
		tcm_hcd->read_length = total_length;
#endif
	if (tcm_hcd->is_detected)
		ovt_tcm_dispatch_message(tcm_hcd);

	retval = 0;

exit:
	if (retval < 0) {
		if (atomic_read(&tcm_hcd->command_status) == CMD_BUSY) {
			atomic_set(&tcm_hcd->command_status, CMD_ERROR);
			complete(&response_complete);
		}
	}

	

	return retval;
}


const unsigned char mode_romboot_loader_valid_command_list[] = {
		CMD_IDENTIFY, 0x03, CMD_RESET, CMD_GET_ROMBOOT_INFO,
		CMD_WRITE_PROGRAM_RAM, CMD_ROMBOOT_RUN_BOOTLOADER_FIRMWARE,
		CMD_SPI_MASTER_WRITE_THEN_READ_EXTENDED, CMD_ENTER_IO_BRIDGE_MODE,
		CMD_ROMBOOT_DOWNLOAD, 0x46, 0x47, 0xa0, 0xa1
};

size_t mode_romboot_loader_valid_command_list_length = sizeof(
		mode_romboot_loader_valid_command_list);

static bool _is_command_in_command_list(unsigned char command,
		const unsigned char* command_list, size_t command_list_length)
{
	int i;
	for(i = 0; i < command_list_length; i++){
		if (command == command_list[i]){
			return true;
		}
	}
	return false;
}

static bool ovt_tcm_is_command_valid_in_mode(struct ovt_tcm_hcd *tcm_hcd,
		unsigned char command)
{
	switch(tcm_hcd->id_info.mode)
	{
	case MODE_BOOTLOADER:
	case MODE_TDDI_BOOTLOADER:
	case MODE_TDDI_HOSTDOWNLOAD_BOOTLOADER:
	case MODE_PRODUCTIONTEST_FIRMWARE:
		LOGE(tcm_hcd->pdev->dev.parent,
				"not implemented checking command valid for mode %02x\n",
				tcm_hcd->id_info.mode);
		return true;
	case MODE_APPLICATION_FIRMWARE:
	case MODE_HOSTDOWNLOAD_FIRMWARE:
		return true;
	case MODE_ROMBOOTLOADER:
		return _is_command_in_command_list(command,
				mode_romboot_loader_valid_command_list,
				mode_romboot_loader_valid_command_list_length);
	default:
		LOGE(tcm_hcd->pdev->dev.parent,
				"mode %02x is not in touchcomm protocol\n",
				tcm_hcd->id_info.mode);
		return false;
	}
}


/**
 * ovt_tcm_write_message() - write message to device and receive response
 *
 * @tcm_hcd: handle of core module
 * @command: command to send to device
 * @payload: payload of command
 * @length: length of payload in bytes
 * @resp_buf: buffer for storing command response
 * @resp_buf_size: size of response buffer in bytes
 * @resp_length: length of command response in bytes
 * @response_code: status code returned in command response
 * @polling_delay_ms: delay time after sending command before resuming polling
 *
 * If resp_buf is NULL, raw write mode is used and ovt_tcm_raw_write() is
 * called. Otherwise, a command and its payload, if any, are sent to the device
 * and the response to the command generated by the device is read in.
 */
static int ovt_tcm_write_message(struct ovt_tcm_hcd *tcm_hcd,
		unsigned char command, unsigned char *payload,
		unsigned int length, unsigned char **resp_buf,
		unsigned int *resp_buf_size, unsigned int *resp_length,
		unsigned char *response_code, unsigned int polling_delay_ms)
{
	int retval;
	unsigned int idx;
	unsigned int chunks;
	unsigned int chunk_space;
	unsigned int xfer_length;
	unsigned int remaining_length;
	unsigned int command_status;
	bool is_romboot_hdl = (command == CMD_ROMBOOT_DOWNLOAD) ? true : false;
	bool is_hdl_reset = (command == CMD_RESET) && (tcm_hcd->in_hdl_mode);

	if (response_code != NULL)
		*response_code = STATUS_INVALID;

	if (!tcm_hcd->do_polling && current->pid == tcm_hcd->isr_pid) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid execution context\n");
		return -EINVAL;
	}

	mutex_lock(&tcm_hcd->command_mutex);

	if (!ovt_tcm_is_command_valid_in_mode(tcm_hcd, command)){
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid command %02x in current mode %02x\n",
				command, tcm_hcd->id_info.mode);
		mutex_unlock(&tcm_hcd->command_mutex);
		return -EINVAL;
	}

	mutex_lock(&tcm_hcd->rw_ctrl_mutex);

	if (resp_buf == NULL) {
		retval = ovt_tcm_raw_write(tcm_hcd, command, payload, length);
		mutex_unlock(&tcm_hcd->rw_ctrl_mutex);
		goto exit;
	}

	if (tcm_hcd->do_polling && polling_delay_ms) {
		cancel_delayed_work_sync(&tcm_hcd->polling_work);
		flush_workqueue(tcm_hcd->polling_workqueue);
	}

	atomic_set(&tcm_hcd->command_status, CMD_BUSY);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
	reinit_completion(&response_complete);
#else
	INIT_COMPLETION(response_complete);
#endif

	tcm_hcd->command = command;

	LOCK_BUFFER(tcm_hcd->resp);

	tcm_hcd->resp.buf = *resp_buf;
	tcm_hcd->resp.buf_size = *resp_buf_size;
	tcm_hcd->resp.data_length = 0;

	UNLOCK_BUFFER(tcm_hcd->resp);

	/* adding two length bytes as part of payload */
	remaining_length = length + 2;

	/* available chunk space for payload = total chunk size minus command
	 * byte */
	if (tcm_hcd->wr_chunk_size == 0)
		chunk_space = remaining_length;
	else
		chunk_space = tcm_hcd->wr_chunk_size - 1;

	if (is_romboot_hdl) {
		if (0) { //force to remaining_length when romboot hdl
			chunk_space = WR_CHUNK_SIZE - 1;
			chunk_space = chunk_space -
					(chunk_space % ROMBOOT_DOWNLOAD_UNIT);
		} else {
			chunk_space = remaining_length;
		}
	}

	chunks = ceil_div(remaining_length, chunk_space);

	chunks = chunks == 0 ? 1 : chunks;

	LOGN(tcm_hcd->pdev->dev.parent,
			"Command = 0x%02x\n",
			command);

	LOCK_BUFFER(tcm_hcd->out);

	for (idx = 0; idx < chunks; idx++) {
		if (remaining_length > chunk_space)
			xfer_length = chunk_space;
		else
			xfer_length = remaining_length;

		retval = ovt_tcm_alloc_mem(tcm_hcd,
				&tcm_hcd->out,
				xfer_length + 1);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to allocate memory for tcm_hcd->out.buf\n");
			UNLOCK_BUFFER(tcm_hcd->out);
			mutex_unlock(&tcm_hcd->rw_ctrl_mutex);
			goto exit;
		}

		if (idx == 0) {
			tcm_hcd->out.buf[0] = command;
			tcm_hcd->out.buf[1] = (unsigned char)length;
			tcm_hcd->out.buf[2] = (unsigned char)(length >> 8);

			if (xfer_length > 2) {
				retval = secure_memcpy(&tcm_hcd->out.buf[3],
						tcm_hcd->out.buf_size - 3,
						payload,
						remaining_length - 2,
						xfer_length - 2);
				if (retval < 0) {
					LOGE(tcm_hcd->pdev->dev.parent,
							"Failed to copy payload\n");
					UNLOCK_BUFFER(tcm_hcd->out);
					mutex_unlock(&tcm_hcd->rw_ctrl_mutex);
					goto exit;
				}
			}
		} else {
			tcm_hcd->out.buf[0] = CMD_CONTINUE_WRITE;

			retval = secure_memcpy(&tcm_hcd->out.buf[1],
					tcm_hcd->out.buf_size - 1,
					&payload[idx * chunk_space - 2],
					remaining_length,
					xfer_length);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to copy payload\n");
				UNLOCK_BUFFER(tcm_hcd->out);
				mutex_unlock(&tcm_hcd->rw_ctrl_mutex);
				goto exit;
			}
		}

		retval = ovt_tcm_write(tcm_hcd,
				tcm_hcd->out.buf,
				xfer_length + 1);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to write to device\n");
			UNLOCK_BUFFER(tcm_hcd->out);
			mutex_unlock(&tcm_hcd->rw_ctrl_mutex);
			goto exit;
		}

		remaining_length -= xfer_length;

		if (chunks > 1)
			usleep_range(WRITE_DELAY_US_MIN, WRITE_DELAY_US_MAX);
	}

	UNLOCK_BUFFER(tcm_hcd->out);

	mutex_unlock(&tcm_hcd->rw_ctrl_mutex);

	if (is_hdl_reset)
		goto exit;

	if (tcm_hcd->do_polling && polling_delay_ms) {
		queue_delayed_work(tcm_hcd->polling_workqueue,
				&tcm_hcd->polling_work,
				msecs_to_jiffies(polling_delay_ms));
	}

	retval = wait_for_completion_timeout(&response_complete,
			msecs_to_jiffies(RESPONSE_TIMEOUT_MS));
	if (retval == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Timed out waiting for response (command 0x%02x)\n",
				tcm_hcd->command);
		retval = -ETIME;
		goto exit;
	}

	command_status = atomic_read(&tcm_hcd->command_status);
	if (command_status != CMD_IDLE) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get valid response (command 0x%02x)\n",
				tcm_hcd->command);
		retval = -EIO;
		goto exit;
	}

	LOCK_BUFFER(tcm_hcd->resp);

	if (tcm_hcd->response_code != STATUS_OK) {
		if (tcm_hcd->resp.data_length) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Error code = 0x%02x (command 0x%02x)\n",
					tcm_hcd->resp.buf[0], tcm_hcd->command);
		}
		retval = -EIO;
	} else {
		retval = 0;
	}

	*resp_buf = tcm_hcd->resp.buf;
	*resp_buf_size = tcm_hcd->resp.buf_size;
	*resp_length = tcm_hcd->resp.data_length;

	if (response_code != NULL)
		*response_code = tcm_hcd->response_code;

	UNLOCK_BUFFER(tcm_hcd->resp);

exit:
	tcm_hcd->command = CMD_NONE;

	atomic_set(&tcm_hcd->command_status, CMD_IDLE);

	mutex_unlock(&tcm_hcd->command_mutex);

	return retval;
}

static int ovt_tcm_wait_hdl(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;

	msleep(HOST_DOWNLOAD_WAIT_MS);

	if (!atomic_read(&tcm_hcd->host_downloading))
		return 0;

	retval = wait_event_interruptible_timeout(tcm_hcd->hdl_wq,
			!atomic_read(&tcm_hcd->host_downloading),
			msecs_to_jiffies(HOST_DOWNLOAD_TIMEOUT_MS));
	if (retval == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Timed out waiting for completion of host download\n");
		atomic_set(&tcm_hcd->host_downloading, 0);
		retval = -EIO;
	} else {
		retval = 0;
	}

	return retval;
}

static void ovt_tcm_check_hdl(struct ovt_tcm_hcd *tcm_hcd, unsigned char id)
{
	struct ovt_tcm_module_handler *mod_handler;

	LOCK_BUFFER(tcm_hcd->report.buffer);

	tcm_hcd->report.buffer.buf = NULL;
	tcm_hcd->report.buffer.buf_size = 0;
	tcm_hcd->report.buffer.data_length = 0;
	tcm_hcd->report.id = id;

	UNLOCK_BUFFER(tcm_hcd->report.buffer);

	mutex_lock(&mod_pool.mutex);

	if (!list_empty(&mod_pool.list)) {
		list_for_each_entry(mod_handler, &mod_pool.list, link) {
			if (!mod_handler->insert &&
					!mod_handler->detach &&
					(mod_handler->mod_cb->syncbox))
				mod_handler->mod_cb->syncbox(tcm_hcd);
		}
	}

	mutex_unlock(&mod_pool.mutex);

	return;
}

#ifdef WATCHDOG_SW
static void ovt_tcm_update_watchdog(struct ovt_tcm_hcd *tcm_hcd, bool en)
{
	cancel_delayed_work_sync(&tcm_hcd->watchdog.work);
	flush_workqueue(tcm_hcd->watchdog.workqueue);

	if (!tcm_hcd->watchdog.run) {
		tcm_hcd->watchdog.count = 0;
		return;
	}

	if (en) {
		queue_delayed_work(tcm_hcd->watchdog.workqueue,
				&tcm_hcd->watchdog.work,
				msecs_to_jiffies(WATCHDOG_DELAY_MS));
	} else {
		tcm_hcd->watchdog.count = 0;
	}

	return;
}

static void ovt_tcm_watchdog_work(struct work_struct *work)
{
	int retval;
	unsigned char marker;
	struct delayed_work *delayed_work =
			container_of(work, struct delayed_work, work);
	struct ovt_tcm_watchdog *watchdog =
			container_of(delayed_work, struct ovt_tcm_watchdog,
			work);
	struct ovt_tcm_hcd *tcm_hcd =
			container_of(watchdog, struct ovt_tcm_hcd, watchdog);

	if (mutex_is_locked(&tcm_hcd->rw_ctrl_mutex))
		goto exit;

	mutex_lock(&tcm_hcd->rw_ctrl_mutex);

	retval = ovt_tcm_read(tcm_hcd,
			&marker,
			1);

	mutex_unlock(&tcm_hcd->rw_ctrl_mutex);

	if (retval < 0 || marker != MESSAGE_MARKER) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read from device\n");

		tcm_hcd->watchdog.count++;

		if (tcm_hcd->watchdog.count >= WATCHDOG_TRIGGER_COUNT) {
			retval = tcm_hcd->reset_n_reinit(tcm_hcd, true, false);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to do reset and reinit\n");
			}
			tcm_hcd->watchdog.count = 0;
		}
	}

exit:
	queue_delayed_work(tcm_hcd->watchdog.workqueue,
			&tcm_hcd->watchdog.work,
			msecs_to_jiffies(WATCHDOG_DELAY_MS));

	return;
}
#endif

static void ovt_tcm_polling_work(struct work_struct *work)
{
	int retval;
	struct delayed_work *delayed_work =
			container_of(work, struct delayed_work, work);
	struct ovt_tcm_hcd *tcm_hcd =
			container_of(delayed_work, struct ovt_tcm_hcd,
			polling_work);

	if (!tcm_hcd->do_polling)
		return;

	retval = tcm_hcd->read_message(tcm_hcd,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read message\n");
		if (retval == -ENXIO && tcm_hcd->hw_if->bus_io->type == BUS_SPI)
			ovt_tcm_check_hdl(tcm_hcd, REPORT_HDL_F35);
	}

	if (!(tcm_hcd->in_suspend && retval < 0)) {
		queue_delayed_work(tcm_hcd->polling_workqueue,
				&tcm_hcd->polling_work,
				msecs_to_jiffies(POLLING_DELAY_MS));
	}

	return;
}

static irqreturn_t ovt_tcm_isr(int irq, void *data)
{
	int retval;
	struct ovt_tcm_hcd *tcm_hcd = data;
	const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	if (unlikely(gpio_get_value(bdata->irq_gpio) != bdata->irq_on_state))
		goto exit;

	tcm_hcd->isr_pid = current->pid;

	if (tcm_hcd->ovt_tcm_driver_removing) {
		msleep(5);
		goto exit;
	}

	retval = tcm_hcd->read_message(tcm_hcd,
			NULL,
			0);
	if (retval < 0) {
        if (tcm_hcd->ovt_tcm_driver_removing) goto exit;

		if (tcm_hcd->sensor_type == TYPE_F35)
			ovt_tcm_check_hdl(tcm_hcd, REPORT_HDL_F35);
		else
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read message\n");
	}

exit:
	return IRQ_HANDLED;
}

static int ovt_tcm_enable_irq(struct ovt_tcm_hcd *tcm_hcd, bool en, bool ns)
{
	int retval;
	const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;
	static bool irq_freed = true;

	mutex_lock(&tcm_hcd->irq_en_mutex);

	if (en) {
		if (tcm_hcd->irq_enabled) {
			LOGD(tcm_hcd->pdev->dev.parent,
					"Interrupt already enabled\n");
			retval = 0;
			goto exit;
		}

		if (bdata->irq_gpio < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Invalid IRQ GPIO\n");
			retval = -EINVAL;
			goto queue_polling_work;
		}

		if (irq_freed) {
			retval = request_threaded_irq(tcm_hcd->irq, NULL,
					ovt_tcm_isr, IRQF_ONESHOT | IRQF_TRIGGER_LOW,
					PLATFORM_DRIVER_NAME, tcm_hcd);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to create interrupt thread\n");
			}
		} else {
			enable_irq(tcm_hcd->irq);
			retval = 0;
		}

queue_polling_work:
		if (retval < 0) {
#ifdef FALL_BACK_ON_POLLING
			queue_delayed_work(tcm_hcd->polling_workqueue,
					&tcm_hcd->polling_work,
					msecs_to_jiffies(POLLING_DELAY_MS));
			tcm_hcd->do_polling = true;
			retval = 0;
#endif
		}

		if (retval < 0)
			goto exit;
		else
			msleep(ENABLE_IRQ_DELAY_MS);
	} else {
		if (!tcm_hcd->irq_enabled) {
			LOGD(tcm_hcd->pdev->dev.parent,
					"Interrupt already disabled\n");
			retval = 0;
			goto exit;
		}

		if (bdata->irq_gpio >= 0)
		{
			if (ns) {
				disable_irq_nosync(tcm_hcd->irq);
			} else {
				disable_irq(tcm_hcd->irq);
				free_irq(tcm_hcd->irq, tcm_hcd);
			}
			irq_freed = !ns;
		}

#ifdef FALL_BACK_ON_POLLING
		if (ns) {
			cancel_delayed_work(&tcm_hcd->polling_work);
		} else {
			cancel_delayed_work_sync(&tcm_hcd->polling_work);
			flush_workqueue(tcm_hcd->polling_workqueue);
		}

		tcm_hcd->do_polling = false;
#endif
	}

	retval = 0;

exit:
	if (retval == 0)
		tcm_hcd->irq_enabled = en;

	mutex_unlock(&tcm_hcd->irq_en_mutex);

	return retval;
}

static int ovt_tcm_set_gpio(struct ovt_tcm_hcd *tcm_hcd, int gpio,
		bool config, int dir, int state)
{
	int retval;
	char label[16];

	if (config) {
		retval = snprintf(label, 16, "tcm_gpio_%d\n", gpio);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to set GPIO label\n");
			return retval;
		}

		retval = gpio_request(gpio, label);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to request GPIO %d\n",
					gpio);
			return retval;
		}

		if (dir == 0)
			retval = gpio_direction_input(gpio);
		else
			retval = gpio_direction_output(gpio, state);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to set GPIO %d direction\n",
					gpio);
			return retval;
		}
	} else {
		gpio_free(gpio);
	}

	return 0;
}

static int ovt_tcm_config_gpio(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;
	const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	if (bdata->irq_gpio >= 0) {
		retval = ovt_tcm_set_gpio(tcm_hcd, bdata->irq_gpio,
				true, 0, 0);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to configure interrupt GPIO\n");
			goto err_set_gpio_irq;
		}
	}

	if (bdata->power_gpio >= 0) {
		retval = ovt_tcm_set_gpio(tcm_hcd, bdata->power_gpio,
				true, 1, !bdata->power_on_state);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to configure power GPIO\n");
			goto err_set_gpio_power;
		}
	}

	if (bdata->reset_gpio >= 0) {
		retval = ovt_tcm_set_gpio(tcm_hcd, bdata->reset_gpio,
				true, 1, !bdata->reset_on_state);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to configure reset GPIO\n");
			goto err_set_gpio_reset;
		}
	}

	if (bdata->power_gpio >= 0) {
		gpio_set_value(bdata->power_gpio, bdata->power_on_state);
		msleep(bdata->power_delay_ms);
	}

	if (bdata->reset_gpio >= 0) {
		gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
		msleep(bdata->reset_active_ms);
		gpio_set_value(bdata->reset_gpio, !bdata->reset_on_state);
		msleep(bdata->reset_delay_ms);
	}

	return 0;

err_set_gpio_reset:
	if (bdata->power_gpio >= 0)
		ovt_tcm_set_gpio(tcm_hcd, bdata->power_gpio, false, 0, 0);

err_set_gpio_power:
	if (bdata->irq_gpio >= 0)
		ovt_tcm_set_gpio(tcm_hcd, bdata->irq_gpio, false, 0, 0);

err_set_gpio_irq:
	return retval;
}

static int ovt_tcm_enable_regulator(struct ovt_tcm_hcd *tcm_hcd, bool en)
{
	int retval;
	const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	if (!en) {
		retval = 0;
		goto disable_pwr_reg;
	}

	if (tcm_hcd->bus_reg) {
		retval = regulator_enable(tcm_hcd->bus_reg);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to enable bus regulator\n");
			goto exit;
		}
	}

	if (tcm_hcd->pwr_reg) {
		retval = regulator_enable(tcm_hcd->pwr_reg);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to enable power regulator\n");
			goto disable_bus_reg;
		}
		msleep(bdata->power_delay_ms);
	}

	return 0;

disable_pwr_reg:
	if (tcm_hcd->pwr_reg)
		regulator_disable(tcm_hcd->pwr_reg);

disable_bus_reg:
	if (tcm_hcd->bus_reg)
		regulator_disable(tcm_hcd->bus_reg);

exit:
	return retval;
}

static int ovt_tcm_get_regulator(struct ovt_tcm_hcd *tcm_hcd, bool get)
{
	int retval;
	const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	if (!get) {
		retval = 0;
		goto regulator_put;
	}

	if (bdata->bus_reg_name != NULL && *bdata->bus_reg_name != 0) {
		tcm_hcd->bus_reg = regulator_get(tcm_hcd->pdev->dev.parent,
				bdata->bus_reg_name);
		if (IS_ERR(tcm_hcd->bus_reg)) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to get bus regulator\n");
			retval = PTR_ERR(tcm_hcd->bus_reg);
			goto regulator_put;
		}
	}

	if (bdata->pwr_reg_name != NULL && *bdata->pwr_reg_name != 0) {
		tcm_hcd->pwr_reg = regulator_get(tcm_hcd->pdev->dev.parent,
				bdata->pwr_reg_name);
		if (IS_ERR(tcm_hcd->pwr_reg)) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to get power regulator\n");
			retval = PTR_ERR(tcm_hcd->pwr_reg);
			goto regulator_put;
		}
	}

	return 0;

regulator_put:
	if (tcm_hcd->bus_reg) {
		regulator_put(tcm_hcd->bus_reg);
		tcm_hcd->bus_reg = NULL;
	}

	if (tcm_hcd->pwr_reg) {
		regulator_put(tcm_hcd->pwr_reg);
		tcm_hcd->pwr_reg = NULL;
	}

	return retval;
}

static int ovt_tcm_get_app_info(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	unsigned int timeout;

	timeout = APP_STATUS_POLL_TIMEOUT_MS;

	resp_buf = NULL;
	resp_buf_size = 0;

get_app_info:
	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_GET_APPLICATION_INFO,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_GET_APPLICATION_INFO));
		goto exit;
	}

	retval = secure_memcpy((unsigned char *)&tcm_hcd->app_info,
			sizeof(tcm_hcd->app_info),
			resp_buf,
			resp_buf_size,
			MIN(sizeof(tcm_hcd->app_info), resp_length));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy application info\n");
		goto exit;
	}

	tcm_hcd->app_status = le2_to_uint(tcm_hcd->app_info.status);

	if (tcm_hcd->app_status == APP_STATUS_BOOTING ||
			tcm_hcd->app_status == APP_STATUS_UPDATING) {
		if (timeout > 0) {
			msleep(APP_STATUS_POLL_MS);
			timeout -= APP_STATUS_POLL_MS;
			goto get_app_info;
		}
	}

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int ovt_tcm_get_boot_info(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	resp_buf = NULL;
	resp_buf_size = 0;

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_GET_BOOT_INFO,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_GET_BOOT_INFO));
		goto exit;
	}

	retval = secure_memcpy((unsigned char *)&tcm_hcd->boot_info,
			sizeof(tcm_hcd->boot_info),
			resp_buf,
			resp_buf_size,
			MIN(sizeof(tcm_hcd->boot_info), resp_length));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy boot info\n");
		goto exit;
	}

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int ovt_tcm_get_romboot_info(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	resp_buf = NULL;
	resp_buf_size = 0;

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_GET_ROMBOOT_INFO,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_GET_ROMBOOT_INFO));
		goto exit;
	}

	retval = secure_memcpy((unsigned char *)&tcm_hcd->romboot_info,
			sizeof(tcm_hcd->romboot_info),
			resp_buf,
			resp_buf_size,
			MIN(sizeof(tcm_hcd->romboot_info), resp_length));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy boot info\n");
		goto exit;
	}

	LOGD(tcm_hcd->pdev->dev.parent,
			"version = %d\n", tcm_hcd->romboot_info.version);

	LOGD(tcm_hcd->pdev->dev.parent,
			"status = 0x%02x\n", tcm_hcd->romboot_info.status);

	LOGD(tcm_hcd->pdev->dev.parent,
			"version = 0x%02x 0x%02x\n",
			tcm_hcd->romboot_info.asic_id[0],
			tcm_hcd->romboot_info.asic_id[1]);

	LOGD(tcm_hcd->pdev->dev.parent,
			"write_block_size_words = %d\n",
			tcm_hcd->romboot_info.write_block_size_words);

	LOGD(tcm_hcd->pdev->dev.parent,
			"max_write_payload_size = %d\n",
			tcm_hcd->romboot_info.max_write_payload_size[0] |
			tcm_hcd->romboot_info.max_write_payload_size[1] << 8);

	LOGD(tcm_hcd->pdev->dev.parent,
			"last_reset_reason = 0x%02x\n",
			tcm_hcd->romboot_info.last_reset_reason);

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int ovt_tcm_identify(struct ovt_tcm_hcd *tcm_hcd, bool id)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	unsigned int max_write_size;

	resp_buf = NULL;
	resp_buf_size = 0;

	mutex_lock(&tcm_hcd->identify_mutex);

	if (!id)
		goto get_info;
	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_IDENTIFY,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_IDENTIFY));
			goto exit;
	}

	retval = secure_memcpy((unsigned char *)&tcm_hcd->id_info,
			sizeof(tcm_hcd->id_info),
			resp_buf,
			resp_buf_size,
			MIN(sizeof(tcm_hcd->id_info), resp_length));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy identification info\n");
		goto exit;
	}

	tcm_hcd->packrat_number = le4_to_uint(tcm_hcd->id_info.build_id);

	max_write_size = le2_to_uint(tcm_hcd->id_info.max_write_size);
	tcm_hcd->wr_chunk_size = MIN(max_write_size, WR_CHUNK_SIZE);
	if (tcm_hcd->wr_chunk_size == 0)
		tcm_hcd->wr_chunk_size = max_write_size;

	LOGN(tcm_hcd->pdev->dev.parent,
		"Firmware build id = %d\n", tcm_hcd->packrat_number);
get_info:
	switch (tcm_hcd->id_info.mode) {
	case MODE_APPLICATION_FIRMWARE:
	case MODE_HOSTDOWNLOAD_FIRMWARE:
		retval = ovt_tcm_get_app_info(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to get application info\n");
			goto exit;
		}
		break;
	case MODE_BOOTLOADER:
	case MODE_TDDI_BOOTLOADER:

		LOGD(tcm_hcd->pdev->dev.parent,
			"In bootloader mode\n");

		retval = ovt_tcm_get_boot_info(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to get boot info\n");
			goto exit;
		}
		break;
	case MODE_ROMBOOTLOADER:

		LOGD(tcm_hcd->pdev->dev.parent,
			"In rombootloader mode\n");

		retval = ovt_tcm_get_romboot_info(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to get application info\n");
			goto exit;
		}
		break;
	default:
		break;
	}

	retval = 0;
/*prize add by lvyuanchuan 20220214 start */
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
	sprintf(current_tp_info.id,"FW:0x%02x", tcm_hcd->packrat_number);
	strcpy(current_tp_info.chip, "td4160");
	strcpy(current_tp_info.vendor, "txd");
	sprintf(current_tp_info.more, "%d*%d", 720, 1612); 
#endif
/*prize add by lvyuanchuan 20220214 end */
exit:
	mutex_unlock(&tcm_hcd->identify_mutex);

	kfree(resp_buf);

	return retval;
}

static int ovt_tcm_run_production_test_firmware(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;
	bool retry;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	retry = true;

	resp_buf = NULL;
	resp_buf_size = 0;

retry:
	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_ENTER_PRODUCTION_TEST_MODE,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			MODE_SWITCH_DELAY_MS);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_ENTER_PRODUCTION_TEST_MODE));
		goto exit;
	}

	if (tcm_hcd->id_info.mode != MODE_PRODUCTIONTEST_FIRMWARE) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run production test firmware\n");
		if (retry) {
			retry = false;
			goto retry;
		}
		retval = -EINVAL;
		goto exit;
	} else if (tcm_hcd->app_status != APP_STATUS_OK) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Application status = 0x%02x\n",
				tcm_hcd->app_status);
	}

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int ovt_tcm_run_application_firmware(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;
	bool retry;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	retry = true;

	resp_buf = NULL;
	resp_buf_size = 0;

retry:
	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_RUN_APPLICATION_FIRMWARE,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			MODE_SWITCH_DELAY_MS);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_RUN_APPLICATION_FIRMWARE));
		goto exit;
	}

	retval = tcm_hcd->identify(tcm_hcd, false);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do identification\n");
		goto exit;
	}

	if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode)) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run application firmware (boot status = 0x%02x)\n",
				tcm_hcd->boot_info.status);
		if (retry) {
			retry = false;
			goto retry;
		}
		retval = -EINVAL;
		goto exit;
	} else if (tcm_hcd->app_status != APP_STATUS_OK) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Application status = 0x%02x\n",
				tcm_hcd->app_status);
	}

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int ovt_tcm_run_bootloader_firmware(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	unsigned char command;

	resp_buf = NULL;
	resp_buf_size = 0;
	command = (tcm_hcd->id_info.mode == MODE_ROMBOOTLOADER) ?
			CMD_ROMBOOT_RUN_BOOTLOADER_FIRMWARE :
			CMD_RUN_BOOTLOADER_FIRMWARE;

	retval = tcm_hcd->write_message(tcm_hcd,
			command,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			MODE_SWITCH_DELAY_MS);
	if (retval < 0) {
		if (tcm_hcd->id_info.mode == MODE_ROMBOOTLOADER) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_ROMBOOT_RUN_BOOTLOADER_FIRMWARE));
		} else {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_RUN_BOOTLOADER_FIRMWARE));
		}
		goto exit;
	}

	if (command != CMD_ROMBOOT_RUN_BOOTLOADER_FIRMWARE) {
		retval = tcm_hcd->identify(tcm_hcd, false);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do identification\n");
		goto exit;
		}

		if (IS_FW_MODE(tcm_hcd->id_info.mode)) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to enter bootloader mode\n");
			retval = -EINVAL;
			goto exit;
		}
	}

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int ovt_tcm_switch_mode(struct ovt_tcm_hcd *tcm_hcd,
		enum firmware_mode mode)
{
	int retval;

	mutex_lock(&tcm_hcd->reset_mutex);

#ifdef WATCHDOG_SW
	tcm_hcd->update_watchdog(tcm_hcd, false);
#endif

	switch (mode) {
	case FW_MODE_BOOTLOADER:
		retval = ovt_tcm_run_bootloader_firmware(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to switch to bootloader mode\n");
			goto exit;
		}
		break;
	case FW_MODE_APPLICATION:
		retval = ovt_tcm_run_application_firmware(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to switch to application mode\n");
			goto exit;
		}
		break;
	case FW_MODE_PRODUCTION_TEST:
		retval = ovt_tcm_run_production_test_firmware(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to switch to production test mode\n");
			goto exit;
		}
		break;
	default:
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid firmware mode\n");
		retval = -EINVAL;
		goto exit;
	}

	retval = 0;

exit:
#ifdef WATCHDOG_SW
	tcm_hcd->update_watchdog(tcm_hcd, true);
#endif

	mutex_unlock(&tcm_hcd->reset_mutex);

	return retval;
}

static int ovt_tcm_get_dynamic_config(struct ovt_tcm_hcd *tcm_hcd,
		enum dynamic_config_id id, unsigned short *value)
{
	int retval;
	unsigned char out_buf;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	resp_buf = NULL;
	resp_buf_size = 0;

	out_buf = (unsigned char)id;

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_GET_DYNAMIC_CONFIG,
			&out_buf,
			sizeof(out_buf),
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_GET_DYNAMIC_CONFIG));
		goto exit;
	}

	if (resp_length < 2) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid data length\n");
		retval = -EINVAL;
		goto exit;
	}

	*value = (unsigned short)le2_to_uint(resp_buf);

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int ovt_tcm_set_dynamic_config(struct ovt_tcm_hcd *tcm_hcd,
		enum dynamic_config_id id, unsigned short value)
{
	int retval;
	unsigned char out_buf[3];
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	resp_buf = NULL;
	resp_buf_size = 0;

	out_buf[0] = (unsigned char)id;
	out_buf[1] = (unsigned char)value;
	out_buf[2] = (unsigned char)(value >> 8);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_SET_DYNAMIC_CONFIG,
			out_buf,
			sizeof(out_buf),
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_SET_DYNAMIC_CONFIG));
		goto exit;
	}

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static void ovt_tcm_reinit_func_when_hdl_done(void)
{
	int retval = 0;
	struct ovt_tcm_hcd *tcm_hcd = g_tcm_hcd;
	int i = 0;
	for (i = 0; i < 256; i++) {
		if (g_ovt_host_dynamic_set_value[i].host_setting_flag) {
			retval = ovt_tcm_set_dynamic_config(tcm_hcd, i, g_ovt_host_dynamic_set_value[i].setting_value);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,"Failed to set dynamic command id 0x%x  to %d\n", i, g_ovt_host_dynamic_set_value[i].setting_value);
			}
		}
	}
}

static int ovt_tcm_get_data_location(struct ovt_tcm_hcd *tcm_hcd,
		enum flash_area area, unsigned int *addr, unsigned int *length)
{
	int retval;
	unsigned char out_buf;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	switch (area) {
	case CUSTOM_LCM:
		out_buf = LCM_DATA;
		break;
	case CUSTOM_OEM:
		out_buf = OEM_DATA;
		break;
	case PPDT:
		out_buf = PPDT_DATA;
		break;
	default:
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid flash area\n");
		return -EINVAL;
	}

	resp_buf = NULL;
	resp_buf_size = 0;

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_GET_DATA_LOCATION,
			&out_buf,
			sizeof(out_buf),
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_GET_DATA_LOCATION));
		goto exit;
	}

	if (resp_length != 4) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid data length\n");
		retval = -EINVAL;
		goto exit;
	}

	*addr = le2_to_uint(&resp_buf[0]);
	*length = le2_to_uint(&resp_buf[2]);

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int ovt_tcm_sleep(struct ovt_tcm_hcd *tcm_hcd, bool en)
{
	int retval;
	unsigned char command;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	command = en ? CMD_ENTER_DEEP_SLEEP : CMD_EXIT_DEEP_SLEEP;

	resp_buf = NULL;
	resp_buf_size = 0;

	retval = tcm_hcd->write_message(tcm_hcd,
			command,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				en ?
				STR(CMD_ENTER_DEEP_SLEEP) :
				STR(CMD_EXIT_DEEP_SLEEP));
		goto exit;
	}

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static int ovt_tcm_reset(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	retval = tcm_hcd->write_message(tcm_hcd,
				CMD_RESET,
				NULL,
				0,
				&resp_buf,
				&resp_buf_size,
				&resp_length,
				NULL,
				bdata->reset_delay_ms);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_RESET));
	}

	return retval;
}


//if lcd request the gpio firstly, maybe lcd driver should provide a function to control tp reset
void ovt_tcm_set_reset_gpio(int value)
{
	struct ovt_tcm_hcd *tcm_hcd;
	tcm_hcd = g_tcm_hcd;
	if (tcm_hcd) {
		const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;
		if (bdata->reset_gpio >= 0) {
			LOGE(tcm_hcd->pdev->dev.parent,"ovt_tcm_set_reset_gpio %d\n", value);
			//already gpio_request and gpio_direction_output, just set value here
			gpio_set_value(bdata->reset_gpio, value ? 1 : 0);
		}
	}
}


static int ovt_tcm_reset_and_reinit(struct ovt_tcm_hcd *tcm_hcd,
		bool hw, bool update_wd)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	struct ovt_tcm_module_handler *mod_handler;
	const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	resp_buf = NULL;
	resp_buf_size = 0;

	mutex_lock(&tcm_hcd->reset_mutex);

#ifdef WATCHDOG_SW
	if (update_wd)
		tcm_hcd->update_watchdog(tcm_hcd, false);
#endif

	if (hw) {
		if (bdata->reset_gpio < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Hardware reset unavailable\n");
			retval = -EINVAL;
			goto exit;
		}
		gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
		msleep(bdata->reset_active_ms);
		gpio_set_value(bdata->reset_gpio, !bdata->reset_on_state);
	} else {
		retval = ovt_tcm_reset(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
			goto exit;
		}
	}

	/* for hdl, the remaining re-init process will be done */
	/* in the helper thread, so wait for the completion here */
	if (tcm_hcd->in_hdl_mode) {
		mutex_unlock(&tcm_hcd->reset_mutex);
		kfree(resp_buf);

		retval = ovt_tcm_wait_hdl(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to wait for completion of host download\n");
			return retval;
		}

#ifdef WATCHDOG_SW
		if (update_wd)
			tcm_hcd->update_watchdog(tcm_hcd, true);
#endif
		return 0;
	}

	msleep(bdata->reset_delay_ms);

	retval = tcm_hcd->identify(tcm_hcd, false);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do identification\n");
		goto exit;
	}

	if (IS_FW_MODE(tcm_hcd->id_info.mode))
		goto get_features;

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_RUN_APPLICATION_FIRMWARE,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			MODE_SWITCH_DELAY_MS);
	if (retval < 0) {
		LOGN(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_RUN_APPLICATION_FIRMWARE));
	}

	retval = tcm_hcd->identify(tcm_hcd, false);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do identification\n");
		goto exit;
	}

get_features:
	LOGN(tcm_hcd->pdev->dev.parent,
			"Firmware mode = 0x%02x\n",
			tcm_hcd->id_info.mode);

	if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode)) {
		LOGN(tcm_hcd->pdev->dev.parent,
				"Boot status = 0x%02x\n",
				tcm_hcd->boot_info.status);
	} else if (tcm_hcd->app_status != APP_STATUS_OK) {
		LOGN(tcm_hcd->pdev->dev.parent,
				"Application status = 0x%02x\n",
				tcm_hcd->app_status);
	}

	if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode))
		goto dispatch_reinit;

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_GET_FEATURES,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGN(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_GET_FEATURES));
	} else {
		retval = secure_memcpy((unsigned char *)&tcm_hcd->features,
				sizeof(tcm_hcd->features),
				resp_buf,
				resp_buf_size,
				MIN(sizeof(tcm_hcd->features), resp_length));
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to copy feature description\n");
		}
	}

dispatch_reinit:
	mutex_lock(&mod_pool.mutex);

	if (!list_empty(&mod_pool.list)) {
		list_for_each_entry(mod_handler, &mod_pool.list, link) {
			if (!mod_handler->insert &&
					!mod_handler->detach &&
					(mod_handler->mod_cb->reinit))
				mod_handler->mod_cb->reinit(tcm_hcd);
		}
	}

	mutex_unlock(&mod_pool.mutex);

	retval = 0;

exit:
#ifdef WATCHDOG_SW
	if (update_wd)
		tcm_hcd->update_watchdog(tcm_hcd, true);
#endif

	mutex_unlock(&tcm_hcd->reset_mutex);

	kfree(resp_buf);

	return retval;
}

static int ovt_tcm_rezero(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;

	resp_buf = NULL;
	resp_buf_size = 0;

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_REZERO,
			NULL,
			0,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_REZERO));
		goto exit;
	}

	retval = 0;

exit:
	kfree(resp_buf);

	return retval;
}

static void ovt_tcm_helper_work(struct work_struct *work)
{
	int retval;
	unsigned char task;
	struct ovt_tcm_module_handler *mod_handler;
	struct ovt_tcm_helper *helper =
			container_of(work, struct ovt_tcm_helper, work);
	struct ovt_tcm_hcd *tcm_hcd =
			container_of(helper, struct ovt_tcm_hcd, helper);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
	reinit_completion(helper->helper_completion);
#else
	INIT_COMPLETION(*(helper->helper_completion));
#endif
	task = atomic_read(&helper->task);
    if (tcm_hcd->ovt_tcm_driver_removing) return;

	switch (task) {

	/* this helper can help to run the application firmware */
	case HELP_RUN_APPLICATION_FIRMWARE:
		mutex_lock(&tcm_hcd->reset_mutex);

#ifdef WATCHDOG_SW
		tcm_hcd->update_watchdog(tcm_hcd, false);
#endif
		retval = ovt_tcm_run_application_firmware(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to switch to application mode\n");
		}
#ifdef WATCHDOG_SW
		tcm_hcd->update_watchdog(tcm_hcd, true);
#endif
		mutex_unlock(&tcm_hcd->reset_mutex);
		break;

	/* the reinit helper is used to notify all installed modules to */
	/* do the re-initialization process, since the HDL is completed */
	case HELP_SEND_REINIT_NOTIFICATION:
		mutex_lock(&tcm_hcd->reset_mutex);

		/* do identify to ensure application firmware is running */
		retval = tcm_hcd->identify(tcm_hcd, true);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Application firmware is not running\n");
			mutex_unlock(&tcm_hcd->reset_mutex);
			break;
		}

		/* init the touch reporting here */
		/* since the HDL is completed */
		retval = touch_reinit(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to initialze touch reporting\n");
			mutex_unlock(&tcm_hcd->reset_mutex);		
			break;
		}
		ovt_tcm_reinit_func_when_hdl_done();
		mutex_lock(&mod_pool.mutex);
		if (!list_empty(&mod_pool.list)) {
			list_for_each_entry(mod_handler, &mod_pool.list, link) {
				if (!mod_handler->insert &&
						!mod_handler->detach &&
						(mod_handler->mod_cb->reinit))
					mod_handler->mod_cb->reinit(tcm_hcd);
			}
		}
		mutex_unlock(&mod_pool.mutex);
		mutex_unlock(&tcm_hcd->reset_mutex);
		wake_up_interruptible(&tcm_hcd->hdl_wq);
		break;

	/* this helper is used to reinit the touch reporting */
	case HELP_TOUCH_REINIT:
		retval = touch_reinit(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to re-initialze touch reporting\n");
		}
		break;

	/* this helper is used to trigger a romboot hdl */
	case HELP_SEND_ROMBOOT_HDL:
		ovt_tcm_check_hdl(tcm_hcd, REPORT_HDL_ROMBOOT);
		break;
	default:
		break;
	}

	atomic_set(&helper->task, HELP_NONE);
	complete(helper->helper_completion);
	return;
}

static int ovt_tcm_do_suspend(struct ovt_tcm_hcd *tcm_hcd)
{
	struct ovt_tcm_module_handler *mod_handler;
	int retval = 0;

	if (tcm_hcd->in_suspend  || tcm_hcd->ovt_tcm_driver_removing)
		return 0;

		LOGE(tcm_hcd->pdev->dev.parent,
				"ovt_tcm_do_suspend\n");

	mutex_lock(&tcm_hcd->suspend_resume_mutex);

#ifdef WATCHDOG_SW
	tcm_hcd->update_watchdog(tcm_hcd, false);
#endif
	if (atomic_read(&tcm_hcd->host_downloading)) {
		retval = ovt_tcm_wait_hdl(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent, "Failed to wait for completion of host download\n");
		}
	}
	//todo wait for hostdownload done here
	touch_suspend(tcm_hcd);

	mutex_lock(&mod_pool.mutex);

	if (!list_empty(&mod_pool.list)) {
		list_for_each_entry(mod_handler, &mod_pool.list, link) {
			if (!mod_handler->insert &&
					!mod_handler->detach &&
					(mod_handler->mod_cb->suspend))
				mod_handler->mod_cb->suspend(tcm_hcd);
		}
	}

	mutex_unlock(&mod_pool.mutex);

	if (!tcm_hcd->wakeup_gesture_enabled) {
		tcm_hcd->enable_irq(tcm_hcd, false, true);
		if (atomic_read(&tcm_hcd->command_status) != CMD_IDLE) {
			atomic_set(&tcm_hcd->command_status, CMD_ERROR);
			complete(&response_complete);
		}
	}

	tcm_hcd->in_suspend = true;
	mutex_unlock(&tcm_hcd->suspend_resume_mutex);
	return 0;
}

static int ovt_tcm_do_resume(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;
	struct ovt_tcm_module_handler *mod_handler;

	if (!tcm_hcd->in_suspend  || tcm_hcd->ovt_tcm_driver_removing)
		return 0;

		LOGE(tcm_hcd->pdev->dev.parent,
				"ovt_tcm_do_resume\n");

	mutex_lock(&tcm_hcd->suspend_resume_mutex);

	if (tcm_hcd->in_hdl_mode) {
		tcm_hcd->enable_irq(tcm_hcd, true, NULL);
		retval = ovt_tcm_wait_hdl(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to wait for completion of host download\n");
			goto exit;
		}
		goto mod_resume;
	} else {
		if (!tcm_hcd->wakeup_gesture_enabled)
			tcm_hcd->enable_irq(tcm_hcd, true, NULL);

#ifdef RESET_ON_RESUME
		msleep(RESET_ON_RESUME_DELAY_MS);
		goto do_reset;
#endif
	}

	if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode) ||
			tcm_hcd->app_status != APP_STATUS_OK) {
		LOGN(tcm_hcd->pdev->dev.parent,
				"Identifying mode = 0x%02x\n",
				tcm_hcd->id_info.mode);
		goto do_reset;
	}

	retval = tcm_hcd->sleep(tcm_hcd, false);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to exit deep sleep\n");
		goto exit;
	}

	retval = ovt_tcm_rezero(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to rezero\n");
		goto exit;
	}

	goto mod_resume;

do_reset:
	retval = tcm_hcd->reset_n_reinit(tcm_hcd, false, true);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do reset and reinit\n");
		goto exit;
	}

	if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode) ||
			tcm_hcd->app_status != APP_STATUS_OK) {
		LOGN(tcm_hcd->pdev->dev.parent,
				"Identifying mode = 0x%02x\n",
				tcm_hcd->id_info.mode);
		retval = 0;
		goto exit;
	}

mod_resume:
	touch_resume(tcm_hcd);

#if OMNIVISION_TCM_CHARGER_MODE_EN
	if (tcm_hcd->charger_mode) {
		pr_err("ovt_tcm_resume charger mode enable need swith");
		retval = tcm_hcd->set_dynamic_config(tcm_hcd,
				 DC_CHARGER_CONNECTED,
				 tcm_hcd->charger_mode);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to switch charger mode\n");
		}
	}
#endif

#ifdef CONFIG_TOUCHSCREEN_OMNIVISION_TCM_EAR_PHONE
	if (tcm_hcd->earphone_mode) {
		pr_err("ovt_tcm_resume earphone mode enable need swith");
		retval = tcm_hcd->set_dynamic_config(tcm_hcd,
				 DC_ENABLE_EAR_PHONE,
				 tcm_hcd->earphone_mode);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to switch earphone mode\n");
		}
	}
#endif
//drv modify by kuangliangjun for double tap wakeup function 20241010 start
#if OMNIVISION_TCM_GLOVE_MODE_EN
	if (tcm_hcd->glove_mode_enable) {
		pr_err("ovt_tcm_resume charger mode enable need swith");
		retval = tcm_hcd->set_dynamic_config(tcm_hcd,
				 DC_CHARGER_CONNECTED,
				 tcm_hcd->glove_mode_enable);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to switch charger mode\n");
		}
	}
#endif
//drv modify by kuangliangjun for double tap wakeup function 20241010 end
#ifdef WATCHDOG_SW
	tcm_hcd->update_watchdog(tcm_hcd, true);
#endif

	mutex_lock(&mod_pool.mutex);

	if (!list_empty(&mod_pool.list)) {
		list_for_each_entry(mod_handler, &mod_pool.list, link) {
			if (!mod_handler->insert &&
					!mod_handler->detach &&
					(mod_handler->mod_cb->resume))
				mod_handler->mod_cb->resume(tcm_hcd);
		}
	}

	mutex_unlock(&mod_pool.mutex);

	retval = 0;

exit:
	tcm_hcd->in_suspend = false;
	mutex_unlock(&tcm_hcd->suspend_resume_mutex);
	return retval;
}

static int ovt_tcm_early_suspend(struct device *dev)
{
	int retval;
	struct ovt_tcm_hcd *tcm_hcd = dev_get_drvdata(dev);
	retval = ovt_tcm_do_suspend(tcm_hcd);
	return 0;
}

static int ovt_tcm_suspend(struct device *dev)
{
	int retval;
	struct ovt_tcm_hcd *tcm_hcd = dev_get_drvdata(dev);
	retval = ovt_tcm_do_suspend(tcm_hcd);
	return 0;
}
static int ovt_tcm_resume(struct device *dev)
{
	struct ovt_tcm_hcd *tcm_hcd = dev_get_drvdata(dev);
#ifdef SPEED_UP_RESUME
	queue_work(tcm_hcd->speed_up_resume_workqueue, &tcm_hcd->speed_up_work);
#else
	ovt_tcm_do_resume(tcm_hcd);
#endif
	return 0;
}
#ifdef SPEED_UP_RESUME
static void speedup_resume(struct work_struct *work)
{
	struct ovt_tcm_hcd *tcm_hcd = container_of(work, struct ovt_tcm_hcd, speed_up_work);
	ovt_tcm_do_resume(tcm_hcd);
	LOGE(tcm_hcd->pdev->dev.parent,"speed up resume end\n");
	return;
}
#endif

//#define MTK_DISP_EARLY_EVENT_BLANK	0x00
//#define MTK_DISP_EVENT_BLANK		0x01
//enum {
//	/* disp power on */
//	MTK_DISP_BLANK_UNBLANK,
//	/* disp power off */
//	MTK_DISP_BLANK_POWERDOWN,
//};

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
static int ovt_tcm_fb_notifier_cb(struct notifier_block *nb,
		unsigned long action, void *data)
{
	int retval;
	int *evdata = data;
	struct ovt_tcm_hcd *tcm_hcd =
			container_of(nb, struct ovt_tcm_hcd, fb_notifier);
	retval = 0;

	LOGE(tcm_hcd->pdev->dev.parent,
				"ovt_tcm_disp_notifier_cb evdata = %d action = %d\n", *evdata ,action);

	if (data && tcm_hcd) {

		if (atomic_read(&tcm_hcd->firmware_flashing) &&
				*evdata == MTK_DISP_BLANK_POWERDOWN) {

			retval = wait_event_interruptible_timeout(
				tcm_hcd->reflash_wq,
				!atomic_read(&tcm_hcd->firmware_flashing),
				msecs_to_jiffies(RESPONSE_TIMEOUT_MS)
				);
			if (retval == 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Timed out waiting for completion of flashing firmware\n");
				atomic_set(&tcm_hcd->firmware_flashing, 0);
				return -EIO;
			} else {
				retval = 0;
			}
		}
/*
		if (action == MTK_DISP_EARLY_EVENT_BLANK && *evdata == MTK_DISP_BLANK_UNBLANK) {
			ovt_tcm_set_reset_gpio(0);
			msleep(5);
			ovt_tcm_set_reset_gpio(1);
			msleep(5);
		}
*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
		if(0)
#else
		if (action == MTK_DISP_EARLY_EVENT_BLANK &&
				*evdata == MTK_DISP_BLANK_POWERDOWN)
#endif
			retval = ovt_tcm_early_suspend(&tcm_hcd->pdev->dev);
		else if (action == MTK_DISP_EVENT_BLANK) {
			if (*evdata == MTK_DISP_BLANK_POWERDOWN) {
				retval = ovt_tcm_suspend(&tcm_hcd->pdev->dev);
				tcm_hcd->fb_ready = 0;
			} else if (*evdata == MTK_DISP_BLANK_UNBLANK) {
#ifndef RESUME_EARLY_UNBLANK
	LOGE(tcm_hcd->pdev->dev.parent,"ovt_tcm_resume\n");
				retval = ovt_tcm_resume(&tcm_hcd->pdev->dev);
				tcm_hcd->fb_ready++;
#endif
			}
		} else 
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
		if (0)
#else
		if (action == MTK_DISP_EARLY_EVENT_BLANK &&
				*evdata == MTK_DISP_BLANK_UNBLANK) 
#endif
		{
#ifdef RESUME_EARLY_UNBLANK
				retval = ovt_tcm_resume(&tcm_hcd->pdev->dev);
				tcm_hcd->fb_ready++;
#endif
		}
	}

	return 0;
}
#elif IS_ENABLED(CONFIG_FB)
static int ovt_tcm_fb_notifier_cb(struct notifier_block *nb,
		unsigned long action, void *data)
{
	int retval;
	int *transition;
	struct fb_event *evdata = data;
	struct ovt_tcm_hcd *tcm_hcd =
			container_of(nb, struct ovt_tcm_hcd, fb_notifier);

	retval = 0;
	if (evdata && evdata->data && tcm_hcd) {
		transition = evdata->data;

		if (atomic_read(&tcm_hcd->firmware_flashing) &&
				*transition == FB_BLANK_POWERDOWN) {

			retval = wait_event_interruptible_timeout(
				tcm_hcd->reflash_wq,
				!atomic_read(&tcm_hcd->firmware_flashing),
				msecs_to_jiffies(RESPONSE_TIMEOUT_MS)
				);
			if (retval == 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Timed out waiting for completion of flashing firmware\n");
				atomic_set(&tcm_hcd->firmware_flashing, 0);
				return -EIO;
			} else {
				retval = 0;
			}
		}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
		if(0)
#else
		if (action == FB_EARLY_EVENT_BLANK &&
				*transition == FB_BLANK_POWERDOWN)
#endif
			retval = ovt_tcm_early_suspend(&tcm_hcd->pdev->dev);
		else if (action == FB_EVENT_BLANK) {
			if (*transition == FB_BLANK_POWERDOWN) {
				retval = ovt_tcm_suspend(&tcm_hcd->pdev->dev);
				tcm_hcd->fb_ready = 0;
			} else if (*transition == FB_BLANK_UNBLANK) {
#ifndef RESUME_EARLY_UNBLANK
				retval = ovt_tcm_resume(&tcm_hcd->pdev->dev);
				tcm_hcd->fb_ready++;
#endif
			}
		} else 
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
		if (0)
#else
		if (action == FB_EARLY_EVENT_BLANK &&
				*transition == FB_BLANK_UNBLANK) 
#endif
		{
#ifdef RESUME_EARLY_UNBLANK
				retval = ovt_tcm_resume(&tcm_hcd->pdev->dev);
				tcm_hcd->fb_ready++;
#endif
		}
	}

	return 0;
}
#endif

static int ovt_tcm_check_f35(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char fn_number;
	int retry = 0;
	const int retry_max = 10;
    const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

f35_boot_recheck:
			retval = ovt_tcm_rmi_read(tcm_hcd,
						PDT_END_ADDR,
						&fn_number,
						sizeof(fn_number));
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read F35 function number\n");
				tcm_hcd->is_detected = false;
				return -ENODEV;
			}

			LOGE(tcm_hcd->pdev->dev.parent,
					"Found F$%02x\n",
					fn_number);

			if (fn_number != RMI_UBL_FN_NUMBER) {
					LOGE(tcm_hcd->pdev->dev.parent,
							"Failed to find F$35, try_times = %d\n",
							retry);
				if (retry < retry_max) {
					msleep(100);                   
                    gpio_set_value(bdata->reset_gpio, 0);
                    msleep(5);
                    gpio_set_value(bdata->reset_gpio, 1);        
                    msleep(5);
					retry++;
			goto f35_boot_recheck;
				}
				tcm_hcd->is_detected = false;
				return -ENODEV;
			}
	return 0;
}

static int ovt_tcm_sensor_detection(struct ovt_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *build_id;
	unsigned int payload_length;
	unsigned int max_write_size;

	tcm_hcd->in_hdl_mode = false;
	tcm_hcd->sensor_type = TYPE_UNKNOWN;

	/* read sensor info for identification */
	retval = tcm_hcd->read_message(tcm_hcd,
			NULL,
			0);

	/* once the tcm communication interface is not ready, */
	/* check whether the device is in F35 mode        */
	if (retval < 0) {
		if (retval == -ENXIO &&
				tcm_hcd->hw_if->bus_io->type == BUS_SPI) {

			retval = ovt_tcm_check_f35(tcm_hcd);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to read TCM message, default to F35\n");
				//retval = 0;
				return retval;
			}
			tcm_hcd->in_hdl_mode = true;
			tcm_hcd->sensor_type = TYPE_F35;
			tcm_hcd->is_detected = true;
			tcm_hcd->rd_chunk_size = HDL_RD_CHUNK_SIZE;
			tcm_hcd->wr_chunk_size = HDL_WR_CHUNK_SIZE;
			LOGN(tcm_hcd->pdev->dev.parent,
					"F35 mode\n");

			return retval;
		} else {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read TCM message\n");

			return retval;
		}
	}

	/* expect to get an identify report after powering on */

	if (tcm_hcd->status_report_code != REPORT_IDENTIFY) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Unexpected report code (0x%02x)\n",
				tcm_hcd->status_report_code);

		return -ENODEV;
	}

	tcm_hcd->is_detected = true;
	payload_length = tcm_hcd->payload_length;

	LOCK_BUFFER(tcm_hcd->in);

	retval = secure_memcpy((unsigned char *)&tcm_hcd->id_info,
				sizeof(tcm_hcd->id_info),
				&tcm_hcd->in.buf[MESSAGE_HEADER_SIZE],
				tcm_hcd->in.buf_size - MESSAGE_HEADER_SIZE,
				MIN(sizeof(tcm_hcd->id_info), payload_length));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy identification info\n");
		UNLOCK_BUFFER(tcm_hcd->in);
		return retval;
	}

	UNLOCK_BUFFER(tcm_hcd->in);

	build_id = tcm_hcd->id_info.build_id;
	tcm_hcd->packrat_number = le4_to_uint(build_id);

	max_write_size = le2_to_uint(tcm_hcd->id_info.max_write_size);
	tcm_hcd->wr_chunk_size = MIN(max_write_size, WR_CHUNK_SIZE);
	if (tcm_hcd->wr_chunk_size == 0)
		tcm_hcd->wr_chunk_size = max_write_size;

	if (tcm_hcd->id_info.mode == MODE_ROMBOOTLOADER) {
		tcm_hcd->in_hdl_mode = true;
		tcm_hcd->sensor_type = TYPE_ROMBOOT;
		tcm_hcd->rd_chunk_size = HDL_RD_CHUNK_SIZE;
		tcm_hcd->wr_chunk_size = HDL_WR_CHUNK_SIZE;
		LOGN(tcm_hcd->pdev->dev.parent,
					"RomBoot mode\n");
	} else if (tcm_hcd->id_info.mode == MODE_APPLICATION_FIRMWARE) {
		tcm_hcd->sensor_type = TYPE_FLASH;
		LOGN(tcm_hcd->pdev->dev.parent,
				"Application mode (build id = %d)\n",
				tcm_hcd->packrat_number);
	} else {
		LOGW(tcm_hcd->pdev->dev.parent,
				"TCM is detected, but mode is 0x%02x\n",
			tcm_hcd->id_info.mode);
	}

	return 0;
}

//drv modify by kuangliangjun for double tap wakeup function 20241010 start 
#if IS_ENABLED(CONFIG_PRIZE_COMMON_NODE)
extern int g_tp_gesture_flag;
extern void prize_common_node_register(char* name,void(*set)(unsigned char on_off));
#endif
static void ovt_tcm_double_type_func(unsigned char on)
{
    struct ovt_tcm_hcd *tcm_hcd;

	tcm_hcd = g_tcm_hcd;

    if (tcm_hcd->in_suspend) {
        LOGE(tcm_hcd->pdev->dev.parent,
				"In suspend,not operation gesture mode!");
        return;
    }
	
    mutex_lock(&tcm_hcd->extif_mutex);
    if (1 == on) {
        LOGE(tcm_hcd->pdev->dev.parent,"enable gesture");
        tcm_hcd->wakeup_gesture_enabled = 1;
	#if IS_ENABLED(CONFIG_PRIZE_COMMON_NODE)
		g_tp_gesture_flag = 1;
	#endif
    } else if (0 == on) {
        LOGE(tcm_hcd->pdev->dev.parent,"disable gesture");
        tcm_hcd->wakeup_gesture_enabled = 0;
	#if IS_ENABLED(CONFIG_PRIZE_COMMON_NODE)
		g_tp_gesture_flag = 0;
	#endif
    }
    mutex_unlock(&tcm_hcd->extif_mutex);
}

//drv modify by kuangliangjun for double tap wakeup function 20241010 end


//drv-modify by shenwenbin for TP charger mode 20240304 start
#if OMNIVISION_TCM_CHARGER_MODE_EN || OMNIVISION_TCM_GLOVE_MODE_EN || defined(CONFIG_TOUCHSCREEN_OMNIVISION_TCM_EAR_PHONE)
/* add_for_charger_mode */
static int ovt_tcm_switch_func_mode(unsigned char id, unsigned short value) 
{ 
	int retval;  
	struct ovt_tcm_hcd *tcm_hcd; 

	tcm_hcd = g_tcm_hcd; 

	mutex_lock(&tcm_hcd->extif_mutex); 

	retval = ovt_tcm_set_dynamic_config_host_interface(id, value); 
	if (retval < 0) { 
		LOGE(tcm_hcd->pdev->dev.parent, 
				"Failed to set dynamic config  #c_name\n"); 
		goto exit; 
	} 

exit: 
	mutex_unlock(&tcm_hcd->extif_mutex); 

	return retval; 
}
#endif

#if OMNIVISION_TCM_CHARGER_MODE_EN
static void ovt_tcm_charger_switch_work(struct work_struct *work)
{
	int ret = 0;
	struct ovt_tcm_hcd *tcm_hcd = g_tcm_hcd;

	pr_info("ovt_tcm tcm_hcd->charger_mode = %d",tcm_hcd->charger_mode);

	ret = ovt_tcm_switch_func_mode(DC_CHARGER_CONNECTED, tcm_hcd->charger_mode);
	if (ret >= 0) {
		pr_err("ovt_tcm switch charger mode successfully");
	}

	return;
}

static int ovt_tcm_charger_notifier_callback(struct notifier_block *nb, unsigned long val, void *v)
{
	int ret = 0;
	struct power_supply *psy = NULL;
	union power_supply_propval prop;
	struct ovt_tcm_hcd *tcm_hcd = g_tcm_hcd;
	bool isPsyBattery = false;

	if (tcm_hcd->psy_name && !strcmp(tcm_hcd->psy_name, "battery")) {
		psy = power_supply_get_by_name("battery");
		isPsyBattery = true;
	}
	else
		psy = power_supply_get_by_name("usb");

	if (!psy) {
		LOGE(tcm_hcd->pdev->dev.parent,"Couldn't get psy\n");
		return -EINVAL;
	}
	if (!strcmp(psy->desc->name, "usb") || !strcmp(psy->desc->name, "battery")) {
		if (psy && val == POWER_SUPPLY_PROP_STATUS) {
			if (isPsyBattery) {
				ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &prop);
			}
			else {
				ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &prop);
			}
			if (ret < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,"Couldn't get POWER_SUPPLY_PROP_STATUS rc=%d\n", ret);
				return ret;
			} else {
				if (tcm_hcd->charger_status != prop.intval) {
					pr_err("ovt_tcm usb prop.intval = %d;tcm_hcd->charger_status = %d;tcm_hcd->charger_mode = %d", prop.intval,tcm_hcd->charger_status,tcm_hcd->charger_mode);
					tcm_hcd->charger_status = prop.intval;

					switch (tcm_hcd->charger_status){
						case POWER_SUPPLY_STATUS_CHARGING:
					    case POWER_SUPPLY_STATUS_NOT_CHARGING:
						case POWER_SUPPLY_STATUS_FULL:
							tcm_hcd->charger_mode = 1;
							break;
						default:
							tcm_hcd->charger_mode = 0;
							break;
					};

					pr_debug("ovt_tcm tcm_hcd->charger_status = %d;tcm_hcd->charger_mode = %d",tcm_hcd->charger_status,tcm_hcd->charger_mode);	
					if (!tcm_hcd->in_suspend && (!atomic_read(&tcm_hcd->host_downloading))) {
			            pr_debug("ovt_tcm enter charger mode swith");
						queue_delayed_work(tcm_hcd->switch_charger_mode_workqueue,
								&tcm_hcd->switch_charger_mode_work,
								msecs_to_jiffies(500));
        			}
				}
			}
		}
	}
	return 0;
}

static int ovt_tcm_charger_notifier_callback_init(struct ovt_tcm_hcd *tcm_hcd)
{
	int ret = 0;

	tcm_hcd->notifier_charger.notifier_call = ovt_tcm_charger_notifier_callback;
	ret = power_supply_reg_notifier(&tcm_hcd->notifier_charger);
	if (ret < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,"power_supply_reg_notifier failed\n");
	}
    return ret;
}

static int ovt_tcm_charger_notifier_callback_exit(struct ovt_tcm_hcd *tcm_hcd)
{
    power_supply_unreg_notifier(&tcm_hcd->notifier_charger);
    LOGE(tcm_hcd->pdev->dev.parent,"Unregistering ts_data->notifier_charger.\n");
    return 0;
}
#endif
//drv-modify by shenwenbin for TP charger mode 20240304 end

//drv add by shenwenbin for headset detect 20240806 start
#ifdef CONFIG_TOUCHSCREEN_OMNIVISION_TCM_EAR_PHONE
void ovt_tcm_switch_ear_mode(unsigned short value)
{
	int ret = 0;

	if(ovt_tcm_probe_initialized != true){
		pr_info("ovt_tcm-%s.ovt_tcm_probe_initialized not successfully",__func__);
		return;
	}

	struct ovt_tcm_hcd *tcm_hcd = g_tcm_hcd;

	pr_info("ovt_tcm tcm_hcd->earphone_mode before :%d ;Now : %d",tcm_hcd->earphone_mode,value);
	tcm_hcd->earphone_mode = value;
	
	if (!tcm_hcd->in_suspend && (!atomic_read(&tcm_hcd->host_downloading))) {
	    switch (value) {
	    case 1:
	        pr_info("ovt_tcm: Headset Plugged In");
	        ret = ovt_tcm_switch_func_mode(DC_ENABLE_EAR_PHONE, tcm_hcd->earphone_mode);
			if (ret >= 0) {
				pr_info("ovt_tcm switch enter earphone mode successfully");
			}
	        break;
	    case 0:
	        pr_info("ovt_tcm: Headset Plugged Out");
	        ret = ovt_tcm_switch_func_mode(DC_ENABLE_EAR_PHONE, tcm_hcd->earphone_mode);
			if (ret >= 0) {
				pr_info("ovt_tcm switch out earphone mode successfully");
			}
	        break;
	    default:
	        break;
	    }
	}
    return;
}
EXPORT_SYMBOL(ovt_tcm_switch_ear_mode);
#endif
//drv add by shenwenbin for headset detect 20240806 end

//drv modify by kuangliangjun for glove mode function 20241010 start
#if OMNIVISION_TCM_GLOVE_MODE_EN
static ssize_t ovt_glove_mode_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;
	struct ovt_tcm_hcd *tcm_hcd = g_tcm_hcd;

    count = snprintf(buf, PAGE_SIZE, "Glove Mode:%s\n", tcm_hcd->glove_mode_enable ? "On" : "Off");

    return count;
}

static ssize_t ovt_glove_mode_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    //int ret = 0;
	//struct himax_ts_data *ts = private_ts;

	struct ovt_tcm_hcd *tcm_hcd = g_tcm_hcd;
    if (buf[0] == '1') {
		tcm_hcd->glove_mode_enable = 1;
		ovt_tcm_switch_func_mode(DC_ENABLE_GLOVE, 1);
        pr_info("enable glove mode");
    } else if (buf[0] == '0') {
		tcm_hcd->glove_mode_enable = 0;
		ovt_tcm_switch_func_mode(DC_ENABLE_GLOVE, 0);
        pr_info("disable glove mode");
    }

    return count;
}

static DEVICE_ATTR(state, S_IRUGO | S_IWUSR,
                   ovt_glove_mode_show, ovt_glove_mode_store);
#endif  
//drv modify by kuangliangjun for glove mode function 20241010 end

static int ovt_tcm_probe(struct platform_device *pdev)
{
	int retval;
	int idx;
	struct ovt_tcm_hcd *tcm_hcd;
	const struct ovt_tcm_board_data *bdata;
	const struct ovt_tcm_hw_interface *hw_if;
//drv modify by kuangliangjun for glove mode function 20241010 start
#if OMNIVISION_TCM_GLOVE_MODE_EN
	static struct kobject *sysfs_rootdir = NULL;
	struct kobject *drv_glove = NULL;
	int err = 0;
#endif
//drv modify by kuangliangjun for glove mode function 20241010 end

	LOGE(&pdev->dev,"ovt_tcm_probe\n");

	hw_if = pdev->dev.platform_data;
	if (!hw_if) {
		LOGE(&pdev->dev,
				"Hardware interface not found\n");
		return -ENODEV;
	}

	bdata = hw_if->bdata;
	if (!bdata) {
		LOGE(&pdev->dev,
				"Board data not found\n");
		return -ENODEV;
	}

	tcm_hcd = kzalloc(sizeof(*tcm_hcd), GFP_KERNEL);
	g_tcm_hcd = tcm_hcd;

	if (!tcm_hcd) {
		LOGE(&pdev->dev,
				"Failed to allocate memory for tcm_hcd\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, tcm_hcd);

	tcm_hcd->pdev = pdev;
	tcm_hcd->hw_if = hw_if;
	tcm_hcd->reset = ovt_tcm_reset;
	tcm_hcd->reset_n_reinit = ovt_tcm_reset_and_reinit;
	tcm_hcd->sleep = ovt_tcm_sleep;
	tcm_hcd->identify = ovt_tcm_identify;
	tcm_hcd->enable_irq = ovt_tcm_enable_irq;
	tcm_hcd->switch_mode = ovt_tcm_switch_mode;
	tcm_hcd->read_message = ovt_tcm_read_message;
	tcm_hcd->write_message = ovt_tcm_write_message;
	tcm_hcd->get_dynamic_config = ovt_tcm_get_dynamic_config;
	tcm_hcd->set_dynamic_config = ovt_tcm_set_dynamic_config;
	tcm_hcd->get_data_location = ovt_tcm_get_data_location;

	tcm_hcd->rd_chunk_size = RD_CHUNK_SIZE;
	tcm_hcd->wr_chunk_size = WR_CHUNK_SIZE;
	tcm_hcd->is_detected = false;
	tcm_hcd->wakeup_gesture_enabled = WAKEUP_GESTURE;

#ifdef PREDICTIVE_READING
	tcm_hcd->read_length = MIN_READ_LENGTH;
#else
	tcm_hcd->read_length = MESSAGE_HEADER_SIZE;
#endif

#ifdef WATCHDOG_SW
	tcm_hcd->watchdog.run = RUN_WATCHDOG;
	tcm_hcd->update_watchdog = ovt_tcm_update_watchdog;
#endif

	if (bdata->irq_gpio >= 0)
		tcm_hcd->irq = gpio_to_irq(bdata->irq_gpio);
	else
		tcm_hcd->irq = bdata->irq_gpio;

	mutex_init(&tcm_hcd->extif_mutex);
	mutex_init(&tcm_hcd->reset_mutex);
	mutex_init(&tcm_hcd->irq_en_mutex);
	mutex_init(&tcm_hcd->io_ctrl_mutex);
	mutex_init(&tcm_hcd->rw_ctrl_mutex);
	mutex_init(&tcm_hcd->command_mutex);
	mutex_init(&tcm_hcd->identify_mutex);
	mutex_init(&tcm_hcd->suspend_resume_mutex);

	mutex_lock(&tcm_hcd->suspend_resume_mutex);
	INIT_BUFFER(tcm_hcd->in, false);
	INIT_BUFFER(tcm_hcd->out, false);
	INIT_BUFFER(tcm_hcd->resp, true);
	INIT_BUFFER(tcm_hcd->temp, false);
	INIT_BUFFER(tcm_hcd->config, false);
	INIT_BUFFER(tcm_hcd->report.buffer, true);

	LOCK_BUFFER(tcm_hcd->in);

	retval = ovt_tcm_alloc_mem(tcm_hcd,
			&tcm_hcd->in,
			tcm_hcd->read_length + 1);
	if (retval < 0) {
		LOGE(&pdev->dev,
				"Failed to allocate memory for tcm_hcd->in.buf\n");
		UNLOCK_BUFFER(tcm_hcd->in);
		goto err_alloc_mem;
	}

	UNLOCK_BUFFER(tcm_hcd->in);

	atomic_set(&tcm_hcd->command_status, CMD_IDLE);

	atomic_set(&tcm_hcd->helper.task, HELP_NONE);

	tcm_hcd->helper.helper_completion = &helper_complete;
	complete(tcm_hcd->helper.helper_completion);

	device_init_wakeup(&pdev->dev, 1);

	init_waitqueue_head(&tcm_hcd->hdl_wq);

	init_waitqueue_head(&tcm_hcd->reflash_wq);
	atomic_set(&tcm_hcd->firmware_flashing, 0);

	if (!mod_pool.initialized) {
		mutex_init(&mod_pool.mutex);
		INIT_LIST_HEAD(&mod_pool.list);
		mod_pool.initialized = true;
	}
	retval = ovt_tcm_get_regulator(tcm_hcd, true);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get regulators\n");
		goto err_get_regulator;
	}

	retval = ovt_tcm_enable_regulator(tcm_hcd, true);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to enable regulators\n");
		goto err_enable_regulator;
	}

	retval = ovt_tcm_config_gpio(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to configure GPIO's\n");
		goto err_config_gpio;
	}
	/* detect the type of touch controller */
	retval = ovt_tcm_sensor_detection(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to detect the sensor\n");
		goto err_sysfs_create_dir;
	}

#ifdef BUILD_AS_KO_MODULE
	device_module_init();
	testing_module_init();
	zeroflash_module_init();
	diag_module_init();
	//reflash_module_init();
	recovery_module_init();
#endif

	sysfs_dir = kobject_create_and_add(PLATFORM_DRIVER_NAME,
			NULL); //&pdev->dev.kobj);  move to /sys
	if (!sysfs_dir) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs directory\n");
		retval = -EINVAL;
		goto err_sysfs_create_dir;
	}

	tcm_hcd->sysfs_dir = sysfs_dir;

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++) {
		//retval = sysfs_create_file(tcm_hcd->sysfs_dir,
		retval = sysfs_create_file(&tcm_hcd->pdev->dev.kobj,	//default path
				&(*attrs[idx]).attr);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to create sysfs file\n");
			goto err_sysfs_create_file;
		}
	}

#ifdef ZHANRUI_PLATFORM
/*create /sys/touchscreen */
	retval = sysfs_create_link(NULL, &tcm_hcd->pdev->dev.kobj,"touchscreen");
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to create /sys/touchscreen link\n");		
	}
#endif

	tcm_hcd->dynamnic_config_sysfs_dir =
			kobject_create_and_add(DYNAMIC_CONFIG_SYSFS_DIR_NAME,
			tcm_hcd->sysfs_dir);
	if (!tcm_hcd->dynamnic_config_sysfs_dir) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create dynamic config sysfs directory\n");
		retval = -EINVAL;
		goto err_sysfs_create_dynamic_config_dir;
	}

	for (idx = 0; idx < ARRAY_SIZE(dynamic_config_attrs); idx++) {
		//retval = sysfs_create_file(tcm_hcd->dynamnic_config_sysfs_dir,
		retval = sysfs_create_file(&tcm_hcd->pdev->dev.kobj,	//default path
				&(*dynamic_config_attrs[idx]).attr);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to create dynamic config sysfs file\n");
			goto err_sysfs_create_dynamic_config_file;
		}
	}

//drv modify by kuangliangjun for glove mode function 20241010 start
#if OMNIVISION_TCM_GLOVE_MODE_EN
	if (!sysfs_rootdir) {
		// this kobject is shared between modules, do not free it when error occur
		sysfs_rootdir = kobject_create_and_add("prize", kernel_kobj);
	}

	if (!drv_glove)
		drv_glove = kobject_create_and_add("smartcover", sysfs_rootdir);

	err = sysfs_create_link(drv_glove, &tcm_hcd->pdev->dev.kobj, "common_node");

	if (err) {
		pr_info("prize himax sysfs_create_link fail\n");
		return -ENOMEM;
	}
	if (sysfs_create_file(&tcm_hcd->pdev->dev.kobj, &dev_attr_state.attr)) {
		return -ENOMEM;
	}
#endif
//drv modify by kuangliangjun for glove mode function 20241010 end

//drv modify by kuangliangjun for double tap wakeup function 20241010 start
#if IS_ENABLED(CONFIG_PRIZE_COMMON_NODE)
	prize_common_node_register("GESTURE", &ovt_tcm_double_type_func);
#endif

//drv modify by kuangliangjun for double tap wakeup function 20241010 end
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
	tcm_hcd->fb_notifier.notifier_call = ovt_tcm_fb_notifier_cb;
	retval = mtk_disp_notifier_register("tcm_ts_notifier",&tcm_hcd->fb_notifier);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to register disp notifier client\n");
	}
#elif IS_ENABLED(CONFIG_FB)
	tcm_hcd->fb_notifier.notifier_call = ovt_tcm_fb_notifier_cb;
	retval = fb_register_client(&tcm_hcd->fb_notifier);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to register FB notifier client\n");
	}
#endif

//drv-modify by shenwenbin for TP charger mode 20240304 start
#if OMNIVISION_TCM_CHARGER_MODE_EN
	tcm_hcd->psy_name = "battery";
	pr_info("ovt_tcm_charger_mode_notifier_callback event psy->desc->name is %s",tcm_hcd->psy_name);
	tcm_hcd->charger_status = 2;
	tcm_hcd->charger_mode = 0;

	tcm_hcd->switch_charger_mode_workqueue =
			create_singlethread_workqueue("ovt_tcm_switch_charger");
	INIT_DELAYED_WORK(&tcm_hcd->switch_charger_mode_work, ovt_tcm_charger_switch_work);

    retval = ovt_tcm_charger_notifier_callback_init(tcm_hcd);
    if (retval<0) {
       LOGE(tcm_hcd->pdev->dev.parent,"init notifier callback fail");
    }
#endif
//drv-modify by shenwenbin for TP charger mode 20240304 end

#ifdef REPORT_NOTIFIER
	tcm_hcd->notifier_thread = kthread_run(ovt_tcm_report_notifier,
			tcm_hcd, "ovt_tcm_report_notifier");
	if (IS_ERR(tcm_hcd->notifier_thread)) {
		retval = PTR_ERR(tcm_hcd->notifier_thread);
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create and run tcm_hcd->notifier_thread\n");
		goto err_create_run_kthread;
	}
#endif

	tcm_hcd->helper.workqueue =
			create_singlethread_workqueue("ovt_tcm_helper");
	INIT_WORK(&tcm_hcd->helper.work, ovt_tcm_helper_work);
#ifdef SPEED_UP_RESUME
	tcm_hcd->speed_up_resume_workqueue = create_singlethread_workqueue("speedup_resume_wq");
	INIT_WORK(&tcm_hcd->speed_up_work, speedup_resume);
#endif
#ifdef WATCHDOG_SW
	tcm_hcd->watchdog.workqueue =
			create_singlethread_workqueue("ovt_tcm_watchdog");
	INIT_DELAYED_WORK(&tcm_hcd->watchdog.work, ovt_tcm_watchdog_work);
#endif

	tcm_hcd->polling_workqueue =
			create_singlethread_workqueue("ovt_tcm_polling");
	INIT_DELAYED_WORK(&tcm_hcd->polling_work, ovt_tcm_polling_work);


	/* skip the following initialization */
	/* since the fw is not ready for hdl devices */
	if (tcm_hcd->in_hdl_mode)
		goto prepare_modules;


	/* register and enable the interrupt in probe */
	/* if this is not the hdl device */
	retval = tcm_hcd->enable_irq(tcm_hcd, true, NULL);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to enable interrupt\n");
		goto err_enable_irq;
	}
	LOGD(tcm_hcd->pdev->dev.parent,
			"Interrupt is registered\n");

	/* ensure the app firmware is running */
	retval = ovt_tcm_identify(tcm_hcd, false);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Application firmware is not running\n");
		goto err_enable_irq;
	}
	/* initialize the touch reporting */
	retval = touch_init(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to initialze touch reporting\n");
		goto err_enable_irq;
	}

prepare_modules:
	/* prepare to add other modules */
	mod_pool.workqueue =
			create_singlethread_workqueue("ovt_tcm_module");
	INIT_WORK(&mod_pool.work, ovt_tcm_module_work);
	mod_pool.tcm_hcd = tcm_hcd;
	mod_pool.queue_work = true;
	queue_work(mod_pool.workqueue, &mod_pool.work);
	mutex_unlock(&tcm_hcd->suspend_resume_mutex);

#ifdef CONFIG_TOUCHSCREEN_OMNIVISION_TCM_EAR_PHONE
	ovt_tcm_probe_initialized = true;
#endif
	return 0;

err_enable_irq:
	cancel_delayed_work_sync(&tcm_hcd->polling_work);
	flush_workqueue(tcm_hcd->polling_workqueue);
	destroy_workqueue(tcm_hcd->polling_workqueue);

#ifdef WATCHDOG_SW
	cancel_delayed_work_sync(&tcm_hcd->watchdog.work);
	flush_workqueue(tcm_hcd->watchdog.workqueue);
	destroy_workqueue(tcm_hcd->watchdog.workqueue);
#endif

	cancel_work_sync(&tcm_hcd->helper.work);
	flush_workqueue(tcm_hcd->helper.workqueue);
	destroy_workqueue(tcm_hcd->helper.workqueue);

#ifdef REPORT_NOTIFIER
	kthread_stop(tcm_hcd->notifier_thread);

err_create_run_kthread:
#endif

//drv-modify by shenwenbin for TP charger mode 20240304 start
#if OMNIVISION_TCM_CHARGER_MODE_EN
	ovt_tcm_charger_notifier_callback_exit(tcm_hcd);
	cancel_delayed_work_sync(&tcm_hcd->switch_charger_mode_work);
	flush_workqueue(tcm_hcd->switch_charger_mode_workqueue);
	destroy_workqueue(tcm_hcd->switch_charger_mode_workqueue);
#endif
//drv-modify by shenwenbin for TP charger mode 20240304 end

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
    if (mtk_disp_notifier_unregister(&tcm_hcd->fb_notifier))
		LOGE(tcm_hcd->pdev->dev.parent,"Error occurred while unregistering disp_notifier\n");
#elif IS_ENABLED(CONFIG_FB)
    if (fb_unregister_client(&tcm_hcd->fb_notifier))
		LOGE(tcm_hcd->pdev->dev.parent,"Error occurred while unregistering fb notifier\n");
#endif


err_sysfs_create_dynamic_config_file:
	for (idx--; idx >= 0; idx--) {
		sysfs_remove_file(tcm_hcd->dynamnic_config_sysfs_dir,
				&(*dynamic_config_attrs[idx]).attr);
	}

	kobject_put(tcm_hcd->dynamnic_config_sysfs_dir);

	idx = ARRAY_SIZE(attrs);

err_sysfs_create_dynamic_config_dir:
err_sysfs_create_file:
	for (idx--; idx >= 0; idx--)
		sysfs_remove_file(tcm_hcd->sysfs_dir, &(*attrs[idx]).attr);

	kobject_put(tcm_hcd->sysfs_dir);

err_sysfs_create_dir:
	if (bdata->irq_gpio >= 0)
		ovt_tcm_set_gpio(tcm_hcd, bdata->irq_gpio, false, 0, 0);

	if (bdata->power_gpio >= 0)
		ovt_tcm_set_gpio(tcm_hcd, bdata->power_gpio, false, 0, 0);

	if (bdata->reset_gpio >= 0)
		ovt_tcm_set_gpio(tcm_hcd, bdata->reset_gpio, false, 0, 0);

err_config_gpio:
	ovt_tcm_enable_regulator(tcm_hcd, false);

err_enable_regulator:
	ovt_tcm_get_regulator(tcm_hcd, false);

err_get_regulator:
	device_init_wakeup(&pdev->dev, 0);

err_alloc_mem:
	RELEASE_BUFFER(tcm_hcd->report.buffer);
	RELEASE_BUFFER(tcm_hcd->config);
	RELEASE_BUFFER(tcm_hcd->temp);
	RELEASE_BUFFER(tcm_hcd->resp);
	RELEASE_BUFFER(tcm_hcd->out);
	RELEASE_BUFFER(tcm_hcd->in);
	mutex_unlock(&tcm_hcd->suspend_resume_mutex);

	kfree(tcm_hcd);
	return retval;
}

static int ovt_tcm_remove(struct platform_device *pdev)
{
	int idx;
	struct ovt_tcm_module_handler *mod_handler;
	struct ovt_tcm_module_handler *tmp_handler;
	struct ovt_tcm_hcd *tcm_hcd = platform_get_drvdata(pdev);
	const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	printk("ovt_tcm_remove enter\n");
    tcm_hcd->ovt_tcm_driver_removing = 1;

	cancel_work_sync(&tcm_hcd->helper.work);
	flush_workqueue(tcm_hcd->helper.workqueue);
	destroy_workqueue(tcm_hcd->helper.workqueue);


	mutex_lock(&mod_pool.mutex);

	if (!list_empty(&mod_pool.list)) {
		list_for_each_entry_safe(mod_handler,
				tmp_handler,
				&mod_pool.list,
				link) {
			if (mod_handler->mod_cb->remove)
				mod_handler->mod_cb->remove(tcm_hcd);
			list_del(&mod_handler->link);
			kfree(mod_handler);
		}
	}

	mod_pool.queue_work = false;
	cancel_work_sync(&mod_pool.work);
	flush_workqueue(mod_pool.workqueue);
	destroy_workqueue(mod_pool.workqueue);

	mutex_unlock(&mod_pool.mutex);

//drv-modify by shenwenbin for TP charger mode 20240304 start
#if OMNIVISION_TCM_CHARGER_MODE_EN
	ovt_tcm_charger_notifier_callback_exit(tcm_hcd);
	cancel_delayed_work_sync(&tcm_hcd->switch_charger_mode_work);
	flush_workqueue(tcm_hcd->switch_charger_mode_workqueue);
	destroy_workqueue(tcm_hcd->switch_charger_mode_workqueue);
#endif
//drv-modify by shenwenbin for TP charger mode 20240304 end

	touch_remove(tcm_hcd);

	if (tcm_hcd->irq_enabled && bdata->irq_gpio >= 0) {
		disable_irq(tcm_hcd->irq);
		free_irq(tcm_hcd->irq, tcm_hcd);
	}

	cancel_delayed_work_sync(&tcm_hcd->polling_work);
	flush_workqueue(tcm_hcd->polling_workqueue);
	destroy_workqueue(tcm_hcd->polling_workqueue);

#ifdef WATCHDOG_SW
	cancel_delayed_work_sync(&tcm_hcd->watchdog.work);
	flush_workqueue(tcm_hcd->watchdog.workqueue);
	destroy_workqueue(tcm_hcd->watchdog.workqueue);
#endif

#ifdef REPORT_NOTIFIER
	kthread_stop(tcm_hcd->notifier_thread);
#endif

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
    if (mtk_disp_notifier_unregister(&tcm_hcd->fb_notifier))
		LOGE(tcm_hcd->pdev->dev.parent,"Error occurred while unregistering disp_notifier\n");
#elif IS_ENABLED(CONFIG_FB)
    if (fb_unregister_client(&tcm_hcd->fb_notifier))
		LOGE(tcm_hcd->pdev->dev.parent,"Error occurred while unregistering fb notifier\n");
#endif


	for (idx = 0; idx < ARRAY_SIZE(dynamic_config_attrs); idx++) {
		sysfs_remove_file(tcm_hcd->dynamnic_config_sysfs_dir,
				&(*dynamic_config_attrs[idx]).attr);
	}

	kobject_put(tcm_hcd->dynamnic_config_sysfs_dir);

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++)
		sysfs_remove_file(tcm_hcd->sysfs_dir, &(*attrs[idx]).attr);

	kobject_put(tcm_hcd->sysfs_dir);

	if (bdata->irq_gpio >= 0)
		ovt_tcm_set_gpio(tcm_hcd, bdata->irq_gpio, false, 0, 0);

	if (bdata->power_gpio >= 0)
		ovt_tcm_set_gpio(tcm_hcd, bdata->power_gpio, false, 0, 0);

	if (bdata->reset_gpio >= 0)
		ovt_tcm_set_gpio(tcm_hcd, bdata->reset_gpio, false, 0, 0);

	ovt_tcm_enable_regulator(tcm_hcd, false);

	ovt_tcm_get_regulator(tcm_hcd, false);

	device_init_wakeup(&pdev->dev, 0);

	RELEASE_BUFFER(tcm_hcd->report.buffer);
	RELEASE_BUFFER(tcm_hcd->config);
	RELEASE_BUFFER(tcm_hcd->temp);
	RELEASE_BUFFER(tcm_hcd->resp);
	RELEASE_BUFFER(tcm_hcd->out);
	RELEASE_BUFFER(tcm_hcd->in);

	kfree(tcm_hcd);

	return 0;
}

static void ovt_tcm_shutdown(struct platform_device *pdev)
{
	int retval;

	retval = ovt_tcm_remove(pdev);
}

#ifdef CONFIG_PM
static const struct dev_pm_ops ovt_tcm_dev_pm_ops = {
#if !defined(CONFIG_DRM) && !defined(CONFIG_FB)
	.suspend = ovt_tcm_suspend,
	.resume = ovt_tcm_resume,
#endif
};
#endif

static struct platform_driver ovt_tcm_driver = {
	.driver = {
		.name = PLATFORM_DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &ovt_tcm_dev_pm_ops,
#endif
	},
	.probe = ovt_tcm_probe,
	.remove = ovt_tcm_remove,
	.shutdown = ovt_tcm_shutdown,
};

static int __init ovt_tcm_module_init(void)
{
	int retval;

	retval = ovt_tcm_bus_init();
	if (retval < 0)
		return retval;

	return platform_driver_register(&ovt_tcm_driver);
}

static void __exit ovt_tcm_module_exit(void)
{
	platform_driver_unregister(&ovt_tcm_driver);

	ovt_tcm_bus_exit();

	return;
}

late_initcall(ovt_tcm_module_init);
module_exit(ovt_tcm_module_exit);

MODULE_AUTHOR("Omnivision, Inc.");
MODULE_DESCRIPTION("Omnivision TCM Touch Driver");
MODULE_LICENSE("GPL v2");
