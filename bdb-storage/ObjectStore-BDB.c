//
//  File.c
//  LoungeAct
//
//  Created by Casey Marshall on 2/23/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <db.h>
#include "../Storage/ObjectStore.h"

#define min(a,b) ((a) < (b) ? (a) : (b))

#if DEBUG
#define debug(fmt,args...) fprintf(stderr, fmt, ##args)
#else
#define debug(fmt,args...)
#endif

static char *
keycpy(char *key, u_int32_t len)
{
    char *ret = (char *) malloc(len + 1);
    strncpy(ret, key, len);
    ret[len] = '\0';
    return ret;
}

struct ObjectHeader
{
    char rev[MAX_REVISION_LEN + 1];
};

struct LAStorageEnvironment
{
    DB_ENV *env;
};

struct LAStorageObjectStore
{
    LAStorageEnvironment *env;
    DB *db;
};

struct LAStorageObjectIterator
{
    LAStorageObjectStore *store;
    DBC *cursor;
};

const char *
la_storage_driver(void)
{
    return "BDB";
}

LAStorageEnvironment *la_storage_open_env(const char *name)
{
    int ret;
    struct stat st;
    struct LAStorageEnvironment *env = (LAStorageEnvironment *) malloc(sizeof(struct LAStorageEnvironment));
    if (env == NULL)
        return NULL;
    if (db_env_create(&env->env, 0) != 0)
    {
        free(env);
        return NULL;
    }
    ret = stat(name, &st);
    if (ret != 0)
    {
        if (errno == ENOENT)
        {
            debug("creating directory %s\n", name);
            if (mkdir(name, 0777) != 0)
            {
                free(env);
                return NULL;
            }
        }
        else
        {
            debug("failed to read directory %s: %s\n", name, strerror(errno));
            free(env);
            return NULL;
        }
    }
    else
    {
        if (!S_ISDIR(st.st_mode))
        {
            debug("%s: %s\n", name, strerror(ENOTDIR));
            free(env);
            return NULL;
        }
    }
    if (env->env->open(env->env, name, DB_CREATE | DB_INIT_LOG | DB_INIT_LOCK | DB_INIT_MPOOL | DB_THREAD | DB_INIT_TXN, 0) != 0)
    {
        free(env);
        return NULL;
    }
    return env;
}

void la_storage_close_env(LAStorageEnvironment *env)
{
    env->env->close(env->env, 0);
    free(env);
}

LAStorageObjectStore *la_storage_open(LAStorageEnvironment *env, const char *path)
{
    LAStorageObjectStore *store = (LAStorageObjectStore *) malloc(sizeof(struct LAStorageObjectStore));
    DB_TXN *txn;
    if (store == NULL)
        return NULL;
    store->env = env;
    if (env->env->txn_begin(env->env, NULL, &txn, DB_TXN_SNAPSHOT | DB_TXN_WRITE_NOSYNC) != 0)
    {
        free(store);
        return NULL;
    }
    if (db_create(&store->db, env->env, 0) != 0)
    {
        free(store);
        return NULL;
    }
    if (store->db->open(store->db, txn, path, NULL, DB_BTREE, DB_CREATE | DB_MULTIVERSION | DB_THREAD | DB_READ_UNCOMMITTED, 0) != 0)
    {
        txn->abort(txn);
        free(store);
        return NULL;
    }
    txn->commit(txn, 0);
    return store;
}

/**
 * Get an object from the store, placing it in the given pointer.
 *
 * @param store The object store handle.
 * @param key The key of the object to get (null-terminated).
 * @param rev The revision to match when getting the object (if null, get the latest rev).
 * @param obj The object to .
 */
