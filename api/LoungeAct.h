//
//  LoungeAct.h
//  LoungeAct
//
//  Created by Casey Marshall on 2/29/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#ifndef LoungeAct_LoungeAct_h
#define LoungeAct_LoungeAct_h

#include <Codec/Codec.h>
#include <revgen/revgen.h>
#include <compress/compress.h>

#ifndef LA_API_KEY_NAME
#define LA_API_KEY_NAME "_id"
#endif

#ifndef LA_API_REV_NAME
#define LA_API_REV_NAME "_rev"
#endif

#ifndef LA_API_DELETED_NAME
#define LA_API_DELETED_NAME "_deleted"
#endif

#if defined (__APPLE__) /* Jerks. */
# include <CommonCrypto/CommonDigest.h>
# define SHA1_DIGEST_LENGTH CC_SHA1_DIGEST_LENGTH
#else
# include <openssl/sha.h>
#endif

// <sequence in decimal> + '-' + <SHA1 of object>
#define LA_API_MAX_REVLEN 20 + 1 + (SHA1_DIGEST_LENGTH * 2)

typedef struct la_host la_host_t;
typedef struct la_db la_db_t;
typedef struct la_view_iterator la_view_iterator_t;

typedef enum
{
    LA_DB_GET_OK,
    LA_DB_GET_NOT_FOUND,
    LA_DB_GET_ERROR
} la_db_get_result;

typedef enum
{
    LA_DB_PUT_OK,
    LA_DB_PUT_CONFLICT,
    LA_DB_PUT_INVALID_ARG,
    LA_DB_PUT_ERROR
} la_db_put_result;

typedef enum
{
    LA_DB_DELETE_OK,
    LA_DB_DELETE_CONFLICT,
    LA_DB_DELETE_ERROR
} la_db_delete_result;

typedef enum
{
    LA_VIEW_ITERATOR_GOT_NEXT,
    LA_VIEW_ITERATOR_END,
    LA_VIEW_ITERATOR_ERROR
} la_view_iterator_result;

la_host_t *la_host_open(const char *driver, const char *hosthome);
void la_host_close(la_host_t *host);

void la_host_configure_compressor(la_host_t *host, la_compressor_t *compressor);

/**
 * A view state object, passed in to the la_view_mapfn to handle
 * map results.
 *
 * For each mapped value the map function produces, call the emit
 * function with the iterator handle and the mapped value.
 */
typedef struct la_view_state
{
    la_view_iterator_t *iterator;
    void (*emit)(struct la_view_state *state, la_codec_value_t *value);
} la_view_state_t;

/**
 * A map function. This function should take the value passed in, and for
 * all resulting mapped values, should call the emit function in the view
 * state for each mapped value.
 */
typedef void (*la_view_mapfn)(la_codec_value_t *value, la_view_state_t *view, void *baton);

/**
 * A reduce function. This should reduce the current accumulated value with
 * the next value, returning the next accumulated value.
 */
typedef la_codec_value_t *(*la_view_reducefn)(la_codec_value_t *accum, la_codec_value_t *value, void *baton);

/**
 * A re-reduce function. This is given an array of partial reductions, and this
 * function should reduce the partial reductions together, and return a
 * single, reduced value.
 */
typedef la_codec_value_t *(*la_view_rereducefn)(la_codec_value_t *partials, void *baton);

typedef enum
{
    LA_DB_OPEN_FLAG_CREATE = LA_STORAGE_OPEN_FLAG_CREATE, /* Create the DB if it doesn't exist. */
    LA_DB_OPEN_FLAG_EXCL   = LA_STORAGE_OPEN_FLAG_EXCL  /* Fail if creating a DB and it already exists. */
} la_db_open_flag_t;

typedef enum
{
    LA_DB_OPEN_OK        = LA_STORAGE_OPEN_OK,
    LA_DB_OPEN_CREATED   = LA_STORAGE_OPEN_CREATED,
    LA_DB_OPEN_NOT_FOUND = LA_STORAGE_OPEN_NOT_FOUND,
    LA_DB_OPEN_EXISTS    = LA_STORAGE_OPEN_EXISTS,
    LA_DB_OPEN_ERROR     = LA_STORAGE_OPEN_ERROR
} la_db_open_result_t;

la_db_open_result_t la_db_open(la_host_t *host, const char *name, int flags, la_db_t **db);
int la_db_delete_db(la_db_t *db);
la_db_get_result la_db_get(la_db_t *db, const char *key, la_rev_t *rev, la_codec_value_t **value,
                           la_rev_t *current_rev, la_codec_error_t *error);
int la_db_get_allrevs(la_db_t *db, const char *key, uint64_t *start, la_storage_rev_t **revs);
uint64_t la_db_last_seq(la_db_t *db);
int la_db_stat(la_db_t *db, la_storage_stat_t *stat);
la_db_put_result la_db_put(la_db_t *db, const char *key, const la_rev_t *rev, const la_codec_value_t *doc, la_rev_t *newrev);
la_db_put_result la_db_replace(la_db_t *db, const char *key, const la_rev_t *rev, const la_codec_value_t *doc,
                               const la_storage_rev_t *oldrevs, size_t revcount);
la_db_delete_result la_db_delete(la_db_t *db, const char *key, const la_rev_t *rev);
la_view_iterator_t *la_db_view(la_db_t *db, la_view_mapfn map, la_view_reducefn reduce, la_view_rereducefn rereduce, void *baton);
la_view_iterator_result la_view_iterator_next(la_view_iterator_t *it, la_codec_value_t **value, la_codec_error_t *error);
void la_view_iterator_close(la_view_iterator_t *it);
void la_db_close(la_db_t *db);

#endif
