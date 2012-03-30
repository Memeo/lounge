//
//  codec-bson.c
//  LoungeAct
//
//  Created by Casey Marshall on 3/28/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../Codec/Codec.h"
#include "jansson.h"

#define HASH_ENTRY_TYPE la_codec_value_t
#define HASH_INCREF la_codec_incref
#define HASH_DECREF la_codec_decref
#include "hashtable.h" /* Reuse jansson's hashtable. */

typedef enum
{
    BSON_EOO        = 0x00,
    BSON_DOUBLE     = 0x01,
    BSON_STRING     = 0x02,
    BSON_DOC        = 0x03,
    BSON_ARRAY      = 0x04,
    BSON_BINARY     = 0x05,
    BSON_OBJECTID   = 0x07,
    BSON_BOOLEAN    = 0x08,
    BSON_DATETIME   = 0x09,
    BSON_NULL       = 0x0A,
    BSON_REGEX      = 0x0B,
    BSON_CODE       = 0x0D,
    BSON_SYMBOL     = 0x0E,
    BSON_CODE_SCOPE = 0x0F,
    BSON_INTEGER    = 0x10,
    BSON_TIMESTAMP  = 0x11,
    BSON_INT64      = 0x12
} bson_type;

const char *la_codec_name(void)
{
    return "BSON";
}

struct la_codec_value
{
    la_codec_type type;
    union
    {
        la_codec_int_t integer;
        double real;
        char *string;
        struct {
            la_codec_value_t **ptr;
            size_t count;
        } array;
        hashtable_t hash;
    } val;
    size_t refcount;
};

int la_codec_typeof(const la_codec_value_t *value)
{
    return value->type;
}

int la_codec_is_object(const la_codec_value_t *value)
{
    return la_codec_typeof(value) == LA_CODEC_OBJECT;
}

int la_codec_is_array(const la_codec_value_t *value)
{
    return la_codec_typeof(value) == LA_CODEC_ARRAY;
}

int la_codec_is_string(const la_codec_value_t *value)
{
    return la_codec_typeof(value) == LA_CODEC_STRING;
}

int la_codec_is_integer(const la_codec_value_t *value)
{
    return la_codec_typeof(value) == LA_CODEC_INTEGER;
}

int la_codec_is_real(const la_codec_value_t *value)
{
    return la_codec_typeof(value) == LA_CODEC_REAL;
}

int la_codec_is_true(const la_codec_value_t *value)
{
    return la_codec_typeof(value) == LA_CODEC_TRUE;
}

int la_codec_is_false(const la_codec_value_t *value)
{
    return la_codec_typeof(value) == LA_CODEC_FALSE;
}

int la_codec_is_null(const la_codec_value_t *value)
{
    return la_codec_typeof(value) == LA_CODEC_NULL;
}

int la_codec_is_number(const la_codec_value_t *value)
{
    return la_codec_is_integer(value) || la_codec_is_real(value);
}

int la_codec_is_boolean(const la_codec_value_t *value)
{
    return la_codec_is_true(value) || la_codec_is_false(value);
}

la_codec_value_t *la_codec_object(void)
{
    la_codec_value_t *value = (la_codec_value_t *) malloc(sizeof(struct la_codec_value));
    if (value == NULL)
        return NULL;
    memset(value, 0, sizeof(struct la_codec_value));
    
    value->type = LA_CODEC_OBJECT;
    hashtable_init(&value->val.hash);
    return la_codec_incref(value);
}

la_codec_value_t *la_codec_array(void)
{
    la_codec_value_t *value = (la_codec_value_t *) malloc(sizeof(struct la_codec_value));
    if (value == NULL)
        return NULL;
    memset(value, 0, sizeof(struct la_codec_value));
    
    value->type = LA_CODEC_ARRAY;
    value->val.array.ptr = NULL;
    value->val.array.count = 0;
    return la_codec_incref(value);
}

la_codec_value_t *la_codec_string(const char *str)
{
    la_codec_value_t *value = (la_codec_value_t *) malloc(sizeof(struct la_codec_value));
    if (value == NULL)
        return NULL;
    memset(value, 0, sizeof(struct la_codec_value));

    value->type = LA_CODEC_STRING;
    if (str != NULL)
    {
        value->val.string = strdup(str);
        if (value->val.string == NULL)
        {
            free(value);
            return NULL;
        }
    }
    else
        value->val.string = NULL;
    return la_codec_incref(value);
}

la_codec_value_t *la_codec_string_nocheck(const char *value)
{
    return la_codec_string(value);
}

