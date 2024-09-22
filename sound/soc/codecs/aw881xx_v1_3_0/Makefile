#obj-$(CONFIG_SND_SMARTPA_AW881XX_V1_3_0) += aw881xx.o aw881xx_monitor.o aw881xx_cali.o

obj-$(CONFIG_SND_SMARTPA_AW881XX_V1_3_0) += snd-soc-aw881xx.o
snd-soc-aw881xx-y	+= aw881xx.o
snd-soc-aw881xx-y	+= aw881xx_monitor.o
snd-soc-aw881xx-y	+= aw881xx_cali.o

subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/audio_dsp
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/base