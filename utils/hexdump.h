//
//  hexdump.h
//  LoungeAct
//
//  Created by Casey Marshall on 3/3/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#ifndef LoungeAct_hexdump_h
#define LoungeAct_hexdump_h

#include <stdio.h>

void la_hexdump(const char *buf, size_t len);
void la_fhexdump(FILE *out, const char *buf, size_t len);

#endif
