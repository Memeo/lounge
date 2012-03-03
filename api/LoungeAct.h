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

/*
 * value is a valid object, which was read from the iterator.
 *   - If the callback needs it to survive, it must incref it.
 * return value must be a new reference; if the map function just returns value,
 *   it must incref the value.
 */
typedef la_codec_value_t *(*la_view_mapfn)(la_codec_value_t *value, void *baton);
typedef la_codec_value_t *(*la_view_reducefn)(la_codec_value_t *accum, la_codec_value_t *value, void *baton);

la_db_t *la_db_open(la_host_t *host, const char *name);
la_db_get_result la_db_get(la_db_t *db, const char *key, const char *rev, la_codec_value_t **value, la_codec_error_t *error);
la_db_put_result la_db_put(la_db_t *db, const char *key, const char *rev, const la_codec_value_t *doc, char *newrev);
la_db_delete_result la_db_delete(la_db_t *db, const char *key, const char *rev);
la_view_iterator_t *la_db_view(la_db_t *db, la_view_mapfn map, la_view_reducefn reduce, void *baton);
la_view_iterator_result la_view_iterator_next(la_view_iterator_t *it, la_codec_value_t **value, la_codec_error_t *error);
void la_view_iterator_close(la_view_iterator_t *it);
void la_db_close(la_db_t *db);

#endif
