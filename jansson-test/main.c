//
//  main.c
//  jansson-test
//
//  Created by Casey Marshall on 3/7/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <CoreFoundation/CoreFoundation.h>
#include <jansson.h>

int main (int argc, const char * argv[])
{
    json_error_t error;
    const char *json = "{\"_rev\":\"2-13839535feb250d3d8290998b8af17c3\",\"foo\":\"bar\",\"baz\":\"baz\",\"quux\":1234}";
    json_t *obj = json_loads(json, 0, &error);
    if (obj == NULL)
    {
        printf("failed to parse: %d %d %s\n", error.line, error.column, error.text);
        return 1;
    }
    if (!json_is_object(obj))
    {
        printf("did not get an object back\n");
        return 1;
    }
    
    void *iter = json_object_iter(obj);
    while (iter)
    {
        const char *key = json_object_iter_key(iter);
        printf("object key %s\n", key);
        iter = json_object_iter_next(obj, iter);
    }
}

