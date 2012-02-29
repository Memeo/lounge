//
//  stringutils.h
//  LoungeAct
//
//  Created by Casey Marshall on 2/28/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#ifndef LoungeAct_stringutils_h
#define LoungeAct_stringutils_h

char *string_join(const char *sep, char * const *parts, size_t length);
char *string_append(const char *prefix, const char *suffix);

#endif
