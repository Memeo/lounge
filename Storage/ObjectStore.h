//
//  ObjectStore.h
//  LoungeAct
//
//  Created by Casey Marshall on 2/23/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#ifndef LoungeAct_ObjectStore_h
#define LoungeAct_ObjectStore_h

#include <stdint.h>

#define MAX_REVISION_LEN 255

typedef struct LAStorageEnvironment LAStorageEnvironment;
typedef struct LAStorageObjectStore LAStorageObjectStore;
typedef struct LAStorageObjectIterator LAStorageObjectIterator;

typedef struct LAStorageObjectHeader
{
    uint64_t seq;
    unsigned char rev_data[];
} LAStorageObjectHeader;

typedef struct LAStorageObject
{
    char *key;
    LAStorageObjectHeader *header;
    uint32_t data_length;
} LAStorageObject;

#define la_storage_object_get_data(obj) ((obj)->header->rev_data + strlen((const char *) (obj)->header->rev_data) + 1)
#define la_storage_object_total_size(obj) (sizeof(struct LAStorageObjectHeader) + strlen((const char *) (obj)->header->rev_data) + (obj)->data_length + 1)

typedef enum
{
    LAStorageObjectGetSuccess = 0,      /**< No error, get succeeded. */
    LAStorageObjectGetNotFound,         /**< The requested object was not found. */
    LAStorageObjectGetRevisionNotFound, /**< The requested revision was not the current one. */
    LAStorageObjectGetInternalError     /**< The driver experienced some internal error. */
} LAStorageObjectGetResult;

typedef enum
{
    LAStorageObjectGetOptionNoAllocate = 1<<0, /**< Don't allocate 'data' member. */
    LAStorageObjectGetOptionRevOnly    = 1<<1  /**< Fetch the revision only. */
} LAStorageObjectGetOption;

typedef enum
{
    LAStorageObjectPutSuccess = 0,   /**< No error, put succeeded. */
    LAStorageObjectPutConflict,      /**< Putting the object conflicted. */
    LAStorageObjectPutInternalError
} LAStorageObjectPutResult;

typedef enum
{
    LAStorageObjectIteratorGotNext = 0,
    LAStorageObjectIteratorEnd,
    LAStorageObjectIteratorError
} LAStorageObjectIteratorResult;

const char *la_storage_driver(void);

LAStorageObject *la_storage_create_object(const char *key, const char *rev, const unsigned char *data, uint32_t length);
void la_storage_destroy_object(LAStorageObject *object);

LAStorageEnvironment *la_storage_open_env(const char *name);
void la_storage_close_env(LAStorageEnvironment *env);

/**
 * Open an object store.
 *
 * Returns NULL if opening the store fails.
 */
LAStorageObjectStore *la_storage_open(LAStorageEnvironment *env, const char *name);

/**
 * Get an object from the store, placing it in the given pointer.
 *
 * @param store The object store handle.
 * @param key The key of the object to get (null-terminated).
 * @param rev The revision to match when getting the object (if null, get the latest rev).
 * @param obj Pointer for the object. Must be released with storage_destroy_object.
 */
LAStorageObjectGetResult la_storage_get(LAStorageObjectStore *store, const char *key, const char *rev, LAStorageObject **obj);

LAStorageObjectGetResult la_storage_get_rev(LAStorageObjectStore *store, const char *key, char *rev, const size_t maxlen);

/**
 * Put an object into the store.
 *
 * @param store The object store handle.
 * @param rev The revision of the previous version, or NULL if you believe you are creating an new object.
 * @param obj The object to put.
 */
LAStorageObjectPutResult la_storage_put(LAStorageObjectStore *store, const char *rev, LAStorageObject *obj);

uint64_t la_storage_lastseq(LAStorageObjectStore *store);

LAStorageObjectIterator *la_storage_iterator_open(LAStorageObjectStore *store, uint64_t since);
LAStorageObjectIteratorResult la_storage_iterator_next(LAStorageObjectIterator *iterator, LAStorageObject **obj);
void la_storage_iterator_close(LAStorageObjectIterator *iterator);

void la_storage_close(LAStorageObjectStore *store);

#endif
