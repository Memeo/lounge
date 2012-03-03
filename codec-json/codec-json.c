//
//  codec-json.c
//  LoungeAct
//
//  Created by Casey Marshall on 2/29/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdarg.h>

#include "../Codec/Codec.h"
#include "Codec-json.h"

/* We define this struct, but la_codec_value_t is really just
   aliased to json_t. */
struct la_codec_value
{
    json_t json;
};

const char *la_codec_name(void)
{
    return "JSON";
}

int la_codec_typeof(const la_codec_value_t *value)
{
    switch (json_typeof((json_t *) value))
    {
        case JSON_ARRAY: return LA_CODEC_ARRAY;
        case JSON_FALSE: return LA_CODEC_FALSE;
        case JSON_INTEGER: return LA_CODEC_INTEGER;
        case JSON_NULL: return LA_CODEC_NULL;
        case JSON_OBJECT: return LA_CODEC_OBJECT;
        case JSON_REAL: return LA_CODEC_REAL;
        case JSON_STRING: return LA_CODEC_STRING;
        case JSON_TRUE: return LA_CODEC_TRUE;
    }
    return -1;
}

int la_codec_is_object(const la_codec_value_t *value)
{
    return json_is_object((json_t *) value);
}

int la_codec_is_array(const la_codec_value_t *value)
{
    return json_is_array((json_t *) value);
}

int la_codec_is_string(const la_codec_value_t *value)
{
    return json_is_string((json_t *) value);
}

int la_codec_is_integer(const la_codec_value_t *value)
{
    return json_is_integer((json_t *) value);
}

int la_codec_is_real(const la_codec_value_t *value)
{
    return json_is_real((json_t *) value);
}

int la_codec_is_true(const la_codec_value_t *value)
{
    return json_is_true((json_t *) value);
}

int la_codec_is_false(const la_codec_value_t *value)
{
    return json_is_false((json_t *) value);
}

int la_codec_is_null(const la_codec_value_t *value)
{
    return json_is_null((json_t *) value);
}

int la_codec_is_number(const la_codec_value_t *value)
{
    return json_is_number((json_t *) value);
}

int la_codec_is_boolean(const la_codec_value_t *value)
{
    return json_is_boolean((json_t *) value);
}

la_codec_value_t *la_codec_incref(la_codec_value_t *value)
{
    return (la_codec_value_t *) json_incref((json_t *) value);
}

void la_codec_decref(la_codec_value_t *value)
{
    return json_decref((json_t *) value);
}

la_codec_value_t *la_codec_true(void)
{
    return (la_codec_value_t *) json_true();
}

la_codec_value_t *la_codec_false(void)
{
    return (la_codec_value_t *) json_false();
}

la_codec_value_t *la_codec_null(void)
{
    return (la_codec_value_t *) json_null();
}

/* Strings */

la_codec_value_t *la_codec_string(const char *value)
{
    return (la_codec_value_t *) json_string(value);
}

la_codec_value_t *la_codec_string_nocheck(const char *value)
{
    return (la_codec_value_t *) json_string_nocheck(value);
}

const char *la_codec_string_value(const la_codec_value_t *value)
{
    return json_string_value((json_t *) value);
}

int la_codec_string_set(la_codec_value_t *string, const char *value)
{
    return json_string_set((json_t *) string, value);
}

int la_codec_string_set_nocheck(la_codec_value_t *string, const char *value)
{
    return json_string_set_nocheck((json_t *) string, value);
}

la_codec_value_t *la_codec_integer(la_codec_int_t i)
{
    return (la_codec_value_t *) json_integer(i);
}

la_codec_int_t la_codec_integer_value(const la_codec_value_t *value)
{
    return json_integer_value((json_t *) value);
}

int la_codec_integer_set(la_codec_value_t *integer, la_codec_int_t value)
{
    return json_integer_set((json_t *) integer, value);
}

la_codec_value_t *la_codec_real(double value)
{
    return (la_codec_value_t *) json_real(value);
}

double la_codec_real_value(const la_codec_value_t *real)
{
    return json_real_value((const json_t *) real);
}

int la_codec_real_set(la_codec_value_t *real, double value)
{
    return json_real_set((json_t *) real, value);
}

