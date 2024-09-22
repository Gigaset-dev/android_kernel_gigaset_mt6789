#
# Makefile for TMS NFC device
#

obj-$(CONFIG_TMS_NFC_DEVICE) += tms_nfc_module.o
ifeq ($(CONFIG_TMS_GUIDE_DEVICE), y)
tms_nfc_module-objs          := ../nfc_common.o \
                                ../../guidev/guide_driver.o \
                                nfc_driver.o
else ifeq ($(CONFIG_TMS_GUIDE_DEVICE), m)
tms_nfc_module-objs          := ../nfc_common.o \
                                ../../guidev/guide_driver.o \
                                nfc_driver.o
else
tms_nfc_module-objs          := ../nfc_common.o \
                                nfc_driver.o
endif
