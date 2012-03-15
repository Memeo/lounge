//
//  api.c
//  LoungeAct
//
//  Created by Casey Marshall on 2/29/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include "LoungeAct.h"
#include <stdlib.h>
#include <string.h>
#include "../Utils/buffer.h"
#include "../Utils/stringutils.h"

#if defined (__APPLE__) /* Jerks. */
# include <CommonCrypto/CommonDigest.h>
# define SHA1_CTX CC_SHA1_CTX
# define SHA1_Update CC_SHA1_Update
# define SHA1_Init CC_SHA1_Init
# define SHA1_Final CC_SHA1_Final
#else
# include <openssl/sha.h>
#endif

#include <Storage/ObjectStore.h>

struct la_host
{
    la_storage_env *env;
};

struct la_db
{
    la_host_t *host;
    la_storage_object_store *store;
};

struct la_view_iterator
{
    la_db_t *db;
    la_storage_object_iterator *it;
    la_view_mapfn map;
    la_view_reducefn reduce;
    la_codec_value_t *accum;
    void *baton;
    la_codec_value_t *mapped;
};

la_host_t *la_host_open(const char *hosthome)
{
    la_host_t *host = (la_host_t *) malloc(sizeof(struct la_host));
    if (host == NULL)
        return NULL;
    host->env = la_storage_open_env(hosthome);
    if (host->env == NULL)
    {
        free(host);
        return NULL;
    }
    return host;
}

void la_host_close(la_host_t *host)
{
    la_storage_close_env(host->env);
    free(host);
}

la_db_t *la_db_open(la_host_t *host, const char *name)
{
    la_db_t *db = (la_db_t *) malloc(sizeof(struct la_db));
    if (db == NULL)
        return NULL;
    db->host = host;
    db->store = la_storage_open(host->env, name);
    if (db->store == NULL)
    {
        free(db);
        return NULL;
    }
    return db;
}

static int is_tombstone(la_codec_value_t *doc)
{
    if (doc == NULL)
        return 0;
    if (!la_codec_is_object(doc))
        return 0;
    if (la_codec_object_size(doc) > 3)
        return 0;
    la_codec_value_t *val = la_codec_object_get(doc, LA_API_DELETED_NAME);
    if (val == NULL)
        return 0;
    return la_codec_is_true(val);
}

la_db_get_result la_db_get(la_db_t *db, const char *key, la_rev_t *rev, la_codec_value_t **value,
                           la_rev_t *current_rev, la_codec_error_t *error)
{
    la_codec_value_t *v;
    la_storage_object *object;
    la_storage_rev_t *srev = rev ? &rev->rev : NULL;
    la_storage_object_get_result result = la_storage_get(db->store, key, srev, &object);
    const char *object_data = NULL;

    if (result != LA_STORAGE_OBJECT_GET_OK)
    {
        if (result == LA_STORAGE_OBJECT_GET_NOT_FOUND || result == LA_STORAGE_OBJECT_GET_REVISION_NOT_FOUND)
            return LA_DB_GET_NOT_FOUND;
        return LA_DB_GET_ERROR;
    }
    if (object->header->deleted)
        return LA_DB_GET_NOT_FOUND;
    object_data = (const char *) la_storage_object_get_data(object);
    v = la_codec_loadb(object_data, object->data_length, 0, error);
    if (current_rev != NULL)
    {
        current_rev->seq = object->header->doc_seq;
        memcpy(&current_rev->rev, &object->header->rev, sizeof(la_storage_rev_t));
    }            
    la_storage_destroy_object(object);
    if (v == NULL)
        return LA_DB_GET_ERROR;
    if (is_tombstone(v))
    {
        la_codec_decref(v);
        return LA_DB_GET_NOT_FOUND;
    }
    if (value != NULL)
        *value = v;
    else
        la_codec_decref(v);
    return LA_DB_GET_OK;
}

static int hashdoc(const char *str, size_t size, void *data)
{
    SHA1_CTX *ctx = (SHA1_CTX *) data;
    SHA1_Update(ctx, str, (unsigned int) size);
    return 0;
}

static int accumulate(const char *str, size_t size, void *data)
{
    la_buffer_t *buffer = (la_buffer_t *) data;
    if (la_buffer_append(buffer, str, size) != 0)
        return -1;
    return 0;
}

