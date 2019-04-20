/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <inttypes.h>

#include <rte_common.h>
#include <rte_ethdev_driver.h>

#include "../base/ixgbe_common.h"
#include "ixgbe_cleanq.h"

int ixgbe_logtype_cleanq;
RTE_INIT(ixgbe_cleanq_log)
{
	ixgbe_logtype_cleanq = rte_log_register("pmd.net.ixgbe.cleanq");
	if (ixgbe_logtype_cleanq >= 0)
		rte_log_set_level(ixgbe_logtype_init, RTE_LOG_NOTICE);
}
