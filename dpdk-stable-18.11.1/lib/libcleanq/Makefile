# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = libcleanq.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

LDLIBS += -lcleanq

LIBABIVER := 5

VPATH += $(SRCDIR)/include
VPATH += $(SRCDIR)/src

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBCLEANQL) += queue_interface.c
SRCS-$(CONFIG_RTE_LIBCLEANQ) += queue_interface_internal.c
SRCS-$(CONFIG_RTE_LIBCLEANQ) += region_pool.c
SRCS-$(CONFIG_RTE_LIBCLEANQ) += region.c
SRCS-$(CONFIG_RTE_LIBCLEANQ) += slab.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBCLEANQ)-include := bench.h
SYMLINK-$(CONFIG_RTE_LIBCLEANQ)-include += cleanq_module.h
SYMLINK-$(CONFIG_RTE_LIBCLEANQ)-include += cleanq.h
SYMLINK-$(CONFIG_RTE_LIBCLEANQ)-include += slab.h

include $(RTE_SDK)/mk/rte.lib.mk
