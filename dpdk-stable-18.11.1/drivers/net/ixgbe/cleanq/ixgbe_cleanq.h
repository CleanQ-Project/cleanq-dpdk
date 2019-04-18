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

#define IXGBE_USE_CLEANQ

/**
 * Structure associated with each RX queue.
 */
struct ixgbe_cleanq {
	volatile union ixgbe_adv_rx_desc *rx_ring; /**< RX ring virtual address. */
	uint64_t            rx_ring_phys_addr; /**< RX ring DMA address. */
	volatile uint32_t   *rdt_reg_addr; /**< RDT register address. */
	volatile uint32_t   *rdh_reg_addr; /**< RDH register address. */
	struct rte_mbuf     *sw_ring; /**< address of RX software ring. */
	uint16_t            nb_rx_desc; /**< number of RX descriptors. */
	uint16_t            rx_tail;  /**< current value of RDT register. */
};

bool ixgbe_cleanq_create(struct ixgbe_cleanq **q);
bool ixgbe_cleanq_dequeue(struct ixgbe_cleanq *q, struct rte_mbuf **mbuf);


#endif /* _IXGBE_CLEANQ_H_ */