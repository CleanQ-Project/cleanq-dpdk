/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef QUEUE_DPDK_H_
#define QUEUE_DPDK_H_ 1

struct cleanq;
struct cleanq_buf;
struct rte_mbuf;

void
mbuf_to_cleanq_buf(
    struct cleanq *q,
    struct rte_mbuf *mbuf,
    struct cleanq_buf *cqbuf);

void
cleanq_buf_to_mbuf(
    struct cleanq *q,
    struct cleanq_buf cqbuf,
    struct rte_mbuf **mbuf);

#endif /* QUEUE_DPDK_H_ */