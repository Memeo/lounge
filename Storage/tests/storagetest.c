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

static LAStorageEnvironment *env = NULL;
static LAStorageObjectStore *store = NULL;

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
    LAStorageObject *object = la_storage_create_object("newobject", "1", (const unsigned char *) data, (uint32_t) strlen(data));
    if (object == NULL)
    {
        printf("FAIL\n");
        return 3;
    }
    printf("OK\n");
    
    printf("probing for object not there (1)... ");
    LAStorageObjectGetResult get;
    if ((get = la_storage_get(store, "not_there", NULL, NULL)) != LAStorageObjectGetNotFound)
    {
        printf("FAIL (%d)\n", get);
        return 1;
    }
    printf("OK\n");

    printf("probing for object not there (2)... ");
    if ((get = la_storage_get_rev(store, "not_there", NULL, 0)) != LAStorageObjectGetNotFound)
    {
        printf("FAIL (%d)\n", get);
        return 1;
    }
    printf("OK\n");

    printf("adding the object... ");
    LAStorageObjectPutResult put;
    if ((put = la_storage_put(store, NULL, object)) != LAStorageObjectPutSuccess)
    {
        printf("FAIL (%d)\n", put);
        return 4;
    }
    printf("OK\n");
    
    LAStorageObject *getObject = NULL;
    printf("probing for added object (1)... ");
    if ((get = la_storage_get(store, "newobject", NULL, &getObject)) != LAStorageObjectGetSuccess)
    {
        printf("FAIL (%d)\n", get);
        return 1;
    }
    printf("OK\n");
    la_storage_destroy_object(getObject);

    char revbuf[256];
    revbuf[0] = '\0';
    printf("probing for added object (2)... ");
    if ((get = la_storage_get_rev(store, "newobject", revbuf, 255)) != LAStorageObjectGetSuccess
        && strcmp(revbuf, "1") != 0)
    {
        printf("FAIL (%d)\n", get);
        return 1;
    }
    printf("OK\n");
    
    printf("adding conflicting...");
    if ((put = la_storage_put(store, "0", object)) != LAStorageObjectPutConflict)
    {
        printf("FAIL (%d)\n", put);
        return 1;
    }
    printf("OK\n");
    
    la_storage_destroy_object(object);
    
    printf("creating another object... ");
    object = la_storage_create_object("newobject", "2", (const unsigned char *) data, (uint32_t) strlen(data));
    if (object == NULL)
    {
        printf("FAIL\n");
        return 1;
    }
    printf("OK\n");
    
    printf("overwriting an object... ");
    if ((put = la_storage_put(store, "1", object)) != LAStorageObjectPutSuccess)
    {
        printf("FAIL (%d)\n", put);
        return 1;
    }
    printf("OK\n");
    
    printf("putting more objects... ");
    la_storage_destroy_object(object);
    object = la_storage_create_object("object1", "1", "abcd", 4);
    if ((put = la_storage_put(store, NULL, object)) != LAStorageObjectPutSuccess)
    {
        printf("FAIL (%d)\n", put);
        return 1;
    }
    printf("OK ");
    la_storage_destroy_object(object);
    object = la_storage_create_object("object2", "1", "abcd", 4);
    if ((put = la_storage_put(store, NULL, object)) != LAStorageObjectPutSuccess)
    {
        printf("FAIL (%d)\n", put);
        return 1;
    }
    printf("OK ");
    la_storage_destroy_object(object);
    object = la_storage_create_object("object3", "1", "abcd", 4);
    if ((put = la_storage_put(store, NULL, object)) != LAStorageObjectPutSuccess)
    {
        printf("FAIL (%d)\n", put);
        return 1;
    }
    printf("OK\n");
    
    printf("iterating... ");
    LAStorageObjectIterator *it = la_storage_iterator_open(store, 0);
    int ret;
    do {
        ret = la_storage_iterator_next(it, &object);
        if (ret == LAStorageObjectIteratorGotNext)
        {
            printf("%s(%llu, %s) ", object->key, object->header->seq, object->header->rev_data);
            la_storage_destroy_object(object);
        }
    } while (ret == LAStorageObjectIteratorGotNext);
    la_storage_iterator_close(it);
    if (ret != LAStorageObjectIteratorEnd)
    {
        printf("FAIL\n");
        return 1;
    }
    printf("OK\n");
    
    printf("lastseq is %llu\n", la_storage_lastseq(store));
    return 0;
}