#ifndef _CLEANQ_PMD_IXGBE_H_
#define _CLEANQ_PMD_IXGBE_H_

#include <rte_ethdev_driver.h>

errval_t cleanq_pmd_ixgbe_tx_register(
    uint16_t port_id,
    uint16_t tx_queue_id,
	struct rte_mempool *mp,
    regionid_t *region_id);

errval_t cleanq_pmd_ixgbe_tx_deregister(
    uint16_t port_id,
    uint16_t tx_queue_id,
    regionid_t region_id);

#endif /* _CLEANQ_PMD_IXGBE_H_ */