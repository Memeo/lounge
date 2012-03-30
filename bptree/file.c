//
//  file.c
//  LoungeAct
//
//  Created by Casey Marshall on 2/22/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <stdint.h>

#include "file.h"
#include "config.h"

struct RecordHeader
{
    int has_md5 : 1;
    uint32_t length : 31;
} LA_PACKED;

FileHandle *
file_open(const char *filename)
{
    FileHandle *ret = malloc(sizeof(FileHandle));
    if (ret)
    {
        ret->file = fopen(filename, "rb+");
        if (ret->file == NULL)
        {
            free(ret);
            return NULL;
        }
    }
    return ret;
}

FileError
file_append(FileHandle *f, const FileTerm *term)
{
    struct RecordHeader header;
    struct iovec v[2];
    if (fseek(f->file, 0, SEEK_END) != 0)
        return FILE_SEEK_ERROR;
    header.has_md5 = 0;
    header.length = (uint32_t) term->length;
    v[0].iov_base = &header;
    v[0].iov_len = sizeof(struct RecordHeader);
    v[1].iov_base = term->data;
    v[1].iov_len = term->length;
    if (writev(fileno(f->file), (const struct iovec *) &v, 2) != 0)
    {
        return FILE_WRITE_ERROR;
    }
    return FILE_SUCCESS;
}

FileError
file_append_md5(FileHandle *f, const FileTerm *term, const unsigned char md5[MD5_DIGEST_LENGTH])
{
    struct RecordHeader header;
    struct iovec v[3];
    if (fseek(f->file, 0, SEEK_END) != 0)
        return FILE_SEEK_ERROR;
    header.has_md5 = 1;
    header.length = (uint32_t) term->length;
    v[0].iov_base = &header;
    v[0].iov_len = sizeof(struct RecordHeader);
    v[1].iov_base = (void *) md5;
    v[1].iov_len = MD5_DIGEST_LENGTH;
    v[2].iov_base = term->data;
    v[2].iov_len = term->length;
    if (writev(fileno(f->file), (const struct iovec *) &v, 3) != 0)
        return FILE_WRITE_ERROR;
    return FILE_SUCCESS;
}

FileError
file_read(FileHandle *f, const off_t offset, FileTerm *term)
{
    struct RecordHeader header;
    int owned = 0;
    unsigned char file_md5[MD5_DIGEST_LENGTH], comp_md5[MD5_DIGEST_LENGTH];
    if (fseeko(f->file, offset, SEEK_SET) != 0)
        return FILE_SEEK_ERROR;
    if (fread(&header, sizeof(struct RecordHeader), 1, f->file) != 1)
        return FILE_READ_ERROR;
    if (header.has_md5)
    {
        if (fread(file_md5, MD5_DIGEST_LENGTH, 1, f->file) != 1)
            return FILE_READ_ERROR;
    }
    if (term->data == NULL)
    {
        term->data = malloc(header.length);
        term->length = header.length;
        if (term->data == NULL)
            return FILE_MEMORY_ERROR;
        owned = 1;
    }
    else
    {
        if (header.length < term->length)
            term->length = header.length;
    }
    if (fread(term->data, term->length, 1, f->file) != 1)
    {
        if (owned)
            free(term->data);
        return -1;
    }
    if (header.has_md5)
    {
        MD5(term->data, header.length, comp_md5);
        if (memcmp(file_md5, comp_md5, MD5_DIGEST_LENGTH) != 0)
        {
            if (owned)
                free(term->data);
            return FILE_CORRUPTION;
        }
    }
    if (term->length < header.length)
    {
        term->length = header.length;
        return FILE_SHORT_READ;
    }
    return FILE_SUCCESS;
}

void
file_read_cb(FileHandle *f, const off_t offset, ReadCallback cb)
{
    struct RecordHeader header;
    FileTerm term;
    unsigned char file_md5[MD5_DIGEST_LENGTH], comp_md5[MD5_DIGEST_LENGTH];
    if (fseeko(f->file, offset, SEEK_SET) != 0)
    {
        cb(NULL, FILE_SEEK_ERROR);
        return;
    }
    if (fread(&header, sizeof(struct RecordHeader), 1, f->file) != 1)
    {
        cb(NULL, FILE_READ_ERROR);
        return;
    }
    if (header.has_md5)
    {
        if (fread(&file_md5, MD5_DIGEST_LENGTH, 1, f->file) != 1)
        {
            cb(NULL, FILE_READ_ERROR);
            return;
        }
    }
    term.data = malloc(header.length);
    term.length = header.length;
    if (term.data == NULL)
    {
        cb(NULL, FILE_MEMORY_ERROR);
        return;
    }
    if (fread(term.data, term.length, 1, f->file) != 1)
    {
        cb(NULL, FILE_READ_ERROR);
        free(term.data);
        return;
    }
    if (header.has_md5)
    {
        MD5(term.data, term.length, comp_md5);
        if (memcmp(file_md5, comp_md5, MD5_DIGEST_LENGTH) != 0)
        {
            
        }
    }
}

int
file_flush(FileHandle *f)
{
    return fflush(f->file);
}

int
file_close(FileHandle *f)
{
    fclose(f->file);
    free(f);
    return 0;
}