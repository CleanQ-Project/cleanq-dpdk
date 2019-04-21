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
#include <rte_ip.h>
#include <rte_udp.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

#define DRIVER_LOG_LEVEL RTE_LOG_WARNING
#define CLEANQ_LOG_LEVEL RTE_LOG_WARNING
#define MAIN_LOG_LEVEL RTE_LOG_INFO

int logtype;
#define LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, logtype, fmt , ##args)

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

            struct rte_mbuf *tx_bufs[BURST_SIZE];
            uint16_t nb_to_send = 0;

            for (uint16_t buf = 0; buf < nb_rx; buf++) {
                struct rte_mbuf *buffer = rx_bufs[buf];

                if (unlikely(!(buffer->packet_type & RTE_PTYPE_L2_ETHER))) {
                    rte_pktmbuf_free(buffer);
                    continue;
                }
                
                struct ether_hdr *eth_hdr = rte_pktmbuf_mtod(buffer, struct ether_hdr *);

                if (is_same_ether_addr(&port_addr, &eth_hdr->d_addr)) {
                    /* Packet was for us */
                    LOG(NOTICE, "\nPacket %"PRIu16"/%"PRIu16" received on port %"PRIu16"\n",
                        buf,
                        nb_rx,
                        port
                    );

                    char type_name[64];
                    rte_get_ptype_name(buffer->packet_type, type_name, 64);
                    LOG(NOTICE, "Type: %s\n", type_name);

                    char s_addr[ETHER_ADDR_FMT_SIZE];
                    char d_addr[ETHER_ADDR_FMT_SIZE];
                    ether_format_addr(s_addr, ETHER_ADDR_FMT_SIZE, &eth_hdr->s_addr);
                    ether_format_addr(d_addr, ETHER_ADDR_FMT_SIZE, &eth_hdr->d_addr);

                    LOG(INFO, "MAC: %s -> %s\n", s_addr, d_addr);

                    // Switch hardware addresses
                    struct ether_addr tmp_hw_addr;
                    ether_addr_copy(&eth_hdr->s_addr, &tmp_hw_addr);
                    ether_addr_copy(&eth_hdr->d_addr, &eth_hdr->s_addr);
                    ether_addr_copy(&tmp_hw_addr, &eth_hdr->d_addr);

                    if (buffer->packet_type & RTE_PTYPE_L3_IPV4) {
                        struct ipv4_hdr *ip_hdr = rte_pktmbuf_mtod_offset(
                            buffer,
                            struct ipv4_hdr *,
                            sizeof(struct ether_hdr)
                        );

			            uint8_t ip_hdr_len = (ip_hdr->version_ihl & IPV4_HDR_IHL_MASK) * IPV4_IHL_MULTIPLIER;
                        LOG(INFO,
                            "IP: %"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8" -> "
                            "%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8" (ID: %"PRIu16")\n",
                            ip_hdr->src_addr & 0xff,
                            (ip_hdr->src_addr >> 8) & 0xff,
                            (ip_hdr->src_addr >> 16) & 0xff,
                            ip_hdr->src_addr >> 24,
                            ip_hdr->dst_addr & 0xff,
                            (ip_hdr->dst_addr >> 8) & 0xff,
                            (ip_hdr->dst_addr >> 16) & 0xff,
                            ip_hdr->dst_addr >> 24,
                            rte_be_to_cpu_16(ip_hdr->packet_id)
                        );

                        // Switch IP addresses
                        const uint32_t tmp_ip_addr = ip_hdr->src_addr;
                        ip_hdr->src_addr = ip_hdr->dst_addr;
                        ip_hdr->dst_addr = tmp_ip_addr;

                        // Zero checksum
                        ip_hdr->hdr_checksum = 0;

                        if (buffer->packet_type & RTE_PTYPE_L4_UDP) {
                            struct udp_hdr *udp_hdr = rte_pktmbuf_mtod_offset(
                                buffer,
                                struct udp_hdr *,
                                sizeof(struct ether_hdr) + ip_hdr_len
                            );
                            LOG(INFO, "UDP: %"PRIu16" -> %"PRIu16" (length: %"PRIu16")\n",
                                rte_be_to_cpu_16(udp_hdr->src_port),
                                rte_be_to_cpu_16(udp_hdr->dst_port),
                                rte_be_to_cpu_16(udp_hdr->dgram_len)
                            );

                            // Switch ports
                            const uint32_t tmp_udp_port = udp_hdr->src_port;
                            udp_hdr->src_port = udp_hdr->dst_port;
                            udp_hdr->dst_port = tmp_udp_port;

                            // New checksum
                            udp_hdr->dgram_cksum = 0;
                            udp_hdr->dgram_cksum = rte_ipv4_udptcp_cksum(ip_hdr, udp_hdr);
                        }

                        // New checksum
                        ip_hdr->hdr_checksum = rte_ipv4_cksum(ip_hdr);
                    }

                    // Enqueue buffer for sending
                    tx_bufs[nb_to_send] = buffer;
                    nb_to_send++;
                }
                else {
                    rte_pktmbuf_free(buffer);
                }
            }

            if (nb_to_send > 0) {
                /* Send burst of TX packets, to same port */
                const uint16_t nb_tx = rte_eth_tx_burst(port, 0, tx_bufs, nb_to_send);

                LOG(NOTICE, "%"PRIu16" packets sent over port %"PRIu16"\n", nb_tx, port);

                /* Free any unsent packets. */
                if (unlikely(nb_tx < nb_to_send)) {
                    for (uint16_t buf = nb_tx; buf < nb_to_send; buf++) {
                        rte_pktmbuf_free(tx_bufs[buf]);
                    }
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

    rte_log_set_level(RTE_LOGTYPE_PMD, DRIVER_LOG_LEVEL);
    rte_log_set_level(rte_log_register("pmd.net.ixgbe.init"), DRIVER_LOG_LEVEL);
    rte_log_set_level(rte_log_register("pmd.net.ixgbe.cleanq"), CLEANQ_LOG_LEVEL);
    logtype = rte_log_register("cleanq.testapp");
    rte_log_set_level(logtype, MAIN_LOG_LEVEL);

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
