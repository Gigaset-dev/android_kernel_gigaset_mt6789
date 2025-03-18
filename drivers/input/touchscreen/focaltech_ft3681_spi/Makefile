# Makefile for the focaltech touchscreen drivers.


ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/misc/mediatek/prize/prize_common_node/

obj-m += focaltech_ft3681_spi.o
focaltech_ft3681_spi-y := focaltech_core.o \
		focaltech_ex_fun.o \
		focaltech_ex_mode.o \
		focaltech_gesture.o \
		focaltech_esdcheck.o \
		focaltech_point_report_check.o



focaltech_ft3681_spi-y += focaltech_spi_v2.o


focaltech_ft3681_spi-y += focaltech_proximity.o



focaltech_ft3681_spi-y += focaltech_flash.o
focaltech_ft3681_spi-y += focaltech_flash/focaltech_upgrade_ft3681.o
focaltech_ft3681_spi-y += focaltech_test/focaltech_test.o
focaltech_ft3681_spi-y += focaltech_test/focaltech_test_ini.o
focaltech_ft3681_spi-y += focaltech_test/supported_ic/focaltech_test_ft3681.o
