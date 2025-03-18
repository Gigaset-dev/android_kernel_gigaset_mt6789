$(info [fm_drv:Android.mk] LINUX_KERNEL_VERSION = $(LINUX_KERNEL_VERSION))

ifneq ($(LINUX_KERNEL_VERSION),kernel-6.6)
LOCAL_PATH := $(call my-dir)
MAIN_PATH := $(LOCAL_PATH)

$(info [fm_drv:Android.mk] MTK_FM_SUPPORT = $(MTK_FM_SUPPORT))
$(info [fm_drv:Android.mk] FM_CHIP_ID = $(FM_CHIP_ID))

ifeq ($(strip $(MTK_FM_SUPPORT)), yes)
    # LD 1.0 should have FM_CHIP_ID
    # FM_CHIP/FM_PLAT is assigned by Android.mk
    ifneq ($(FM_CHIP_ID),)
        LEGACY_BUILD := yes
        include $(MAIN_PATH)/Include.mk
    else
        # mt6631 connac 1.x
        BUILD_CONNAC2 := false
        FM_CHIP := mt6631
        FM_PLAT := mt6631
        LEGACY_BUILD := yes
        include $(MAIN_PATH)/Include.mk

        # mt6635 connac 1.x
        BUILD_CONNAC2 := false
        FM_CHIP := mt6635
        FM_PLAT := mt6635
        LEGACY_BUILD := yes
        include $(MAIN_PATH)/Include.mk

        # dynamic mt6631/mt6635 connac 1.x
        BUILD_CONNAC2 := false
        FM_CHIP :=
        FM_PLAT := mt6631_6635
        LEGACY_BUILD := yes
        include $(MAIN_PATH)/Include.mk

        # mt6635 connac 2.x
        BUILD_CONNAC2 := true
        FM_CHIP := mt6635
        FM_PLAT := connac2x
        LEGACY_BUILD := yes
        include $(MAIN_PATH)/Include.mk
    endif
endif
endif
