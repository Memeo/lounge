//
//  file.h
//  LoungeAct
//
//  Created by Casey Marshall on 2/22/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#ifndef LoungeAct_file_h
#define LoungeAct_file_h

#if defined (__APPLE__) /* Jerks. */
# include <CommonCrypto/CommonDigest.h>
# define MD5 CC_MD5
# define MD5_DIGEST_LENGTH CC_MD2_DIGEST_LENGTH
#else
# include <openssl/md5.h>
#endif

typedef struct
{
    FILE *file;
} FileHandle;

typedef struct
{
    void *data;
    size_t length;
} FileTerm;

typedef enum
{
    FILE_SUCCESS = 0,
    FILE_SHORT_READ,
    FILE_SEEK_ERROR,
    FILE_READ_ERROR,
    FILE_WRITE_ERROR,
    FILE_CORRUPTION,
    FILE_MEMORY_ERROR
} FileError;

/**
 * Open a file.
 */
FileHandle *file_open(const char *path);
FileError file_append(FileHandle *handle, const FileTerm *term);
FileError file_append_md5(FileHandle *handle, const FileTerm *term, const unsigned char md5[MD5_DIGEST_LENGTH]);

/**
 * Read a value from a file.
 *
 * This function will allocate enough space for the value if and only
 * if term->data is NULL; this data is allocated with malloc and must
 * be freed with free when done. If the data member is non-null, length
 * should be the number of bytes allocated; if this length is shorter
 * than the value's length, a SHORT_READ error is returned, and the
 * actual value length is stored in the term->length field.
 */
FileError file_read(FileHandle *handle, off_t offset, FileTerm *term);

int file_flush(FileHandle *handle);
int file_close(FileHandle *handle);

#endif
