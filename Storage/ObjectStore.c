//
//  ObjectStore-common.c
//  LoungeAct
//
//  Created by Casey Marshall on 2/23/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../utils/stringutils.h"
#include "../utils/utils.h"
#include "../utils/hexdump.h"
#include "../utils/uthash.h"

#include "ObjectStore.h"

struct driver_entry
{
    const char *name;
    const la_object_store_driver_t *driver;
    UT_hash_handle hh;
};

static struct driver_entry *drivers = NULL;

struct la_object_store
{
    la_storage_env *env;
    la_storage_object_store *store;
    const la_object_store_driver_t *driver;
};

int la_storage_install_driver(const char *name, const la_object_store_driver_t *driver)
{
    struct driver_entry *e = NULL;
    HASH_FIND_STR(drivers, name, e);
    if (e != NULL)
    {
        errno = EEXIST;
        return -1;
    }
    e = malloc(sizeof(struct driver_entry));
    e->name = name;
    e->driver = driver;
    HASH_ADD_STR(drivers, name, e);
    return 0;
}

la_storage_object *
la_storage_create_object(const char *key, const la_storage_rev_t rev, const unsigned char *data, uint32_t length,
                         const la_storage_rev_t *revs, size_t revcount)
{
    size_t total_len = 0;
    la_storage_object *obj = (la_storage_object *) malloc(sizeof(struct la_storage_object));
    if (obj == NULL)
        return NULL;
    obj->key = strdup(key);
    if (obj->key == NULL)
    {
        free(obj);
        return NULL;
    }
    total_len = sizeof(struct la_storage_object_header) + length + (revcount * sizeof(la_storage_rev_t));
    obj->header = malloc(total_len);
    if (obj->header == NULL)
    {
        free(obj->key);
        free(obj);
        return NULL;
    }
    memset(obj->header, 0, total_len);
    memcpy(&obj->header->rev, &rev, LA_OBJECT_REVISION_LEN);
    if (revs != NULL)
    {
        obj->header->rev_count = revcount;
        memcpy(obj->header->revs_data, revs, revcount * sizeof(la_storage_rev_t));
    }
    else
    {
        obj->header->rev_count = 0;
    }
    memcpy(la_storage_object_get_data(obj), (const char *) data, length);
    obj->data_length = length;
#if DEBUG
    printf("total_len: %zu (header:%lu, length: %d, revs size: %lu)\n", total_len, sizeof(struct la_storage_object_header),
           length, revcount * sizeof(la_storage_rev_t));
    printf("created object (%d revs, %d bytes):\n", obj->header->rev_count, obj->data_length);
    la_hexdump(obj->header, la_storage_object_total_size(obj));
#endif
    return (la_storage_object *) obj;
}

void
la_storage_destroy_object(la_storage_object *object)
{
    free(object->key);
    free(object->header);
    free(object);
}

int
la_storage_object_get_all_revs(const la_storage_object *object, la_storage_rev_t **revs)
{
    if (revs != NULL)
        *revs = &object->header->rev;
    return object->header->rev_count + 1;
}

int
la_storage_scan_rev(const char *str, la_storage_rev_t *rev)
{
    return string_unhex(str, rev->rev, LA_OBJECT_REVISION_LEN);
}

int la_storage_revs_overlap(const la_storage_rev_t *revs1, int count1, const la_storage_rev_t *revs2, int count2)
{
    int i;
    
    for (i = 0; i < count2 - 1; i++)
    {
        if (memcmp(revs1, &revs2[i], sizeof(la_storage_rev_t) * la_min(count1, count2 - i)) == 0)
            return 1;
    }
    
    return 0;
}

la_storage_env *la_storage_open_env(const char *driver, const char *name)
{
    struct driver_entry *entry = NULL;
    HASH_FIND_STR(drivers, driver, entry);
    if (entry == NULL)
        return NULL;
    return entry->driver->open_env(name);
}

