/*
 * Copyright (c) 2017 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */
#ifndef CLEANQ_IP_H_
#define CLEANQ_IP_H_ 1

#include <cleanq.h>

//#define BENCH 1
#ifdef BENCH
#define BENCH_SIZE 100000
#endif

struct bench_ctl; 
struct ip_q;
    
struct ether_addr;
/**
 *  @param q        ip queue to destroy
 */
errval_t ip_destroy(struct ip_q* q);

/**
 * @brief initalized a queue that can send IP packets with a certain requirements.
 *        all other packets received on this queue will be dropped.
 *
 * @param q            ip queue return value
 * @param card_name    the card name from which a hardware queue will be used
 *                      to send IP packets. Internally a queue to the device with
 *                      the card_name will be initalized
 * @param prot         The protocol that is running on top of IP
 * @param dst_ip       Destination IP
 * @param interrupt    Interrupt handler
 * @param poll         If the queue is polled or should use interrupts             
 *
 */
errval_t ip_create(struct ip_q** q, struct cleanq* nic_rx, struct cleanq* nic_tx, 
		   uint8_t prot, uint32_t src_ip , uint32_t dst_ip,
		   struct ether_addr* src_mac, struct ether_addr* dst_mac);

//struct bench_ctl* ip_get_benchmark_data(struct ip_q* q, uint8_t type);
#endif /* CLEANQ_IP_H_ */
