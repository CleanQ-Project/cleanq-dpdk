/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdlib.h>
#include <cleanq.h>
#include <cleanq_module.h>
#include <backends/ipcq.h>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "ipcq_debug.h"

#define CMD_REG 1
#define CMD_DEREG 2

struct __attribute__((aligned(IPCQ_ALIGNMENT))) desc {
    genoffset_t offset; // 8
    genoffset_t length; // 16
    genoffset_t valid_data; // 24
    genoffset_t valid_length; // 32
    uint64_t flags; // 40
    uint64_t seq; // 48
    uint64_t cmd; // 56
    regionid_t rid; // 60
    uint8_t pad[4];
};

union __attribute__((aligned(IPCQ_ALIGNMENT))) pointer {
    volatile size_t value;
    uint8_t pad[64];
};


struct ipcq {
    struct cleanq q;
    struct ipcq_func_pointer f;

    // to get endpoints
    struct ipcq_endpoint_state* state;

    // General info
    size_t slots;
    char* name;
    bool bound_done;
 
    // Descriptor Ring
    struct desc* rx_descs;
    struct desc* tx_descs;
    
    // Flow control
    uint64_t rx_seq;
    uint64_t tx_seq;
    union pointer* rx_seq_ack;
    union pointer* tx_seq_ack;
  
    // Flounder
    struct ipcq_binding* binding;
    bool local_bind;
    uint64_t resend_args;
  
    // linked list
    struct ipcq* next;
    uint64_t qid;  
};

struct ipcq_endpoint_state {
    char* name;
    struct ipcq_func_pointer f;
    struct ipcq* head;
    struct ipcq* tail;
    uint64_t qid;
};

// Check if there's anything to read from the queue
static bool ipcq_can_read(void *arg)
{
    struct ipcq *q = (struct ipcq*) arg;
    uint64_t seq = q->rx_descs[q->rx_seq % q->slots].seq;

    if (q->rx_seq > seq) { // the queue is empty
        return false;
    }
    return true;
}

// Check if we can write to the queue
static bool ipcq_can_write(void *arg)
{
    struct ipcq *q = (struct ipcq*) arg;

    if ((q->tx_seq - q->tx_seq_ack->value) >= q->slots) { // the queue is full
        return false;
    }
    return true;
}

static inline errval_t ipcq_enqueue_internal(struct ipcq* queue,
                                             regionid_t region_id,
                                             genoffset_t offset,
                                             genoffset_t length,
                                             genoffset_t valid_data,
                                             genoffset_t valid_length,
                                             uint64_t misc_flags,
                                             uint64_t cmd)
{
    struct ipcq* q = (struct ipcq*) queue;
    size_t head = q->tx_seq % q->slots;

    if (!ipcq_can_write(queue)) {
        return CLEANQ_ERR_QUEUE_FULL;
    }

    q->tx_descs[head].rid = region_id;
    q->tx_descs[head].offset = offset;
    q->tx_descs[head].length = length;
    q->tx_descs[head].valid_data = valid_data;
    q->tx_descs[head].valid_length = valid_length;
    q->tx_descs[head].flags = misc_flags;
    q->tx_descs[head].cmd = cmd;

    //__sync_synchronize();

    q->tx_descs[head].seq = q->tx_seq;

    // only write local head
    q->tx_seq++;

    IPCQ_DEBUG("tx_seq=%lu tx_seq_ack=%lu rx_seq_ack=%lu \n", q->tx_seq, 
               q->tx_seq_ack->value, q->rx_seq_ack->value);

    return SYS_ERR_OK;

}
/**
 * @brief Enqueue a descriptor (as seperate fields)
 *        into the descriptor queue
 *
 * @param q                     The descriptor queue
 * @param region_id             Region id of the enqueued buffer
 * @param offset                Offset into the region where the buffer resides
 * @param length                Length of the buffer
 * @param valid_data            Offset into the region where the valid data
 *                              of the buffer resides
 * @param valid_length          Length of the valid data of the buffer
 * @param misc_flags            Miscellaneous flags
 *
 * @returns error if queue is full or SYS_ERR_OK on success
 */
