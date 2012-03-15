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

la_host_t *la_host_open(const char *hosthome);
void la_host_close(la_host_t *host);

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

la_db_t *la_db_open(la_host_t *host, const char *name);
la_db_get_result la_db_get(la_db_t *db, const char *key, la_rev_t *rev, la_codec_value_t **value,
                           la_rev_t *current_rev, la_codec_error_t *error);
la_db_put_result la_db_put(la_db_t *db, const char *key, const la_rev_t *rev, const la_codec_value_t *doc, la_rev_t *newrev);
la_db_delete_result la_db_delete(la_db_t *db, const char *key, const la_rev_t *rev);
la_view_iterator_t *la_db_view(la_db_t *db, la_view_mapfn map, la_view_reducefn reduce, la_view_rereducefn rereduce, void *baton);
la_view_iterator_result la_view_iterator_next(la_view_iterator_t *it, la_codec_value_t **value, la_codec_error_t *error);
void la_view_iterator_close(la_view_iterator_t *it);
void la_db_close(la_db_t *db);

#endif
