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
#include "../utils/hexdump.h"

#define die(fmt,args...) do { fprintf(stderr, fmt, ##args); exit(1); } while(0)

int main (int argc, const char * argv[])
{
    la_storage_rev_t rev;
    la_codec_error_t error;
    la_codec_value_t *value = la_codec_object();
    la_revgen(value, 0, NULL, 0, &rev);
    const char test_empty_rev[] = { 150,122,0,223,245,224,42,221,65,129,145,56,171,179,40,77 };
    if (memcmp(rev.rev, test_empty_rev, 16) != 0)
    {
        la_hexdump(rev.rev, LA_OBJECT_REVISION_LEN);
        la_hexdump(test_empty_rev, LA_OBJECT_REVISION_LEN);
        die("FAIL empty new first object\n");
    }
    la_codec_decref(value);
    
    value = la_codec_loads("{\"int1\":1,\"int2\":1234}", 0, &error);
    if (!value)
        die("parse int test: %s\n", error.text);
    const char test_int_rev[] = { 0xf1, 0x9d, 0x26, 0xc1, 0xb4, 0xd6, 0xdc, 0x3a, 0x4a, 0x84, 0x3d, 0xe6, 0x9e, 0x36, 0xac, 0x47 };
    la_revgen(value, 0, NULL, 0, &rev);
    if (memcmp(rev.rev, test_int_rev, 16) != 0)
        die("FAIL int new object\n");
    la_codec_decref(value);
    
    value = la_codec_loads("{\"a\":true,\"b\":false}", 0, &error);
    if (!value)
        die("parse bool test: %s\n", error.text);
    const char test_bool_rev[] = "\xab\xef\xf4\xc3\xa5\xb7\x45\xd4\x5d\xa2\xb8\x34\xb9\x1a\xac\xf5";
    la_revgen(value, 0, NULL, 0, &rev);
    if (memcmp(rev.rev, test_bool_rev, 16) != 0)
        die("FAIL bool new object\n");
    la_codec_decref(value);
    
    const char *test_str_rev = "\xd8\x96\xe3\x56\xc2\x8b\x75\x21\x28\xeb\xb3\xe5\xe2\x24\xff\xbd";
    value = la_codec_loads("{\"s\":\"string\",\"nothing\":\"\"}", 0, &error);
    if (!value)
        die("parse string test: %s\n", error.text);
    la_revgen(value, 0, NULL, 0, &rev);
    if (memcmp(rev.rev, test_str_rev, 16) != 0)
        die("FAIL string new object\n");
    la_codec_decref(value);
    
    const char *test_float_rev = "\xfe\xad\x27\x96\x2c\xe8\xbd\xac\x43\x8d\x03\x6e\x70\x22\x94\xdc";
    value = la_codec_loads("{\"zero\":0.0,\"pi\":3.14159,\"e\":2.71828183}", 0, &error);
    if (!value)
        die("parse float test: %s\n", error.text);
    la_revgen(value, 0, NULL, 0, &rev);
    if (memcmp(rev.rev, test_float_rev, 16) != 0)
        die("FAIL float new object\n");
    la_codec_decref(value);

    const char *test_null_rev = "\x04\xfb\x42\x47\xa2\x96\xee\x1c\xb1\x1c\x34\x80\x81\xdb\x43\xc3";
    value = la_codec_loads("{\"nada\":null}", 0, &error);
    if (!value)
        die("parse null test: %s\n", error.text);
    la_revgen(value, 0, NULL, 0, &rev);
    if (memcmp(rev.rev, test_null_rev, 16) != 0)
        die("FAIL null new object\n");
    la_codec_decref(value);
    
    const char *test_0list_rev = "\x34\xda\x82\xaa\x76\xe3\x23\x1c\x3b\x02\x10\x79\xbf\x7d\xb0\x73";
    value = la_codec_loads("{\"nada\":[]}", 0, &error);
    if (!value)
        die("parse empty list test: %s\n", error.text);
    la_revgen(value, 0, NULL, 0, &rev);
    if (memcmp(rev.rev, test_0list_rev, 16) != 0)
        die("FAIL empty list new object\n");
    la_codec_decref(value);
    
    const char *test_list_rev = "\x29\xe5\x48\x63\xd0\x81\x2d\xf3\xb0\x9a\xb5\x2e\xa7\x5c\x1a\xab";
    value = la_codec_loads("{\"list\":[\"this\",\"is\",12345,40.04,true,null]}", 0, &error);
    if (!value)
        die("parse list test: %s\n", error.text);
    la_revgen(value, 0, NULL, 0, &rev);
    if (memcmp(rev.rev, test_list_rev, 16) != 0)
        die("FAIL list new object\n");
    la_codec_decref(value);
    
    const char *test_0object_rev = "\xc2\xe3\xbf\x13\xca\x65\xa9\xc7\x6b\x5e\x92\x7e\xdd\x40\x3f\xd9";
    value = la_codec_loads("{\"nada\":{}}", 0, &error);
    if (!value)
        die("parse empty object test: %s\n", error.text);
    la_revgen(value, 0, NULL, 0, &rev);
    if (memcmp(rev.rev, test_0object_rev, 16) != 0)
        die("FAIL empty object new object\n");
    la_codec_decref(value);
    
    const char *test_object_rev = "\xa8\xf7\x9d\x07\x06\x69\xc9\xd8\x57\x07\x5d\x66\x45\x1d\x44\x9b";
    value = la_codec_loads("{\"object\":{\"int\":1234,\"float\":3.14159,\"string\":\"Hello, World!\",\"boolean\":true,\"nada\":null,\"array\":[\"a\"],\"obj\":{\"foo\":\"bar\"}}}", LA_CODEC_PRESERVE_ORDER, &error);
    if (!value)
        die("parse object test: %s\n", error.text);
    la_revgen(value, 0, NULL, 0, &rev);
    if (memcmp(rev.rev, test_object_rev, 16) != 0)
        die("FAIL object new object\n");
    la_codec_decref(value);
    
    const char *revstr = "1234-deadbeefdeadbeefdeadbeefdeadbeef";
    la_rev_t scanrev;
    la_rev_t testrev = { 1234, { 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, } };
    printf("%p\n", revstr);
    if (la_rev_scan(revstr, &scanrev) != 0)
        die("FAIL scan rev\n");
    printf("%p\n", revstr);
    if (memcmp(&scanrev, &testrev, sizeof(la_rev_t)) != 0)
        die("FAIL scanned rev equality\n");
    char buf[100];
    la_rev_print(testrev, buf, 100);
    if (strcmp(revstr, buf) != 0)
        die("FAIL print rev\n");
    
    printf("test update object...\n");
    la_storage_rev_t test_update_rev1 = { 0xee, 0x9f, 0x67, 0x03, 0xbf, 0xe4, 0xe7, 0x14, 0xf2, 0xba, 0x37, 0x89, 0x19, 0xfa, 0x2a, 0x2c };
    const char *test_update_rev2 = "\x35\x97\x37\x17\x8d\x12\xf1\x4a\x9b\x3c\x7f\xa4\xf7\x1a\x78\x14";
    value = la_codec_loads("{\"foo\": \"bar\"}", 0, &error);
    la_revgen(value, 1, &test_update_rev1, 0, &rev);
    if (memcmp(rev.rev, test_update_rev2, 16) != 0)
        die("FAIL updated document\n");
    
    printf("OK\n");
    return 0;
}

