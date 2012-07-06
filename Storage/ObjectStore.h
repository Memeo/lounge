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

#pragma pack(push)
#pragma pack(1)
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
    uint16_t rev_count:15;
    
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
#pragma pack(pop)

typedef struct la_storage_object
{
    char *key;
    la_storage_object_header *header;
    uint32_t data_length;
} la_storage_object;

typedef struct la_storage_stat_s
{
    int numkeys;
    uint64_t size;
} la_storage_stat_t;

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
    LA_STORAGE_OPEN_FLAG_CREATE = 1 << 0,
    LA_STORAGE_OPEN_FLAG_EXCL   = 1 << 1
} la_storage_open_flag_t;

typedef enum
{
    LA_STORAGE_OPEN_OK        = 0,
    LA_STORAGE_OPEN_CREATED   = 1,
    LA_STORAGE_OPEN_NOT_FOUND = 2,
    LA_STORAGE_OPEN_EXISTS    = 3,
    LA_STORAGE_OPEN_ERROR     = 4,
    LA_STORAGE_OPEN_NO_DRIVER = 5
} la_storage_open_result_t;

typedef enum
{
    LA_STORAGE_OBJECT_ITERATOR_GOT_NEXT = 0,
    LA_STORAGE_OBJECT_ITERATOR_END,
    LA_STORAGE_OBJECT_ITERATOR_ERROR
} la_storage_object_iterator_result;

typedef struct la_object_store_driver
{
    const char *name;
    la_storage_env * (*open_env)(const char *name);
    int (*close_env)(la_storage_env *env);
    la_storage_open_result_t (*open_store)(la_storage_env *env, const char *name, int flags, la_storage_object_store **store);
    int (*close_store)(la_storage_object_store *store);
    int (*delete_store)(la_storage_object_store *store);
    
    /**
     * Get an object from the store, placing it in the given pointer.
     *
     * @param store The object store handle.
     * @param key The key of the object to get (null-terminated).
     * @param rev The revision to match when getting the object (if null, get the latest rev).
     * @param obj Pointer for the object. Must be released with storage_destroy_object.
     */
    la_storage_object_get_result (*get)(la_storage_object_store *store, const char *key,
                                        const la_storage_rev_t *rev, la_storage_object **obj);
    
    /**
     * Get the revision of an object.
     */
    la_storage_object_get_result (*get_rev)(la_storage_object_store *store, const char *key, la_storage_rev_t *rev);
    
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
    int (*get_all_revs)(la_storage_object_store *store, const char *key, uint64_t *start, la_storage_rev_t **revs);
    
    /**
     * Set the revision history of an object to the given revisions.
     */
    la_storage_object_put_result (*set_revs)(la_storage_object_store *store, const char *key, la_storage_rev_t *revs, size_t revcount);

    /**
     * Put an object into the store.
     *
     * @param store The object store handle.
     * @param rev The revision of the previous version, or NULL if you believe you are creating a new object.
     * @param obj The object to put.
     */
    la_storage_object_put_result (*put)(la_storage_object_store *store, const la_storage_rev_t *rev, la_storage_object *obj);
    
    /**
     * Insert the given document and revisions into the DB, replacing
     * existing values without checking for conflicts.
     */
    la_storage_object_put_result (*replace)(la_storage_object_store *store, la_storage_object *obj);
    
    /**
     * Fetch the last sequence number of this object store.
     */
    uint64_t (*lastseq)(la_storage_object_store *store);
    
    /**
     * Fetch statistics about this object store.
     */
    int (*stat)(la_storage_object_store *store, la_storage_stat_t *stat);
    
    la_storage_object_iterator * (*iterator_open)(la_storage_object_store *store, uint64_t since);
    la_storage_object_iterator_result (*iterator_next)(la_storage_object_iterator *iterator, la_storage_object **obj);
    void (*iterator_close)(la_storage_object_iterator *iterator);    
} la_object_store_driver_t;

int la_storage_install_driver(const char *name, const la_object_store_driver_t *driver);

typedef struct la_object_store la_object_store_t;

#define la_storage_object_get_data(obj) ((obj)->header->revs_data + (LA_OBJECT_REVISION_LEN * (obj)->header->rev_count))
#define la_storage_object_total_size(obj) (sizeof(struct la_storage_object_header) + (LA_OBJECT_REVISION_LEN * (obj)->header->rev_count) + (obj)->data_length)

