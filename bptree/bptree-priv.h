//
//  bptree-priv.h
//  LoungeAct
//
//  Created by Casey Marshall on 2/22/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#ifndef LoungeAct_bptree_priv_h
#define LoungeAct_bptree_priv_h

#include <stdint.h>

typedef union
{
    struct header
    {
        
    },
    char pad[4096]
} _BPTreeHeader;

typedef struct BPTree
{
    int fd; /** File descriptor. */
} BPTree;

#endif
