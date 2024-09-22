#
# Makefile for the Novatek NT36xxx touchscreen driver.
#

# Each configuration option enables a list of files.
ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
obj-m += nvt_touch_i2c.o
nvt_touch_i2c-y += nt36xxx.o
nvt_touch_i2c-y += nt36xxx_fw_update_gcm.o
nvt_touch_i2c-y += nt36xxx_ext_proc.o
nvt_touch_i2c-y += nt36xxx_mp_ctrlram.o
