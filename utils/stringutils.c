//
//  stringutils.c
//  LoungeAct
//
//  Created by Casey Marshall on 2/28/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stringutils.h"

char *
string_join(const char *sep, char * const *parts, size_t length)
{
    char *buf;
    size_t size = 0;
    int i;
    
    for (i = 0; i < length; i++)
    {
        size += strlen(parts[i]);
        if (i < length - 1)
            size += strlen(sep);
    }
    size++;
    buf = malloc(size);
    if (buf == NULL)
        return NULL;
    buf[0] = '\0';
    for (i = 0; i < length; i++)
    {
        strcat(buf, parts[i]);
        if (i < length - 1)
            strcat(buf, sep);
    }
    return buf;
}

char *
string_append(const char *prefix, const char *suffix)
{
    char *buf = (char *) malloc(strlen(prefix) + strlen(suffix));
    buf[0] = '\0';
    strcat(buf, prefix);
    strcat(buf, suffix);
    return buf;
}

#define hexify(nybble) (((nybble) < 10) ? ('0' + (nybble)) : ('a' + ((nybble) - 10)))

int
string_randhex(char *str, size_t len)
{
    FILE *urandom = fopen("/dev/urandom", "r");
    unsigned char b;
    
    if (urandom == NULL)
        return -1;
    
    for (int i = 0; i < len; i += 2)
    {
        if (fread(&b, 1, 1, urandom) != 1)
        {
            fclose(urandom);
            return -1;
        }
        str[i] = hexify((b >> 4) & 0xf);
        if (i + 1 < len)
            str[i + 1] = hexify(b & 0xf);
    }
    fclose(urandom);
    str[len] = '\0';
    return 0;
}
