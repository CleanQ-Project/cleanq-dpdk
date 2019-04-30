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
#include <cleanq.h>
#include <cleanq_module.h>
#include <cleanq_bench.h>
#include <cleanq_ip.h>
#include <cleanq_pkt_headers.h>

#include <arpa/inet.h>
#include <rte_ip.h>
#include "inet_chksum.h"

#define MAX_NUM_REGIONS 64

//#define DEBUG_ENABLED

#if defined(DEBUG_ENABLED) 
#define DEBUG(x...) do { printf("IP_QUEUE: %s:%d: ", \
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

struct pkt_ip_headers {
    struct eth_hdr eth;
    struct ip_hdr ip;
} __attribute__ ((packed));

struct ip_q {
    struct cleanq my_q;
    struct cleanq* rx;
    struct cleanq* tx;
    struct pkt_ip_headers header; // can fill in this header and reuse it by copying
    struct region_vaddr regions[MAX_NUM_REGIONS];
    uint16_t hdr_len;
    uint8_t proto;

    const char* name;
#ifdef BENCH
    bench_ctl_t en_rx;
    bench_ctl_t en_tx;
    bench_ctl_t deq_rx;
    bench_ctl_t deq_tx;
#endif
};


#ifdef DEBUG_ENABLED
static void print_buffer(struct ip_q* q, void* start, uint64_t len)
{
    uint8_t* buf = (uint8_t*) start;
    printf("IP header Packet in region at address %p len %zu \n",
           buf, len);
    for (int i = 0; i < len; i+=2) {
        if (((i % 16) == 0) && i > 0) {
            printf("\n");
        }
        printf("%2X", buf[i]);
        printf("%2X ", buf[i+1]);
    }
    printf("\n");
}
#endif

static errval_t ip_register(struct cleanq* q, struct capref cap,
                            regionid_t rid) 
{  
    errval_t err;
    struct ip_q* que = (struct ip_q*) q;

    // dont have to map it, just store va
    que->regions[rid % MAX_NUM_REGIONS].va = cap.vaddr;
    que->regions[rid % MAX_NUM_REGIONS].rid = rid;
    DEBUG("id-%d va-%p \n", que->regions[rid % MAX_NUM_REGIONS].rid, 
          que->regions[rid % MAX_NUM_REGIONS].va);
	
    err = que->tx->f.reg(que->tx, cap, rid);
    if (err_is_fail(err)) {
	printf("IP register TX adding region failed \n");
        return err;
    }

    /*
    err = cleanq_add_region(que->rx, cap, rid);
    if (err_is_fail(err)) {
	 printf("IP register RX adding region failed \n");
        return err;
    }
    */
    return err;
}

static errval_t ip_deregister(struct cleanq* q, regionid_t rid) 
{
    errval_t err;
    struct ip_q* que = (struct ip_q*) q;
    que->regions[rid % MAX_NUM_REGIONS].va = NULL;
    que->regions[rid % MAX_NUM_REGIONS].rid = 0;
    err = que->tx->f.dereg(que->tx, rid);
    if (err_is_fail(err)) {
        return err;
    }
    err = cleanq_remove_region(que->rx, rid);
    return err;
}


static errval_t ip_control(struct cleanq* q, uint64_t cmd, uint64_t value,
                           uint64_t* result)
{
    struct ip_q* que = (struct ip_q*) q;
    return que->rx->f.ctrl(que->rx, cmd, value, result);
}


static errval_t ip_notify(struct cleanq* q)
{
    struct ip_q* que = (struct ip_q*) q;
    return que->rx->f.notify(que->rx);
}

