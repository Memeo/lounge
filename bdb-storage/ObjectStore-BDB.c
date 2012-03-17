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
#include "../Utils/stringutils.h"
#include "../Utils/hexdump.h"
#include "../utils/utils.h"

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

struct la_storage_env
{
    DB_ENV *env;
};

struct la_storage_object_store
{
    la_storage_env *env;
    DB *db;
    DB *seq_db;
    DB_SEQUENCE *seq;
};

struct la_storage_object_iterator
{
    la_storage_object_store *store;
    DBC *cursor;
};

const char *
la_storage_driver(void)
{
    return "BDB";
}

la_storage_env *la_storage_open_env(const char *name)
{
    int ret;
    struct stat st;
    struct la_storage_env *env = (la_storage_env *) malloc(sizeof(struct la_storage_env));
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

void la_storage_close_env(la_storage_env *env)
{
    env->env->close(env->env, 0);
    free(env);
}

static int seqindex(DB *secondary, const DBT *key, const DBT *value, DBT *result)
{
    if (((char *) key->data)[0] == '\0')
        return DB_DONOTINDEX;
#if DEBUG
    printf("seqindex\n");
    la_hexdump(value->data, value->size);
#endif
    la_storage_object_header *header = value->data;
    result->data = &header->seq;
    result->size = sizeof(uint64_t);
    return 0;
}

static int compare_seq(DB *db, const DBT *key1, const DBT *key2)
{
    uint64_t seq1, seq2;

    if (key1->size > key2->size)
        return 1;
    if (key1->size < key2->size)
        return -1;
    
    seq1 = * ((uint64_t *) key1->data);
    seq2 = * ((uint64_t *) key2->data);
    if (seq1 < seq2)
        return -1;
    if (seq1 > seq2)
        return 1;
    return 0;
}

la_storage_object_store *la_storage_open(la_storage_env *env, const char *path)
{
    la_storage_object_store *store = (la_storage_object_store *) malloc(sizeof(struct la_storage_object_store));
    DB_TXN *txn;
    char *seqpath;
    DBT seq_key;
    char seq_name[4];
    
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
    if (db_create(&store->seq_db, env->env, 0) != 0)
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
    seqpath = string_append(path, ".seq");
    if (seqpath == NULL)
    {
        store->db->close(store->db, 0);
        txn->abort(txn);
        free(store);
        return NULL;
    }
    if (store->seq_db->set_bt_compare(store->seq_db, compare_seq) != 0)
    {
        store->db->close(store->db, 0);
        txn->abort(txn);
        free(store);
        return NULL;
    }
    if (store->seq_db->open(store->seq_db, txn, seqpath, NULL, DB_BTREE, DB_CREATE | DB_THREAD | DB_READ_UNCOMMITTED, 0) != 0)
    {
        free(seqpath);
        store->db->close(store->db, 0);
        txn->abort(txn);
        free(store);
        return NULL;
    }
    free(seqpath);
    store->db->associate(store->db, txn, store->seq_db, seqindex, 0);
    if (db_sequence_create(&store->seq, store->db, 0) != 0)
    {
        store->seq_db->close(store->seq_db, 0);
        store->db->close(store->db, 0);
        txn->abort(txn);
        free(store);
        return NULL;
    }
    store->seq->initial_value(store->seq, 1);
    seq_name[0] = '\0';
    seq_name[1] = 'S';
    seq_name[2] = 'E';
    seq_name[3] = 'Q';
    seq_key.data = seq_name;
    seq_key.size = 4;
    seq_key.ulen = 4;
    seq_key.flags = DB_DBT_USERMEM;
    if (store->seq->open(store->seq, txn, &seq_key, DB_CREATE | DB_THREAD) != 0)
    {
        store->seq_db->close(store->seq_db, 0);
        store->db->close(store->db, 0);
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
la_storage_object_get_result la_storage_get(la_storage_object_store *store, const char *key, const la_storage_rev_t *rev, la_storage_object **obj)
{
    la_storage_object_header header;
    DBT db_key;
    DBT db_value;
    int result;
    
    memset(&header, 0, sizeof(la_storage_object_header));
    
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
        db_value.data = &header;
        db_value.ulen = sizeof(la_storage_object_header);
        db_value.dlen = sizeof(la_storage_object_header);
        db_value.doff = 0;
        db_value.flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;
    }

    result = store->db->get(store->db, NULL, &db_key, &db_value, DB_READ_COMMITTED);
    if (result != 0)
    {
        if (result == DB_NOTFOUND)
            return LA_STORAGE_OBJECT_GET_NOT_FOUND;
        return LA_STORAGE_OBJECT_GET_ERROR;
    }
    
    debug("got { size: %u, data: %s }\n", db_value.size, (char *) db_value.data);
    
    if (rev != NULL && memcmp(rev, &header.rev, sizeof(la_storage_rev_t)) != 0)
        return LA_STORAGE_OBJECT_GET_NOT_FOUND;
    
    if (obj != NULL)
    {
        *obj = (la_storage_object *) malloc(sizeof(la_storage_object));
        (*obj)->key = strdup(key);
        (*obj)->header = db_value.data;
        (*obj)->data_length = (uint32_t) (db_value.size - ((*obj)->header->rev_count * sizeof(la_storage_rev_t)) - sizeof(struct la_storage_object_header));
#if DEBUG
        printf("got %u bytes\n", (*obj)->data_length);
        la_hexdump(la_storage_object_get_data((*obj)), (*obj)->data_length);
#endif
    }
    
    return LA_STORAGE_OBJECT_GET_OK;
}

la_storage_object_get_result la_storage_get_rev(la_storage_object_store *store, const char *key, la_storage_rev_t *rev)
{
    la_storage_object_header header;
    DBT db_key;
    DBT db_value;
    int result;
    
    memset(&header, 0, sizeof(header));
    db_key.data = (void *) key;
    db_key.size = (u_int32_t) strlen(key);
    db_key.ulen = (u_int32_t) strlen(key);
    db_key.flags = DB_DBT_USERMEM;
    
    db_value.data = &header;
    db_value.ulen = sizeof(header);
    db_value.dlen = sizeof(header);
    db_value.doff = 0;
    db_value.flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;

    result = store->db->get(store->db, NULL, &db_key, &db_value, DB_READ_COMMITTED);
    if (result != 0)
    {
        if (result == DB_NOTFOUND)
            return LA_STORAGE_OBJECT_GET_NOT_FOUND;
        return LA_STORAGE_OBJECT_GET_ERROR;
    }
    
    debug("got { size: %u, data: '%s' }\n", db_value.size, db_value.data);
    if (rev != NULL)
    {
        memcpy(rev, &header.rev, sizeof(la_storage_rev_t));
    }

    return LA_STORAGE_OBJECT_GET_OK;
}

int la_storage_get_all_revs(la_storage_object_store *store, const char *key, uint64_t *start, la_storage_rev_t **revs)
{
    DBT db_key;
    DBT db_value;
    int result;
    struct la_storage_object_header *header;
    
    memset(&db_key, 0, sizeof(DBT));
    memset(&db_value, 0, sizeof(DBT));
    
    db_key.data = key;
    db_key.size = db_key.ulen = strlen(key);
    db_key.flags = DB_DBT_USERMEM;
    
    db_value.data = NULL;
    db_value.dlen = sizeof(struct la_storage_object_header) + (sizeof(la_storage_rev_t) * LA_OBJECT_MAX_REVISION_COUNT);
    db_value.flags = DB_DBT_MALLOC | DB_DBT_PARTIAL;
    
    result = store->db->get(store->db, NULL, &db_key, &db_value, DB_READ_COMMITTED);
    if (result != 0)
        return -1;

    header = db_value.data;
    if (start != NULL)
        *start = header->doc_seq;
    if (revs != NULL)
    {
        *revs = malloc(sizeof(la_storage_rev_t) * header->rev_count);
        if (*revs == NULL)
        {
            *revs = NULL;
        }
        else
        {
            memcpy(*revs, header->revs_data, header->rev_count * sizeof(la_storage_rev_t));
        }
    }
    
    return header->rev_count;
}

la_storage_object_put_result la_storage_set_revs(la_storage_object_store *store, const char *key, la_storage_rev_t *revs, size_t revcount)
{
    DB_TXN *txn;
    DBT db_key;
    DBT db_value;
    int result;
    la_storage_object object;
    int shift;
    
    revcount = la_min(revcount, LA_OBJECT_MAX_REVISION_COUNT);
    memset(&db_key, 0, sizeof(DBT));
    memset(&db_value, 0, sizeof(DBT));
    
    db_key.data = key;
    db_key.size = strlen(key);
    db_key.ulen = strlen(key);
    db_key.flags = DB_DBT_USERMEM;
    
    db_value.data = NULL;
    db_value.ulen = 0;
    db_value.flags = DB_DBT_MALLOC;
    
    if (store->env->env->txn_begin(store->env->env, NULL, &txn, DB_TXN_SYNC | DB_TXN_NOWAIT) != 0)
        return LA_STORAGE_OBJECT_PUT_ERROR;
    
    result = store->db->get(store->db, txn, &db_key, &db_value, DB_RMW);
    if (result != 0)
    {
        txn->abort(txn);
        if (result == DB_LOCK_NOTGRANTED)
            return LA_STORAGE_OBJECT_PUT_CONFLICT;
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }

    object.header = (la_storage_object_header *) db_value.data;
    object.data_length = db_value.size - sizeof(la_storage_object_header) - (object.header->rev_count * sizeof(la_storage_rev_t));
    shift = revcount - object.header->rev_count;
    if (shift > 0)
    {
        object.header = realloc(object.header, db_value.size + shift);
        if (object.header == NULL)
        {
            free(object.header);
            return LA_STORAGE_OBJECT_PUT_ERROR;
        }
    }
    memmove(la_storage_object_get_data(&object) + shift, la_storage_object_get_data(&object), object.data_length);
    memcpy(object.header->revs_data, revs, revcount * sizeof(la_storage_rev_t));

    db_value.data = object.header;
    db_value.size = db_value.size + shift;
    db_value.ulen = db_value.size;
    db_value.flags = DB_DBT_USERMEM;
    
    result = store->db->put(store->db, txn, &db_key, &db_value, 0);
    free(db_value.data);
    if (result != 0)
    {
        txn->abort(txn);
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    txn->commit(txn, 0);
    return 0;
}

/**
 * Put an object into the store.
 */
la_storage_object_put_result la_storage_put(la_storage_object_store *store, const la_storage_rev_t *rev, la_storage_object *obj)
{
    DB_TXN *txn;
    la_storage_object_header header;
    DBT db_key;
    DBT db_value_read, db_value_write;
    int result;
    
#if DEBUG
    printf("putting %u bytes:\n", obj->data_length);
    la_hexdump(la_storage_object_get_data(obj), obj->data_length);
#endif
    
    memset(&db_key, 0, sizeof(DBT));
    memset(&db_value_read, 0, sizeof(DBT));
    memset(&db_value_write, 0, sizeof(DBT));
    
    db_key.data = obj->key;
    db_key.size = (u_int32_t) strlen(obj->key);
    db_key.ulen = (u_int32_t) strlen(obj->key);
    db_key.flags = DB_DBT_USERMEM;
    
    db_value_read.data = &header;
    db_value_read.ulen = sizeof(la_storage_object_header);
    db_value_read.dlen = sizeof(la_storage_object_header);
    db_value_read.doff = 0;
    db_value_read.flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;
    
    if (store->env->env->txn_begin(store->env->env, NULL, &txn, DB_TXN_SYNC | DB_TXN_NOWAIT) != 0)
        return LA_STORAGE_OBJECT_PUT_ERROR;
    
    result = store->db->get(store->db, txn, &db_key, &db_value_read, DB_RMW);
    if (result != 0 && result != DB_NOTFOUND)
    {
        txn->abort(txn);
        if (result == DB_LOCK_NOTGRANTED)
            return LA_STORAGE_OBJECT_PUT_CONFLICT;
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    
    if (result != DB_NOTFOUND)
    {
        debug("data size: %d, data: %s\n", db_value_read.size, db_value_read.data);
        if (rev == NULL || memcmp(rev, &header.rev, sizeof(la_storage_rev_t)) != 0)
        {
            txn->abort(txn);
            return LA_STORAGE_OBJECT_PUT_CONFLICT;
        }
        obj->header->doc_seq = header.doc_seq + 1;
        if (header.rev_count < LA_OBJECT_MAX_REVISION_COUNT)
        {
            obj->header = realloc(obj->header, la_storage_object_total_size(obj) + sizeof(la_storage_rev_t));
            // If we added a revision, move the object data up to make room.
            memmove(la_storage_object_get_data(obj) + sizeof(la_storage_rev_t), la_storage_object_get_data(obj), obj->data_length);
            obj->header->rev_count = header.rev_count + 1;
        }
        // Move the rev_count-1 previous revisons over...
        memmove(obj->header->revs_data + sizeof(la_storage_rev_t), obj->header->revs_data, sizeof(la_storage_rev_t) * (obj->header->rev_count - 1));
        // Copy the previous revision in.
        memcpy(obj->header->revs_data, &header.rev, sizeof(la_storage_rev_t));
#if DEBUG
        printf("After moving data and revisions:\nOld revisions:\n");
        la_hexdump(obj->header->revs_data, obj->header->rev_count * sizeof(la_storage_rev_t));
        printf("data:\n");
        la_hexdump(la_storage_object_get_data(obj), obj->data_length);
#endif
    }
    else
    {
        obj->header->doc_seq = 1;
        obj->header->rev_count = 0;
    }
    
    db_seq_t seq;
    store->seq->get(store->seq, txn, 1, &seq, 0);
    obj->header->seq = seq;

    db_value_write.size = (u_int32_t) la_storage_object_total_size(obj);
    db_value_write.ulen = (u_int32_t) la_storage_object_total_size(obj);
    db_value_write.data = obj->header;
    db_value_write.flags = DB_DBT_USERMEM;
    
    debug("putting { size: %u, ulen: %u, data: %s, flags: %x }\n", db_value_write.size,
          db_value_write.ulen, db_value_write.data, db_value_write.flags);
    
    result = store->db->put(store->db, txn, &db_key, &db_value_write, 0);
    if (result != 0)
    {
        txn->abort(txn);
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    txn->commit(txn, DB_TXN_SYNC);
    return LA_STORAGE_OBJECT_PUT_SUCCESS;
}

la_storage_object_put_result la_storage_replace(la_storage_object_store *store, la_storage_object *obj)
{
    DB_TXN *txn;
    la_storage_object_header header;
    DBT db_key;
    DBT db_value;
    int result;

    memset(&db_key, 0, sizeof(DBT));
    memset(&db_value, 0, sizeof(DBT));
    
    db_key.data = obj->key;
    db_key.size = db_key.ulen = strlen(obj->key);
    db_key.flags = DB_DBT_USERMEM;
    
    db_value.data = obj->header;
    db_value.size = db_value.ulen = obj->data_length;
    db_value.flags = DB_DBT_USERMEM;
    
    if (store->env->env->txn_begin(store->env->env, NULL, &txn, DB_TXN_SYNC | DB_TXN_NOWAIT) != 0)
        return LA_STORAGE_OBJECT_PUT_ERROR;
    
    if (store->db->put(store->db, txn, &db_key, &db_value, 0) != 0)
    {
        txn->abort(txn);
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    
    txn->commit(txn, 0);
    return LA_STORAGE_OBJECT_PUT_SUCCESS;
}

uint64_t la_storage_lastseq(la_storage_object_store *store)
{
    DB_SEQUENCE_STAT *stat;
    db_seq_t seq;
    store->seq->stat(store->seq, &stat, 0);
    seq = stat->st_current;
    free(stat);
    return seq;
}

la_storage_object_iterator *la_storage_iterator_open(la_storage_object_store *store, uint64_t since)
{
    DBT seq_key, seq_value;
    seq_key.data = &since;
    seq_key.size = sizeof(uint64_t);
    seq_key.flags = DB_DBT_USERMEM;
    seq_value.data = NULL;
    seq_value.ulen = 0;
    seq_value.dlen = 0;
    seq_value.doff = 0;
    seq_value.flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;
    la_storage_object_iterator *it = (la_storage_object_iterator *) malloc(sizeof(struct la_storage_object_iterator));
    if (it == NULL)
        return NULL;
    it->store = store;
    if (store->db->cursor(store->seq_db, NULL, &it->cursor, DB_TXN_SNAPSHOT) != 0)
    {
        free(it);
        return NULL;
    }
    if (since > 0)
    {
        if (it->cursor->get(it->cursor, &seq_key, &seq_value, DB_SET) != 0)
        {
            it->cursor->close(it->cursor);
            free(it);
            return NULL;
        }
    }
    return it;
}

la_storage_object_iterator_result la_storage_iterator_next(la_storage_object_iterator *it, la_storage_object **obj)
{
    DBT db_pkey;
    DBT db_key;
    DBT db_value;
    int result;
    
    memset(&db_pkey, 0, sizeof(DBT));
    memset(&db_key, 0, sizeof(DBT));
    memset(&db_value, 0, sizeof(DBT));
    
    db_pkey.flags = DB_DBT_MALLOC;
    db_key.flags = DB_DBT_MALLOC;
    if (obj != NULL)
        db_value.flags = DB_DBT_MALLOC;
    else
        db_value.flags = DB_DBT_USERMEM;
    
    result = it->cursor->pget(it->cursor, &db_key, &db_pkey, &db_value, DB_NEXT);
    if (result != 0)
    {
        if (result == DB_NOTFOUND)
            return LA_STORAGE_OBJECT_ITERATOR_END;
        return LA_STORAGE_OBJECT_ITERATOR_ERROR;
    }
    
    printf("cursor key:\n");
    la_hexdump(db_key.data, db_key.size);
    free(db_key.data);
    if (obj != NULL)
    {
        *obj = (la_storage_object *) malloc(sizeof(la_storage_object));
        if (*obj == NULL)
            return LA_STORAGE_OBJECT_ITERATOR_ERROR;
        (*obj)->key = keycpy(db_pkey.data, db_pkey.size);
        if ((*obj)->key == NULL)
        {
            free(*obj);
            return LA_STORAGE_OBJECT_ITERATOR_ERROR;
        }
        (*obj)->header = db_value.data;
        (*obj)->data_length = (uint32_t) (db_value.size - sizeof(struct la_storage_object_header) 
                                          - (sizeof(la_storage_rev_t) * (*obj)->header->rev_count));
#if DEBUG
        printf("got %u bytes\n", (*obj)->data_length);
        la_hexdump(la_storage_object_get_data((*obj)), (*obj)->data_length);
#endif
    }
    
    return LA_STORAGE_OBJECT_ITERATOR_GOT_NEXT;
}

void la_storage_iterator_close(la_storage_object_iterator *it)
{
    it->cursor->close(it->cursor);
    free(it);
}

void la_storage_close(la_storage_object_store *store)
{
    store->seq->close(store->seq, 0);
    store->seq_db->close(store->seq_db, 0);
    store->db->close(store->db, 0);
    free(store);
}
