#
# Makefile for the Omnivision TCM touchscreen driver.
#

# Each configuration option enables a list of files.
KBUILD_CFLAGS += -Wno-unused-function

ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
obj-m += omnivision_tcm.o
omnivision_tcm-objs := omnivision_tcm_core.o omnivision_tcm_touch.o omnivision_tcm_spi.o \
omnivision_tcm_zeroflash.o omnivision_tcm_device.o omnivision_tcm_testing.o \
omnivision_tcm_recovery.o omnivision_tcm_diagnostics.o