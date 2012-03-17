//
//  buffer.h
//  LoungeAct
//
//  Created by Casey Marshall on 2/29/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#ifndef LoungeAct_buffer_h
#define LoungeAct_buffer_h

#include <stdarg.h>
#include <sys/types.h>

typedef struct la_buffer la_buffer_t;

/**
 * Creates a new buffer.
 *
 * @param capacity The initial buffer capacity. The amount allocated may be
 *                 greater than this value, to round to a page size.
 */
la_buffer_t *la_buffer_new(size_t capacity);

/**
 * Wrap existing bytes in a byte buffer. The resulting buffer will have a
 * fixed capacity.
 *
 * @param ptr The pointer to the start of the bytes.
 * @param capacity The number of bytes available.
 * @param size The current size of valid bytes.
 */
la_buffer_t *la_buffer_wrap(void *ptr, size_t capacity, size_t size);

/**
 * Get the current size of the buffer (the number of valid bytes).
 *
 * @param buffer The buffer.
 * @return The size.
 */
size_t la_buffer_size(const la_buffer_t *buffer);
size_t la_buffer_capacity(const la_buffer_t *buffer);
void *la_buffer_data(const la_buffer_t *buffer);
void *la_buffer_copy(const la_buffer_t *buffer);
size_t la_buffer_get(const la_buffer_t *buffer, size_t offset, void *data, size_t length);

/**
 * Discards 'length' bytes starting from 'offset'.
 */
int la_buffer_remove(la_buffer_t *buffer, size_t offset, size_t length);
int la_buffer_append(la_buffer_t *buffer, const void *data, size_t length);
int la_buffer_insert(la_buffer_t *buffer, size_t index, const void *data, size_t length);
int la_buffer_overwrite(la_buffer_t *buffer, size_t index, const void *data, size_t length);
int la_buffer_move(la_buffer_t *buffer, size_t index, size_t newindex, size_t length);
int la_buffer_ensure_capacity(la_buffer_t *buffer, size_t capacity);

/**
 * Compact this buffer. Reallocates memory so the buffer's capacity is exactly
 * the buffer's current size.
 *
 * @param buffer The buffer.
 */
int la_buffer_compact(la_buffer_t *buffer);
int la_buffer_truncate(la_buffer_t *buffer, size_t newsize);
#define la_buffer_clear(buf) do { \
  la_buffer_truncate(buf, 0); \
  la_buffer_compact(buf); \
} while (0)
void la_buffer_destroy(la_buffer_t *buffer);

int la_buffer_appendf(la_buffer_t *buffer, const char *fmt, ...);
int la_buffer_vappendf(la_buffer_t *buffer, const char *fmt, va_list ap);

char *la_buffer_string(la_buffer_t *buffer);

#endif