la_codec_value_t *la_codec_integer(la_codec_int_t i)
{
    la_codec_value_t *value = (la_codec_value_t *) malloc(sizeof(struct la_codec_value));
    if (value == NULL)
        return NULL;
    memset(value, 0, sizeof(struct la_codec_value));

    value->type = LA_CODEC_INTEGER;
    value->val.integer = i;
    return la_codec_incref(value);
}

la_codec_value_t *la_codec_real(double d)
{
    la_codec_value_t *value = (la_codec_value_t *) malloc(sizeof(struct la_codec_value));
    if (value == NULL)
        return NULL;
    memset(value, 0, sizeof(struct la_codec_value));
    
    value->type = LA_CODEC_REAL;
    value->val.real = d;
    return la_codec_incref(value);
}

la_codec_value_t *la_codec_true(void)
{
    static la_codec_value_t the_true = { LA_CODEC_TRUE, 0, -1 };
    return &the_true;
}

la_codec_value_t *la_codec_false(void)
{
    static la_codec_value_t the_false = { LA_CODEC_FALSE, 0, -1 };
    return &the_false;
}

la_codec_value_t *la_codec_null(void)
{
    static la_codec_value_t the_null = { LA_CODEC_NULL, 0, -1 };
    return &the_null;
}

la_codec_value_t *la_codec_incref(la_codec_value_t *value)
{
    if (value && value->refcount != (size_t) -1)
        value->refcount++;
    return value;
}

void la_codec_delete(la_codec_value_t *value)
{
    int i;
    switch (value->type)
    {
        case LA_CODEC_OBJECT:
        {
            hashtable_close(&value->val.hash);
            free(value);
        }
            break;
            
        case LA_CODEC_ARRAY:
            for (i = 0; i < value->val.array.count; i++)
                la_codec_decref(value->val.array.ptr[i]);
            if (value->val.array.ptr != NULL)
                free(value->val.array.ptr);
            free(value);
            break;
            
        case LA_CODEC_STRING:
            if (value->val.string != NULL)
                free(value->val.string);
            free(value);
            break;
            
        case LA_CODEC_INTEGER:
        case LA_CODEC_REAL:
            free(value);
            break;
            
        case LA_CODEC_TRUE:
        case LA_CODEC_FALSE:
        case LA_CODEC_NULL:
            break;
    }
    free(value);
}

void la_codec_decref(la_codec_value_t *value)
{
    if (value && value->refcount != (size_t) -1 && --value->refcount == 0)
        la_codec_delete(value);
}

size_t la_codec_object_size(const la_codec_value_t *object)
{
    if (la_codec_is_object(object))
    {
        int i = 0;
        for (void *it = hashtable_iter(&object->val.hash); it != NULL; it = hashtable_iter_next(&object->val.hash, it))
            i++;
        return i;
    }
    return -1;
}

la_codec_value_t *la_codec_object_get(const la_codec_value_t *object, const char *key)
{
    if (la_codec_is_object(object))
    {
        void *p = hashtable_get(&object->val.hash, key);
        if (p != NULL)
            return (la_codec_value_t *) p;
    }
    return NULL;
}

int la_codec_object_set_new(la_codec_value_t *object, const char *key, la_codec_value_t *value)
{
    if (la_codec_is_object(object))
    {
        hashtable_set(&object->val.hash, key, 0, value);
    }
    return -1;
}

int la_codec_object_set_new_nocheck(la_codec_value_t *object, const char *key, la_codec_value_t *value)
{
    hashtable_set(&object->val.hash, key, 0, value);
    return 0;
}

int la_codec_object_del(la_codec_value_t *object, const char *key)
{
    if (la_codec_is_object(object))
    {
        hashtable_del(object, key);
        return 0;
    }
    return -1;
}

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

int la_codec_unpack(la_codec_value_t *root, const char *fmt, ...);
int la_codec_unpack_ex(la_codec_value_t *root, la_codec_error_t *error, size_t flags, const char *fmt, ...);
int la_codec_vunpack_ex(la_codec_value_t *root, la_codec_error_t *error, size_t flags, const char *fmt, va_list ap);

int la_codec_equal(la_codec_value_t *value1, la_codec_value_t *value2);

la_codec_value_t *la_codec_copy(la_codec_value_t *value);
la_codec_value_t *la_codec_deep_copy(la_codec_value_t *value);

la_codec_value_t *la_codec_loads(const char *input, size_t flags, la_codec_error_t *error);
la_codec_value_t *la_codec_loadb(const char *buffer, size_t buflen, size_t flags, la_codec_error_t *error);
la_codec_value_t *la_codec_loadf(FILE *input, size_t flags, la_codec_error_t *error);
la_codec_value_t *la_codec_load_file(const char *path, size_t flags, la_codec_error_t *error);

