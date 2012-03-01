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

LAStorageObject *
la_storage_create_object(const char *key, const char *rev, const unsigned char *data, uint32_t length)
{
    size_t revlen = strlen(rev);
    LAStorageObject *obj = (LAStorageObject *) malloc(sizeof(struct LAStorageObject));
    if (obj == NULL)
        return NULL;
    obj->key = strdup(key);
    if (obj->key == NULL)
    {
        free(obj);
        return NULL;
    }
    obj->header = malloc(sizeof(struct LAStorageObjectHeader) + revlen + length + 1);
    if (obj->header == NULL)
    {
        free(obj->key);
        free(obj);
        return NULL;
    }
    obj->header->seq = 0;
    memset(obj->header->rev_data, 0, revlen + length + 1);
    memcpy(obj->header->rev_data, rev, revlen + 1);
    memcpy(la_storage_object_get_data(obj), (const char *) data, length);
    obj->data_length = length;
    return (LAStorageObject *) obj;
}

void
la_storage_destroy_object(LAStorageObject *object)
{
    free(object->key);
    free(object->header);
    free(object);
}
