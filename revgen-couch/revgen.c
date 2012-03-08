//
//  revgen.c
//  LoungeAct
//
//  Created by Casey Marshall on 3/7/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdio.h>

#include "../revgen/revgen.h"
#include "../api/LoungeAct.h"

#if defined(__APPLE__) // jerks
#include <CommonCrypto/CommonDigest.h>
#define MD5_DIGEST_LENGTH CC_MD2_DIGEST_LENGTH
#define MD5_CTX CC_MD5_CTX
#define MD5_Init CC_MD5_Init
#define MD5_Update CC_MD5_Update
#define MD5_Final CC_MD5_Final
#else
#include <openssl/md5.h>
#endif

/*
 JSON: {"_rev":"3-a74c2aa095463ef74e0347049efe665b","foo":"bar","baz":"baz","quux":1234,"boolean":true,"otherboolean":false,"list":[1,2,3],"obj":{"a":"b"}}'
 
 Couch produces (custom log statement):
 [debug] [<0.143.0>] new_revid [false, 3, <<167,76,42,160,149,70,62,247,78,3,71,4,158,254,102,91>>, {[{<<102,111,111>>,<<98,97,114>>},{<<98,97,122>>,<<98,97,122>>},{<<113,117,117,120>>,1234},{<<98,111,111,108,101,97,110>>,true},{<<111,116,104,101,114,98,111,111,108,101,97,110>>,false},{<<108,105,115,116>>,[1,2,3]},{<<111,98,106>>,{[{<<97>>,<<98>>}]}}]}, []] <<131,108,0,0,0,5,100,0,5,102,97,108,115,101,97,3,109,0,0,0,16,167,76,42,160,149,70,62,247,78,3,71,4,158,254,102,91,104,1,108,0,0,0,7,104,2,109,0,0,0,3,102,111,111,109,0,0,0,3,98,97,114,104,2,109,0,0,0,3,98,97,122,109,0,0,0,3,98,97,122,104,2,109,0,0,0,4,113,117,117,120,98,0,0,4,210,104,2,109,0,0,0,7,98,111,111,108,101,97,110,100,0,4,116,114,117,101,104,2,109,0,0,0,12,111,116,104,101,114,98,111,111,108,101,97,110,100,0,5,102,97,108,115,101,104,2,109,0,0,0,4,108,105,115,116,107,0,3,1,2,3,104,2,109,0,0,0,3,111,98,106,104,1,108,0,0,0,1,104,2,109,0,0,0,1,97,109,0,0,0,1,98,106,106,106,106>>
 
 Object breakdown:
                                        <<131,
 [                                          108,0,0,0,5,
 false,                                     100,0,5,102,97,108,115,101,
 3,                                         97,3,
 <<167,76,42,160,149,70,62,247,78,3,        109,0,0,0,16,167,76,42,160,149,70,62,247,78,3,
 71,4,158,254,102,91>>,                     71,4,158,254,102,91,
 {                                          104,1,
 [                                          108,0,0,0,7,
 {<<102,111,111>>,<<98,97,114>>},           104,2,109,0,0,0,3,102,111,111,109,3,98,97,114,
 {<<98,97,122>>,<<98,97,122>>},
 {<<113,117,117,120>>,1234},
 {<<98,111,111,108,101,97,110>>,true},
 {<<111,116,104,101,114,98,111,111,108,101,97,110>>,false},
 {<<108,105,115,116>>,[1,2,3]},
 {<<111,98,106>>,{[{<<97>>,<<98>>}]}}]}, []]
 
 JSON doc: {"_rev":"2-13839535feb250d3d8290998b8af17c3","foo":"bar","baz":"baz","quux":1234}
 
 Produces in couch:
 [debug] [<0.295.0>] new_revid [false, 2, <<19,131,149,53,254,178,80,211,216,41,9,152,184,175,23,195>>, {[{<<102,111,111>>,<<98,97,114>>},{<<98,97,122>>,<<98,97,122>>},{<<113,117,117,120>>,1234}]}, []] <<131,108,0,0,0,5,100,0,5,102,97,108,115,101,97,2,109,0,0,0,16,19,131,149,53,254,178,80,211,216,41,9,152,184,175,23,195,104,1,108,0,0,0,3,104,2,109,0,0,0,3,102,111,111,109,0,0,0,3,98,97,114,104,2,109,0,0,0,3,98,97,122,109,0,0,0,3,98,97,122,104,2,109,0,0,0,4,113,117,117,120,98,0,0,4,210,106,106,106>>
 
 Object breakdown:
 (start)                    <<131,
 [                              108,0,0,0,5,
 false,                         100,0,5,102,97,108,115,101,
 2,                             97,2,
 <<19,131,149,53,254,178,       109,0,0,0,16,19,131,149,53,254,178,
 80,211,216,41,9,152,184,       80,211,216,41,9,152,184,
 175,23,195>>,                  175,23,195,
 {                              104,1,
 [                              108,0,0,0,3,
 {                              104,2,
 <<102,111,111>>,<<98,97,114>>},    109,
 {<<98,97,122>>,<<98,97,122>>},{<<113,117,117,120>>,1234}]}, []]
 */