//#define la_storage_object_get_data(obj) ((obj)->header->rev_data + strlen((const char *) (obj)->header->rev_data) + 1)
//#define la_storage_object_total_size(obj) (sizeof(struct la_storage_object_header) + strlen((const char *) (obj)->header->rev_data) + (obj)->data_length + 1)

typedef la_storage_object *(*la_storage_view_map)(la_storage_object *object, void *baton);
typedef la_storage_object *(*la_storage_view_reduce)(la_storage_object *accum, la_storage_object *object, void *baton);
typedef la_storage_object *(*la_storage_view_rereduce)(la_storage_object **accums, size_t count, void *baton);

la_storage_object *la_storage_create_object(const char *key, const la_storage_rev_t rev,
                                            const unsigned char *data, uint32_t length,
                                            const la_storage_rev_t *revs, size_t revcount);
void la_storage_destroy_object(la_storage_object *object);
int la_storage_scan_rev(const char *str, la_storage_rev_t *rev);

int la_storage_object_get_all_revs(const la_storage_object *object, la_storage_rev_t **revs);

la_storage_env *la_storage_open_env(const char *driver, const char *name);
void la_storage_close_env(const char *driver, la_storage_env *env);

/**
 * Tell if one or more revs starting at revs1 exist in revs2.
 *
 * That is, the following scenario returns 1:
 *           ____ ____ ____ ____
 *  revs1:  | r4 | r3 | r2 | r1 |
 *           ____ ____ ____ ____
 *  revs2:  | r6 | r5 | r4 | r3 |
 *
 * In other words, tell if some nonempty subsequence of revs1
 * exists in revs2.
 */
int la_storage_revs_overlap(const la_storage_rev_t *revs1, int count1, const la_storage_rev_t *revs2, int count2);

/**
 * Open an object store.
 *
 * @param driver The driver name.
 * @param env The environment.
 * @param name The name of the object store to open.
 * @param flags Open flags; bit set of la_storage_open_flag_t.
 * @param store Pointer for the created object store.
 */
la_storage_open_result_t la_storage_open(const char *driver, la_storage_env *env, const char *name, int flags, la_object_store_t **store);

/**
 * Get an object from the store, placing it in the given pointer.
 *
 * @param store The object store handle.
 * @param key The key of the object to get (null-terminated).
 * @param rev The revision to match when getting the object (if null, get the latest rev).
 * @param obj Pointer for the object. Must be released with storage_destroy_object.
 */
la_storage_object_get_result la_storage_get(la_object_store_t *store, const char *key, const la_storage_rev_t *rev, la_storage_object **obj);

la_storage_object_get_result la_storage_get_rev(la_object_store_t *store, const char *key, la_storage_rev_t *rev);

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
int la_storage_get_all_revs(la_object_store_t *store, const char *key, uint64_t *start, la_storage_rev_t **revs);

la_storage_object_put_result la_storage_set_revs(la_object_store_t *store, const char *key, la_storage_rev_t *revs, size_t revcount);

/**
 * Put an object into the store.
 *
 * @param store The object store handle.
 * @param rev The revision of the previous version, or NULL if you believe you are creating a new object.
 * @param obj The object to put.
 */
la_storage_object_put_result la_storage_put(la_object_store_t *store, const la_storage_rev_t *rev, la_storage_object *obj);

/**
 * Insert the given document and revisions into the DB, replacing
 * existing values without checking for conflicts.
 */
la_storage_object_put_result la_storage_replace(la_object_store_t *store, la_storage_object *obj);

uint64_t la_storage_lastseq(la_object_store_t *store);
int la_storage_stat(la_object_store_t *store, la_storage_stat_t *stat);

la_storage_object_iterator *la_storage_iterator_open(la_object_store_t *store, uint64_t since);
la_storage_object_iterator_result la_storage_iterator_next(la_object_store_t *store, la_storage_object_iterator *it, la_storage_object **obj);
void la_storage_iterator_close(la_object_store_t *store, la_storage_object_iterator *it);

int la_storage_install_view(la_object_store_t *store, const char *name, la_storage_view_map mapfn, la_storage_view_reduce reducefn, void *baton);

void la_storage_close(la_object_store_t *store);

void la_storage_delete_store(la_object_store_t *store);

#endif
