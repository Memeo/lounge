//
//  Codec.h
//  LoungeAct
//
//  Created by Casey Marshall on 2/27/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#ifndef LoungeAct_Codec_h
#define LoungeAct_Codec_h

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>

/*
 * This API borrows much from the jansson API, which is the
 * first and most important implementation of this API.
 */

const char *la_codec_name(void);

typedef struct la_codec_value la_codec_value_t;
typedef long long la_codec_int_t;

typedef enum
{
    LA_CODEC_OBJECT,
    LA_CODEC_ARRAY,
    LA_CODEC_STRING,
    LA_CODEC_INTEGER,
    LA_CODEC_REAL,
    LA_CODEC_TRUE,
    LA_CODEC_FALSE,
    LA_CODEC_NULL
} la_codec_type;

int la_codec_typeof(const la_codec_value_t *value);
int la_codec_is_object(const la_codec_value_t *value);
int la_codec_is_array(const la_codec_value_t *value);
int la_codec_is_string(const la_codec_value_t *value);
int la_codec_is_integer(const la_codec_value_t *value);
int la_codec_is_real(const la_codec_value_t *value);
int la_codec_is_true(const la_codec_value_t *value);
int la_codec_is_false(const la_codec_value_t *value);
int la_codec_is_null(const la_codec_value_t *value);
int la_codec_is_number(const la_codec_value_t *value);
int la_codec_is_boolean(const la_codec_value_t *value);

la_codec_value_t *la_codec_object(void);
la_codec_value_t *la_codec_array(void);
la_codec_value_t *la_codec_string(const char *value);
la_codec_value_t *la_codec_string_nocheck(const char *value);
la_codec_value_t *la_codec_integer(la_codec_int_t value);
la_codec_value_t *la_codec_real(double value);
la_codec_value_t *la_codec_true(void);
la_codec_value_t *la_codec_false(void);
la_codec_value_t *la_codec_null(void);

la_codec_value_t *la_codec_incref(la_codec_value_t *value);
void la_codec_decref(la_codec_value_t *value);

#define LA_CODEC_ERROR_TEXT_LENGTH    160
#define LA_CODEC_ERROR_SOURCE_LENGTH   80

typedef struct {
    int line;
    int column;
    int position;
    char source[LA_CODEC_ERROR_SOURCE_LENGTH];
    char text[LA_CODEC_ERROR_TEXT_LENGTH];
} la_codec_error_t;

size_t la_codec_object_size(const la_codec_value_t *object);
la_codec_value_t *la_codec_object_get(const la_codec_value_t *object, const char *key);
int la_codec_object_set_new(la_codec_value_t *object, const char *key, la_codec_value_t *value);
int la_codec_object_set_new_nocheck(la_codec_value_t *object, const char *key, la_codec_value_t *value);
int la_codec_object_del(la_codec_value_t *object, const char *key);
int la_codec_object_clear(la_codec_value_t *object);
int la_codec_object_update(la_codec_value_t *object, la_codec_value_t *other);
int la_codec_object_update_existing(la_codec_value_t *object, la_codec_value_t *other);
int la_codec_object_update_missing(la_codec_value_t *object, la_codec_value_t *other);
void *la_codec_object_iter(la_codec_value_t *object);
void *la_codec_object_iter_at(la_codec_value_t *object, const char *key);
void *la_codec_object_key_to_iter(const char *key);
void *la_codec_object_iter_next(la_codec_value_t *object, void *iter);
const char *la_codec_object_iter_key(void *iter);
la_codec_value_t *la_codec_object_iter_value(void *iter);
int la_codec_object_iter_set_new(la_codec_value_t *object, void *iter, la_codec_value_t *value);

#define la_codec_object_foreach(object, key, value) \
    for(key = la_codec_object_iter_key(la_codec_object_iter(object)); \
        key && (value = la_codec_object_iter_value(la_codec_object_key_to_iter(key))); \
        key = la_codec_object_iter_key(la_codec_object_iter_next(object, la_codec_object_key_to_iter(key))))

int la_codec_object_set(la_codec_value_t *object, const char *key, la_codec_value_t *value);
int la_codec_object_set_nocheck(la_codec_value_t *object, const char *key, la_codec_value_t *value);
int la_codec_object_iter_set(la_codec_value_t *object, void *iter, la_codec_value_t *value);

