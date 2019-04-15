/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ether.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {
        .max_rx_pkt_len = ETHER_MAX_LEN,
    },
};

/* basicfwd.c: Basic DPDK skeleton forwarding example. */

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
    struct rte_eth_conf port_conf = port_conf_default;
    const uint16_t rx_rings = 1, tx_rings = 1;
    uint16_t nb_rxd = RX_RING_SIZE;
    uint16_t nb_txd = TX_RING_SIZE;
    int retval;
    uint16_t q;
    struct rte_eth_dev_info dev_info;
    struct rte_eth_txconf txconf;

    if (!rte_eth_dev_is_valid_port(port))
        return -1;

    rte_eth_dev_info_get(port, &dev_info);
    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
        port_conf.txmode.offloads |=
            DEV_TX_OFFLOAD_MBUF_FAST_FREE;

    /* Configure the Ethernet device. */
    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if (retval != 0)
        return retval;

    retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
    if (retval != 0)
        return retval;

    /* Allocate and set up 1 RX queue per Ethernet port. */
    for (q = 0; q < rx_rings; q++) {
        retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
                rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if (retval < 0)
            return retval;
    }

    txconf = dev_info.default_txconf;
    txconf.offloads = port_conf.txmode.offloads;
    /* Allocate and set up 1 TX queue per Ethernet port. */
    for (q = 0; q < tx_rings; q++) {
        retval = rte_eth_tx_queue_setup(port, q, nb_txd,
                rte_eth_dev_socket_id(port), &txconf);
        if (retval < 0)
            return retval;
    }

    /* Start the Ethernet port. */
    retval = rte_eth_dev_start(port);
    if (retval < 0)
        return retval;

    /* Display the port MAC address. */
    struct ether_addr addr;
    rte_eth_macaddr_get(port, &addr);
    char addr_string[ETHER_ADDR_FMT_SIZE];
    ether_format_addr(addr_string, ETHER_ADDR_FMT_SIZE, &addr);
    printf("Port %u MAC: %s\n", port, addr_string);

    /* Enable RX in promiscuous mode for the Ethernet device. */
    rte_eth_promiscuous_enable(port);

    return 0;
}

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static __attribute__((noreturn)) void
lcore_main(void)
{
    uint16_t port;

    /*
     * Check that the port is on the same NUMA node as the polling thread
     * for best performance.
     */
    RTE_ETH_FOREACH_DEV(port)
        if (rte_eth_dev_socket_id(port) > 0 &&
                rte_eth_dev_socket_id(port) !=
                        (int)rte_socket_id())
            printf("WARNING, port %u is on remote NUMA node to "
                    "polling thread.\n\tPerformance will "
                    "not be optimal.\n", port);

    printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
            rte_lcore_id());

    /* Run until the application is quit or killed. */
    for (;;) {
        /*
         * Receive packets on a port and echo them
         */
        RTE_ETH_FOREACH_DEV(port) {
            struct ether_addr port_addr;
            rte_eth_macaddr_get(port, &port_addr);

            /* Get burst of RX packets. */
            struct rte_mbuf *rx_bufs[BURST_SIZE];
            const uint16_t nb_rx = rte_eth_rx_burst(port, 0, rx_bufs, BURST_SIZE);

            if (unlikely(nb_rx == 0))
                continue;

            printf("%u packets received on port %u\n", nb_rx, port);

            struct rte_mbuf *tx_bufs[BURST_SIZE];
            uint16_t nb_to_send = 0;

            for (uint16_t buf = 0; buf < nb_rx; buf++) {
                struct rte_mbuf *buffer = rx_bufs[buf];
                const char *l2_name = rte_get_ptype_l2_name(buffer->packet_type);
                const char *l3_name = rte_get_ptype_l3_name(buffer->packet_type);
                const char *l4_name = rte_get_ptype_l4_name(buffer->packet_type);
                printf("Packet %u has types %s, %s, %s\n", buf, l2_name, l3_name, l4_name);
                
                struct ether_hdr *eth_hdr = rte_pktmbuf_mtod(buffer, struct eth_hdr *);
                            
                char s_addr[ETHER_ADDR_FMT_SIZE];
                char d_addr[ETHER_ADDR_FMT_SIZE];
                ether_format_addr(s_addr, ETHER_ADDR_FMT_SIZE, &eth_hdr->s_addr);
                ether_format_addr(d_addr, ETHER_ADDR_FMT_SIZE, &eth_hdr->d_addr);
                printf("Packet %u %s -> %s\n", buf, s_addr, d_addr);

                if (is_same_ether_addr(&port_addr, &eth_hdr->d_addr)) {
                    /* Packet was for us */
                    printf("Reflecting packet %u", buf);

                    // Switch addresses
                    struct ether_addr tmp_addr;
                    ether_addr_copy(&eth_hdr->s_addr, &tmp_addr);
                    ether_addr_copy(&eth_hdr->d_addr, &eth_hdr->s_addr);
                    ether_addr_copy(&tmp_addr, &eth_hdr->d_addr);

                    // Enqueue buffer for sending
                    tx_bufs[nb_to_send] = buffer;
                    nb_to_send++;
                }
            }


            /* Send burst of TX packets, to same port */
            const uint16_t nb_tx = rte_eth_tx_burst(port, 0, tx_bufs, nb_to_send);

            printf("%u packets sent over port %u\n\n", nb_tx, port);

            /* Free any unsent packets. */
            if (unlikely(nb_tx < nb_to_send)) {
                for (uint16_t buf = nb_tx; buf < nb_to_send; buf++) {
                    rte_pktmbuf_free(tx_bufs[buf]);
                }
            }
        }
    }
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
    struct rte_mempool *mbuf_pool;
    unsigned nb_ports;
    uint16_t portid;

    /* Initialize the Environment Abstraction Layer (EAL). */
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

    argc -= ret;
    argv += ret;

    /* Check that there is an even number of ports to send/receive on. */
    nb_ports = rte_eth_dev_count_avail();
    if (nb_ports < 1)
        rte_exit(EXIT_FAILURE, "Error: no ports available\n");

    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* Initialize all ports. */
    RTE_ETH_FOREACH_DEV(portid)
        if (port_init(portid, mbuf_pool) != 0)
            rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu16 "\n",
                    portid);

    if (rte_lcore_count() > 1)
        printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

    /* Call lcore_main on the master core only. */
    lcore_main();

    return 0;
}
