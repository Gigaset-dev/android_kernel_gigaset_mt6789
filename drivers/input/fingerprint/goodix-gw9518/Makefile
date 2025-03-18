#
# Makefile for the fingerprint device.
#

# Each configuration option enables a list of files.
#include $(srctree)/drivers/misc/mediatek/Makefile.custom

# for microtrust tee start
ifneq ($(filter y m, $(CONFIG_MICROTRUST_TEE_SUPPORT)),)
ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)

include $(srctree)/drivers/tee/teei/Makefile.include
endif
# for microtrust tee end

# for trustkernel tee start
ifeq ($(filter y m, $(CONFIG_TRUSTKERNEL_TEE_SUPPORT)),)
ccflags-y += -DCONFIG_FINGERPRINT_FOCALTECH_SPI_SUPPORT
ccflags-y += -DCONFIG_FINGERPRINT_FOCALTECH_SPICLK_SUPPORT
ccflags-y += -I$(srctree)/drivers/misc/mediatek/tkcore/include/
endif
# for trustkernel tee end

# for trustonic tee start
ifneq ($(filter y m, $(CONFIG_TRUSTONIC_TEE_SUPPORT)),)

endif
# for trustonic tee end

obj-m	+= gf_tee.o
#gf_tee-y := gf_spi_tee.o gf_spi_factory.o
gf_tee-y := gf_spi_tee.o