static errval_t ipcq_enqueue(struct cleanq* queue,
                              regionid_t region_id,
                              genoffset_t offset,
                              genoffset_t length,
                              genoffset_t valid_data,
                              genoffset_t valid_length,
                              uint64_t misc_flags)
{
    return ipcq_enqueue_internal((struct ipcq*) queue, region_id, offset, length, 
                                 valid_data, valid_length, misc_flags, 0);
}


static errval_t ipc_reg(struct ipcq* q, struct capref cap, uint32_t rid)
{
    errval_t err;
    err = cleanq_add_region((struct cleanq*) q, cap, rid);
    if (err_is_fail(err)) {        
        // should not happen, but is fine!
        return SYS_ERR_OK;
    }

    err = q->f.reg(q, cap, rid);
    return SYS_ERR_OK;
}

static errval_t ipc_dereg(struct ipcq* q, regionid_t rid)
{
    errval_t err;
    err = cleanq_remove_region((struct cleanq*) q, rid);
    if (err_is_fail(err)) { 
        // should not happen, but is fine!
        return SYS_ERR_OK;
    }

    err = q->f.dereg(q, rid);
    return SYS_ERR_OK;
}

/**
 * @brief Dequeue a descriptor (as seperate fields)
 *        from the descriptor queue
 *
 * @param q                     The descriptor queue
 * @param region_id             Return pointer to the region id of
 *                              the denqueued buffer
 * @param offset                Return pointer to the offset into the region
 *                              where the buffer resides
 * @param length                Return pointer to the length of the buffer
 * @param valid_data            Return pointer to the offset into the region
 *                              where the valid data of the buffer resides
 * @param valid_lenght          Return pointer to the length of the valid
 *                              data of the buffer
 * @param misc_flags            Return pointer to miscellaneous flags
 *
 * @returns error if queue is empty or SYS_ERR_OK on success
 */
static struct capref cap;
static errval_t ipcq_dequeue(struct cleanq* queue,
                              regionid_t* region_id,
                              genoffset_t* offset,
                              genoffset_t* length,
                              genoffset_t* valid_data,
                              genoffset_t* valid_length,
                              uint64_t* misc_flags)
{
    struct ipcq* q = (struct ipcq*) queue;
    errval_t err;

    if (!ipcq_can_read(queue)) {
        return CLEANQ_ERR_QUEUE_EMPTY;
    }

    size_t tail = q->rx_seq % q->slots;
    if (q->rx_descs[tail].cmd != 0) {

        *region_id = q->rx_descs[tail].rid;

        // TODO
        // handle special message reg/dereg! 
        if (q->rx_descs[tail].cmd == CMD_REG) {
            cap.len = q->rx_descs[tail].length;
            cap.vaddr = (void*) q->rx_descs[tail].offset;
            cap.paddr = (uint64_t) q->rx_descs[tail].offset;
            err = ipc_reg((struct ipcq*) queue, cap, *region_id);
        } else {
            err = ipc_dereg((struct ipcq*) queue, *region_id);
        }
        q->rx_seq++;
        q->rx_seq_ack->value = q->rx_seq;

        IPCQ_DEBUG("rx_seq_ack=%lu tx_seq_ack=%lu reg/dereg\n", 
                   q->rx_seq_ack->value, q->tx_seq_ack->value);
        // try dequeing again
        err = ipcq_dequeue(queue, region_id, offset,
                           length, valid_data, valid_length, misc_flags);
        if (err_is_fail(err)) {
            return err;
        }
    
    } else {
        *region_id = q->rx_descs[tail].rid;
        *offset = q->rx_descs[tail].offset;
        *length = q->rx_descs[tail].length;
        *valid_data = q->rx_descs[tail].valid_data;
        *valid_length = q->rx_descs[tail].valid_length;
        *misc_flags = q->rx_descs[tail].flags;

        q->rx_seq++;
        q->rx_seq_ack->value = q->rx_seq;
    }
    IPCQ_DEBUG("rx_seq_ack=%lu tx_seq_ack=%lu \n", q->rx_seq_ack->value,
               q->tx_seq_ack->value);
    return SYS_ERR_OK;
}

