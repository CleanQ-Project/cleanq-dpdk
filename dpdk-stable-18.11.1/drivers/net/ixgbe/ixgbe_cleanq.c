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
#include <rte_mbuf.h>
#include <rte_ethdev_driver.h>

#include <cleanq.h>

#include "ixgbe_ethdev.h"
#include "base/ixgbe_common.h"

#include "ixgbe_cleanq.h"
#include "ixgbe_rxtx.h"

int ixgbe_logtype_cleanq;
RTE_INIT(ixgbe_cleanq_log)
{
	ixgbe_logtype_cleanq = rte_log_register("pmd.net.ixgbe.cleanq");
	if (ixgbe_logtype_cleanq >= 0)
		rte_log_set_level(ixgbe_logtype_init, RTE_LOG_NOTICE);
}

bool ixgbe_tx_cleanq_dequeue(struct ixgbe_tx_queue *txq, struct rte_mbuf **ret_mb) {
	struct ixgbe_tx_entry *txep;
	struct rte_mbuf *mb;
	uint32_t status;

	if (unlikely(txq->tx_recl == txq->tx_tail)) {
		PMD_CLEANQ_LOG(NOTICE, "TX: Descriptor ring full (%"PRIx32")", txq->tx_recl);
		return false;
	}

	/* check DD bit on threshold descriptor */
	status = rte_le_to_cpu_32(txq->tx_ring[txq->tx_next_dd].wb.status);
	if (!(status & IXGBE_ADVTXD_STAT_DD)) {
		PMD_CLEANQ_LOG(DEBUG, "TX: No buffer to dequeue (%"PRIx32")", status);
		return false;
	}

	/*
	 * first buffer to free from S/W ring is at index
	 * tx_next_dd - (tx_rs_thresh-1)
	 */
	txep = &txq->sw_ring[txq->tx_recl];

	mb = txep->mbuf;
	txep->mbuf = NULL;

	PMD_CLEANQ_LOG(INFO, "TX: Dequeued buffer %"PRIu16, txq->tx_recl);

	txq->tx_recl = (uint16_t)(txq->tx_recl + 1);
	if (txq->tx_recl > txq->tx_next_dd) {
		txq->tx_next_dd = (uint16_t)(txq->tx_next_dd + txq->tx_rs_thresh);
		if (txq->tx_next_dd >= txq->nb_tx_desc) {
			txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);
		}
	}
	if (txq->tx_recl >= txq->nb_tx_desc) {
		txq->tx_recl = 0;
	}

	*ret_mb = mb;
	return true;
}

static inline uint32_t
ixgbe_rxd_pkt_info_to_pkt_type(uint32_t pkt_info, uint16_t ptype_mask)
{

	if (unlikely(pkt_info & IXGBE_RXDADV_PKTTYPE_ETQF))
		return RTE_PTYPE_UNKNOWN;

	pkt_info = (pkt_info >> IXGBE_PACKET_TYPE_SHIFT) & ptype_mask;

	/* For tunnel packet */
	if (pkt_info & IXGBE_PACKET_TYPE_TUNNEL_BIT) {
		/* Remove the tunnel bit to save the space. */
		pkt_info &= IXGBE_PACKET_TYPE_MASK_TUNNEL;
		return ptype_table_tn[pkt_info];
	}

	/**
	 * For x550, if it's not tunnel,
	 * tunnel type bit should be set to 0.
	 * Reuse 82599's mask.
	 */
	pkt_info &= IXGBE_PACKET_TYPE_MASK_82599;

	return ptype_table[pkt_info];
}

static inline uint64_t
ixgbe_rxd_pkt_info_to_pkt_flags(uint16_t pkt_info)
{
	static uint64_t ip_rss_types_map[16] __rte_cache_aligned = {
		0, PKT_RX_RSS_HASH, PKT_RX_RSS_HASH, PKT_RX_RSS_HASH,
		0, PKT_RX_RSS_HASH, 0, PKT_RX_RSS_HASH,
		PKT_RX_RSS_HASH, 0, 0, 0,
		0, 0, 0,  PKT_RX_FDIR,
	};
#ifdef RTE_LIBRTE_IEEE1588
	static uint64_t ip_pkt_etqf_map[8] = {
		0, 0, 0, PKT_RX_IEEE1588_PTP,
		0, 0, 0, 0,
	};

	if (likely(pkt_info & IXGBE_RXDADV_PKTTYPE_ETQF))
		return ip_pkt_etqf_map[(pkt_info >> 4) & 0X07] |
				ip_rss_types_map[pkt_info & 0XF];
	else
		return ip_rss_types_map[pkt_info & 0XF];
#else
	return ip_rss_types_map[pkt_info & 0XF];
#endif
}

static inline uint64_t
rx_desc_status_to_pkt_flags(uint32_t rx_status, uint64_t vlan_flags)
{
	uint64_t pkt_flags;

	/*
	 * Check if VLAN present only.
	 * Do not check whether L3/L4 rx checksum done by NIC or not,
	 * That can be found from rte_eth_rxmode.offloads flag
	 */
	pkt_flags = (rx_status & IXGBE_RXD_STAT_VP) ?  vlan_flags : 0;

#ifdef RTE_LIBRTE_IEEE1588
	if (rx_status & IXGBE_RXD_STAT_TMST)
		pkt_flags = pkt_flags | PKT_RX_IEEE1588_TMST;
#endif
	return pkt_flags;
}

