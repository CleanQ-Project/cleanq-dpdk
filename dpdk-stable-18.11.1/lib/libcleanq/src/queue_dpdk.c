/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */
#include <stdbool.h>
#include <sys/queue.h>

#include <rte_mbuf.h>

#include <cleanq.h>
#include <cleanq_module.h>

#include <cleanq_dpdk.h>

#include "dqi_debug.h"
#include "region_pool.h"

static inline uint64_t
mempool_base_addr(struct rte_mempool *mp)
{
    struct rte_mempool_memhdr *first_chunk = STAILQ_FIRST(&mp->mem_list);
    return (uint64_t)first_chunk->addr;
}

static inline uint64_t
mempool_length(struct rte_mempool *mp)
{
    uint64_t length = 0;
    struct rte_mempool_memhdr *mem_chunk;
    // Assume mempool is contiguous
    STAILQ_FOREACH(mem_chunk, &mp->mem_list, next) {
        length += mem_chunk->len;
    }
    return length;
}

static inline void
mempool_to_cap(struct rte_mempool *mp, struct capref *cap)
{
    uint64_t base_addr = mempool_base_addr(mp);
    cap->len = mempool_length(mp);
    // Only use virtual addresses
    cap->paddr = base_addr;
    cap->vaddr = (void *)base_addr;
}

errval_t
cleanq_register_mempool(struct cleanq *q, struct rte_mempool *mp)
{
    struct capref cap;
    regionid_t region_id;

    mempool_to_cap(mp, &cap);

    DQI_DEBUG("Registering mempool: base_addr=0x%"PRIx64", length=%"PRIu64"\n",
        cap.paddr,
        cap.len
    );

    return cleanq_register(q, cap, &region_id);
}

errval_t
cleanq_deregister_mempool(struct cleanq *q, struct rte_mempool *mp)
{
    struct capref cap;
    regionid_t region_id;

    uint64_t base_addr = mempool_base_addr(mp);
    region_id = region_with_base_addr(q->pool, base_addr);

    DQI_DEBUG("Deregistering mempool: base_addr=0x%"PRIx64", region_id=%"PRIu32"\n",
        cap.paddr,
        region_id
    );

    return cleanq_deregister(q, region_id, &cap);
}

inline void
mbuf_to_cleanq_buf(
    struct cleanq *q,
    struct rte_mbuf *mbuf,
    struct cleanq_buf *cqbuf)
{
    uint64_t base_addr = mempool_base_addr(mbuf->pool);
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
