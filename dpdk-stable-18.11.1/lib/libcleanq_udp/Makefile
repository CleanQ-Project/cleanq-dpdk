# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = libcleanq_udp.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)/include -O3

LDLIBS += -libcleanq

LIBABIVER := 5

VPATH += $(SRCDIR)/include
VPATH += $(SRCDIR)/src

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBCLEANQ) := cleanq_module_ip.c
SRCS-$(CONFIG_RTE_LIBCLEANQ) += cleanq_module_udp.c
SRCS-$(CONFIG_RTE_LIBCLEANQ) += inet_chksum.c


# install this header file
SYMLINK-$(CONFIG_RTE_LIBCLEANQ)-include := cleanq_ip.h
SYMLINK-$(CONFIG_RTE_LIBCLEANQ)-include += cleanq_udp.h
SYMLINK-$(CONFIG_RTE_LIBCLEANQ)-include += cleanq_pkt_headers.h

include $(RTE_SDK)/mk/rte.lib.mk