LAStorageObjectGetResult la_storage_get(LAStorageObjectStore *store, const char *key, const char *rev, LAStorageObject **obj)
{
    struct ObjectHeader header;
    DBT db_key;
    DBT db_value;
    int result;
    
    memset(&header, 0, sizeof(struct ObjectHeader));
    
    db_key.data = (void *) key;
    db_key.size = (u_int32_t) strlen(key);
    db_key.ulen = db_key.size;
    db_key.flags = DB_DBT_USERMEM;
    
    if (obj != NULL)
    {
        db_value.data = NULL;
        db_value.ulen = 0;
        db_value.size = 0;
        db_value.flags = DB_DBT_MALLOC;
    }
    else
    {
        db_value.data = header.rev;
        db_value.ulen = MAX_REVISION_LEN;
        db_value.dlen = MAX_REVISION_LEN;
        db_value.doff = 0;
        db_value.flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;
    }

    result = store->db->get(store->db, NULL, &db_key, &db_value, DB_READ_COMMITTED);
    if (result != 0)
    {
        if (result == DB_NOTFOUND)
            return LAStorageObjectGetNotFound;
        return LAStorageObjectGetInternalError;
    }
    
    if (strnlen(db_value.data, min(MAX_REVISION_LEN + 1, db_value.size)) > MAX_REVISION_LEN)
    {
        if (obj != NULL)
            free(db_value.data);
        return LAStorageObjectGetInternalError;
    }
    
    debug("got { size: %u, data: %s }\n", db_value.size, db_value.data);
    
    if (rev != NULL && strcmp(rev, db_value.data) != 0)
        return LAStorageObjectGetRevisionNotFound;
    
    if (obj != NULL)
    {
        *obj = (LAStorageObject *) malloc(sizeof(LAStorageObject));
        (*obj)->key = strdup(key);
        (*obj)->rev_data = db_value.data;
        (*obj)->data_length = (uint32_t) (db_value.size - strlen((const char*) (*obj)->rev_data) - 1);
    }
    
    return LAStorageObjectGetSuccess;
}

LAStorageObjectGetResult la_storage_get_rev(LAStorageObjectStore *store, const char *key, char *rev, size_t maxlen)
{
    char header[MAX_REVISION_LEN];
    DBT db_key;
    DBT db_value;
    int result;
    
    memset(header, 0, MAX_REVISION_LEN);
    db_key.data = (void *) key;
    db_key.size = (u_int32_t) strlen(key);
    db_key.ulen = (u_int32_t) strlen(key);
    db_key.flags = DB_DBT_USERMEM;
    
    db_value.data = header;
    db_value.ulen = MAX_REVISION_LEN;
    db_value.dlen = MAX_REVISION_LEN;
    db_value.doff = 0;
    db_value.flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;

    result = store->db->get(store->db, NULL, &db_key, &db_value, DB_READ_COMMITTED);
    if (result != 0)
    {
        if (result == DB_NOTFOUND)
            return LAStorageObjectGetNotFound;
        return LAStorageObjectGetInternalError;
    }
    
    debug("got { size: %u, data: '%s' (%s) }\n", db_value.size, db_value.data, header);
    if (rev != NULL)
    {
        strncpy(rev, header, maxlen);
    }

    return LAStorageObjectGetSuccess;
}

/**
 * Put an object into the store.
 */
