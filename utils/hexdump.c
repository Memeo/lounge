//
//  hexdump.c
//  LoungeAct
//
//  Created by Casey Marshall on 3/3/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdio.h>
#include <ctype.h>
#include "hexdump.h"

static char _ch(char c)
{
    if (isgraph(c))
        return c;
    return '.';
}

void la_hexdump(const char *buf, size_t len)
{
    la_fhexdump(stdout, buf, len);
}

void la_fhexdump(FILE *out, const char *buf, size_t len)
{
    size_t i, c, j;
    for (i = 0; i < len; i += 16)
    {
        c = len - i;
        if (c > 16)
            c = 16;
        fprintf(out, "%08lx  ", (size_t) buf + i);
        for (j = 0; j < 16; j++)
        {
            if (j == 8)
                fputc(' ', out);
            if (i + j < len)
                fprintf(out, "%02x ", (unsigned int) buf[i+j] & 0xFF);
            else
                fprintf(out, "   ");
        }
        fprintf(out, "    ");
        for (j = 0; j < 16 && i + j < len; j++)
        {
            fputc(_ch(buf[i+j]), out);
        }
        fputc('\n', out);
    }
}