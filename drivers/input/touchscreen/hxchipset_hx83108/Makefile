# Makefile for the Himax touchscreen drivers.

ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/misc/mediatek/prize/prize_common_node/

ifneq ($(filter y, $(CONFIG_KALLSYMS_ALL)),)
	ccflags-y += -D__KERNEL_KALLSYMS_ALL_ENABLED__
endif

ifneq ($(filter y, $(CONFIG_FB)),)
	ccflags-y += -DHX_CONFIG_FB
endif

  ccflags-y += -DHX_CONFIG_DRM

ifneq ($(filter y, $(CONFIG_PM)),)
	ccflags-y += -DHX_CONFIG_PM
endif

# obj-y += himax_modular_table.o
  hxchipset_hx83108-y += himax_debug.o
  hxchipset_hx83108-y += himax_inspection.o
  ccflags-y += -D__HIMAX_HX83108_MOD__
  hxchipset_hx83108-y += himax_ic_HX83108.o

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102J)),)
  ccflags-y += -D__HIMAX_HX83102J_MOD__
	hxchipset-y += himax_ic_HX83102j.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83112F)),)
  ccflags-y += -D__HIMAX_HX83112F_MOD__
	hxchipset-y += himax_ic_HX83112f.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83121A)),)
  ccflags-y += -D__HIMAX_HX83121A_MOD__
	hxchipset-y += himax_ic_HX83121a.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83122A)),)
  ccflags-y += -D__HIMAX_HX83122A_MOD__
	hxchipset-y += himax_ic_HX83122a.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX85200A)),)
  ccflags-y += -D__HIMAX_HX85200A_MOD__
	hxchipset-y += himax_ic_HX85200a.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83132A)),)
  ccflags-y += -D__HIMAX_HX83132A_MOD__
	hxchipset-y += himax_ic_HX83132a.o
endif


  ccflags-y += -D__HIMAX_MOD__
  hxchipset_hx83108-y += himax_ic_core.o
  hxchipset_hx83108-y += himax_common.o
  hxchipset_hx83108-y += himax_platform.o
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_EMBEDDED_FIRMWARE)),)
    ccflags-y += -DHX_FIRMWARE_HEADER
    hxchipset-y += himax_firmware.o
  endif
  obj-m += hxchipset_hx83108.o

ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
  himax_mmi-objs += himax_ic_core.o
  himax_mmi-objs += himax_common.o
  himax_mmi-objs += himax_platform.o
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_EMBEDDED_FIRMWARE)),)
	ccflags-y += -DHX_FIRMWARE_HEADER
	himax_mmi-objs += himax_firmware.o
  endif
  obj-y += himax_mmi.o
endif

