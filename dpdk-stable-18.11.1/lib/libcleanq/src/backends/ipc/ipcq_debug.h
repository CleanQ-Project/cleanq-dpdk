/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */
#ifndef IPCQ_DEBUG_H_
#define IPCQ_DEBUG_H_ 1

#include <stdio.h>
#include <sched.h>

//#define IPCQ_DEBUG_ENABLED 1

/*****************************************************************
 * Debug printer:
 *****************************************************************/

extern char* __progname;

#if defined(IPCQ_DEBUG_ENABLED) 
#define IPCQ_DEBUG(x...) do { printf("IPCQ:%s.%d:%s:%d: ", \
            __progname, sched_getcpu(), __func__, __LINE__); \
                printf(x);\
        } while (0)

#else
#define IPCQ_DEBUG(x...) ((void)0)
#endif 

#endif /* IPCQ_DEBUG_H_ */
