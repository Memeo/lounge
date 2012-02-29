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

static const char *initsql = "CREATE TABLE IF NOT EXISTS meta ( seq INTEGER PRIMARY KEY DEFAULT (0) ); "
"INSERT OR IGNORE INTO meta VALUES ( 0 ); "
"CREATE TABLE IF NOT EXISTS docs ( "
"  id TEXT UNIQUE PRIMARY KEY NOT NULL,"
"  rev TEXT NOT NULL,"
"  doc BLOB NOT NULL );"
"CREATE TRIGGER IF NOT EXISTS onupdate AFTER UPDATE ON docs BEGIN"
"  UPDATE meta SET seq = seq + 1; "
"END;"
"CREATE TRIGGER IF NOT EXISTS oninsert AFTER INSERT ON docs BEGIN"
"  UPDATE meta SET seq = seq + 1; "
"END;"
"CREATE TRIGGER IF NOT EXISTS ondelete AFTER DELETE ON docs BEGIN"
"  UPDATE meta SET seq = seq + 1; "
"END;";

static const char *getbyid = "SELECT * FROM docs WHERE id = ?;";
static const char *getbyidrev = "SELECT * FROM docs WHERE id = ? AND rev = ?;";
static const char *getrev = "SELECT rev FROM docs WHERE id = ?;";
static const char *getall = "SELECT * FROM docs;";
static const char *putdoc = "INSERT OR REPLACE INTO docs VALUES (?, ?, ?);";

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
                                           sqlite3_column_blob(stmt, 2), 
                                           sqlite3_column_bytes(stmt, 2));
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
    
    if (sqlite3_prepare(store->db, getrev, (int) strlen(getrev), &stmt, NULL) != SQLITE_OK)
        return LAStorageObjectPutInternalError;
    if (sqlite3_bind_text(stmt, 1, obj->key, (int) strlen(obj->key), NULL) != SQLITE_OK)
    {
        sqlite3_finalize(stmt);
        return LAStorageObjectPutInternalError;
    }
    ret = sqlite3_step(stmt);
    if (ret == SQLITE_ROW)
    {
        const char *r = sqlite3_column_text(stmt, 0);
        if (rev == NULL || strcmp(rev, r) != 0)
        {
            sqlite3_finalize(stmt);
            return LAStorageObjectPutConflict;
        }
    }
    else if (ret != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return LAStorageObjectPutInternalError;
    }
    sqlite3_finalize(stmt);
    if (sqlite3_prepare(store->db, putdoc, (int) strlen(putdoc), &stmt, NULL) != SQLITE_OK)
        return LAStorageObjectPutInternalError;
    if (sqlite3_bind_text(stmt, 1, obj->key, (int) strlen(obj->key), SQLITE_TRANSIENT) != SQLITE_OK)
    {
        sqlite3_finalize(stmt);
        return LAStorageObjectPutInternalError;
    }
    if (sqlite3_bind_text(stmt, 2, obj->rev_data, (int) strlen((const char *) obj->rev_data), SQLITE_TRANSIENT) != SQLITE_OK)
    {
        sqlite3_finalize(stmt);
        return LAStorageObjectPutInternalError;
    }
    if (sqlite3_bind_blob(stmt, 3, la_storage_object_get_data(obj), obj->data_length, SQLITE_TRANSIENT) != SQLITE_OK)
    {
        sqlite3_finalize(stmt);
        return LAStorageObjectPutInternalError;
    }
    ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return LAStorageObjectPutInternalError;
    }
    sqlite3_finalize(stmt);
    
    return LAStorageObjectPutSuccess;
}

LAStorageObjectIterator *la_storage_iterator_open(LAStorageObjectStore *store)
{
    LAStorageObjectIterator *it = (LAStorageObjectIterator *) malloc(sizeof(struct LAStorageObjectIterator));
    if (it == NULL)
        return NULL;
    if (sqlite3_prepare(store->db, getall, (int) strlen(getall), &it->stmt, NULL) != SQLITE_OK)
    {
        free(it);
        return NULL;
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
                                            sqlite3_column_blob(iterator->stmt, 2),
                                            sqlite3_column_bytes(iterator->stmt, 2));
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
