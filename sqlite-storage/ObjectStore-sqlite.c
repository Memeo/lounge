//
//  ObjectStore-sqlite.c
//  LoungeAct
//
//  Created by Casey Marshall on 2/27/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include <sqlite3.h>

#include "../utils/stringutils.h"
#include "../Storage/ObjectStore.h"

#if DEBUG
#define debug(fmt,args...) fprintf(stderr, fmt, ##args)
#else
#define debug(fmt,args...)
#endif

static const char *initsql = "BEGIN TRANSACTION; "
"CREATE TABLE IF NOT EXISTS meta ( id TEXT UNIQUE PRIMARY KEY NOT NULL,"
" seq INTEGER DEFAULT (1) ); "
"INSERT OR IGNORE INTO meta VALUES ( 'meta', 1 ); "
"CREATE TABLE IF NOT EXISTS docs ( "
"  id TEXT UNIQUE PRIMARY KEY NOT NULL,"
"  rev TEXT NOT NULL,"
"  seq INTEGER NOT NULL,"
"  doc BLOB NOT NULL );"
"CREATE TRIGGER IF NOT EXISTS onupdate AFTER UPDATE ON docs BEGIN"
"  UPDATE meta SET seq = seq + 1 WHERE id = 'meta'; "
"END;"
"CREATE TRIGGER IF NOT EXISTS oninsert AFTER INSERT ON docs BEGIN"
"  UPDATE meta SET seq = seq + 1 WHERE id = 'meta'; "
"END;"
"CREATE TRIGGER IF NOT EXISTS ondelete AFTER DELETE ON docs BEGIN"
"  UPDATE meta SET seq = seq + 1 WHERE id = 'meta'; "
"END; "
"CREATE INDEX IF NOT EXISTS seqindex ON docs ( seq ASC ); "
"COMMIT;";

static const char *begintxn = "BEGIN EXCLUSIVE TRANSACTION";
static const char *endtxn = "COMMIT TRANSACTION";
static const char *rollback = "ROLLBACK";
static const char *getbyid = "SELECT * FROM docs WHERE id = ?;";
static const char *getbyidrev = "SELECT * FROM docs WHERE id = ? AND rev = ?;";
static const char *getrev = "SELECT rev FROM docs WHERE id = ?;";
static const char *getall = "SELECT * FROM docs;";
static const char *getsince = "SELECT * FROM docs WHERE seq >= ?";
static const char *putdoc = "INSERT OR REPLACE INTO docs VALUES (?, ?, ?, ?);";
static const char *getseq = "SELECT seq FROM meta";

struct LAStorageEnvironment
{
    char *name;
};

struct LAStorageObjectStore
{
    LAStorageEnvironment *env;
    sqlite3 *db;
};

struct LAStorageObjectIterator
{
    LAStorageObjectStore *store;
    sqlite3_stmt *stmt;
};

const char *
la_storage_driver(void)
{
    return "SQLite";
}

LAStorageEnvironment *la_storage_open_env(const char *name)
{
    struct stat st;
    int ret;
    LAStorageEnvironment *env = (LAStorageEnvironment *) malloc(sizeof(struct LAStorageEnvironment));
    if (env == NULL)
        return NULL;
    env->name = strdup(name);
    ret = stat(name, &st);
    if (ret != 0)
    {
        if (errno == ENOENT)
        {
            debug("creating directory %s\n", name);
            if (mkdir(name, 0777) != 0)
            {
                free(env);
                return NULL;
            }
        }
        else
        {
            debug("failed to read directory %s: %s\n", name, strerror(errno));
            free(env);
            return NULL;
        }
    }
    else
    {
        if (!S_ISDIR(st.st_mode))
        {
            debug("%s: %s\n", name, strerror(ENOTDIR));
            free(env);
            return NULL;
        }
    }
    return env;
}

void la_storage_close_env(LAStorageEnvironment *env)
{
    free(env->name);
    free(env);
}