double la_codec_number_value(const la_codec_value_t *number)
{
    return json_number_value((json_t *) number);
}

la_codec_value_t *la_codec_array(void)
{
    return (la_codec_value_t *) json_array();
}

size_t la_codec_array_size(const la_codec_value_t *array)
{
    return json_array_size((const json_t *) array);
}

la_codec_value_t *la_codec_array_get(const la_codec_value_t *array, size_t index)
{
    return (la_codec_value_t *) json_array_get((const json_t *) array, index);
}

int la_codec_array_set(la_codec_value_t *array, size_t index, la_codec_value_t *value)
{
    return json_array_set((json_t *) array, index, (json_t *) value);
}

int la_codec_array_set_new(la_codec_value_t *array, size_t index, la_codec_value_t *value)
{
    return json_array_set_new((json_t *) array, index, (json_t *) value);
}

int la_codec_array_append(la_codec_value_t *array, la_codec_value_t *value)
{
    return json_array_append((json_t *) array, (json_t *) value);
}

int la_codec_array_append_new(la_codec_value_t *array, la_codec_value_t *value)
{
    return json_array_append_new((json_t *) array, (json_t *) value);
}

int la_codec_array_insert(la_codec_value_t *array, size_t index, la_codec_value_t *value)
{
    return json_array_insert((json_t*) array, index, (json_t*) value);
}

int la_codec_array_insert_new(la_codec_value_t *array, size_t index, la_codec_value_t *value)
{
    return json_array_insert_new((json_t*) array, index, (json_t*) value);
}

int la_codec_array_remove(la_codec_value_t *array, size_t index)
{
    return json_array_remove((json_t*) array, index);
}

int la_codec_array_clear(la_codec_value_t *array)
{
    return json_array_clear((json_t*) array);
}

int la_codec_array_extend(la_codec_value_t *array, la_codec_value_t *other_array)
{
    return json_array_extend((json_t*) array, (json_t*) other_array);
}

size_t la_codec_object_size(const la_codec_value_t *object)
{
    return json_object_size((const json_t *) object);
}

la_codec_value_t *la_codec_object_get(const la_codec_value_t *object, const char *key)
{
    return (la_codec_value_t *) json_object_get((const json_t *) object, key);
}

int la_codec_object_set_new(la_codec_value_t *object, const char *key, la_codec_value_t *value)
{
    return json_object_set_new((json_t *) object, key, (json_t *) value);
}

int la_codec_object_set_new_nocheck(la_codec_value_t *object, const char *key, la_codec_value_t *value)
{
    return json_object_set_new_nocheck((json_t *) object, key, (json_t *) value);
}

int la_codec_object_del(la_codec_value_t *object, const char *key)
{
    return json_object_del((json_t *) object, key);
}

int la_codec_object_clear(la_codec_value_t *object)
{
    return json_object_clear((json_t *) object);
}

int la_codec_object_update(la_codec_value_t *object, la_codec_value_t *other)
{
    return json_object_update((json_t *) object, (json_t *) other);
}

int la_codec_object_update_existing(la_codec_value_t *object, la_codec_value_t *other)
{
    return json_object_update_existing((json_t *) object, (json_t *) other);
}

int la_codec_object_update_missing(la_codec_value_t *object, la_codec_value_t *other)
{
    return json_object_update_missing((json_t *) object, (json_t *) other);
}

void *la_codec_object_iter(la_codec_value_t *object)
{
    return json_object_iter((json_t *) object);
}

void *la_codec_object_iter_at(la_codec_value_t *object, const char *key)
{
    return json_object_iter_at((json_t *) object, key);
}

void *la_codec_object_key_to_iter(const char *key)
{
    return json_object_key_to_iter(key);
}

void *la_codec_object_iter_next(la_codec_value_t *object, void *iter)
{
    return json_object_iter_next((json_t *) object, iter);
}

const char *la_codec_object_iter_key(void *iter)
{
    return json_object_iter_key(iter);
}

la_codec_value_t *la_codec_object_iter_value(void *iter)
{
    return (la_codec_value_t *) json_object_iter_value(iter);
}

int la_codec_object_iter_set_new(la_codec_value_t *object, void *iter, la_codec_value_t *value)
{
    return json_object_iter_set_new((json_t *) object, iter, (json_t *) value);
}

