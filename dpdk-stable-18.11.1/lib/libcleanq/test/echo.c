/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2011, 2012, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <cleanq.h>
#include <backends/ipcq.h>
#include <bench.h>
#include <sched.h>


#define BENCH
//#define DEBUG(x...) printf("devif_test: " x)
#define DEBUG(x...) do {} while (0)

#define BUF_SIZE 2048
#define NUM_BUFS 128
#define MEMORY_SIZE BUF_SIZE*NUM_BUFS

static struct ipcq* ipc_queue;
static struct cleanq* que;


static errval_t ipcq_register(struct ipcq *q, struct capref cap,
                              regionid_t region_id)
{
    return SYS_ERR_OK;
}

static errval_t ipcq_deregister(struct ipcq *q, regionid_t region_id)
{
    return SYS_ERR_OK;
}


int main(int argc, char *argv[])
{
    errval_t err;


    printf("IPC echo queue started\n");
    struct ipcq_func_pointer func = {
        .reg = ipcq_register,
        .dereg = ipcq_deregister
    };

    err = ipcq_create(&ipc_queue, (char*) "recv", (char*) "send", true, &func);
    assert(err_is_ok(err));

    que = (struct cleanq*) ipc_queue;

    regionid_t regid_ret;
    genoffset_t offset, length, valid_data, valid_length;
    uint64_t flags;
    printf("Starting echo\n");
    while (true) {
        err = cleanq_dequeue(que, &regid_ret, &offset, &length,
                             &valid_data, &valid_length, &flags);
        if (err_is_fail(err)){
            if (err == CLEANQ_ERR_QUEUE_EMPTY) {
                continue;
            } else {
                printf("Dequeue error %d\n", err);
                exit(1);
            }
        } else {
            err = cleanq_enqueue(que, regid_ret, offset, length, 
                                 valid_data, valid_length, flags);
            if (err_is_fail(err)){
                if (err == CLEANQ_ERR_QUEUE_FULL) {
                    continue;
                } else {
                    printf("Enqueue error %d\n", err);
                    exit(1);
                }
            }
        }
    }
}