void la_storage_close_env(const char *driver, la_storage_env *env)
{
    struct driver_entry *entry = NULL;
    HASH_FIND_STR(drivers, driver, entry);
    if (entry == NULL)
        return;
    entry->driver->close_env(env);
}

la_storage_open_result_t la_storage_open(const char *driver, la_storage_env *env, const char *name, int flags, la_object_store_t **store)
{
    struct driver_entry *entry = NULL;
    
    HASH_FIND_STR(drivers, name, entry);
    if (entry == NULL)
    {
        return LA_STORAGE_OPEN_NO_DRIVER;
    }
    
    la_storage_object_store *objstore = NULL;
    la_storage_open_result_t storeopen = entry->driver->open_store(env, name, flags, &objstore);
    if (storeopen != LA_STORAGE_OPEN_OK)
    {
        if (env != NULL)
        {
            entry->driver->close_env(env);
            return LA_STORAGE_OPEN_ERROR;
        }
        return storeopen;
    }
    
    *store = malloc(sizeof(struct la_object_store));
    if (*store == NULL)
    {
        entry->driver->close_store(objstore);
        if (env != NULL)
        {
            entry->driver->close_env(env);
            return LA_STORAGE_OPEN_ERROR;
        }
        return LA_STORAGE_OPEN_ERROR;
    }
    
    (*store)->env = env;
    (*store)->store = objstore;
    (*store)->driver = entry->driver;
    
    return LA_STORAGE_OPEN_OK;
}

void la_storage_close(la_object_store_t *store)
{
    if (store->store != NULL)
    {
        store->driver->close_store(store->store);
        store->store = NULL;
    }
    if (store->env != NULL)
    {
        store->driver->close_env(store->env);
        store->env = NULL;
    }
    free(store);
}

void la_storage_delete_store(la_object_store_t *store)
{
    if (store->driver->delete_store(store->store) == 0)
    {
        la_storage_close(store);
    }
}

la_storage_object_get_result la_storage_get(la_object_store_t *store, const char *key,
                                            const la_storage_rev_t *rev, la_storage_object **obj)
{
    return store->driver->get(store->store, key, rev, obj);
}

la_storage_object_get_result la_storage_get_rev(la_object_store_t *store, const char *key, la_storage_rev_t *rev)
{
    return store->driver->get_rev(store->store, key, rev);
}

int la_storage_get_all_revs(la_object_store_t *store, const char *key, uint64_t *start, la_storage_rev_t **revs)
{
    return store->driver->get_all_revs(store->store, key, start, revs);
}

la_storage_object_put_result la_storage_set_revs(la_object_store_t *store, const char *key, la_storage_rev_t *revs, size_t revcount)
{
    return store->driver->set_revs(store->store, key, revs, revcount);
}

la_storage_object_put_result la_storage_put(la_object_store_t *store, const la_storage_rev_t *rev, la_storage_object *obj)
{
    return store->driver->put(store->store, rev, obj);
}

la_storage_object_put_result la_storage_replace(la_object_store_t *store, la_storage_object *obj)
{
    return store->driver->replace(store->store, obj);
}

uint64_t la_storage_lastseq(la_object_store_t *store)
{
    return store->driver->lastseq(store->store);
}

int la_storage_stat(la_object_store_t *store, la_storage_stat_t *stat)
{
    return store->driver->stat(store->store, stat);
}

la_storage_object_iterator *la_storage_iterator_open(la_object_store_t *store, uint64_t since)
{
    return store->driver->iterator_open(store->store, since);
}

la_storage_object_iterator_result la_storage_iterator_next(la_object_store_t *store, la_storage_object_iterator *it, la_storage_object **obj)
{
    return store->driver->iterator_next(it, obj);
}

void la_storage_iterator_close(la_object_store_t *store, la_storage_object_iterator *it)
{
    store->driver->iterator_close(it);
}
