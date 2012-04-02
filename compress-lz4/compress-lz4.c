//
//  compress-lz4.c
//  LoungeAct
//
//  Created by Casey Marshall on 4/2/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdlib.h>

#include "lz4.h"
#include "../compress/compress.h"

static unsigned char *lz4_compress(unsigned char *in, size_t inlen, size_t *outlen)
{
    int maxlen = LZ4_compressBound(inlen);
    unsigned char *buffer = malloc(maxlen);
    if (buffer == NULL)
        return NULL;
    *outlen = LZ4_compress((const char *) in, (char *) buffer, inlen);
    return buffer;
}

static unsigned char *lz4_decompress(unsigned char *in, size_t inlen, size_t *outlen)
{
    int alloclen = inlen + (inlen / 2);
    unsigned char *buffer = malloc(alloclen);
    int osize = LZ4_uncompress_unknownOutputSize((const char *) in, (char *) buffer, inlen, alloclen);
    while (osize < 0 || osize == alloclen)
    {
        alloclen += 1024;
        buffer = realloc(buffer, alloclen);
        if (buffer == NULL)
            return NULL;
        osize = LZ4_uncompress_unknownOutputSize((const char *) in, (char *) buffer, inlen, alloclen);
    }
    *outlen = osize;
    return buffer;
}

la_compressor_t __lz4_compressor = { lz4_compress, lz4_decompress };
la_compressor_t *la_lz4_compressor = &__lz4_compressor;