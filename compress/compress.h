//
//  compress.h
//  LoungeAct
//
//  Created by Casey Marshall on 3/29/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#ifndef LoungeAct_compress_h
#define LoungeAct_compress_h

unsigned char *la_compress(unsigned char *input, size_t len, size_t *outlen);
unsigned char *la_decompress(unsigned char *input, size_t len, size_t *outlen);

#endif
