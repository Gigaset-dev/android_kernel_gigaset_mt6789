###
# Copyright (C) Fourier Semiconductor Inc. 2016-2021. All rights reserved.
#

# TOPLEVEL=$(PWD)/..
# EXTRA_CFLAGS += -I$(TOPLEVEL)

# EXTRA_CFLAGS += -DDEBUG -DFSM_DEBUG
EXTRA_CFLAGS += -Wall -Werror
MODFLAGS = -fno-pic
CFLAGS_MODULE = $(MODFLAGS)
AFLAGS_MODULE = $(MODFLAGS)

fsa4480-objs += fsa4480-i2c.o
obj-$(CONFIG_SND_SOC_FSA4480_SWITCH) += fsa4480.o
