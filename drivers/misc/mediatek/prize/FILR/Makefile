ccflags-y += -I$(srctree)/drivers/misc/mediatek/leds/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/imgsensor/inc/

obj-m += flir_lepton_drv.o
flir_lepton_drv-y	+= flir_lepton.o
flir_lepton_drv-y	+= lepton_vospi_funcs.o
#flir_lepton_drv-y       += spidev.o

ccflags-y += $(addprefix -DPRIZE_MB_, $(shell echo $(CONFIG_PRIZE_BOARD) | tr a-z A-Z))