//
//  pull.h
//  LoungeAct
//
//  Created by Casey Marshall on 3/5/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#ifndef LoungeAct_pull_h
#define LoungeAct_pull_h

#include <api/LoungeAct.h>
#include <sys/socket.h>

typedef struct la_pull la_pull_t;

typedef enum
{
    // Whether to perform continuous replication, or just a one-off replication
    // now.
    LA_PULL_CONTINUOUS = (1<<0),
    
    // Whether to use HTTPS when connecting.
    LA_PULL_SECURE     = (1<<1),
    
    // Whether to actually use a continuous poll for database changes, or to
    // repeatedly perform a longpoll.
    LA_PULL_CONTINUOUS_WITH_LONGPOLL = (1<<2)
} la_pull_option_t;

typedef struct la_pull_params
{
    const char *host;
    unsigned short port;
    const char *dbname;
    const char *user;
    const char *password;
    const char *filter;
    la_pull_option_t options;
} la_pull_params_t;

la_pull_t *la_pull_create(la_db_t *db, la_pull_params_t *params);
int la_pull_start(la_pull_t *pull);
int la_pull_pause(la_pull_t *pull);
void la_pull_destroy(la_pull_t *pull);

#endif
