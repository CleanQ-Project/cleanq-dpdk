/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */
#include <stdbool.h>

#include <cleanq.h>
#include <cleanq_module.h>

#include "region_pool.h"

 /*
 * ===========================================================================
 * Device queue creation and destruction (General devif initalisation)
 * ===========================================================================
 */
 /**
  * @brief creates a queue 
  *
  * @param q             Return pointer to the cleanq (handle)
  * @param exp           If we keep track of no longer owned buffers
  *                      or buffers that we own         
  *
  * @returns error on failure or SYS_ERR_OK on success
  */

errval_t cleanq_init(struct cleanq *q)
{
    
    errval_t err;
    err = region_pool_init(&(q->pool));
    
    return err;
}

errval_t cleanq_add_region(struct cleanq* q, struct capref cap,
                         regionid_t rid)
{
    errval_t err;
    
    err = region_pool_add_region_with_id(q->pool, cap, rid);
    return err;
}

errval_t cleanq_remove_region(struct cleanq* q, regionid_t rid)
{
    errval_t err;
    struct capref cap; 
   
    err = region_pool_remove_region(q->pool, rid, &cap);
    return err;
}
