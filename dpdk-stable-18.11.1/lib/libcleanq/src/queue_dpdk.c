/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */
#include <stdbool.h>

#include <rte_mbuf.h>

#include <cleanq.h>
#include <cleanq_module.h>

#include <cleanq_dpdk.h>

#include "region_pool.h"

inline void
membpool_to_cap(struct rte_mempool *pool, struct capref *cap)
{
    cap->len = pool->mz->len;
    // Only use virtual addresses
    cap->paddr = (uint64_t)pool->mz->addr;
    cap->vaddr = pool->mz->addr;
}

inline void
mbuf_to_cleanq_buf(
    struct cleanq *q,
    struct rte_mbuf *mbuf,
    struct cleanq_buf *cqbuf)
{
    uint64_t base_addr = (uint64_t)mbuf->pool->mz->addr;
    cqbuf->offset = (genoffset_t)mbuf - base_addr;
	cqbuf->length = mbuf->buf_len;
	cqbuf->valid_data = mbuf->data_off;
	cqbuf->valid_length = mbuf->data_len;
	cqbuf->flags = 0;
	cqbuf->rid = region_with_base_addr(q->pool, base_addr);
}

inline void
cleanq_buf_to_mbuf(
    struct cleanq *q,
    struct cleanq_buf cqbuf,
    struct rte_mbuf **mbuf)
{
    uint64_t base_addr = base_addr_of_region(q->pool, cqbuf.rid);
    struct rte_mbuf *mb = (struct rte_mbuf *)(base_addr + cqbuf.offset);
    if (base_addr > 0) {
        assert(cqbuf.length == mb->buf_len);
        assert(cqbuf.valid_data == mb->data_off);
        assert(cqbuf.valid_length == mb->data_len);
    }
    *mbuf = mb;
}