static errval_t ipcq_register(struct cleanq* q, struct capref cap,
                              regionid_t rid)
{
    struct ipcq* queue = (struct ipcq*) q;

    while (!ipcq_can_write(queue)) {}

    return ipcq_enqueue_internal(queue, rid, (genoffset_t) cap.vaddr, 
                                 (genoffset_t) cap.len, 0, 0, 0, CMD_REG);
}



/**
 * @brief Destroys a descriptor queue and frees its resources
 *
 * @param que                     The descriptor queue
 *
 * @returns error on failure or SYS_ERR_OK on success
 */
errval_t ipcq_destroy(struct cleanq* que)
{
    struct ipcq* q = (struct ipcq*) que;

    munmap(q->tx_descs, IPCQ_MEM_SIZE);
    munmap(q->rx_descs, IPCQ_MEM_SIZE);
    free(q->name);
    free(q);

    return SYS_ERR_OK;
}

static errval_t ipcq_deregister(struct cleanq* q, regionid_t rid)
{
    struct ipcq* queue = (struct ipcq*) q;
    while (!ipcq_can_write(queue)) {}

    return ipcq_enqueue_internal(queue, rid, 0, 0, 0, 0, 0, 
                                 CMD_DEREG);
}

errval_t ipcq_create(struct ipcq** q,
                     char* name_send,
                     char* name_recv,
                     bool clear,
                     struct ipcq_func_pointer* f)
{
    IPCQ_DEBUG("create start\n");
    errval_t err;
    struct ipcq* tmp;

    // Init basic struct fields
    tmp = (struct ipcq*) calloc(sizeof(struct ipcq), 1);
    assert(tmp != NULL);
    
    int fd_send = shm_open(name_send, O_RDWR | O_CREAT, 0777);
    int fd_recv = shm_open(name_recv, O_RDWR | O_CREAT, 0777);
    assert((fd_send != 0) && (fd_recv != 0));

    ftruncate(fd_send, IPCQ_MEM_SIZE);
    ftruncate(fd_recv, IPCQ_MEM_SIZE);

    tmp->f.dereg = f->dereg;
    tmp->f.reg = f->reg;

    IPCQ_DEBUG("Mapping TX frame\n");
    tmp->tx_descs = (struct desc*) mmap(NULL, 
                    IPCQ_MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, 
                    fd_send, 0);
    if (tmp->tx_descs == NULL) {
        err = LIB_ERR_MALLOC_FAIL;
        goto cleanup1;
    }

    IPCQ_DEBUG("Mapping RX frame\n");
    tmp->rx_descs = (struct desc*) mmap(NULL, 
                    IPCQ_MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, 
                    fd_recv, 0);
    if (tmp->rx_descs == NULL) {
        err = LIB_ERR_MALLOC_FAIL;
        goto cleanup2;
    }

    if (clear) {
        memset(tmp->rx_descs, 0, IPCQ_MEM_SIZE);
        memset(tmp->tx_descs, 0, IPCQ_MEM_SIZE);
    }

    IPCQ_DEBUG("INIT TX/RX queue done %p %p \n", tmp->tx_descs, tmp->rx_descs);
    tmp->tx_seq_ack = (union pointer*) tmp->tx_descs;
    tmp->rx_seq_ack = (union pointer*) tmp->rx_descs;
    tmp->tx_seq_ack->value = 0;
    tmp->rx_seq_ack->value = 0;
    tmp->tx_descs++;
    tmp->rx_descs++;
    tmp->slots = IPCQ_DEFAULT_SIZE-1;
    tmp->rx_seq = 1;
    tmp->tx_seq = 1;

    cleanq_init(&tmp->q, false);

    tmp->q.f.enq = ipcq_enqueue;
    tmp->q.f.deq = ipcq_dequeue;
    tmp->q.f.reg = ipcq_register;
    tmp->q.f.dereg = ipcq_deregister;

    *q = tmp;

    IPCQ_DEBUG("create end %p \n", *q);
    return SYS_ERR_OK;

cleanup2:
    munmap(tmp->rx_descs, IPCQ_MEM_SIZE);
cleanup1:
    munmap(tmp->tx_descs, IPCQ_MEM_SIZE);

    return err;

}

