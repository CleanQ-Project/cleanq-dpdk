/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <inttypes.h>

#include <rte_ethdev_driver.h>

#include "../base/ixgbe_common.h"
#include "ixgbe_cleanq.h"

bool ixgbe_cleanq_create(struct ixgbe_cleanq **q)
{
    *q = NULL;
    return false;
}
bool ixgbe_cleanq_dequeue(struct ixgbe_cleanq *q, struct rte_mbuf **mbuf)
{
    *mbuf = NULL;
    return false;
}
