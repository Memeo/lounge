//
//  apitest.c
//  LoungeAct
//
//  Created by Casey Marshall on 3/1/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdio.h>
#include <ftw.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include <API/LoungeAct.h>

int cb1(const char *path, const struct stat *ptr, int flag, struct FTW *ftw);
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

int cb2(const char *path, const struct stat *ptr, int flag, struct FTW *ftw);
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

static la_host_t *host = NULL;
static la_db_t *db = NULL;

void cleanup(void);
void cleanup(void)
{
    printf("cleaning up...\n");
    if (db != NULL)
        la_db_close(db);
    if (host != NULL)
        la_host_close(host);
}

la_codec_value_t *
mymap(la_codec_value_t *value, void *baton);
la_codec_value_t *
mymap(la_codec_value_t *value, void *baton)
{
    la_codec_value_t *mapme;
    printf("mymap ");
    if (!la_codec_is_object(value))
        return NULL;
    mapme = la_codec_object_get(value, "mapme");
    if (mapme == NULL || !la_codec_is_integer(mapme))
        return NULL;
    printf("%lld ", la_codec_integer_value(mapme));
    return la_codec_incref(mapme);
}

la_codec_value_t *
myreduce(la_codec_value_t *accum, la_codec_value_t *value, void *baton);
la_codec_value_t *
myreduce(la_codec_value_t *accum, la_codec_value_t *value, void *baton)
{
    la_codec_int_t i;
    printf("myreduce ");
    if (!la_codec_is_integer(value))
        return NULL;
    i = la_codec_integer_value(value);
    if (accum != NULL && la_codec_is_integer(accum))
        i += la_codec_integer_value(accum);
    printf("%lld ", i);
    return la_codec_integer(i);
}
        
#define OK() printf("OK\n")
#define FAIL(fmt,args...) do { printf("FAIL" fmt "\n", ##args); return -1; } while (0)
#define FAIL0() FAIL("")

int main(int argc, char **argv)
{
    if (nftw("/tmp/apitest", cb1, 10, FTW_DEPTH | FTW_PHYS) == 0)
        nftw("/tmp/apitest", cb2, 10, FTW_DEPTH | FTW_PHYS);
    
    atexit(cleanup);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("opening host... ");
    if ((host = la_host_open("/tmp/apitest")) == NULL)
    {
        FAIL0();
    }
    OK();
    
    printf("opening db... ");
    if ((db = la_db_open(host, "apitest")) == NULL)
    {
        FAIL0();
    }
    OK();
    
    la_codec_value_t *value;
    la_codec_error_t error;
    
    printf("get on nonexistent... ");
    la_db_get_result get;
    if ((get = la_db_get(db, "not-there", NULL, &value, NULL, &error)) != LA_DB_GET_NOT_FOUND)
    {
        FAIL(" (%d)", get);
    }
    OK();
    
    printf("creating object... ");
    value = la_codec_object();
    if (value == NULL)
    {
        FAIL0();
    }
    OK();
    printf("adding values... ");
    if (la_codec_object_set_new(value, "foo", la_codec_string("bar")) != 0)
    {
        FAIL0();
    }
    if (la_codec_object_set_new(value, "baz", la_codec_true()) != 0)
    {
        FAIL0();
    }
    {
        la_codec_value_t *array = la_codec_array();
        if (array == NULL)
        {
            FAIL0();
        }
        if (la_codec_array_append_new(array, la_codec_integer(42)) != 0
            || la_codec_array_append_new(array, la_codec_null()) != 0
            || la_codec_array_append_new(array, la_codec_real(3.14159)))
        {
            FAIL0();
        }
        if (la_codec_object_set_new(value, "quux", array) != 0)
        {
            FAIL0();
        }
    }
    OK();
    
    printf("adding the object... ");
    la_rev_t newrev;
    la_db_put_result put;
    if ((put = la_db_put(db, "newobject", NULL, value, &newrev)) != LA_DB_PUT_OK)
    {
        FAIL(" (%d)", put);
    }
    printf(" revision %s ", la_rev_string(newrev));
    OK();
    la_codec_decref(value);
    
    printf("getting the object... ");
    if ((get = la_db_get(db, "newobject", NULL, &value, NULL, &error)) != LA_DB_PUT_OK)
    {
        FAIL(" (%d)", get);
    }
    printf(" the doc: %s ", la_codec_dumps(value, 0));
    OK();
    
    printf("delete the object... ");
    la_db_delete_result del;
    if ((del = la_db_delete(db, "newobject", &newrev)) != LA_DB_DELETE_OK)
    {
        FAIL(" (%d)", del);
    }
    OK();
    
    printf("get the deleted object... ");
    if ((get = la_db_get(db, "newobject", NULL, &value, NULL, &error)) != LA_DB_GET_NOT_FOUND)
    {
        FAIL(" (%d)", get);
    }
    OK();
    
    printf("adding more objects... ");
    value = la_codec_object();
    if (value == NULL) FAIL0();
    if (la_codec_object_set_new(value, "mapme", la_codec_integer(1)) != 0) FAIL0();
    if ((put = la_db_put(db, "one", NULL, value, &newrev)) != LA_DB_PUT_OK) FAIL(" (%d)", put);
    la_codec_decref(value);
    value = la_codec_object();
    if (value == NULL) FAIL0();
    if (la_codec_object_set_new(value, "mapme", la_codec_integer(2)) != 0) FAIL0();
    if ((put = la_db_put(db, "two", NULL, value, &newrev)) != LA_DB_PUT_OK) FAIL(" (%d)", put);
    la_codec_decref(value);
    value = la_codec_object();
    if (value == NULL) FAIL0();
    if (la_codec_object_set_new(value, "mapme", la_codec_integer(3)) != 0) FAIL0();
    if ((put = la_db_put(db, "three", NULL, value, &newrev)) != LA_DB_PUT_OK) FAIL(" (%d)", put);
    la_codec_decref(value);
    OK();
    
    printf("map/reduce objects... ");
    la_view_iterator_t *it = la_db_view(db, mymap, myreduce, NULL);
    if (it == NULL) FAIL("creating iterator");
    la_view_iterator_result iter;
    memset(&error, 0, sizeof(la_codec_error_t));
    while ((iter = la_view_iterator_next(it, &value, &error)) == LA_VIEW_ITERATOR_GOT_NEXT)
    {
        if (value == NULL) continue;
        if (!la_codec_is_integer(value)) FAIL("didn't get an integer %d", la_codec_typeof(value));
        printf("%lld ", la_codec_integer_value(value));
        la_codec_decref(value);
    }
    if (iter != LA_VIEW_ITERATOR_END)
        FAIL(" (%d) %s", iter, error.text);
    if (!la_codec_is_integer(value))
        FAIL(" didn't get reduced integer %d", la_codec_typeof(value));
    printf("reduced:%lld ", la_codec_integer_value(value));
    if (la_codec_integer_value(value) != 1 + 2 + 3)
        FAIL(" reduced value incorrect");
    OK();
    return 0;
}