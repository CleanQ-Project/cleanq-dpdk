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

extern int ixgbe_logtype_cleanq;
#define PMD_CLEANQ_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, ixgbe_logtype_cleanq, \
		"%s(): " fmt "\n", __func__, ##args)

#define PMD_CLEANQ_LOG_TX_STATUS(level, q) \
	PMD_CLEANQ_LOG(level, "TX: Recl=%"PRIu16", Tail=%"PRIu16"", q->tx_recl, q->tx_tail)

#define PMD_CLEANQ_LOG_RX_STATUS(level, q) \
	PMD_CLEANQ_LOG(level, "RX: Recl=%"PRIu16", Tail=%"PRIu16"", q->rx_recl, q->rx_tail)

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
	uint16_t			rx_recl;  /**< Latest reclaimed buffer */
};

struct ixgbe_tx_queue;
struct ixgbe_rx_queue;

bool ixgbe_tx_cleanq_enqueue(struct ixgbe_tx_queue *txq, struct rte_mbuf *mb);
bool ixgbe_tx_cleanq_dequeue(struct ixgbe_tx_queue *txq, struct rte_mbuf **ret_mb);

void ixgbe_rx_cleanq_enqueue(struct ixgbe_rx_queue *rxq, struct rte_mbuf *mb);
bool ixgbe_rx_cleanq_dequeue(struct ixgbe_rx_queue *rxq, struct rte_mbuf **ret_mb);

#endif /* _IXGBE_CLEANQ_H_ */