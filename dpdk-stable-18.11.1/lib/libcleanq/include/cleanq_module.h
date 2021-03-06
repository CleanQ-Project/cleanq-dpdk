/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */
#ifndef QUEUE_INTERFACE_BACKEND_H_
#define QUEUE_INTERFACE_BACKEND_H_ 1

#include <cleanq.h>
struct region_pool;
struct cleanq;

/*
 * ===========================================================================
 * Backend function definitions
 * ===========================================================================
 */
// Creation of queues is device specific

 /**
  * @brief Notifies the device of new descriptors in the queue.
  *        On a notificaton, the device can dequeue descritpors
  *        from the queue. NOTE: Does nothing for direct queues since there
  *        is no other endpoint to notify! (i.e. it is the same process)
  *
  * @param q         The device queue
  *
  * @returns error on failure or SYS_ERR_OK on success
  */
typedef errval_t (*cleanq_notify_t) (struct cleanq *q);

 /**
  * @brief Registers a memory region. For direct queues this function
  *        Has to handle the communication to the device driver since
  *        there might also be a need to set up some local state for the
  *        direct queue that is device specific
  *
  * @param q         The device queue handle
  * @param cap       The capability of the memory region
  * @param reigon_id The region id
  *
  * @returns error on failure or SYS_ERR_OK on success
  */
typedef errval_t (*cleanq_register_t)(struct cleanq *q, struct capref cap,
                                    regionid_t region_id);

 /**
  * @brief Deregisters a memory region. (Similar communication constraints
  *        as register)
  *
  * @param q         The device queue handle
  * @param reigon_id The region id
  *
  * @returns error on failure or SYS_ERR_OK on success
  */
typedef errval_t (*cleanq_deregister_t)(struct cleanq *q, regionid_t region_id);

 /**
  * @brief handles a control message to the device (Similar communication
  *        constraints as register)
  *
  * @param q         The device queue handle
  * @param request   The request type
  * @param value     The value to the request
  *
  * @returns error on failure or SYS_ERR_OK on success
  */
typedef errval_t (*cleanq_control_t)(struct cleanq *q,
                                   uint64_t request,
                                   uint64_t value,
                                   uint64_t *result);


 /**
  * @brief Directly enqueues something into a hardware queue. Only used by
  *        direct endpoints
  *
  * @param q            The device queue handle
  * @param region_id    The region id of the buffer
  * @param offset       Offset into the region where the buffer starts
  * @param length       Length of the buffer
  * @param valid_data   Offset into the region where the valid data of the
  *                     buffer starts
  * @param valid_length Length of the valid data in this buffer
  * @param misc_flags   Misc flags
  *
  * @returns error on failure or SYS_ERR_OK on success
  */
typedef errval_t (*cleanq_enqueue_t)(struct cleanq *q, regionid_t region_id,
                                   genoffset_t offset, genoffset_t length,
                                   genoffset_t valid_offset,
                                   genoffset_t valid_length,
                                   uint64_t misc_flags);

 /**
  * @brief Directly dequeus something from a hardware queue. Only used by
  *        direct endpoints
  *
  * @param q            The device queue handle
  * @param region_id    The region id of the buffer
  * @param offset       Return pointer to the offset into the region
  *                     where the buffer starts
  * @param length       Return pointer to the length of the buffer
  * @param valid_offset Return pointer to the offset into the region
  *                     where the valid data of the buffer starts
  * @param valid_length Return pointer to the length of the valid data of
  *                     this buffer
  * @param misc_flags   Misc flags
  *
  * @returns error on failure if the queue is empty or SYS_ERR_OK on success
  */
typedef errval_t (*cleanq_dequeue_t)(struct cleanq *q, regionid_t* region_id,
                                   genoffset_t* offset, genoffset_t* length,
                                   genoffset_t* valid_offset,
                                   genoffset_t* valid_length,
                                   uint64_t* misc_flags);

 /**
  * @brief Destroys the queue give as an argument, first the state of the 
  *        library, then the queue specific part by calling a function pointer
  *
  * @param q         The device queue
  *
  * @returns error on failure or SYS_ERR_OK on success
  */
typedef errval_t (*cleanq_destroy_t) (struct cleanq *q);


// The functions that the backend has to set
struct cleanq_func_pointer {
    cleanq_register_t reg;
    cleanq_deregister_t dereg;
    cleanq_control_t ctrl;
    cleanq_notify_t notify;
    cleanq_enqueue_t enq;
    cleanq_dequeue_t deq;
    cleanq_destroy_t destroy;
};

struct cleanq {
    // Region management
    struct region_pool* pool;

    // Funciton pointers
    struct cleanq_func_pointer f;

    void *state;
};

errval_t cleanq_init(struct cleanq *q);

errval_t cleanq_add_region(struct cleanq*, struct capref cap,
                         regionid_t rid);

errval_t cleanq_remove_region(struct cleanq*, regionid_t rid);

#endif /* QUEUE_INTERFACE_BACKEND_H_ */