static la_db_put_result do_la_db_put(la_db_t *db, const char *key, const la_rev_t *oldrev, const la_codec_value_t *doc,
                                     la_rev_t *newrev, int is_delete)
{
    SHA1_CTX sha;
    unsigned char digest[SHA1_DIGEST_LENGTH];
    char rev[LA_API_MAX_REVLEN + 1];
    char uuidkey[65];
    la_storage_object *object;
    la_buffer_t *buffer;
    la_codec_value_t *putdoc;
    la_storage_object_put_result result;
    la_rev_t nextrev;
    
    if (!la_codec_is_object(doc) || (!is_delete && la_codec_object_get(doc, LA_API_DELETED_NAME) != NULL))
    {
        return LA_DB_PUT_INVALID_ARG;
    }
    putdoc = la_codec_copy((la_codec_value_t *) doc);
    if (putdoc == NULL)
    {
        return LA_DB_PUT_ERROR;
    }
    la_codec_object_del(putdoc, LA_API_KEY_NAME);
    la_codec_object_del(putdoc, LA_API_REV_NAME);
    
    if (key == NULL)
    {
        la_codec_value_t *id = la_codec_object_get(doc, LA_API_KEY_NAME);
        if (id == NULL || !la_codec_is_string(id))
        {
            string_randhex(uuidkey, 64);
            key = uuidkey;
        }
        else
        {
            key = la_codec_string_value(id);
        }
    }

    la_revgen(putdoc, oldrev ? oldrev->seq : 1, oldrev ? &oldrev->rev : NULL, is_delete, &nextrev.rev);
    la_codec_object_set_new(putdoc, LA_API_KEY_NAME, la_codec_string(key));
    
    buffer = la_buffer_new(256);
    if (la_codec_dump_callback(putdoc, accumulate, buffer, 0) != 0)
    {
        la_codec_decref(putdoc);
        la_buffer_destroy(buffer);
        return LA_DB_PUT_ERROR;
    }
    la_codec_decref(putdoc);
    
    object = la_storage_create_object(key, nextrev.rev, la_buffer_data(buffer), (uint32_t) la_buffer_size(buffer));
    if (object == NULL)
    {
        la_buffer_destroy(buffer);
        return LA_DB_PUT_ERROR;
    }
    la_buffer_destroy(buffer);
    
    object->header->deleted = is_delete;
    result = la_storage_put(db->store, oldrev ? &oldrev->rev : NULL, object);
    nextrev.seq = object->header->doc_seq;
    if (newrev != NULL)
        memcpy(newrev, &nextrev, sizeof(la_rev_t));
    la_storage_destroy_object(object);
    if (result == LA_STORAGE_OBJECT_PUT_ERROR)
        return LA_DB_PUT_ERROR;
    if (result == LA_STORAGE_OBJECT_PUT_CONFLICT)
        return LA_DB_PUT_CONFLICT;
    return LA_DB_PUT_OK;
}

la_db_put_result la_db_put(la_db_t *db, const char *key, const la_rev_t *rev, const la_codec_value_t *doc, la_rev_t *newrev)
{
    return do_la_db_put(db, key, rev, doc, newrev, 0);
}

la_db_delete_result la_db_delete(la_db_t *db, const char *key, const la_rev_t *rev)
{
    la_codec_value_t *doc = la_codec_object();
    la_db_put_result result;
    
    if (doc == NULL)
        return LA_DB_DELETE_ERROR;
    result = do_la_db_put(db, key, rev, doc, NULL, 1);
    la_codec_decref(doc);
    if (result == LA_DB_PUT_CONFLICT)
        return LA_DB_DELETE_CONFLICT;
    if (result == LA_DB_PUT_OK)
        return LA_DB_DELETE_OK;
    return LA_DB_DELETE_ERROR;
}

static void _do_map_emit(la_view_state_t *state, la_codec_value_t *value)
{
    if (value != NULL)
    {
        la_codec_array_append(state->iterator->mapped, value);
    }
}

la_view_iterator_t *la_db_view(la_db_t *db, la_view_mapfn map, la_view_reducefn reduce, la_view_rereducefn rereduce, void *baton)
{
    la_view_iterator_t *it = (la_view_iterator_t *) malloc(sizeof(struct la_view_iterator));
    if (it == NULL)
        return NULL;
    it->db = db;
    it->it = la_storage_iterator_open(db->store, 0);
    if (it->it == NULL)
    {
        free(it);
        return NULL;
    }
    it->map = map;
    it->reduce = reduce;
    it->baton = baton;
    it->accum = NULL;
    it->mapped = la_codec_array();
    return it;
}

la_view_iterator_result la_view_iterator_next(la_view_iterator_t *it, la_codec_value_t **value, la_codec_error_t *error)
{
    la_storage_object *object = NULL;
    la_storage_object_iterator_result result;
    la_codec_value_t *parsed = NULL, *mapped;
    la_view_state_t mapstate = { it, _do_map_emit };
    
    if (value != NULL)
        *value = NULL;
    while (la_codec_array_size(it->mapped) == 0 && parsed == NULL)
    {
        result = la_storage_iterator_next(it->it, &object);
    
        if (result == LA_STORAGE_OBJECT_ITERATOR_ERROR)
            return LA_VIEW_ITERATOR_ERROR;
        if (result == LA_STORAGE_OBJECT_ITERATOR_END)
        {
            // If we are at the end and we had a reduce, return the reduced value.
            if (value != NULL && it->accum != NULL)
            {
                *value = la_codec_incref(it->accum);
            }
            return LA_VIEW_ITERATOR_END;
        }
        if (object->header->deleted)
        {
            la_storage_destroy_object(object);
            continue;
        }
        parsed = la_codec_loadb((const char *) la_storage_object_get_data(object), object->data_length, 0, error);
        la_storage_destroy_object(object);
        if (parsed == NULL)
        {
            return LA_VIEW_ITERATOR_ERROR;
        }

        if (it->map != NULL)
        {
            it->map(parsed, &mapstate, it->baton);
            la_codec_decref(parsed);
        }
        else
        {
            la_codec_array_append(it->mapped, parsed);
        }
    }
    
    if (la_codec_array_size(it->mapped) == 0)
        return LA_VIEW_ITERATOR_ERROR;
    mapped = la_codec_array_get(it->mapped, 0);
    la_codec_incref(mapped);
    la_codec_array_remove(it->mapped, 0);
    if (it->reduce != NULL)
    {
        la_codec_value_t *next = it->reduce(it->accum, mapped, it->baton);
        if (it->accum != NULL)
            la_codec_decref(it->accum);
        it->accum = next;
    }
    
    if (value != NULL)
        *value = mapped;
    else
        la_codec_decref(mapped);
    return LA_VIEW_ITERATOR_GOT_NEXT;
}

void la_view_iterator_close(la_view_iterator_t *it)
{
    la_storage_iterator_close(it->it);
    if (it->accum != NULL)
        la_codec_decref(it->accum);
    free(it);
}

void la_db_close(la_db_t *db)
{
    la_storage_close(db->store);
    free(db);
}
