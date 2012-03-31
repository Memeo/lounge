//
//  compress.c
//  LoungeAct
//
//  Created by Casey Marshall on 3/29/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

#include "../compress/compress.h"
#include "../utils/buffer.h"

#define CHUNK_SIZE 4096

unsigned char *la_compress(unsigned char *input, size_t len, size_t *outlen)
{
    Bytef out[CHUNK_SIZE];
    z_stream zs;
    la_buffer_t *buffer;
    memset(&zs, 0, sizeof(z_stream));
    
    if ((buffer = la_buffer_new(1024)) == NULL)
        return NULL;
    if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK)
    {
        la_buffer_destroy(buffer);
        return NULL;
    }
    zs.next_in = input;
    zs.avail_in = len;
    do {
        zs.next_out = out;
        zs.avail_out = CHUNK_SIZE;
        int ret = deflate(&zs, Z_FINISH);
        if (ret != Z_OK && ret != Z_STREAM_END)
        {
            deflateEnd(&zs);
            la_buffer_destroy(buffer);
            return NULL;
        }
        if (la_buffer_append(buffer, out, CHUNK_SIZE - zs.avail_out) != 0)
        {
            deflateEnd(&zs);
            la_buffer_destroy(buffer);
            return NULL;
        }
    } while (zs.avail_out == 0);
    deflateEnd(&zs);
    if (outlen != NULL)
        *outlen = la_buffer_size(buffer);
    unsigned char *ret = la_buffer_copy(buffer);
    la_buffer_destroy(buffer);
    return ret;
}

unsigned char *la_decompress(unsigned char *input, size_t len, size_t *outlen)
{
    Bytef out[CHUNK_SIZE];
    z_stream zs;
    la_buffer_t *buffer;
    memset(&zs, 0, sizeof(z_stream));
    
    if ((buffer = la_buffer_new(1024)) == NULL)
        return NULL;
    if (inflateInit(&zs) != Z_OK)
    {
        la_buffer_destroy(buffer);
        return NULL;
    }
    zs.next_in = input;
    zs.avail_in = len;
    do {
        zs.next_out = out;
        zs.avail_out = CHUNK_SIZE;
        int ret = inflate(&zs, Z_FINISH);
        if (ret != Z_OK && ret != Z_STREAM_END)
        {
            inflateEnd(&zs);
            la_buffer_destroy(buffer);
            return NULL;
        }
        if (la_buffer_append(buffer, out, CHUNK_SIZE - zs.avail_out) != 0)
        {
            inflateEnd(&zs);
            la_buffer_destroy(buffer);
            return NULL;
        }
    } while (zs.avail_out == 0);
    inflateEnd(&zs);
    if (outlen != NULL)
        *outlen = la_buffer_size(buffer);
    unsigned char *ret = la_buffer_copy(buffer);
    la_buffer_destroy(buffer);
    return ret;
}
