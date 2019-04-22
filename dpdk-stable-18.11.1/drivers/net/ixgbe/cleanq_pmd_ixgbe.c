#include <cleanq.h>
#include <cleanq_dpdk.h>

#include "ixgbe_ethdev.h"
#include "cleanq_pmd_ixgbe.h"

errval_t cleanq_pmd_ixgbe_tx_register(
    uint16_t port_id,
    uint16_t tx_queue_id,
	struct rte_mempool *mp,
    regionid_t *region_id)
{
	struct rte_eth_dev *dev;
    struct cleanq *q;
    struct capref cap;
    
    RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

    if (tx_queue_id >= dev->data->nb_rx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid RX queue_id=%u\n", tx_queue_id);
		return -EINVAL;
	}

    q = (struct cleanq *)dev->data->tx_queues[tx_queue_id];
    
    membpool_to_cap(mp, &cap);
    return cleanq_register(q, cap, region_id);
}

errval_t cleanq_pmd_ixgbe_tx_deregister(
    uint16_t port_id,
    uint16_t tx_queue_id,
    regionid_t region_id)
{
    struct rte_eth_dev *dev;
    struct cleanq *q;
    struct capref cap;
    
    RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

    if (tx_queue_id >= dev->data->nb_rx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid RX queue_id=%u\n", tx_queue_id);
		return -EINVAL;
	}

    q = (struct cleanq *)dev->data->tx_queues[tx_queue_id];

    return cleanq_deregister(q, region_id, &cap);
}