static const unsigned char _TRUE[] = { 100,0,4,116,114,117,101 };
static const unsigned char _FALSE[] = { 100,0,5,102,97,108,115,101 };
static const unsigned char _NULL[] = { 100,0,4,110,117,108,108 };
static const unsigned char _HEAD[] = { 131 };
static const unsigned char _SMALL_INT[] = { 97 }; // Then value, 1 byte
static const unsigned char _INT[] = { 98 }; // Then value, 4 bytes
static const unsigned char _SMALL_TUPLE_ONE[] = { 104,1 }; // Then count, 1 byte, then elements
static const unsigned char _SMALL_TUPLE_TWO[] = { 104,2 }; // Then count, 1 byte, then elements
static const unsigned char _NIL[] = { 106 };
static const unsigned char _LIST[] = { 108 }; // Then count, 4 bytes, then elements
static const unsigned char _BINARY[] = { 109 }; // Then length, 4 bytes, then value

static void hash_value(const la_codec_value_t *value, MD5_CTX *md5)
{
    switch (la_codec_typeof(value))
    {
        case LA_CODEC_TRUE:
            MD5_Update(md5, _TRUE, sizeof(_TRUE));
            break;
            
        case LA_CODEC_FALSE:
            MD5_Update(md5, _FALSE, sizeof(_FALSE));
            break;
            
        case LA_CODEC_NULL:
            MD5_Update(md5, _NULL, sizeof(_NULL));
            break;
            
        case LA_CODEC_INTEGER:
        {
            la_codec_int_t i = la_codec_integer_value(value);
            if (i < 256)
            {
                unsigned char c = (unsigned char) i;
                MD5_Update(md5, _SMALL_INT, sizeof(_SMALL_INT));
                MD5_Update(md5, &c, 1);
            }
            else
            {
                unsigned char c[] = { (unsigned char) (i >> 24),
                                      (unsigned char) (i >> 16),
                                      (unsigned char) (i >>  8),
                                      (unsigned char)  i };
                MD5_Update(md5, _INT, sizeof(_INT));
                MD5_Update(md5, c, 4);
            }
            break;
        }
            
        case LA_CODEC_REAL:
            break; // TODO
            
        case LA_CODEC_STRING:
        {
            const char *str = la_codec_string_value(value);
            int len = strlen(str);
            unsigned char c[] = { (unsigned char) (len >> 24),
                                  (unsigned char) (len >> 16),
                                  (unsigned char) (len >>  8),
                                  (unsigned char)  len };
            MD5_Update(md5, _BINARY, sizeof(_BINARY));
            MD5_Update(md5, c, 4);
            MD5_Update(md5, str, strlen(str));
        }
            
        case LA_CODEC_ARRAY:
        {
            int len = la_codec_array_size(value);
            unsigned char c[] = { (unsigned char) (len >> 24),
                                  (unsigned char) (len >> 16),
                                  (unsigned char) (len >>  8),
                                  (unsigned char)  len };
            int i;
            MD5_Update(md5, _LIST, sizeof(_LIST));
            MD5_Update(md5, c, 4);
            for (i = 0; i < len; i++)
            {
                la_codec_value_t *v = la_codec_array_get(value, i);
                hash_value(v, md5);
            }
            break;
        }
            
        case LA_CODEC_OBJECT:
        {
            int len = la_codec_object_size(value);
            if (la_codec_object_get(value, LA_API_REV_NAME) != NULL)
                len--;
            if (la_codec_object_get(value, LA_API_KEY_NAME) != NULL)
                len--;
            if (la_codec_object_get(value, LA_API_DELETED_NAME) != NULL)
                len--;
            unsigned char c[] = { (unsigned char) (len >> 24),
                                  (unsigned char) (len >> 16),
                                  (unsigned char) (len >>  8),
                                  (unsigned char)  len };
            void *iter = la_codec_object_iter(value);
            if (len == 0)
            {
                MD5_Update(md5, _NIL, sizeof(_NIL));
            }
            else
            {
                MD5_Update(md5, _LIST, sizeof(_LIST));
                MD5_Update(md5, c, 4);
                while (iter != NULL)
                {
                    const char *key = la_codec_object_iter_key(iter);
                    int keylen = strlen(key);
                    unsigned char kc[] = { (unsigned char) (keylen >> 24),
                                           (unsigned char) (keylen >> 16),
                                           (unsigned char) (keylen >>  8),
                                           (unsigned char)  keylen };
                    if (strcmp(key, LA_API_DELETED_NAME) == 0
                        || strcmp(key, LA_API_KEY_NAME) == 0
                        || strcmp(key, LA_API_REV_NAME) == 0)
                        continue;
                    MD5_Update(md5, _SMALL_TUPLE_TWO, sizeof(_SMALL_TUPLE_TWO));
                    MD5_Update(md5, _BINARY, sizeof(_BINARY));
                    MD5_Update(md5, kc, 4);
                    MD5_Update(md5, key, keylen);
                    hash_value(la_codec_object_iter_value(iter), md5);
                    iter = la_codec_object_iter_next(value, iter);
                }
                MD5_Update(md5, _NIL, sizeof(_NIL));
            }
            break;
        }
    }
}