size_t la_codec_array_size(const la_codec_value_t *array);
la_codec_value_t *la_codec_array_get(const la_codec_value_t *array, size_t index);
int la_codec_array_set_new(la_codec_value_t *array, size_t index, la_codec_value_t *value);
int la_codec_array_append_new(la_codec_value_t *array, la_codec_value_t *value);
int la_codec_array_insert_new(la_codec_value_t *array, size_t index, la_codec_value_t *value);
int la_codec_array_remove(la_codec_value_t *array, size_t index);
int la_codec_array_clear(la_codec_value_t *array);
int la_codec_array_extend(la_codec_value_t *array, la_codec_value_t *other);
int la_codec_array_set(la_codec_value_t *array, size_t index, la_codec_value_t *value);
int la_codec_array_append(la_codec_value_t *array, la_codec_value_t *value);
int la_codec_array_insert(la_codec_value_t *array, size_t index, la_codec_value_t *value);

const char *la_codec_string_value(const la_codec_value_t *string);
la_codec_int_t la_codec_integer_value(const la_codec_value_t *integer);
double la_codec_real_value(const la_codec_value_t *real);
double la_codec_number_value(const la_codec_value_t *json);

int la_codec_string_set(la_codec_value_t *string, const char *value);
int la_codec_string_set_nocheck(la_codec_value_t *string, const char *value);
int la_codec_integer_set(la_codec_value_t *integer, la_codec_int_t value);
int la_codec_real_set(la_codec_value_t *real, double value);

la_codec_value_t *la_codec_pack(const char *fmt, ...);
la_codec_value_t *la_codec_pack_ex(la_codec_error_t *error, size_t flags, const char *fmt, ...);
la_codec_value_t *la_codec_vpack_ex(la_codec_error_t *error, size_t flags, const char *fmt, va_list ap);

#define LA_CODEC_VALIDATE_ONLY  0x1
#define LA_CODEC_STRICT         0x2

int la_codec_unpack(la_codec_value_t *root, const char *fmt, ...);
int la_codec_unpack_ex(la_codec_value_t *root, la_codec_error_t *error, size_t flags, const char *fmt, ...);
int la_codec_vunpack_ex(la_codec_value_t *root, la_codec_error_t *error, size_t flags, const char *fmt, va_list ap);

int la_codec_equal(la_codec_value_t *value1, la_codec_value_t *value2);

la_codec_value_t *la_codec_copy(la_codec_value_t *value);
la_codec_value_t *la_codec_deep_copy(la_codec_value_t *value);

#define LA_CODEC_REJECT_DUPLICATES 0x1
#define LA_CODEC_DISABLE_EOF_CHECK 0x2
#define LA_CODEC_DECODE_ANY        0x4

la_codec_value_t *la_codec_loads(const char *input, size_t flags, la_codec_error_t *error);
la_codec_value_t *la_codec_loadb(const char *buffer, size_t buflen, size_t flags, la_codec_error_t *error);
la_codec_value_t *la_codec_loadf(FILE *input, size_t flags, la_codec_error_t *error);
la_codec_value_t *la_codec_load_file(const char *path, size_t flags, la_codec_error_t *error);

#define LA_CODEC_INDENT(n)      (n & 0x1F)
#define LA_CODEC_COMPACT        0x20
#define LA_CODEC_ENSURE_ASCII   0x40
#define LA_CODEC_SORT_KEYS      0x80
#define LA_CODEC_PRESERVE_ORDER 0x100
#define LA_CODEC_ENCODE_ANY     0x200

typedef int (*la_codec_dump_callback_t)(const char *buffer, size_t size, void *data);

char *la_codec_dumps(const la_codec_value_t *json, size_t flags);
int la_codec_dumpf(const la_codec_value_t *json, FILE *output, size_t flags);
int la_codec_dump_file(const la_codec_value_t *json, const char *path, size_t flags);
int la_codec_dump_callback(const la_codec_value_t *json, la_codec_dump_callback_t callback, void *data, size_t flags);

la_codec_value_t *la_codec_from_json(void /* json_t */ *json);

#endif