LAStorageObjectPutResult la_storage_put(LAStorageObjectStore *store, const char *rev, LAStorageObject *obj)
{
    DB_TXN *txn;
    struct ObjectHeader header;
    DBT db_key;
    DBT db_value_read, db_value_write;
    int result;
    
    memset(&db_key, 0, sizeof(DBT));
    memset(&db_value_read, 0, sizeof(DBT));
    memset(&db_value_write, 0, sizeof(DBT));
    
    db_key.data = obj->key;
    db_key.size = (u_int32_t) strlen(obj->key);
    db_key.ulen = (u_int32_t) strlen(obj->key);
    db_key.flags = DB_DBT_USERMEM;
    
    db_value_read.data = header.rev;
    db_value_read.ulen = MAX_REVISION_LEN;
    db_value_read.dlen = MAX_REVISION_LEN;
    db_value_read.doff = 0;
    db_value_read.flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;
    
    if (store->env->env->txn_begin(store->env->env, NULL, &txn, DB_TXN_SYNC | DB_TXN_NOWAIT) != 0)
        return LAStorageObjectPutInternalError;
    
    result = store->db->get(store->db, txn, &db_key, &db_value_read, DB_RMW);
    if (result != 0 && result != DB_NOTFOUND)
    {
        txn->abort(txn);
        if (result == DB_LOCK_NOTGRANTED)
            return LAStorageObjectPutConflict;
        return LAStorageObjectPutInternalError;
    }
    
    if (result != DB_NOTFOUND)
    {
        debug("data size: %d, data: %s\n", db_value_read.size, db_value_read.data);
        debug("compare rev:%s header.rev:%s\n", rev, header.rev);
        if (strcmp(rev, header.rev) != 0)
        {
            txn->abort(txn);
            return LAStorageObjectPutConflict;
        }
    }
    
    db_value_write.size = (u_int32_t) la_storage_object_total_size(obj);
    db_value_write.ulen = (u_int32_t) la_storage_object_total_size(obj);
    db_value_write.data = obj->rev_data;
    db_value_write.flags = DB_DBT_USERMEM;
    
    debug("putting { size: %u, ulen: %u, data: %s, flags: %x }\n", db_value_write.size,
          db_value_write.ulen, db_value_write.data, db_value_write.flags);
    result = store->db->put(store->db, txn, &db_key, &db_value_write, 0);
    if (result != 0)
    {
        txn->abort(txn);
        return LAStorageObjectPutInternalError;
    }
    txn->commit(txn, 0);
    return LAStorageObjectPutSuccess;
}

LAStorageObjectIterator *la_storage_iterator_open(LAStorageObjectStore *store)
{
    LAStorageObjectIterator *it = (LAStorageObjectIterator *) malloc(sizeof(struct LAStorageObjectIterator));
    if (it == NULL)
        return NULL;
    it->store = store;
    if (store->db->cursor(store->db, NULL, &it->cursor, DB_TXN_SNAPSHOT) != 0)
    {
        free(it);
        return NULL;
    }
    return it;
}

LAStorageObjectIteratorResult la_storage_iterator_next(LAStorageObjectIterator *it, LAStorageObject **obj)
{
    DBT db_key;
    DBT db_value;
    int result;
    
    memset(&db_key, 0, sizeof(DBT));
    memset(&db_value, 0, sizeof(DBT));
    
    db_key.flags = DB_DBT_MALLOC;
    if (obj != NULL)
        db_value.flags = DB_DBT_MALLOC;
    else
        db_value.flags = DB_DBT_USERMEM;
    
    result = it->cursor->get(it->cursor, &db_key, &db_value, DB_NEXT);
    if (result != 0)
    {
        if (result == DB_NOTFOUND)
            return LAStorageObjectIteratorEnd;
        return LAStorageObjectIteratorError;
    }
    
    if (obj != NULL)
    {
        if (strnlen(db_value.data, min(db_value.size, MAX_REVISION_LEN + 1) > MAX_REVISION_LEN))
            return LAStorageObjectIteratorError;
        *obj = (LAStorageObject *) malloc(sizeof(LAStorageObject));
        if (*obj == NULL)
            return LAStorageObjectIteratorError;
        (*obj)->key = keycpy(db_key.data, db_key.size);
        if ((*obj)->key == NULL)
        {
            free(*obj);
            return LAStorageObjectIteratorError;
        }
        (*obj)->rev_data = db_value.data;
        (*obj)->data_length = (uint32_t) (db_value.size - strlen(db_value.data) - 1);
    }
    
    return LAStorageObjectIteratorGotNext;
}

void la_storage_iterator_close(LAStorageObjectIterator *it)
{
    it->cursor->close(it->cursor);
    free(it);
}

void la_storage_close(LAStorageObjectStore *store)
{
    store->db->close(store->db, 0);
    free(store);
}
