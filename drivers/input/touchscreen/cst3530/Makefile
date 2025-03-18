
ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-m += cst3530.o

cst3530-y += hyn_core.o hyn_lib/hyn_i2c.o hyn_lib/hyn_spi.o hyn_lib/hyn_ts_ext.o hyn_lib/hyn_fs_node.o hyn_lib/hyn_prox.o
cst3530-y += hyn_lib/hyn_tool.o
cst3530-y += hyn_lib/hyn_gesture.o
cst3530-y += hyn_chips/hyn_cst66xx.o
cst3530-y += hyn_chips/hyn_cst3240.o
cst3530-y += hyn_chips/hyn_cst92xx.o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement

