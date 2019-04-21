/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */
#ifndef IPCQ_H_
#define IPCQ_H_ 1

#include <cleanq.h>

#define IPCQ_DEFAULT_SIZE 64
#define IPCQ_ALIGNMENT 64
#define IPCQ_MEM_SIZE IPCQ_DEFAULT_SIZE*IPCQ_ALIGNMENT

struct ipcq;

typedef errval_t (*ipcq_register_t)(struct ipcq *q, struct capref cap,
                                    regionid_t region_id);
typedef errval_t (*ipcq_deregister_t)(struct ipcq *q, regionid_t region_id);

struct ipcq_func_pointer {
    ipcq_register_t reg;
    ipcq_deregister_t dereg;
};


/**
 * @brief initialized a descriptor queue
 *
 * @param q                     Return pointer to the descriptor queue
 * @param name_send             Name of the memory use for sending messages
 * @param name_recv             Name of the memory use for receiving messages
 * @param clear                 Write 0 to memory
 * @param f                     Function pointers to be called on message recv
 *
 * @returns error on failure or SYS_ERR_OK on success
 */

errval_t ipcq_create(struct ipcq** q,
                     char* name_send,
                     char* name_recv,
                     bool clear, 
                     struct ipcq_func_pointer* f);

errval_t icpq_destroy(struct ipcq* q);

#endif /* IPCQ_H_ */