static errval_t ip_enqueue(struct cleanq* q, regionid_t rid, 
                           genoffset_t offset, genoffset_t length,
                           genoffset_t valid_data, genoffset_t valid_length,
                           uint64_t flags)
{

    // for now limit length
    //  TODO fragmentation
    struct ip_q* que = (struct ip_q*) q;
    if (flags & NETIF_TXFLAG) {
        
        DEBUG("TX rid: %d offset %ld length %ld valid_length %ld valid_ata %ld \n", 
              rid, offset, length, valid_length, valid_data);
        assert(valid_length <= 1500);    
        //que->header.ip._len = htons(valid_length + IP_HLEN);   
        que->header.ip._len = htons(valid_length - ETH_HLEN);   
        que->header.ip._chksum = 0;
        que->header.ip._chksum = rte_ipv4_cksum((struct ipv4_hdr*) &que->header.ip);

        assert(que->regions[rid % MAX_NUM_REGIONS].va != NULL);

        uint8_t* start = (uint8_t*) que->regions[rid % MAX_NUM_REGIONS].va + 
                         offset + valid_data + 128;   

        memcpy(start, &que->header, sizeof(que->header));   

#ifdef BENCH
        uint64_t b_start, b_end;
        errval_t err;
            
        b_start = rdtscp();
        err = que->tx->f.enq(que->q, rid, offset, length, valid_data, 
                            valid_length, flags);
        b_end = rdtscp();
        if (err_is_ok(err)) {
            uint64_t res = b_end - b_start;
            bench_ctl_add_run(&que->en_tx, &res);
        }
        return err;
#else
        return que->tx->f.enq(que->tx, rid, offset, length, valid_data, 
                             valid_length, flags);
#endif
    } 

    if (flags & NETIF_RXFLAG) {
        assert(valid_length <= 2048);    
        DEBUG("RX rid: %d offset %ld length %ld valid_length %ld \n", rid, offset, 
              length, valid_length);
#ifdef BENCH
        uint64_t start, end;
        errval_t err;
            
        start = rdtscp();
        err = que->rx->f.enq(que->rx, rid, offset, length, valid_data, 
                             valid_length, flags);
        end = rdtscp();
        if (err_is_ok(err)) {
            uint64_t res = end - start;
            bench_ctl_add_run(&que->en_rx, &res);
        }
        return err;
#else
        return que->rx->f.enq(que->rx, rid, offset, length, valid_data, 
                             valid_length, flags);
#endif
    } 

    return CLEANQ_ERR_UNKNOWN_FLAG;
}

static errval_t ip_dequeue(struct cleanq* q, regionid_t* rid, genoffset_t* offset,
                           genoffset_t* length, genoffset_t* valid_data,
                           genoffset_t* valid_length, uint64_t* flags)
{
    errval_t err;
    struct ip_q* que = (struct ip_q*) q;

#ifdef BENCH
    uint64_t start, end;
    start = rdtscp();
    err = que->rx->f.deq(que->rx, rid, offset, length, valid_data, valid_length, flags);
    end = rdtscp();
    if (err_is_fail(err)) {  
    	start = rdtscp();
    	err = que->tx->f.deq(que->tx, rid, offset, length, valid_data, valid_length, flags);
    	end = rdtscp();
	if (err_is_fail(err)) {
	   return err;
	}
        return err;
    }
#else
    err = que->rx->f.deq(que->rx, rid, offset, length, valid_data, valid_length, flags);
    if (err_is_fail(err)) {  
    	err = que->tx->f.deq(que->tx, rid, offset, length, valid_data, valid_length, flags);
	if (err_is_fail(err)) {
	   return err;
	}
    	*flags |= NETIF_TXFLAG;
    } else {
    	*flags |= NETIF_RXFLAG;
    }
#endif

    if (*flags & NETIF_RXFLAG) {
        DEBUG("RX rid: %d offset %ld valid_data %ld length %ld va %p \n", *rid, 
              *offset, *valid_data, 
              *valid_length, ((uint8_t*)que->regions[*rid % MAX_NUM_REGIONS].va) + *offset + *valid_data);

        struct pkt_ip_headers* header = (struct pkt_ip_headers*) 
                                        (((uint8_t*) que->regions[*rid % MAX_NUM_REGIONS].va) +
                                         *offset + *valid_data + 128);
 
        // IP checksum
	/*
	uint16_t chksum = rte_ipv4_cksum((const struct ipv4_hdr *) &header->ip);
	//uint16_t chksum = inet_chksum(&(header->ip), IP_HLEN);
        if (header->ip._chksum != chksum) {
            DEBUG("IP queue: dropping packet wrong checksum is %x should be %x\n",
	          header->ip._chksum, chksum);
            err = que->rx->f.enq(que->rx, *rid, *offset, *length, *valid_data, *valid_length, 
                                 NETIF_RXFLAG);
            return CLEANQ_ERR_IP_CHKSUM;
        }
	*/
        // Correct ip for this queue?
        if (header->ip.src != que->header.ip.dest) {
            DEBUG("IP queue: dropping packet, wrong IP is %d should be %d\n",
                  header->ip.src, que->header.ip.dest);
            err = que->rx->f.enq(que->rx, *rid, *offset, *length, *valid_data, 
			    	 *valid_length, NETIF_RXFLAG);
            return CLEANQ_ERR_IP_WRONG_IP;
        }
        
	if (header->ip._proto != que->proto) {
            DEBUG("IP queue: dropping packet wrong protocol is %d should be %d \n", 
                  header->ip._proto, que->proto);
	  
            err = que->rx->f.enq(que->rx, *rid, *offset, *length, *valid_data, 
			    	 *valid_length, NETIF_RXFLAG);
            return CLEANQ_ERR_IP_WRONG_PROTO;
	}
#ifdef DEBUG_ENABLED
        print_buffer(que, que->regions[*rid % MAX_NUM_REGIONS].va + *offset, *valid_length);
#endif

        //*valid_data += IP_HLEN + ETH_HLEN + 128;
        //*valid_length = ntohs(header->ip._len) - IP_HLEN;
        //print_buffer(que, que->regions[*rid % MAX_NUM_REGIONS].va + *offset+ *valid_data, *valid_length);
        //

#ifdef BENCH
        uint64_t res = end - start;
        bench_ctl_add_run(&que->deq_rx, &res);
#endif
        return CLEANQ_ERR_OK;
    }

    DEBUG("TX rid: %d offset %ld length %ld \n", *rid, *offset, 
          *valid_length);

#ifdef BENCH
        uint64_t res = end - start;
        bench_ctl_add_run(&que->deq_tx, &res);
#endif

    return CLEANQ_ERR_OK;
}