LAStorageObjectStore *la_storage_open(LAStorageEnvironment *env, const char *name)
{
    const char *parts[2];
    char *path;
    LAStorageObjectStore *store = (LAStorageObjectStore *) malloc(sizeof(struct LAStorageObjectStore));
    if (store == NULL)
        return NULL;
    store->env = env;
    parts[0] = env->name;
    parts[1] = name;
    path = string_join("/", (char * const *) parts, 2);
    if (path == NULL)
    {
        free(store);
        return NULL;
    }
    if (sqlite3_open_v2(path, &store->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL) != 0)
    {
        free(store);
        free(path);
        return NULL;
    }
    sqlite3_exec(store->db, initsql, NULL, NULL, NULL);
    
    return store;
}

LAStorageObjectGetResult la_storage_get(LAStorageObjectStore *store, const char *key, const char *rev, LAStorageObject **obj)
{
    sqlite3_stmt *stmt;
    int ret;
    if (rev == NULL)
    {
        if (sqlite3_prepare(store->db, getbyid, (int) strlen(getbyid), &stmt, NULL) != SQLITE_OK)
            return LAStorageObjectGetInternalError;
    }
    else
        if (sqlite3_prepare(store->db, getbyidrev, (int) strlen(getbyidrev), &stmt, NULL) != SQLITE_OK)
            return LAStorageObjectGetInternalError;
    if (sqlite3_bind_text(stmt, 1, key, (int) strlen(key), SQLITE_TRANSIENT) != SQLITE_OK)
    {
        sqlite3_finalize(stmt);
        return LAStorageObjectGetInternalError;
    }
    if (rev != NULL && sqlite3_bind_text(stmt, 2, rev, (int) strlen(rev), SQLITE_TRANSIENT) != SQLITE_OK)
    {
        sqlite3_finalize(stmt);
        return LAStorageObjectGetInternalError;
    }
    ret = sqlite3_step(stmt);
    if (ret == SQLITE_ROW)
    {
        if (obj != NULL)
        {
            *obj = la_storage_create_object((const char *) sqlite3_column_text(stmt, 0),
                                           (const char *) sqlite3_column_text(stmt, 1),
                                           sqlite3_column_blob(stmt, 3), 
                                           sqlite3_column_bytes(stmt, 3));
            (*obj)->header->seq = sqlite3_column_int64(stmt, 2);
        }
        sqlite3_finalize(stmt);
        return LAStorageObjectGetSuccess;
    }
    else if (ret == SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return LAStorageObjectGetNotFound;
    }
    sqlite3_finalize(stmt);
    return LAStorageObjectGetInternalError;
}

LAStorageObjectGetResult la_storage_get_rev(LAStorageObjectStore *store, const char *key, char *rev, const size_t maxlen)
{
    sqlite3_stmt *stmt;
    int ret;
    
    if (sqlite3_prepare(store->db, getrev, (int) strlen(getrev), &stmt, NULL) != SQLITE_OK)
        return LAStorageObjectGetInternalError;
    if (sqlite3_bind_text(stmt, 1, key, (int) strlen(key), SQLITE_TRANSIENT) != SQLITE_OK)
    {
        sqlite3_finalize(stmt);
        return LAStorageObjectGetInternalError;
    }
    ret = sqlite3_step(stmt);
    if (ret == SQLITE_ROW)
    {
        if (rev != NULL)
        {
            strncpy(rev, (const char *) sqlite3_column_text(stmt, 0), maxlen);
        }
        sqlite3_finalize(stmt);
        return LAStorageObjectGetSuccess;
    }
    else if (ret == SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return LAStorageObjectGetNotFound;
    }
    sqlite3_finalize(stmt);
    return LAStorageObjectGetInternalError;
}

