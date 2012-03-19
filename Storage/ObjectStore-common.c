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
#include "../utils/stringutils.h"
#include "../utils/utils.h"
#include "../utils/hexdump.h"

#include "ObjectStore.h"

la_storage_object *
la_storage_create_object(const char *key, const la_storage_rev_t rev, const unsigned char *data, uint32_t length,
                         const la_storage_rev_t *revs, size_t revcount)
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
    total_len = sizeof(struct la_storage_object_header) + length + (revcount * sizeof(la_storage_rev_t));
    obj->header = malloc(total_len);
    if (obj->header == NULL)
    {
        free(obj->key);
        free(obj);
        return NULL;
    }
    memset(obj->header, 0, total_len);
    memcpy(&obj->header->rev, &rev, LA_OBJECT_REVISION_LEN);
    if (revs != NULL)
    {
        obj->header->rev_count = revcount;
        memcpy(obj->header->revs_data, revs, revcount * sizeof(la_storage_rev_t));
    }
    else
    {
        obj->header->rev_count = 0;
    }
    memcpy(la_storage_object_get_data(obj), (const char *) data, length);
    obj->data_length = length;
#if DEBUG
    printf("total_len: %zu (header:%lu, length: %d, revs size: %lu)\n", total_len, sizeof(struct la_storage_object_header),
           length, revcount * sizeof(la_storage_rev_t));
    printf("created object (%d revs, %d bytes):\n", obj->header->rev_count, obj->data_length);
    la_hexdump(obj->header, la_storage_object_total_size(obj));
#endif
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

int
la_storage_scan_rev(const char *str, la_storage_rev_t *rev)
{
    return string_unhex(str, rev->rev, LA_OBJECT_REVISION_LEN);
}

int la_storage_revs_overlap(const la_storage_rev_t *revs1, int count1, const la_storage_rev_t *revs2, int count2)
{
    int i;
    
    for (i = 0; i < count2 - 1; i++)
    {
        if (memcmp(revs1, &revs2[i], sizeof(la_storage_rev_t) * la_min(count1, count2 - i)) == 0)
            return 1;
    }
    
    return 0;
}

