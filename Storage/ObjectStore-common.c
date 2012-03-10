//
//  ObjectStore-common.c
//  LoungeAct
//
//  Created by Casey Marshall on 2/23/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ObjectStore.h"

la_storage_object *
la_storage_create_object(const char *key, const la_storage_rev_t rev, const unsigned char *data, uint32_t length)
{
    size_t total_len = 0;
    la_storage_object *obj = (la_storage_object *) malloc(sizeof(struct la_storage_object));
    if (obj == NULL)
        return NULL;
    obj->key = strdup(key);
    if (obj->key == NULL)
    {
        free(obj);
        return NULL;
    }
    total_len = sizeof(struct la_storage_object_header) + length;
    obj->header = malloc(total_len);
    if (obj->header == NULL)
    {
        free(obj->key);
        free(obj);
        return NULL;
    }
    memset(obj->header, 0, total_len);
    memcpy(&obj->header->rev, &rev, LA_OBJECT_REVISION_LEN);
    memcpy(la_storage_object_get_data(obj), (const char *) data, length);
    obj->data_length = length;
    return (la_storage_object *) obj;
}

void
la_storage_destroy_object(la_storage_object *object)
{
    free(object->key);
    free(object->header);
    free(object);
}

int
la_storage_object_get_all_revs(const la_storage_object *object, la_storage_rev_t **revs)
{
    if (revs != NULL)
        *revs = &object->header->rev;
    return object->header->rev_count + 1;
}
