//
//  main.c
//  revgen-couch-test
//
//  Created by Casey Marshall on 3/7/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <CoreFoundation/CoreFoundation.h>
#include "../revgen/revgen.h"
#include "../Codec/Codec.h"

int main (int argc, const char * argv[])
{
    char rev[16];
    la_codec_value_t *value = la_codec_object();
    la_revgen(value, 0, NULL, 0, 0, rev, sizeof(rev));
    const char test_empty_rev[] = { 150,122,0,223,245,224,42,221,65,129,145,56,171,179,40,77 };
    if (memcmp(rev, test_empty_rev, 16) != 0)
        printf("FAIL empty new first object");
    la_codec_decref(value);
    
    printf("OK");
    return 0;
}