LAStorageObjectPutResult la_storage_put(LAStorageObjectStore *store, const char *rev, LAStorageObject *obj)
{
    sqlite3_stmt *stmt;
    int ret;
    uint64_t seq;
    
    sqlite3_exec(store->db, begintxn, NULL, NULL, NULL);
    
    if (sqlite3_prepare(store->db, getseq, (int) strlen(getseq), &stmt, NULL) != SQLITE_OK)
        return LAStorageObjectPutInternalError;
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        return LAStorageObjectPutInternalError;
    }
    seq = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
        
    if (sqlite3_prepare(store->db, getrev, (int) strlen(getrev), &stmt, NULL) != SQLITE_OK)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        return LAStorageObjectPutInternalError;
    }
    if (sqlite3_bind_text(stmt, 1, obj->key, (int) strlen(obj->key), NULL) != SQLITE_OK)
    {
        sqlite3_finalize(stmt);
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        return LAStorageObjectPutInternalError;
    }
    ret = sqlite3_step(stmt);
    if (ret == SQLITE_ROW)
    {
        const char *r = (const char *) sqlite3_column_text(stmt, 0);
        if (rev == NULL || strcmp(rev, r) != 0)
        {
            sqlite3_finalize(stmt);
            sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
            return LAStorageObjectPutConflict;
        }
    }
    else if (ret != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        return LAStorageObjectPutInternalError;
    }
    sqlite3_finalize(stmt);
    
    obj->header->seq = seq;
    if (sqlite3_prepare(store->db, putdoc, (int) strlen(putdoc), &stmt, NULL) != SQLITE_OK)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        return LAStorageObjectPutInternalError;
    }
    if (sqlite3_bind_text(stmt, 1, obj->key, (int) strlen(obj->key), SQLITE_TRANSIENT) != SQLITE_OK)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        return LAStorageObjectPutInternalError;
    }
    if (sqlite3_bind_text(stmt, 2, (const char *) obj->header->rev_data, (int) strlen((const char *) obj->header->rev_data), SQLITE_TRANSIENT) != SQLITE_OK)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        return LAStorageObjectPutInternalError;
    }
    if (sqlite3_bind_int64(stmt, 3, obj->header->seq) != SQLITE_OK)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        return LAStorageObjectPutInternalError;
    }
    if (sqlite3_bind_blob(stmt, 4, la_storage_object_get_data(obj), obj->data_length, SQLITE_TRANSIENT) != SQLITE_OK)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        return LAStorageObjectPutInternalError;
    }
    ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        return LAStorageObjectPutInternalError;
    }
    sqlite3_finalize(stmt);
    
    sqlite3_exec(store->db, endtxn, NULL, NULL, NULL);
    return LAStorageObjectPutSuccess;
}

uint64_t la_storage_lastseq(LAStorageObjectStore *store)
{
    sqlite3_stmt *stmt;
    uint64_t seq;
    if (sqlite3_prepare(store->db, getseq, (int) strlen(getseq), &stmt, NULL) != SQLITE_OK)
        return 0;
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return 0;
    }
    seq = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return seq;
}

LAStorageObjectIterator *la_storage_iterator_open(LAStorageObjectStore *store, uint64_t since)
{
    LAStorageObjectIterator *it = (LAStorageObjectIterator *) malloc(sizeof(struct LAStorageObjectIterator));
    if (it == NULL)
        return NULL;
    if (since > 0)
    {
        if (sqlite3_prepare(store->db, getsince, (int) strlen(getsince), &it->stmt, NULL) != SQLITE_OK)
        {
            free(it);
            return NULL;
        }
        if (sqlite3_bind_int64(it->stmt, 1, since) != SQLITE_OK)
        {
            sqlite3_finalize(it->stmt);
            free(it);
            return NULL;
        }
    }
    else
    {
        if (sqlite3_prepare(store->db, getall, (int) strlen(getall), &it->stmt, NULL) != SQLITE_OK)
        {
            free(it);
            return NULL;
        }
    }
    it->store = store;
    return it;
}

LAStorageObjectIteratorResult la_storage_iterator_next(LAStorageObjectIterator *iterator, LAStorageObject **obj)
{
    int ret = sqlite3_step(iterator->stmt);
    if (ret == SQLITE_DONE)
    {
        return LAStorageObjectIteratorEnd;
    }
    else if (ret == SQLITE_ROW)
    {
        if (obj != NULL)
        {
            *obj = la_storage_create_object((const char *) sqlite3_column_text(iterator->stmt, 0),
                                            (const char *) sqlite3_column_text(iterator->stmt, 1),
                                            sqlite3_column_blob(iterator->stmt, 3),
                                            sqlite3_column_bytes(iterator->stmt, 3));
            (*obj)->header->seq = sqlite3_column_int64(iterator->stmt, 2);
        }
        return LAStorageObjectIteratorGotNext;
    }
    return LAStorageObjectIteratorError;
}

void la_storage_iterator_close(LAStorageObjectIterator *iterator)
{
    sqlite3_finalize(iterator->stmt);
    free(iterator);
}

void la_storage_close(LAStorageObjectStore *store)
{
    sqlite3_close(store->db);
    free(store);
}
