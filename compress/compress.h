//
//  compress.h
//  LoungeAct
//
//  Created by Casey Marshall on 3/29/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#ifndef LoungeAct_compress_h
#define LoungeAct_compress_h

typedef unsigned char *(*la_compress_fn)(unsigned char *input, size_t len, size_t *outlen);
typedef unsigned char *(*la_decompress_fn)(unsigned char *input, size_t len, size_t *outlen);

typedef struct
{
    la_compress_fn compressor;
    la_decompress_fn decompressor;
} la_compressor_t;

#endif
