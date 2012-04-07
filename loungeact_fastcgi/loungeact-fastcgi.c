//
//  loungeact-fastcgi.c
//  LoungeAct
//
//  Created by Casey Marshall on 4/2/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdio.h>
#include "fcgiapp.h"

#include <jansson.h>

static char *hello_str;

static void handle_request(FCGX_Request *request);

int main(int argc, char **argv)
{
    json_t *hello = json_pack("{ssssss}", "couchdb", "Welcome", "version", "1.1.0", "loungeact", "0.0.0");
    hello_str = json_dumps(hello, JSON_COMPACT);
    
    FCGX_Init();
    
    for (;;)
    {
        FCGX_Request *req = malloc(sizeof(FCGX_Request));
        if (req == NULL)
        {
            fprintf(stderr, "panic: out of memory\n");
            exit(EXIT_FAILURE);
        }
        FCGX_InitRequest(req, 0, 0);
        FCGX_Accept_r(req);
        
    }
}

static void handle_request(FCGX_Request *request)
{
    FCGX_GetLine(<#char *str#>, <#int n#>, <#FCGX_Stream *stream#>)
}