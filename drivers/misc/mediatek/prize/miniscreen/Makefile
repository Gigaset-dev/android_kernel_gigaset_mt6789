
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pwm/pwm_v1/include/

obj-m += miniscreen.o

LCD_DRV := $(subst ",,$(CONFIG_DRV_MINISCREEN_LCD_DRV))
miniscreen-y += lcd_drv/$(LCD_DRV).o
miniscreen-y += spidev.o
