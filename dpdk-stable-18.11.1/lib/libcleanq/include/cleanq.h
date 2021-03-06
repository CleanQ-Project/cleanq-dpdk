/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */
#ifndef QUEUE_INTERFACE_H_
#define QUEUE_INTERFACE_H_ 1

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define CLEANQ_FLAG_LAST (1UL << 30)

typedef uint32_t regionid_t;
typedef uint32_t bufferid_t;
typedef uint64_t genoffset_t;

struct cleanq;
struct region_pool;

struct capref {
    void* vaddr;
    uint64_t paddr;   
    size_t len;
};

// For convinience reason buffer descritpion in one struct
struct cleanq_buf{
    genoffset_t offset; // 8
    genoffset_t length; // 16
    genoffset_t valid_data; // 24
    genoffset_t valid_length; // 32
    uint64_t flags; // 40
    regionid_t rid; // 44
};

typedef enum {
    CLEANQ_ERR_OK = 0,
    CLEANQ_ERR_INIT_QUEUE,
    CLEANQ_ERR_BUFFER_ID,
    CLEANQ_ERR_BUFFER_NOT_IN_REGION,
    CLEANQ_ERR_BUFFER_ALREADY_IN_USE,
    CLEANQ_ERR_INVALID_BUFFER_ARGS,
    CLEANQ_ERR_INVALID_REGION_ID,
    CLEANQ_ERR_REGION_DESTROY,
    CLEANQ_ERR_INVALID_REGION_ARGS,
    CLEANQ_ERR_QUEUE_EMPTY,
    CLEANQ_ERR_QUEUE_FULL,
    CLEANQ_ERR_BUFFER_NOT_IN_USE,
    CLEANQ_ERR_MALLOC_FAIL,
    CLEANQ_ERR_UNKNOWN_FLAG,
    CLEANQ_ERR_UDP_WRONG_PORT,
    CLEANQ_ERR_IP_WRONG_IP,
    CLEANQ_ERR_IP_WRONG_PROTO,
    CLEANQ_ERR_IP_CHKSUM
} errval_t;


inline int err_is_ok(errval_t err) 
{
    return err == CLEANQ_ERR_OK;
}

inline int err_is_fail(errval_t err) 
{
    return err != CLEANQ_ERR_OK;
}

/*
 * ===========================================================================
 * Datapath functions
 * ===========================================================================
 */
/*
 *
 * @brief enqueue a buffer into the device queue
 *
 * @param q             The device queue to call the operation on
 * @param region_id     Id of the memory region the buffer belongs to
 * @param offset        Offset into the region i.e. where the buffer starts
 *                      that is enqueued
 * @param lenght        Lenght of the enqueued buffer
 * @param valid_data    Offset into the buffer where the valid data of this buffer
 *                      starts
 * @param valid_length  Length of the valid data of this buffer
 * @param misc_flags    Any other argument that makes sense to the device queue
 *
 * @returns error on failure or SYS_ERR_OK on success
 *
 */
errval_t cleanq_enqueue(struct cleanq *q,
                      regionid_t region_id,
                      genoffset_t offset,
                      genoffset_t length,
                      genoffset_t valid_data,
                      genoffset_t valid_length,
                      uint64_t misc_flags);

/**
 * @brief dequeue a buffer from the device queue
 *
 * @param q             The device queue to call the operation on
 * @param region_id     Return pointer to the id of the memory
 *                      region the buffer belongs to
 * @param region_offset Return pointer to the offset into the region where
 *                      this buffer starts.
 * @param lenght        Return pointer to the lenght of the dequeue buffer
 * @param valid_data    Return pointer to an offset into the buffer where the
 *                      valid data of this buffer starts
 * @param valid_length  Return pointer to the length of the valid data of
 *                      this buffer
 * @param misc_flags    Return value from other endpoint
 *
 * @returns error on failure or SYS_ERR_OK on success
 *
 */
errval_t cleanq_dequeue(struct cleanq *q,
                      regionid_t* region_id,
                      genoffset_t* offset,
                      genoffset_t* length,
                      genoffset_t* valid_data,
                      genoffset_t* valid_length,
                      uint64_t* misc_flags);

/*
 * ===========================================================================
 * Control Path
 * ===========================================================================
 */

/**
 * @brief Add a memory region that can be used as buffers to
 *        the device queue
 *
 * @param q              The device queue to call the operation on
 * @param cap            A Capability for some memory
 * @param region_id      Return pointer to a region id that is assigned
 *                       to the memory
 *
 * @returns error on failure or SYS_ERR_OK on success
 *
 */
errval_t cleanq_register(struct cleanq *q,
                       struct capref cap,
                       regionid_t* region_id);

/**
 * @brief Remove a memory region
 *
 * @param q              The device queue to call the operation on
 * @param region_id      The region id to remove from the device
 *                       queues memory
 * @param cap            The capability to the removed memory
 *
 * @returns error on failure or SYS_ERR_OK on success
 *
 */
errval_t cleanq_deregister(struct cleanq *q,
                         regionid_t region_id,
                         struct capref* cap);

/**
 * @brief Send a notification about new buffers on the queue
 *
 * @param q      The device queue to call the operation on
 *
 * @returns error on failure or SYS_ERR_OK on success
 *
 */
errval_t cleanq_notify(struct cleanq *q);

/**
 * @brief Send a control message to the device queue
 *
 * @param q          The device queue to call the operation on
 * @param request    The type of the control message*
 * @param value      The value for the request
 *
 * @returns error on failure or SYS_ERR_OK on success
 *
 */
errval_t cleanq_control(struct cleanq *q,
                      uint64_t request,
                      uint64_t value,
                      uint64_t *result);


 /**
  * @brief destroys the device queue
  *
  * @param q           The queue state to free (and the device queue to be 
                       shut down)
  *
  * @returns error on failure or SYS_ERR_OK on success
  */
errval_t cleanq_destroy(struct cleanq *q);

void cleanq_set_state(struct cleanq *q, void *state);
void * cleanq_get_state(struct cleanq *q);


#endif /* QUEUE_INTERFACE_H_ */