static inline uint64_t
rx_desc_error_to_pkt_flags(uint32_t rx_status)
{
	uint64_t pkt_flags;

	/*
	 * Bit 31: IPE, IPv4 checksum error
	 * Bit 30: L4I, L4I integrity error
	 */
	static uint64_t error_to_pkt_flags_map[4] = {
		PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD,
		PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_BAD,
		PKT_RX_IP_CKSUM_BAD | PKT_RX_L4_CKSUM_GOOD,
		PKT_RX_IP_CKSUM_BAD | PKT_RX_L4_CKSUM_BAD
	};
	pkt_flags = error_to_pkt_flags_map[(rx_status >>
		IXGBE_RXDADV_ERR_CKSUM_BIT) & IXGBE_RXDADV_ERR_CKSUM_MSK];

	if ((rx_status & IXGBE_RXD_STAT_OUTERIPCS) &&
	    (rx_status & IXGBE_RXDADV_ERR_OUTERIPER)) {
		pkt_flags |= PKT_RX_EIP_CKSUM_BAD;
	}

#ifdef RTE_LIBRTE_SECURITY
	if (rx_status & IXGBE_RXD_STAT_SECP) {
		pkt_flags |= PKT_RX_SEC_OFFLOAD;
		if (rx_status & IXGBE_RXDADV_LNKSEC_ERROR_BAD_SIG)
			pkt_flags |= PKT_RX_SEC_OFFLOAD_FAILED;
	}
#endif

	return pkt_flags;
}

void ixgbe_rx_cleanq_enqueue(struct ixgbe_rx_queue *rxq, struct rte_mbuf *mb) {
	volatile union ixgbe_adv_rx_desc *rxdp;
	struct ixgbe_rx_entry *rxep;
	__le64 dma_addr;

	rxep = &rxq->sw_ring[rxq->rx_tail];
	rxdp = &rxq->rx_ring[rxq->rx_tail];
	
	/* populate the static rte mbuf fields */
	mb->port = rxq->port_id;
	rte_mbuf_refcnt_set(mb, 1);
	mb->data_off = RTE_PKTMBUF_HEADROOM;
	rxep->mbuf = mb;

 	/* populate the descriptor */
	dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mb));
	rxdp->read.hdr_addr = 0;
	rxdp->read.pkt_addr = dma_addr;

	PMD_CLEANQ_LOG(INFO, "RX: Enqueued buffer %"PRIu16"", rxq->rx_tail);

	rxq->rx_tail = (uint16_t)(rxq->rx_tail + 1);
	if (rxq->rx_tail >= rxq->nb_rx_desc) {
		rxq->rx_tail = 0;
	}

	rte_wmb();

	IXGBE_PCI_REG_WRITE_RELAXED(rxq->rdt_reg_addr, rxq->rx_tail);
}

bool ixgbe_rx_cleanq_dequeue(struct ixgbe_rx_queue *rxq, struct rte_mbuf **ret_mb)
{
	volatile union ixgbe_adv_rx_desc *rxdp;
	struct ixgbe_rx_entry *rxep;
	struct rte_mbuf *mb;
	uint16_t pkt_len;
	uint64_t pkt_flags;
	uint32_t pkt_info;
	uint32_t status;
	uint64_t vlan_flags = rxq->vlan_flags;

    if (unlikely(rxq->rx_recl == rxq->rx_tail)) {
		PMD_CLEANQ_LOG(NOTICE, "RX: Descriptor ring full (%"PRIx32")", rxq->rx_recl);
		return false;
	}

	/* get references to current descriptor and S/W ring entry */
	rxdp = &rxq->rx_ring[rxq->rx_recl];
	rxep = &rxq->sw_ring[rxq->rx_recl];

	status = rte_le_to_cpu_32(rxdp->wb.upper.status_error);
	
	rte_smp_rmb();

	/* Check whether there is a packet to receive */
	if (!(status & IXGBE_RXDADV_STAT_DD)) {
		PMD_CLEANQ_LOG(DEBUG, "RX: No buffer to dequeue (%"PRIx32")", status);
		return false;
	}

	pkt_info = rte_le_to_cpu_32(rxdp->wb.lower.lo_dword.data);
	pkt_len = rte_le_to_cpu_16(rxdp->wb.upper.length) - rxq->crc_len;
	pkt_flags = rx_desc_status_to_pkt_flags(status, vlan_flags);
	pkt_flags |= rx_desc_error_to_pkt_flags(status);
	pkt_flags |= ixgbe_rxd_pkt_info_to_pkt_flags ((uint16_t)pkt_info);

	mb = rxep->mbuf;
	rxep->mbuf = NULL;

	mb->data_len = pkt_len;
	mb->pkt_len = pkt_len;
	mb->vlan_tci = rte_le_to_cpu_16(rxdp->wb.upper.vlan);

	/* convert descriptor fields to rte mbuf flags */
	mb->ol_flags = pkt_flags;
	mb->packet_type = ixgbe_rxd_pkt_info_to_pkt_type(pkt_info, rxq->pkt_type_mask);

	if (likely(pkt_flags & PKT_RX_RSS_HASH)) {
		mb->hash.rss = rte_le_to_cpu_32(
			rxdp->wb.lower.hi_dword.rss);
	}
	else if (pkt_flags & PKT_RX_FDIR) {
		mb->hash.fdir.hash = rte_le_to_cpu_16(
			rxdp->wb.lower.hi_dword.csum_ip.csum) &
			IXGBE_ATR_HASH_MASK;
		mb->hash.fdir.id = rte_le_to_cpu_16(
			rxdp->wb.lower.hi_dword.csum_ip.ip_id);
	}

	PMD_CLEANQ_LOG(INFO, "RX: Dequeued buffer %"PRIu16, rxq->rx_recl);

	rxq->rx_recl = (uint16_t)(rxq->rx_recl + 1);
	if (rxq->rx_recl >= rxq->nb_rx_desc) {
		rxq->rx_recl = 0;
	}

	*ret_mb = mb;
	return true;
}
