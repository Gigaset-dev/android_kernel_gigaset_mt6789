# Makefile for the focaltech touchscreen drivers.


ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/misc/mediatek/prize/prize_common_node
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-m += ft8725_tp.o
ft8725_tp-y := ft8725_core.o \
		ft8725_ex_fun.o \
		ft8725_ex_mode.o \
		ft8725_gesture.o \
		ft8725_esdcheck.o \
		ft8725_point_report_check.o \
		ft8725_proximity.o


ft8725_tp-y += ft8725_flash_spi.o


ft8725_tp-y += ft8725_spi_v2.o


ft8725_tp-y += ft8725_debug.o

ft8725_tp-y += ft8725_hp.o


