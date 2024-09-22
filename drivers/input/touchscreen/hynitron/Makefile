
# Linux driver folder

ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-m += hynitron.o
hynitron-y	+= hynitron_core.o
hynitron-y	+= hynitron_i2c.o
hynitron-y	+= hynitron_esd_check.o
hynitron-y	+= hynitron_tool_debug.o
hynitron-y	+= hynitron_update_firmware.o
hynitron-y	+= hynitron_gesture.o
hynitron-y	+= hynitron_proximity.o
