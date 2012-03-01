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

typedef struct la_host la_host_t;
typedef struct la_db la_db_t;

typedef enum
{
    LA_DB_PUT_OK,
    LA_DB_PUT_CONFLICT,
    LA_DB_PUT_ERROR
} la_db_put_result;

la_host_t *la_host_open(const char *hosthome);
void la_host_close(la_host_t *host);

typedef la_codec_value_t *(*la_view_mapfn)(la_codec_value_t *value, void *baton);
typedef la_codec_value_t *(*la_view_reducefn)(la_codec_value_t *accum, la_codec_value_t *value, void *baton);

la_db_t *la_host_get_db(la_host_t *host, const char *name);
la_codec_value_t *la_db_get(la_db_t *db, const char *key, const char *rev);
la_db_put_result la_db_put(la_db_t *db, const char *key, la_codec_value_t *doc);
la_codec_value_t *la_db_view(la_db_t *db, la_view_mapfn map, la_view_reducefn reduce, void *baton);
void la_db_close(la_db_t *db);

#endif
