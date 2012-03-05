//
//  buffer.c
//  LoungeAct
//
//  Created by Casey Marshall on 2/29/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "buffer.h"

#define MIN_ALLOC 1024
#define __la_buffer_max(a,b) ((a) > (b) ? (a) : (b))
#define __cap(n) __la_buffer_max(n, MIN_ALLOC)

struct la_buffer
{
    void *ptr;
    size_t size;
    size_t capacity;
};

la_buffer_t *la_buffer_new(size_t capacity)
{
    la_buffer_t *buffer = (la_buffer_t *) malloc(sizeof(struct la_buffer));
    if (buffer == NULL)
        return NULL;
    capacity = __cap(capacity);
    buffer->ptr = malloc(capacity);
    if (buffer->ptr == NULL)
    {
        free(buffer);
        return NULL;
    }
    memset(buffer->ptr, 0xaa, capacity);
    buffer->size = 0;
    buffer->capacity = capacity;
    return buffer;
}

size_t la_buffer_size(const la_buffer_t *buffer)
{
    return buffer->size;
}

size_t la_buffer_capacity(const la_buffer_t *buffer)
{
    return buffer->capacity;
}

void *la_buffer_data(const la_buffer_t *buffer)
{
    return buffer->ptr;
}

size_t la_buffer_get(la_buffer_t *buffer, size_t offset, void *data, size_t length)
{
    if (length > buffer->size - offset)
        length = buffer->size - offset;
    memcpy(data, buffer->ptr + offset, length);
    return length;
}

int la_buffer_append(la_buffer_t *buffer, const void *data, size_t length)
{
    if (la_buffer_ensure_capacity(buffer, buffer->size + length) != 0)
        return -1;
    memcpy(buffer->ptr + buffer->size, data, length);
    buffer->size += length;
    return 0;
}

int la_buffer_insert(la_buffer_t *buffer, size_t index, const void *data, size_t length)
{
    size_t remain;
    
    if (buffer->size > index)
        remain = buffer->size - index;
    else
        remain = 0;
    if (la_buffer_ensure_capacity(buffer, index + length + remain) != 0)
        return -1;
    if (remain > 0)
        memmove(buffer->ptr + index + length, buffer->ptr + index, remain);
    memcpy(buffer->ptr + index, data, length);
    return 0;
}

int la_buffer_overwrite(la_buffer_t *buffer, size_t index, const void *data, size_t length)
{
    if (la_buffer_ensure_capacity(buffer, index + length) != 0)
        return -1;
    memcpy(buffer->ptr + index, data, length);
    if (buffer->size < index + length)
        buffer->size = index + length;
    return 0;
}

int la_buffer_ensure_capacity(la_buffer_t *buffer, size_t capacity)
{
    if (buffer->capacity < capacity)
    {
        size_t delta = __cap(capacity - buffer->capacity);
        capacity = buffer->capacity + delta;
        void *p = realloc(buffer->ptr, capacity);
        if (p == NULL)
            return -1;
        buffer->ptr = p;
        buffer->capacity = capacity;
    }
    return 0;
}

int la_buffer_compact(la_buffer_t *buffer)
{
    if (buffer->capacity > buffer->size)
    {
        void *p = realloc(buffer->ptr, buffer->size);
        if (p == NULL)
            return -1;
        buffer->ptr = p;
        buffer->capacity = buffer->size;
    }
    return 0;
}

int la_buffer_truncate(la_buffer_t *buffer, size_t newsize)
{
    if (buffer->size < newsize)
    {
        errno = EINVAL;
        return -1;
    }
    buffer->size = newsize;
    return 0;
}

void la_buffer_destroy(la_buffer_t *buffer)
{
    free(buffer->ptr);
    free(buffer);
}