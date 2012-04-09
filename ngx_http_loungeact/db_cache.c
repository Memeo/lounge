//
//  db_cache.c
//  LoungeAct
//
//  Created by Casey Marshall on 4/7/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stddef.h>
#include <db_cache.h>

#define __container_of(ptr_, type_, member_)  \
    ((type_ *)((char *)ptr_ - offsetof(type_, member_)))

static void cache_decref(db_cache_entry_t *entry)
{
    if (--entry->refcount == 0)
    {
        la_db_close(entry->db);
        free(entry->name);
        free(entry);
    }
}

static void cache_incref(db_cache_entry_t *entry)
{
    entry->refcount++;
}

void db_cache_init(db_cache_t *cache, int maxsize)
{
    pthread_mutex_init(&cache->mutex, PTHREAD_MUTEX_NORMAL);
    cache->maxsize = maxsize;
    cache->head = NULL;
}

void db_cache_destroy(db_cache_t *cache)
{
    
}

la_db_open_result_t db_cache_open(db_cache_t *cache, la_host_t *host, const char *name, int flags, la_db_t **db)
{
    pthread_mutex_lock(&cache->mutex);
    db_cache_entry_t *found;
    HASH_FIND_STR(cache->head, name, found);
    if (found == NULL)
    {
        found = (db_cache_entry_t *) malloc(sizeof(db_cache_entry_t));
        if (found == NULL)
        {
            pthread_mutex_unlock(&cache->mutex);
            return LA_DB_OPEN_ERROR;
        }
        found->name = strdup(name);
        la_db_open_result_t result = la_db_open(host, name, flags, &found->db);
        if (result != LA_DB_OPEN_OK && result != LA_DB_OPEN_CREATED)
        {
            free(found->name);
            free(found);
            pthread_mutex_unlock(&cache->mutex);
            return result;
        }
        HASH_ADD_STR(cache->head, name, found);
        cache_incref(found);
        if (HASH_COUNT(cache->head) > cache->maxsize)
        {
            db_cache_entry_t *e, *tmp;
            HASH_ITER(hh, cache->head, e, tmp)
            {
                HASH_DELETE(hh, cache->head, e);
                cache_decref(e);
                break;
            }
        }
        pthread_mutex_unlock(&cache->mutex);
        *db = found->db;
        return result;
    }
    
    HASH_DELETE(hh, cache->head, found);
    HASH_ADD_STR(cache->head, name, found);
    
    pthread_mutex_unlock(&cache->mutex);
    if ((flags & (LA_DB_OPEN_FLAG_CREATE|LA_DB_OPEN_FLAG_EXCL)) == (LA_DB_OPEN_FLAG_CREATE|LA_DB_OPEN_FLAG_EXCL))
        return LA_DB_OPEN_EXISTS;
    
    cache_incref(found);
    *db = found->db;
    return LA_DB_OPEN_OK;
}

void db_cache_close(db_cache_t *cache, la_db_t *db)
{
    db_cache_entry_t *entry = __container_of(db, db_cache_entry_t, db);
    cache_decref(entry);
}
