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

#ifndef LA_OBJECT_REVISION_LEN
#define LA_OBJECT_REVISION_LEN 16
#endif

#ifndef LA_OBJECT_MAX_REVISION_COUNT
#define LA_OBJECT_MAX_REVISION_COUNT 1024
#endif

typedef struct la_storage_rev
{
    unsigned char rev[LA_OBJECT_REVISION_LEN];
} la_storage_rev_t;

typedef struct la_storage_env la_storage_env;
typedef struct la_storage_object_store la_storage_object_store;
typedef struct la_storage_object_iterator la_storage_object_iterator;

typedef struct la_storage_object_header
{
    /**
     * The overall update sequence for the database.
     */
    uint64_t seq;
    
    /**
     * The update sequence for this document.
     */
    uint64_t doc_seq;
    
    /**
     * Boolean for deleted revisions.
     */
    uint8_t deleted:1;

    /**
     * The number of historical revisions that follow.
     */
    uint16_t rev_count;
    
    /**
     * The current revision of this object.
     */
    la_storage_rev_t rev;
    
    /**
     * The historical revision list (rev_count * LA_OBJECT_REVISION_LEN)
     * followed by the current object revision data.
     */
    unsigned char revs_data[];
} la_storage_object_header;

typedef struct la_storage_object
{
    char *key;
    la_storage_object_header *header;
    uint32_t data_length;
} la_storage_object;

#define la_storage_object_get_data(obj) ((obj)->header->revs_data + (LA_OBJECT_REVISION_LEN * (obj)->header->rev_count))
#define la_storage_object_total_size(obj) (sizeof(struct la_storage_object_header) + (LA_OBJECT_REVISION_LEN * (obj)->header->rev_count) + (obj)->data_length)

//#define la_storage_object_get_data(obj) ((obj)->header->rev_data + strlen((const char *) (obj)->header->rev_data) + 1)
//#define la_storage_object_total_size(obj) (sizeof(struct la_storage_object_header) + strlen((const char *) (obj)->header->rev_data) + (obj)->data_length + 1)

typedef enum
{
    LA_STORAGE_OBJECT_GET_OK = 0,      /**< No error, get succeeded. */
    LA_STORAGE_OBJECT_GET_NOT_FOUND,         /**< The requested object was not found. */
    LA_STORAGE_OBJECT_GET_REVISION_NOT_FOUND, /**< The requested revision was not the current one. */
    LA_STORAGE_OBJECT_GET_ERROR     /**< The driver experienced some internal error. */
} la_storage_object_get_result;

typedef enum
{
    LA_STORAGE_OBJECT_PUT_SUCCESS = 0,   /**< No error, put succeeded. */
    LA_STORAGE_OBJECT_PUT_CONFLICT,      /**< Putting the object conflicted. */
    LA_STORAGE_OBJECT_PUT_ERROR
} la_storage_object_put_result;

typedef enum
{
    LA_STORAGE_OBJECT_ITERATOR_GOT_NEXT = 0,
    LA_STORAGE_OBJECT_ITERATOR_END,
    LA_STORAGE_OBJECT_ITERATOR_ERROR
} la_storage_object_iterator_result;

typedef la_storage_object *(*la_storage_view_map)(la_storage_object *object, void *baton);
typedef la_storage_object *(*la_storage_view_reduce)(la_storage_object *accum, la_storage_object *object, void *baton);
typedef la_storage_object *(*la_storage_view_rereduce)(la_storage_object **accums, size_t count, void *baton);

const char *la_storage_driver(void);

la_storage_object *la_storage_create_object(const char *key, const la_storage_rev_t rev, const unsigned char *data, uint32_t length);
void la_storage_destroy_object(la_storage_object *object);

int la_storage_object_get_all_revs(const la_storage_object *object, la_storage_rev_t **revs);

la_storage_env *la_storage_open_env(const char *name);
void la_storage_close_env(la_storage_env *env);

/**
 * Open an object store.
 *
 * Returns NULL if opening the store fails.
 */
la_storage_object_store *la_storage_open(la_storage_env *env, const char *name);

/**
 * Get an object from the store, placing it in the given pointer.
 *
 * @param store The object store handle.
 * @param key The key of the object to get (null-terminated).
 * @param rev The revision to match when getting the object (if null, get the latest rev).
 * @param obj Pointer for the object. Must be released with storage_destroy_object.
 */
la_storage_object_get_result la_storage_get(la_storage_object_store *store, const char *key, const la_storage_rev_t *rev, la_storage_object **obj);

la_storage_object_get_result la_storage_get_rev(la_storage_object_store *store, const char *key, la_storage_rev_t *rev);

/**
 * Fetch all revisions of the object.
 *
 * @param store
 * @param key
 * @param revs Pointer to where to store the start of the revisions.
 *  The value stored in this pointer does not need to be freed, but
 *  also should not be modified.
 * @param revcount Pointer to where to store the revision count.
 * @return A status enum about the get.
 */
la_storage_object_get_result la_storage_get_all_revs(la_storage_object_store *store, const char *key, la_storage_rev_t *revs, size_t *revcount);

la_storage_object_put_result la_storage_set_revs(la_storage_object_store *store, const char *key, la_storage_rev_t *revs, size_t revcount);

/**
 * Put an object into the store.
 *
 * @param store The object store handle.
 * @param rev The revision of the previous version, or NULL if you believe you are creating an new object.
 * @param obj The object to put.
 */
la_storage_object_put_result la_storage_put(la_storage_object_store *store, const la_storage_rev_t *rev, la_storage_object *obj);

uint64_t la_storage_lastseq(la_storage_object_store *store);

la_storage_object_iterator *la_storage_iterator_open(la_storage_object_store *store, uint64_t since);
la_storage_object_iterator_result la_storage_iterator_next(la_storage_object_iterator *iterator, la_storage_object **obj);
void la_storage_iterator_close(la_storage_object_iterator *iterator);

int la_storage_install_view(la_storage_object_store *store, const char *name, la_storage_view_map mapfn, la_storage_view_reduce reducefn, void *baton);

void la_storage_close(la_storage_object_store *store);

#endif
