//
//  push.h
//  LoungeAct
//
//  Created by Casey Marshall on 7/6/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#ifndef LoungeAct_push_h
#define LoungeAct_push_h

typedef struct la_push la_push_t;

typedef struct la_push_params
{
    const char *host;
    unsigned short port;
    
} la_push_params_t;

la_push_t *la_push_create(la_db_t *db, la_push_params_t *params);
int la_push_run(la_push_t *);


#endif
