//
//  db_cache.h
//  LoungeAct
//
//  Created by Casey Marshall on 4/7/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#ifndef LoungeAct_db_cache_h
#define LoungeAct_db_cache_h

#include <pthread.h>
#include <api/LoungeAct.h>
#include <uthash.h>

typedef struct db_cache_entry_s
{
    char *name;
    la_db_t *db;
    int refcount;
    UT_hash_handle hh;
} db_cache_entry_t;

typedef struct
{
    pthread_mutex_t mutex;
    unsigned int maxsize;
    db_cache_entry_t *head;
} db_cache_t;

void db_cache_init(db_cache_t *cache, int maxsize);
void db_cache_destroy(db_cache_t *cache);
la_db_open_result_t db_cache_open(db_cache_t *cache, la_host_t *host, const char *name, int flags, la_db_t **db);
void db_cache_close(db_cache_t *cache, la_db_t *db);

#endif
