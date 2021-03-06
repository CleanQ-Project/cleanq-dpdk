/*
 * Copyright (c) 2017, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitätstrasse 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <cleanq_module.h>
#include <cleanq.h>
#include <cleanq_bench.h>
#include <cleanq_pkt_headers.h>
#include <cleanq_udp.h>

#include <arpa/inet.h>
#include "inet_chksum.h"

#define MAX_NUM_REGIONS 64

//#define DEBUG_ENABLED

#if defined(DEBUG_ENABLED) 
#define DEBUG(x...) do { printf("UDP_QUEUE:%s:%d: ", \
            __func__, __LINE__); \
                printf(x);\
        } while (0)

#else
#define DEBUG(x...) ((void)0)
#endif 

struct region_vaddr {
    void* va;
    regionid_t rid;
};

struct udp_q {
    struct cleanq my_q;
    struct cleanq* q;
    struct udp_hdr header; // can fill in this header and reuse it by copying
    uint16_t dst_port;
    uint16_t src_port;
    struct region_vaddr regions[MAX_NUM_REGIONS];
};


#ifdef DEBUG_ENABLED
static void print_buffer(void* start, uint64_t len)
{
    uint8_t* buf = (uint8_t*) start;
    printf("Packet in region at address %p len %zu \n",
           buf, len);
    for (uint32_t i = 0; i < len; i+=2) {
        if (((i % 16) == 0) && i > 0) {
            printf("\n");
        }
        printf("%2X", buf[i]);
        printf("%2X ", buf[i+1]);
    }
    printf("\n");
}
#endif

static errval_t udp_register(struct cleanq* q, struct capref cap,
                            regionid_t rid) 
{
    struct udp_q* que = (struct udp_q*) q;

    que->regions[rid % MAX_NUM_REGIONS].va = cap.vaddr;
    que->regions[rid % MAX_NUM_REGIONS].rid = rid;

    return que->q->f.reg(que->q, cap, rid);
}

static errval_t udp_deregister(struct cleanq* q, regionid_t rid) 
{
    
    struct udp_q* que = (struct udp_q*) q;
    que->regions[rid % MAX_NUM_REGIONS].va = NULL;
    que->regions[rid % MAX_NUM_REGIONS].rid = 0;
    return que->q->f.dereg(que->q, rid);
}


static errval_t udp_control(struct cleanq* q, uint64_t cmd, uint64_t value,
                           uint64_t* result)
{
    struct udp_q* que = (struct udp_q*) q;
    return que->q->f.ctrl(que->q, cmd, value, result);
}


static errval_t udp_notify(struct cleanq* q)
{
    struct udp_q* que = (struct udp_q*) q;
    return que->q->f.notify(que->q);
}

static errval_t udp_enqueue(struct cleanq* q, regionid_t rid, 
                           genoffset_t offset, genoffset_t length,
                           genoffset_t valid_data, genoffset_t valid_length,
                           uint64_t flags)
{

    // for now limit length
    //  TODO fragmentation

    struct udp_q* que = (struct udp_q*) q;
    if (flags & NETIF_TXFLAG) {
        
        DEBUG("TX rid: %d offset %ld length %ld valid_length %ld valid_data %ld \n", rid, offset, 
              length, valid_length, valid_data);
        //que->header.len = htons(valid_length + UDP_HLEN);
        que->header.len = htons(valid_length - IP_HLEN - ETH_HLEN);
    	que->header.dest = flags & 0xFFFF;

        assert(que->regions[rid % MAX_NUM_REGIONS].va != NULL);

        uint8_t* start = ((uint8_t*) que->regions[rid % MAX_NUM_REGIONS].va) + 
                         offset + valid_data + ETH_HLEN + IP_HLEN + 128;   

        memcpy(start, &que->header, sizeof(que->header));   

        return que->q->f.enq(que->q, rid, offset, length, valid_data, 
                             valid_length, flags);
    } 

    if (flags & NETIF_RXFLAG) {
        assert(valid_length <= 2048);    
        DEBUG("RX rid: %d offset %ld length %ld valid_length %ld \n", rid, offset, 
              length, valid_length);
        return que->q->f.enq(que->q, rid, offset, length, valid_data, 
                             valid_length, flags);
    } 

    return -1;
}

static errval_t udp_dequeue(struct cleanq* q, regionid_t* rid, genoffset_t* offset,
                           genoffset_t* length, genoffset_t* valid_data,
                           genoffset_t* valid_length, uint64_t* flags)
{
    errval_t err;
    struct udp_q* que = (struct udp_q*) q;

    err = que->q->f.deq(que->q, rid, offset, length, valid_data, valid_length, flags);
    if (err_is_fail(err)) {    
        return err;
    }

    if (*flags & NETIF_RXFLAG) {
        DEBUG("RX rid: %d offset %ld valid_data %ld length %ld va %p \n", *rid, 
              *offset, *valid_data, 
              *valid_length, ((uint8_t*) que->regions[*rid % MAX_NUM_REGIONS].va + 
              *offset) + *valid_data);

        struct udp_hdr* header = (struct udp_hdr*) 
                                 ((uint8_t*)(que->regions[*rid % MAX_NUM_REGIONS].va) +
                                 *offset + *valid_data + IP_HLEN + ETH_HLEN + 128);
 
        // Correct port for this queue?
        if (header->dest != htons(que->dst_port)) {
            printf("UDP queue: dropping packet, wrong port %d %d \n",
                   header->dest, que->dst_port);
            err = que->q->f.enq(que->q, *rid, *offset, *length, *valid_data, 
			    	*valid_length, NETIF_RXFLAG);
            return CLEANQ_ERR_UDP_WRONG_PORT;
        }
        
#ifdef DEBUG_ENABLED
        print_buffer((uint8_t*) que->regions[*rid % MAX_NUM_REGIONS].va + *offset, *valid_length);
#endif

	*flags |= header->src;
        //*valid_length = ntohs(header->len) - UDP_HLEN;
        //*valid_data += UDP_HLEN;
        //print_buffer(que, que->regions[*rid % MAX_NUM_REGIONS].va + *offset+ *valid_data, *valid_length);
        return CLEANQ_ERR_OK;
    }

#ifdef DEBUG_ENABLED
    DEBUG("TX rid: %d offset %ld length %ld \n", *rid, *offset, 
          *valid_length);
#endif

    return CLEANQ_ERR_OK;
}

/*
 * Public functions
 *
 */
