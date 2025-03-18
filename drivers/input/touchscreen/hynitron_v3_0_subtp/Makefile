
# Linux driver folder

ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-m += hynitron_v3_0_subtp.o
hynitron_v3_0_subtp-y	+= hynitron_core.o
hynitron_v3_0_subtp-y	+= hynitron_i2c.o
hynitron_v3_0_subtp-y	+= hynitron_esd_check.o
hynitron_v3_0_subtp-y	+= hynitron_tool_debug.o
hynitron_v3_0_subtp-y	+= hynitron_update_firmware.o
hynitron_v3_0_subtp-y	+= hynitron_gesture.o
hynitron_v3_0_subtp-y	+= hynitron_proximity.o