static char hx(unsigned char c)
{
    if (c < 10)
        return '0' + c;
    return 'a' + (c - 10);
}

int la_revgen(const la_codec_value_t *value, uint64_t start, const char *rev, size_t revlen,
              int is_delete, char *buffer, size_t len)
{
    int i, j;
    MD5_CTX md5;
    unsigned char digest[MD5_DIGEST_LENGTH];
    la_codec_value_t *start_value;
    
    MD5_Init(&md5);
    
    MD5_Update(&md5, _HEAD, sizeof(_HEAD));
    MD5_Update(&md5, _LIST, sizeof(_LIST));
    MD5_Update(&md5, "\0\0\0\5", 4);
    if (is_delete)
        MD5_Update(&md5, _TRUE, sizeof(_TRUE));
    else
        MD5_Update(&md5, _FALSE, sizeof(_FALSE));
    
    start_value = la_codec_integer(start);
    hash_value(start_value, &md5);
    la_codec_decref(start_value);
    
    if (rev == NULL)
    {
        unsigned char c = 0;
        MD5_Update(&md5, _SMALL_INT, sizeof(_SMALL_INT));
        MD5_Update(&md5, &c, 1);
    }
    else
    {
        unsigned char c[] = { (unsigned char) (revlen >> 24),
            (unsigned char) (revlen >> 16),
            (unsigned char) (revlen >>  8),
            (unsigned char)  revlen };
        MD5_Update(&md5, _BINARY, sizeof(_BINARY));
        MD5_Update(&md5, c, 4);
        MD5_Update(&md5, rev, revlen);
    }
    
    // Stored as a 1-tuple containing a list of 2-tuples.
    MD5_Update(&md5, _SMALL_TUPLE_ONE, sizeof(_SMALL_TUPLE_ONE));
    hash_value(value, &md5);
    MD5_Update(&md5, _NIL, sizeof(_NIL));    
    MD5_Update(&md5, _NIL, sizeof(_NIL));    
    
    MD5_Final(digest, &md5);
    
    if (buffer)
        memcpy(buffer, digest, (len < MD5_DIGEST_LENGTH) ? len : MD5_DIGEST_LENGTH);
    
    return MD5_DIGEST_LENGTH;
}