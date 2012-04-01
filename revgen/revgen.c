//
//  revgen.c
//  LoungeAct
//
//  Created by Casey Marshall on 3/9/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include "revgen.h"

static int hx(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return (c - 'a') + 10;
    if (c >= 'A' && c <= 'G')
        return (c - 'A') + 10;
    return -1;
}

int la_rev_scan(const char *str, la_rev_t *rev)
{
    int i, j;
    uint64_t seq;
    const char *p;
    size_t len;

    int ret = sscanf(str, "%llu", &seq);
    if (ret != 1)
        return -1;
    rev->seq = seq;
    p = strchr(str, '-');
    if (p == NULL)
        return -1;
    p++;
    len = strlen(p);
    if (len < LA_OBJECT_REVISION_LEN * 2)
        return -1;
    for (i = 0, j = 0; i < len - 1 && j < LA_OBJECT_REVISION_LEN; i += 2, j++)
    {
        int x = hx(p[i]);
        int y = hx(p[i+1]);
        if (x < 0 || y < 0)
            return -1;
        rev->rev.rev[j] = x << 4 | y;
    }
    return 0;
}

int la_rev_print(const la_rev_t rev, char *buffer, size_t size)
{
    int i;
    int total;
    int ret = total = snprintf(buffer, size, "%llu-", rev.seq);
    size -= ret;
    buffer += ret;
    for (i = 0; i < LA_OBJECT_REVISION_LEN; i++)
    {
        ret = snprintf(buffer, size, "%02x", rev.rev.rev[i]);
        total += ret;
        size -= ret;
        buffer += ret;
    }
    return ret;
}

static char strbuf[20 + (LA_OBJECT_REVISION_LEN * 2) + 1];

const char *la_rev_string(const la_rev_t rev)
{
    la_rev_print(rev, strbuf, sizeof(strbuf));
    return strbuf;
}