//
//  storagetest.c
//  LoungeAct
//
//  Created by Casey Marshall on 2/28/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ftw.h>
#include "../ObjectStore.h"

int cb1(const char *path, const struct stat *ptr, int flag, struct FTW *ftw)
{
    printf("cb1 %s %x\n", path, flag);
    if (flag == FTW_F)
    {
        if (unlink(path) != 0)
            fprintf(stderr, "failed to unlink %s: %s\n", path, strerror(errno));
    }
    return 0;
}

int cb2(const char *path, const struct stat *ptr, int flag, struct FTW *ftw)
{
    printf("cb2 %s %x\n", path, flag);
    if (flag == FTW_DP)
    {
        if (rmdir(path) != 0)
            fprintf(stderr, "failed to rmdir %s: %s\n", path, strerror(errno));
    }
    return 0;
}

static la_storage_env *env = NULL;
static la_storage_object_store *store = NULL;

void cleanup(void)
{
    printf("cleaning up...\n");
    if (store) la_storage_close(store);
    if (env) la_storage_close_env(env);
}

int main(int argc, char **argv)
{
    if (nftw("/tmp/storagetest", cb1, 10, FTW_DEPTH | FTW_PHYS) == 0)
        nftw("/tmp/storagetest", cb2, 10, FTW_DEPTH | FTW_PHYS);

    atexit(cleanup);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("storage test of driver %s\n", la_storage_driver());
    printf("opening environment... ");
    env = la_storage_open_env("/tmp/storagetest");
    if (env == NULL)
    {
        printf("FAIL\n");
        return 1;
    }
    printf("OK\n");
    printf("opening object store \"test\"... ");
    store = la_storage_open(env, "test");
    if (store == NULL)
    {
        printf("FAIL\n");
        return 2;
    }
    printf("OK\n");
    printf("creating an object...");
    const char *data = "{\"name\":\"newobject\"}";
    la_storage_rev_t one;
    memset(&one, 0, sizeof(la_storage_rev_t));
    one.rev[15] = 1;
    la_storage_object *object = la_storage_create_object("newobject", one, (const unsigned char *) data, (uint32_t) strlen(data), NULL, 0);
    if (object == NULL)
    {
        printf("FAIL\n");
        return 3;
    }
    printf("OK\n");
    
    printf("probing for object not there (1)... ");
    la_storage_object_get_result get;
    if ((get = la_storage_get(store, "not_there", NULL, NULL)) != LA_STORAGE_OBJECT_GET_NOT_FOUND)
    {
        printf("FAIL (%d)\n", get);
        return 1;
    }
    printf("OK\n");

    printf("probing for object not there (2)... ");
    if ((get = la_storage_get_rev(store, "not_there", NULL)) != LA_STORAGE_OBJECT_GET_NOT_FOUND)
    {
        printf("FAIL (%d)\n", get);
        return 1;
    }
    printf("OK\n");

    printf("adding the object... ");
    la_storage_object_put_result put;
    if ((put = la_storage_put(store, NULL, object)) != LA_STORAGE_OBJECT_PUT_SUCCESS)
    {
        printf("FAIL (%d)\n", put);
        return 4;
    }
    printf("OK\n");
    
    la_storage_object *getObject = NULL;
    printf("probing for added object (1)... ");
    if ((get = la_storage_get(store, "newobject", NULL, &getObject)) != LA_STORAGE_OBJECT_GET_OK)
    {
        printf("FAIL (%d)\n", get);
        return 1;
    }
    printf("OK\n");
    printf("checking if got object contents the same... ");
    if (getObject->data_length != strlen(data))
    {
        printf("data length %d vs %d", getObject->data_length, strlen(data));
        return -1;
    }
    if (memcmp(la_storage_object_get_data(getObject), data, strlen(data)) != 0)
    {
        printf("data mismatch");
        return -1;
    }
    printf("OK\n");
    la_storage_destroy_object(getObject);

    la_storage_rev_t revbuf;
    printf("probing for added object (2)... ");
    if ((get = la_storage_get_rev(store, "newobject", &revbuf)) != LA_STORAGE_OBJECT_GET_OK
        && memcmp(&revbuf, &one, sizeof(la_storage_rev_t)) != 0)
    {
        printf("FAIL (%d)\n", get);
        return 1;
    }
    printf("OK\n");
    
    printf("adding conflicting...");
    la_storage_rev_t zero;
    memset(&zero, 0, sizeof(la_storage_rev_t));
    if ((put = la_storage_put(store, &zero, object)) != LA_STORAGE_OBJECT_PUT_CONFLICT)
    {
        printf("FAIL (%d)\n", put);
        return 1;
    }
    printf("OK\n");
    
    la_storage_destroy_object(object);
    
    printf("creating another object... ");
    la_storage_rev_t two;
    memset(&two, 0, sizeof(la_storage_rev_t));
    two.rev[15] = 2;
    object = la_storage_create_object("newobject", two, (const unsigned char *) data, (uint32_t) strlen(data), NULL, 0);
    if (object == NULL)
    {
        printf("FAIL\n");
        return 1;
    }
    printf("OK\n");
    
    printf("overwriting an object... ");
    if ((put = la_storage_put(store, &one, object)) != LA_STORAGE_OBJECT_PUT_SUCCESS)
    {
        printf("FAIL (%d)\n", put);
        return 1;
    }
    printf("OK\n");
    
    printf("putting more objects... ");
    la_storage_destroy_object(object);
    object = la_storage_create_object("object1", one, "abcd", 4, NULL, 0);
    if ((put = la_storage_put(store, NULL, object)) != LA_STORAGE_OBJECT_PUT_SUCCESS)
    {
        printf("FAIL (%d)\n", put);
        return 1;
    }
    printf("OK ");
    la_storage_destroy_object(object);
    object = la_storage_create_object("object2", one, "abcd", 4, NULL, 0);
    if ((put = la_storage_put(store, NULL, object)) != LA_STORAGE_OBJECT_PUT_SUCCESS)
    {
        printf("FAIL (%d)\n", put);
        return 1;
    }
    printf("OK ");
    la_storage_destroy_object(object);
    object = la_storage_create_object("object3", one, "abcd", 4, NULL, 0);
    if ((put = la_storage_put(store, NULL, object)) != LA_STORAGE_OBJECT_PUT_SUCCESS)
    {
        printf("FAIL (%d)\n", put);
        return 1;
    }
    printf("OK\n");
    
    printf("iterating... ");
    la_storage_object_iterator *it = la_storage_iterator_open(store, 0);
    int ret;
    do {
        ret = la_storage_iterator_next(it, &object);
        if (ret == LA_STORAGE_OBJECT_ITERATOR_GOT_NEXT)
        {
            printf("%s(%llu, %s) ", object->key, object->header->seq, la_storage_object_get_data(object));
            la_storage_destroy_object(object);
        }
    } while (ret == LA_STORAGE_OBJECT_ITERATOR_GOT_NEXT);
    la_storage_iterator_close(it);
    if (ret != LA_STORAGE_OBJECT_ITERATOR_END)
    {
        printf("FAIL\n");
        return 1;
    }
    printf("OK\n");
    
    printf("lastseq is %llu\n", la_storage_lastseq(store));
    return 0;
}