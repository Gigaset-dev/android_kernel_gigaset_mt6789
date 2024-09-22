#
# Makefile for the Synaptics TCM touchscreen driver.
#

# Each configuration option enables a list of files.

NAME=

TCM_CORE=tcm/
TCM_DRIVER_SRC=drivers/input/touchscreen/synaptics_tcm2/

synaptics_tcm2$(NAME)-objs := syna_tcm2.o

synaptics_tcm2$(NAME)-objs += \
					$(TCM_CORE)synaptics_touchcom_core_v1.o \
					$(TCM_CORE)synaptics_touchcom_core_v2.o \
 					$(TCM_CORE)synaptics_touchcom_func_base.o \
 					$(TCM_CORE)synaptics_touchcom_func_touch.o

	synaptics_tcm2$(NAME)-objs += $(TCM_CORE)synaptics_touchcom_func_reflash.o
	synaptics_tcm2$(NAME)-objs += $(TCM_CORE)synaptics_touchcom_func_romboot.o

	synaptics_tcm2$(NAME)-objs += syna_tcm2_platform_spi.o
	synaptics_tcm2$(NAME)-objs += syna_tcm2_sysfs.o

	synaptics_tcm2$(NAME)-objs += syna_tcm2_testing.o


obj-m += synaptics_tcm2$(NAME).o
ccflags-y += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(srctree)/$(TCM_DRIVER_SRC)
ccflags-y += -I$(srctree)/$(TCM_DRIVER_SRC)$(TCM_CORE)