enum
{
    O32_LITTLE_ENDIAN = 0x03020100ul,
    O32_BIG_ENDIAN = 0x00010203ul,
    O32_PDP_ENDIAN = 0x01000302ul
};

static const union { unsigned char bytes[4]; uint32_t value; } o32_host_order = { { 0, 1, 2, 3 } };

#define O32_HOST_ORDER (o32_host_order.value)

static inline int itole(int i)
{
    switch (O32_HOST_ORDER)
    {
        case O32_LITTLE_ENDIAN:
            return i;
            
        case O32_LITTLE_ENDIAN:
            return ((i >> 24) & 0xFF) | ((i >> 16) & 0xFF00) | ((i << 16) & 0xFF0000) | ((i << 24) & 0xFF000000);
            
        case O32_PDP_ENDIAN:
            return ((i >> 16) & 0xFF) | ((i >> 24) & 0xFF00) | ((i << 24) & 0xFF0000) | ((i << 16) & 0xFF000000);
    }
}

char *la_codec_dumps(const la_codec_value_t *json, size_t flags)
{
    la_buffer_t *buffer = la_buffer_create();
}

int la_codec_dumpf(const la_codec_value_t *json, FILE *output, size_t flags);
int la_codec_dump_file(const la_codec_value_t *json, const char *path, size_t flags);

void do_dump(const char *key, la_codec_value_t *value, la_codec_dump_callback_t callback, void *data)
{
    char t;
    switch (value->type)
    {
        case LA_CODEC_NULL:
            t = BSON_NULL;
            callback(&t, 1, data);
            callback(key, strlen(key) + 1, data);
            break;
            
        case LA_CODEC_TRUE:
        case LA_CODEC_FALSE:
            t = BSON_BOOLEAN;
            callback(&t, 1, data);
            callback(key, strlen(key) + 1, data);
            if (value->type == LA_CODEC_TRUE)
                t = 1;
            else
                t = 0;
            callback(&t, 1, data);
            break;
            
        case LA_CODEC_REAL:
            t = BSON_DOUBLE;
            callback(&t, 1, data);
            callback(key, strlen(key) + 1, data);
            // TODO
            break;
            
        case LA_CODEC_INTEGER:
        {
            int x = itole(value->val.integer);
            t = BSON_INTEGER;
            callback(&t, 1, data);
            callback(key, strlen(key) + 1, data);
            callback((char *) &x, 4, data);
            break;
        }
            
        case LA_CODEC_STRING:
        {
            int n = itole(strlen(value->val.string));
            t = BSON_STRING;
            callback(&t, 1, data);
            callback(key, strlen(key) + 1, data);
            callback((char *) &n, 4, data);
            callback(value->val.string, strlen(value->val.string), data);
            t = 0;
            callback(&t, 1, data);
            break;
        }
            
        case LA_CODEC_ARRAY:
        {
            
        }
            
        case LA_CODEC_OBJECT:
        {
            
        }
    }
}

int la_codec_dump_callback(const la_codec_value_t *value, la_codec_dump_callback_t callback, void *data, size_t flags)
{
    char t;
    char buffer[21];
    if (la_codec_is_object(value))
    {
        const char *key;
        la_codec_value_t *value;
        la_codec_object_foreach(value, key, value)
        {
        }
    }
    else if (la_codec_is_array(value))
    {
        
    }
    else
    {
        return -1;
    }
}

la_codec_value_t *la_codec_from_json(void /* json_t */ *p)
{
    json_t *json = (json_t *) p;
    switch (json_typeof(json))
    {
        case JSON_NULL:
            return la_codec_null();
            
        case JSON_TRUE:
            return la_codec_true();
            
        case JSON_FALSE:
            return la_codec_false();
            
        case JSON_STRING:
            return la_codec_string(json_string_value(json));
        
        case JSON_INTEGER:
            return la_codec_integer(json_integer_value(json));
            
        case JSON_REAL:
            return la_codec_real(json_real_value(json));
            
        case JSON_ARRAY:
        {
            int i;
            int count = json_array_size(json);
            la_codec_value_t *array = la_codec_array();
            for (i = 0; i < count; i++)
            {
                la_codec_array_append_new(array, la_codec_from_json(json_array_get(json, i)));
            }
            return array;
        }
        
        case JSON_OBJECT:
        {
            const char *key;
            const json_t *value;
            la_codec_value_t *object = la_codec_object();
            json_object_foreach(json, key, value)
            {
                la_codec_object_set(object, key, la_codec_from_json(value));
            }
            return object;
        }
            
        default:
            break;
    }
}
