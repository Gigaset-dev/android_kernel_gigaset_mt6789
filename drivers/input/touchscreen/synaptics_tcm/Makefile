#
# Makefile for the Goodix GT9886 touchscreen driver.
#

#For display ESD recovery
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-m += synaptics_tcm.o
synaptics_tcm-y += synaptics_tcm_core.o
synaptics_tcm-y += synaptics_tcm_device.o
synaptics_tcm-y += synaptics_tcm_recovery.o
synaptics_tcm-y += synaptics_tcm_reflash.o
synaptics_tcm-y += synaptics_tcm_spi.o
synaptics_tcm-y += synaptics_tcm_touch.o
synaptics_tcm-y += synaptics_tcm_zeroflash.o
synaptics_tcm-y += synaptics_tcm_diagnostics.o
synaptics_tcm-y += synaptics_tcm_testing.o