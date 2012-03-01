//
//  buffer.h
//  LoungeAct
//
//  Created by Casey Marshall on 2/29/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#ifndef LoungeAct_buffer_h
#define LoungeAct_buffer_h

#include <sys/types.h>

typedef struct la_buffer la_buffer_t;

la_buffer_t *la_buffer_new(size_t capacity);
size_t la_buffer_size(const la_buffer_t *buffer);
size_t la_buffer_capacity(const la_buffer_t *buffer);
void *la_buffer_data(const la_buffer_t *buffer);
size_t la_buffer_get(la_buffer_t *buffer, size_t offset, void *data, size_t length);
int la_buffer_append(la_buffer_t *buffer, const void *data, size_t length);
int la_buffer_insert(la_buffer_t *buffer, size_t index, const void *data, size_t length);
int la_buffer_overwrite(la_buffer_t *buffer, size_t index, const void *data, size_t length);
int la_buffer_ensure_capacity(la_buffer_t *buffer, size_t capacity);
int la_buffer_compact(la_buffer_t *buffer);
int la_buffer_truncate(la_buffer_t *buffer, size_t newsize);
void la_buffer_destroy(la_buffer_t *buffer);
                       
#define la_buffer_clear(buf) la_buffer_truncate(buf, 0)

#endif