/*
 * Public functions
 *
 */

errval_t ip_create(struct ip_q** q, struct cleanq* nic_rx, struct cleanq* nic_tx, 
                   uint8_t prot, uint32_t src_ip , uint32_t dst_ip,
                   struct ether_addr* src_mac, struct ether_addr* dst_mac)
{
    errval_t err;
    struct ip_q* que;
    que = calloc(1, sizeof(struct ip_q));
    assert(que);

    // TODO init cleanq from dpdk

    err = cleanq_init(&que->my_q);
    if (err_is_fail(err)) {
        // TODO net queue destroy
        return err;
    }   

    // fill in header that is reused for each packet
    // Ethernet
    memcpy(&(que->header.eth.dest), dst_mac, ETH_HWADDR_LEN);
    memcpy(&(que->header.eth.src), src_mac, ETH_HWADDR_LEN);

    que->header.eth.type = htons(ETHTYPE_IP);
    que->rx = nic_rx;
    que->tx = nic_tx;

    // IP
    que->header.ip._v_hl = 69;
    IPH_TOS_SET(&que->header.ip, 0x0);
    IPH_ID_SET(&que->header.ip, htons(0x3));
    que->header.ip._offset = htons(IP_DF); // htons?
    que->header.ip._ttl = 0x40; // 64
    que->header.ip.src = src_ip;
    que->header.ip.dest = dst_ip;

    que->my_q.f.reg = ip_register;
    que->my_q.f.dereg = ip_deregister;
    que->my_q.f.ctrl = ip_control;
    que->my_q.f.notify = ip_notify;
    que->my_q.f.enq = ip_enqueue;
    que->my_q.f.deq = ip_dequeue;
    *q = que;
	
    switch(prot) {
        case UDP_PROT:
            que->hdr_len = IP_HLEN + sizeof(struct udp_hdr);
	    que->proto = IP_PROTO_UDP;
	    que->header.ip._proto = IP_PROTO_UDP;
            break;
        case TCP_PROT:
            // TODO
            break;
        default:
            printf("Unknown protocl used when initalizing ip queue \n");
            return CLEANQ_ERR_INIT_QUEUE;
    }

#ifdef BENCH
    bench_init();
   
    que->en_tx.mode = BENCH_MODE_FIXEDRUNS;
    que->en_tx.result_dimensions = 1;
    que->en_tx.min_runs = BENCH_SIZE;
    que->en_tx.data = calloc(que->en_tx.min_runs * que->en_tx.result_dimensions,
                       sizeof(*que->en_tx.data));

    que->en_rx.mode = BENCH_MODE_FIXEDRUNS;
    que->en_rx.result_dimensions = 1;
    que->en_rx.min_runs = BENCH_SIZE;
    que->en_rx.data = calloc(que->en_rx.min_runs * que->en_rx.result_dimensions,
                       sizeof(*que->en_rx.data));

    que->deq_rx.mode = BENCH_MODE_FIXEDRUNS;
    que->deq_rx.result_dimensions = 1;
    que->deq_rx.min_runs = BENCH_SIZE;
    que->deq_rx.data = calloc(que->deq_rx.min_runs * que->deq_rx.result_dimensions,
                       sizeof(*que->deq_rx.data));

    que->deq_tx.mode = BENCH_MODE_FIXEDRUNS;
    que->deq_tx.result_dimensions = 1;
    que->deq_tx.min_runs = BENCH_SIZE;
    que->deq_tx.data = calloc(que->deq_tx.min_runs * que->deq_tx.result_dimensions,
                       sizeof(*que->deq_tx.data));
#endif

    return CLEANQ_ERR_OK;
}

errval_t ip_destroy(struct ip_q* q)
{
    // TODO destroy q->q;
    free(q);    

    return CLEANQ_ERR_OK;
}

/*
struct bench_ctl* ip_get_benchmark_data(struct ip_q* q, uint8_t type)
{
    switch (type) {
#ifdef BENCH
        case 0:
            return &q->en_rx;
        case 1:
            return &q->en_tx;
        case 2:
            return &q->deq_rx;
        case 3:
            return &q->deq_tx;
#endif
        default:
            return NULL;
    }
    return NULL;
}
*/
