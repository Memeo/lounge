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

/**
 * Callback function for doing on-line conflict resolution.
 *
 * @param key The key of the conflicting document.
 * @param mine The version of the document from the local database.
 * @param theirs The version of the document from the remote database.
 * @param baton User-specified pointer.
 */
typedef la_codec_value_t *(*la_pull_conflict_resolver)(const char *key, const la_codec_value_t *mine, const la_codec_value_t *theirs, void *baton);

/**
 * Conflict resolver that always takes the local revision.
 * This is the default resolver if none is specified.
 */
extern la_pull_conflict_resolver la_pull_conflict_resolver_mine;

/**
 * Conflict resolver that always takes the remote revision.
 */
extern la_pull_conflict_resolver la_pull_conflict_resolver_theirs;

typedef struct la_pull_params
{
    const char *host;
    unsigned short port;
    const char *dbname;
    const char *user;
    const char *password;
    const char *filter;
    la_pull_option_t options;
    la_pull_conflict_resolver resolver;
    void *baton;
} la_pull_params_t;

la_pull_t *la_pull_create(la_db_t *db, la_pull_params_t *params);
int la_pull_run(la_pull_t *pull);
void la_pull_destroy(la_pull_t *pull);

#endif