errval_t udp_create(struct udp_q** q, struct cleanq* nic_rx, struct cleanq* nic_tx,
                    uint16_t src_port, uint16_t dst_port,
                    uint32_t src_ip, uint32_t dst_ip,
		    struct ether_addr* src_mac, struct ether_addr* dst_mac)
{
    errval_t err;
    struct udp_q* que;
    que = calloc(1, sizeof(struct udp_q));
    assert(que);

    // init other queue
    err = ip_create((struct ip_q**) &que->q, nic_rx, nic_tx, 
		     UDP_PROT, src_ip, dst_ip, src_mac, 
		     dst_mac);
    if (err_is_fail(err)) {
        return err;
    }

    err = cleanq_init(&que->my_q);
    if (err_is_fail(err)) {
        return err;
    }   

    // UDP fields
    que->header.src = htons(src_port);
    que->header.dest = htons(dst_port);
    que->src_port = src_port;
    que->dst_port = dst_port;
    que->header.chksum = 0x0;

    que->my_q.f.reg = udp_register;
    que->my_q.f.dereg = udp_deregister;
    que->my_q.f.ctrl = udp_control;
    que->my_q.f.notify = udp_notify;
    que->my_q.f.enq = udp_enqueue;
    que->my_q.f.deq = udp_dequeue;
    *q = que;

    return CLEANQ_ERR_OK;
}

errval_t udp_destroy(struct udp_q* q)
{
    // TODO destroy q->q;
    free(q);    

    return CLEANQ_ERR_OK;
}

errval_t udp_write_buffer(struct udp_q* q, regionid_t rid, genoffset_t offset,
                          void* data, uint16_t len) 
{
    assert(len <= 1500);
    if (q->regions[rid % MAX_NUM_REGIONS].va != NULL) {
        uint8_t* start = ((uint8_t*) q->regions[rid % MAX_NUM_REGIONS].va) + offset 
                         + sizeof (struct udp_hdr) 
                         + sizeof (struct ip_hdr)
                         + sizeof (struct eth_hdr);
        memcpy(start, data, len);
        return CLEANQ_ERR_OK;
    } else {
        return CLEANQ_ERR_INVALID_REGION_ARGS;
    }
}

/*
struct bench_ctl* udp_get_benchmark_data(struct udp_q* q, uint32_t type)
{
    return ip_get_benchmark_data((struct ip_q*) q->q, type);
}
*/
