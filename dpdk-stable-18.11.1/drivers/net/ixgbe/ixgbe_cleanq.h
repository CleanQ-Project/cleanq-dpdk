/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _IXGBE_CLEANQ_H_
#define _IXGBE_CLEANQ_H_

#include <cleanq_module.h>

extern int ixgbe_logtype_cleanq_tx;
#define PMD_CLEANQ_LOG_TX(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, ixgbe_logtype_cleanq_tx, \
		"%s(): TX: " fmt "\n", __func__, ##args)

#define PMD_CLEANQ_LOG_TX_STATUS(level, q) \
	PMD_CLEANQ_LOG_TX(level, "Recl=%"PRIu16", Tail=%"PRIu16"", (q)->tx_recl, (q)->tx_tail)

extern int ixgbe_logtype_cleanq_rx;
#define PMD_CLEANQ_LOG_RX(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, ixgbe_logtype_cleanq_rx, \
		"%s(): RX: " fmt "\n", __func__, ##args)

#define PMD_CLEANQ_LOG_RX_STATUS(level, q) \
	PMD_CLEANQ_LOG_RX(level, "Recl=%"PRIu16", Tail=%"PRIu16"", (q)->rx_recl, (q)->rx_tail)

#define PMD_CLEANQ_LOG_CQBUF(rx_tx, level, cqbuf) \
    PMD_CLEANQ_LOG_ ## rx_tx(level, "region=%"PRIu32", " \
        "offset=%"PRIu64", " \
        "length=%"PRIu64", " \
        "valid_offset=%"PRIu64", " \
        "valid_length=%"PRIu64, \
        (cqbuf).rid, \
        (cqbuf).offset, \
        (cqbuf).length, \
        (cqbuf).valid_data, \
        (cqbuf).valid_length)

struct ixgbe_tx_queue;
struct ixgbe_rx_queue;

errval_t ixgbe_cleanq_register(
	struct cleanq *q,
    struct capref cap,
    regionid_t region_id);

errval_t ixgbe_cleanq_deregister(
	struct cleanq *q,
    regionid_t region_id);

errval_t ixgbe_tx_cleanq_create(struct ixgbe_tx_queue *txq);

errval_t ixgbe_tx_cleanq_enqueue(
	struct cleanq *q,
    regionid_t region_id,
    genoffset_t offset,
    genoffset_t length,
    genoffset_t valid_offset,
    genoffset_t valid_length,
    uint64_t misc_flags);

errval_t ixgbe_tx_cleanq_dequeue(
	struct cleanq *q,
    regionid_t* region_id,
    genoffset_t* offset,
    genoffset_t* length,
    genoffset_t* valid_offset,
    genoffset_t* valid_length,
    uint64_t* misc_flags);

errval_t ixgbe_rx_cleanq_create(struct ixgbe_rx_queue *rxq);

errval_t ixgbe_rx_cleanq_enqueue(
    struct cleanq *q,
    regionid_t region_id,
    genoffset_t offset,
    genoffset_t length,
    genoffset_t valid_offset,
    genoffset_t valid_length,
    uint64_t misc_flags);

errval_t ixgbe_rx_cleanq_dequeue(
    struct cleanq *q,
    regionid_t* region_id,
    genoffset_t* offset,
    genoffset_t* length,
    genoffset_t* valid_offset,
    genoffset_t* valid_length,
    uint64_t* misc_flags);

#endif /* _IXGBE_CLEANQ_H_ */