int la_codec_object_set(la_codec_value_t *object, const char *key, la_codec_value_t *value)
{
    return json_object_set((json_t *) object, key, (json_t *) value);
}

int la_codec_object_set_nocheck(la_codec_value_t *object, const char *key, la_codec_value_t *value)
{
    return json_object_set_nocheck((json_t *) object, key, (json_t *) value);
}

int la_codec_object_iter_set(la_codec_value_t *object, void *iter, la_codec_value_t *value)
{
    return json_object_iter_set((json_t *) object, iter, (json_t *) value);
}

la_codec_value_t *la_codec_pack(const char *fmt, ...)
{
    la_codec_value_t *ret;
    va_list ap;
    va_start(ap, fmt);
    ret = (la_codec_value_t *) json_vpack_ex(NULL, 0, fmt, ap);
    va_end(ap);
    return ret;
}

la_codec_value_t *la_codec_pack_ex(la_codec_error_t *error, size_t flags, const char *fmt, ...)
{
    la_codec_value_t *ret;
    va_list ap;
    va_start(ap, fmt);
    ret = (la_codec_value_t *) json_vpack_ex((json_error_t *) error, flags, fmt, ap);
    va_end(ap);
    return ret;
}

la_codec_value_t *la_codec_vpack_ex(la_codec_error_t *error, size_t flags, const char *fmt, va_list ap)
{
    return (la_codec_value_t *) json_vpack_ex((json_error_t *) error, flags, fmt, ap);
}

int la_codec_unpack(la_codec_value_t *root, const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = json_vunpack_ex((json_t *) root, NULL, 0, fmt, ap);
    va_end(ap);
    return ret;
}

int la_codec_unpack_ex(la_codec_value_t *root, la_codec_error_t *error, size_t flags, const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = json_vunpack_ex((json_t *) root, (json_error_t *) error, flags, fmt, ap);
    va_end(ap);
    return ret;
}

int la_codec_vunpack_ex(la_codec_value_t *root, la_codec_error_t *error, size_t flags, const char *fmt, va_list ap)
{
    return json_vunpack_ex((json_t *) root, (json_error_t *) error, flags, fmt, ap);
}

int la_codec_equal(la_codec_value_t *value1, la_codec_value_t *value2)
{
    return json_equal((json_t *) value1, (json_t *) value2);
}

la_codec_value_t *la_codec_copy(la_codec_value_t *value)
{
    return (la_codec_value_t *) json_copy((json_t *) value);
}

la_codec_value_t *la_codec_deep_copy(la_codec_value_t *value)
{
    return (la_codec_value_t *) json_deep_copy((json_t *) value);
}

la_codec_value_t *la_codec_loads(const char *input, size_t flags, la_codec_error_t *error)
{
    return (la_codec_value_t *) json_loads(input, flags, (json_error_t *) error);
}

la_codec_value_t *la_codec_loadb(const char *buffer, size_t buflen, size_t flags, la_codec_error_t *error)
{
    return (la_codec_value_t *) json_loadb(buffer, buflen, flags, (json_error_t *) error);
}

la_codec_value_t *la_codec_loadf(FILE *input, size_t flags, la_codec_error_t *error)
{
    return (la_codec_value_t *) json_loadf(input, flags, (json_error_t *) error);
}

la_codec_value_t *la_codec_load_file(const char *path, size_t flags, la_codec_error_t *error)
{
    return (la_codec_value_t *) json_load_file(path, flags, (json_error_t *) error);
}

char *la_codec_dumps(const la_codec_value_t *json, size_t flags)
{
    return json_dumps((const json_t *) json, flags);
}

int la_codec_dumpf(const la_codec_value_t *json, FILE *output, size_t flags)
{
    return json_dumpf((const json_t *) json, output, flags);
}

int la_codec_dump_file(const la_codec_value_t *json, const char *path, size_t flags)
{
    return json_dump_file((const json_t *) json, path, flags);
}

int la_codec_dump_callback(const la_codec_value_t *json, la_codec_dump_callback_t callback, void *data, size_t flags)
{
    return json_dump_callback((const json_t *) json, callback, data, flags);
}

la_codec_value_t *la_codec_object(void)
{
    return (la_codec_value_t *) json_object();
}