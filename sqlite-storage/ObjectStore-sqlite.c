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
#include <libgen.h>

#include <sqlite3.h>

#include "../utils/stringutils.h"
#include "../utils/buffer.h"
#include "../utils/utils.h"
#include "../Storage/ObjectStore.h"

#if DEBUG
#define debug(fmt,args...) fprintf(stderr, "%s:%d -- " fmt, basename( __FILE__ ), __LINE__, ##args)
#else
#define debug(fmt,args...)
#endif

static const char *initsql = "BEGIN TRANSACTION; "
"CREATE TABLE IF NOT EXISTS meta ( id TEXT UNIQUE PRIMARY KEY NOT NULL,"
" seq INTEGER DEFAULT (1) ); "
"INSERT OR IGNORE INTO meta VALUES ( 'meta', 1 ); "
"CREATE TABLE IF NOT EXISTS docs ( "
"  id TEXT UNIQUE PRIMARY KEY NOT NULL,"
"  deleted INTEGER NOT NULL default (0),"
"  rev BLOB NOT NULL,"
"  oldrevs BLOB NOT NULL,"
"  seq INTEGER NOT NULL,"
"  doc_seq INTEGER NOT NULL,"
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

#define COLUMN_ID       1
#define COLUMN_DELETED  2
#define COLUMN_REV      3
#define COLUMN_OLDREVS  4
#define COLUMN_SEQ      5
#define COLUMN_DOCSEQ   6
#define COLUMN_DOC      7

#define RCOLUMN_ID      0
#define RCOLUMN_DELETED 1
#define RCOLUMN_REV     2
#define RCOLUMN_OLDREVS 3
#define RCOLUMN_SEQ     4
#define RCOLUMN_DOCSEQ  5
#define RCOLUMN_DOC     6

static const char *begintxn = "BEGIN EXCLUSIVE TRANSACTION";
static const char *endtxn = "COMMIT TRANSACTION";
static const char *rollback = "ROLLBACK";
static const char *getbyid = "SELECT * FROM docs WHERE id = ?;";
static const char *getbyidrev = "SELECT * FROM docs WHERE id = ? AND rev = ?;";
static const char *getrev = "SELECT rev FROM docs WHERE id = ?;";
static const char *getmeta = "SELECT rev, oldrevs, seq, doc_seq FROM docs WHERE id = ?";
static const char *getall = "SELECT * FROM docs;";
static const char *getsince = "SELECT * FROM docs WHERE seq >= ?";
static const char *putdoc = "INSERT OR REPLACE INTO docs VALUES (?, ?, ?, ?, ?, ?, ?);";
static const char *getseq = "SELECT seq FROM meta";

struct la_storage_env
{
    char *name;
};

struct la_storage_object_store
{
    la_storage_env *env;
    sqlite3 *db;
};

struct la_storage_object_iterator
{
    la_storage_object_store *store;
    sqlite3_stmt *stmt;
};

const char *
la_storage_driver(void)
{
    return "SQLite";
}

la_storage_env *la_storage_open_env(const char *name)
{
    struct stat st;
    int ret;
    la_storage_env *env = (la_storage_env *) malloc(sizeof(struct la_storage_env));
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

void la_storage_close_env(la_storage_env *env)
{
    free(env->name);
    free(env);
}

la_storage_object_store *la_storage_open(la_storage_env *env, const char *name)
{
    const char *parts[2];
    char *path;
    la_storage_object_store *store = (la_storage_object_store *) malloc(sizeof(struct la_storage_object_store));
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

la_storage_object_get_result la_storage_get(la_storage_object_store *store, const char *key, const la_storage_rev_t *rev, la_storage_object **obj)
{
    sqlite3_stmt *stmt;
    int ret;
    if (rev == NULL)
    {
        if (sqlite3_prepare(store->db, getbyid, (int) strlen(getbyid), &stmt, NULL) != SQLITE_OK)
            return LA_STORAGE_OBJECT_GET_ERROR;
    }
    else
        if (sqlite3_prepare(store->db, getbyidrev, (int) strlen(getbyidrev), &stmt, NULL) != SQLITE_OK)
            return LA_STORAGE_OBJECT_GET_ERROR;
    if (sqlite3_bind_text(stmt, 1, key, (int) strlen(key), SQLITE_TRANSIENT) != SQLITE_OK)
    {
        sqlite3_finalize(stmt);
        return LA_STORAGE_OBJECT_GET_ERROR;
    }
    if (rev != NULL && sqlite3_bind_blob(stmt, 2, rev, sizeof(la_storage_rev_t), SQLITE_TRANSIENT) != SQLITE_OK)
    {
        sqlite3_finalize(stmt);
        return LA_STORAGE_OBJECT_GET_ERROR;
    }
    ret = sqlite3_step(stmt);
    if (ret == SQLITE_ROW)
    {
        if (obj != NULL)
        {
            la_storage_rev_t *rev = (la_storage_rev_t *) sqlite3_column_blob(stmt, RCOLUMN_REV);
            la_storage_rev_t *oldrev = (la_storage_rev_t *) sqlite3_column_blob(stmt, RCOLUMN_OLDREVS);
            size_t oldrev_count = sqlite3_column_bytes(stmt, RCOLUMN_OLDREVS);
            *obj = malloc(sizeof(la_storage_object));
            (*obj)->key = strdup((const char *) sqlite3_column_text(stmt, RCOLUMN_ID));
            (*obj)->data_length = sqlite3_column_bytes(stmt, RCOLUMN_DOC);
            (*obj)->header = malloc(sizeof(la_storage_object_header) + (oldrev_count * sizeof(la_storage_rev_t)) + (*obj)->data_length);
            (*obj)->header->deleted = sqlite3_column_int(stmt, RCOLUMN_DELETED);
            (*obj)->header->seq = sqlite3_column_int64(stmt, RCOLUMN_SEQ);
            (*obj)->header->doc_seq = sqlite3_column_int64(stmt, RCOLUMN_DOCSEQ);
            (*obj)->header->rev_count = oldrev_count;
            memcpy(&(*obj)->header->rev, rev, sizeof(la_storage_rev_t));
            memcpy((*obj)->header->revs_data, oldrev, oldrev_count * sizeof(la_storage_rev_t));
            memcpy(la_storage_object_get_data(*obj), sqlite3_column_blob(stmt, RCOLUMN_DOC), (*obj)->data_length);
        }
        sqlite3_finalize(stmt);
        return LA_STORAGE_OBJECT_GET_OK;
    }
    else if (ret == SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return LA_STORAGE_OBJECT_GET_NOT_FOUND;
    }
    sqlite3_finalize(stmt);
    return LA_STORAGE_OBJECT_GET_ERROR;
}

la_storage_object_get_result la_storage_get_rev(la_storage_object_store *store, const char *key, la_storage_rev_t *rev)
{
    sqlite3_stmt *stmt;
    int ret;
    
    if (sqlite3_prepare(store->db, getrev, (int) strlen(getrev), &stmt, NULL) != SQLITE_OK)
        return LA_STORAGE_OBJECT_GET_ERROR;
    if (sqlite3_bind_text(stmt, 1, key, (int) strlen(key), SQLITE_TRANSIENT) != SQLITE_OK)
    {
        sqlite3_finalize(stmt);
        return LA_STORAGE_OBJECT_GET_ERROR;
    }
    ret = sqlite3_step(stmt);
    if (ret == SQLITE_ROW)
    {
        if (rev != NULL)
        {
            memcpy(rev, sqlite3_column_blob(stmt, 0), sizeof(la_storage_rev_t));
        }
        sqlite3_finalize(stmt);
        return LA_STORAGE_OBJECT_GET_OK;
    }
    else if (ret == SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return LA_STORAGE_OBJECT_GET_NOT_FOUND;
    }
    sqlite3_finalize(stmt);
    return LA_STORAGE_OBJECT_GET_ERROR;
}

la_storage_object_put_result la_storage_put(la_storage_object_store *store, const la_storage_rev_t *rev, la_storage_object *obj)
{
    sqlite3_stmt *stmt;
    int ret;
    uint64_t seq, doc_seq;
    
    sqlite3_exec(store->db, begintxn, NULL, NULL, NULL);
    
    if (sqlite3_prepare(store->db, getseq, (int) strlen(getseq), &stmt, NULL) != SQLITE_OK)
        return LA_STORAGE_OBJECT_PUT_ERROR;
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    seq = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
        
    if (sqlite3_prepare(store->db, getmeta, (int) strlen(getmeta), &stmt, NULL) != SQLITE_OK)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    if (sqlite3_bind_text(stmt, 1, obj->key, (int) strlen(obj->key), NULL) != SQLITE_OK)
    {
        sqlite3_finalize(stmt);
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    ret = sqlite3_step(stmt);
    if (ret == SQLITE_ROW)
    {
        la_storage_rev_t *r = (la_storage_rev_t *) sqlite3_column_blob(stmt, 0);
        if (rev == NULL || memcmp(rev, r, sizeof(la_storage_rev_t)) != 0)
        {
            sqlite3_finalize(stmt);
            sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
            return LA_STORAGE_OBJECT_PUT_CONFLICT;
        }
        doc_seq = sqlite3_column_int64(stmt, 3);
        unsigned int oldrev_bytes = (unsigned int) sqlite3_column_bytes(stmt, 1);
        unsigned int oldrev_count = oldrev_bytes / (unsigned int) sizeof(la_storage_rev_t);
        if (oldrev_count < LA_OBJECT_MAX_REVISION_COUNT)
            oldrev_count++;
        int rc = obj->header->rev_count;
        obj->header->rev_count = oldrev_count;
        void *newheader = realloc(obj->header, la_storage_object_total_size(obj));
        if (newheader == NULL)
        {
            sqlite3_finalize(stmt);
            sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
            return LA_STORAGE_OBJECT_PUT_ERROR;
        }
        obj->header = newheader;
        memmove(la_storage_object_get_data(obj), obj->header->revs_data + (rc * sizeof(la_storage_rev_t)), obj->data_length);
        memcpy(obj->header->revs_data + sizeof(la_storage_rev_t),
               sqlite3_column_blob(stmt, 1), la_min(oldrev_bytes, LA_OBJECT_MAX_REVISION_COUNT * sizeof(la_storage_rev_t)));
        memcpy(obj->header->revs_data, r, sizeof(la_storage_rev_t));
    }
    else if (ret != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    else
    {
        obj->header->rev_count = 0;
    }
    sqlite3_finalize(stmt);
    
    obj->header->seq = seq;
    if (sqlite3_prepare(store->db, putdoc, (int) strlen(putdoc), &stmt, NULL) != SQLITE_OK)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    if (sqlite3_bind_text(stmt, COLUMN_ID, obj->key, (int) strlen(obj->key), SQLITE_TRANSIENT) != SQLITE_OK)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    if (sqlite3_bind_int(stmt, COLUMN_DELETED, obj->header->deleted) != SQLITE_OK)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    if (sqlite3_bind_blob(stmt, COLUMN_REV, &obj->header->rev, sizeof(la_storage_rev_t), SQLITE_TRANSIENT) != SQLITE_OK)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    if (sqlite3_bind_blob(stmt, COLUMN_OLDREVS, obj->header->revs_data, sizeof(la_storage_rev_t) * obj->header->rev_count, SQLITE_TRANSIENT) != SQLITE_OK)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    if (sqlite3_bind_int64(stmt, COLUMN_SEQ, obj->header->seq) != SQLITE_OK)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    if (sqlite3_bind_int64(stmt, COLUMN_DOCSEQ, obj->header->doc_seq) != SQLITE_OK)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    if (sqlite3_bind_blob(stmt, COLUMN_DOC, la_storage_object_get_data(obj), obj->data_length, SQLITE_TRANSIENT) != SQLITE_OK)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE)
    {
        sqlite3_exec(store->db, rollback, NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        return LA_STORAGE_OBJECT_PUT_ERROR;
    }
    sqlite3_finalize(stmt);
    
    sqlite3_exec(store->db, endtxn, NULL, NULL, NULL);
    return LA_STORAGE_OBJECT_PUT_SUCCESS;
}

uint64_t la_storage_lastseq(la_storage_object_store *store)
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

la_storage_object_iterator *la_storage_iterator_open(la_storage_object_store *store, uint64_t since)
{
    la_storage_object_iterator *it = (la_storage_object_iterator *) malloc(sizeof(struct la_storage_object_iterator));
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

la_storage_object_iterator_result la_storage_iterator_next(la_storage_object_iterator *iterator, la_storage_object **obj)
{
    int ret = sqlite3_step(iterator->stmt);
    if (ret == SQLITE_DONE)
    {
        return LA_STORAGE_OBJECT_ITERATOR_END;
    }
    else if (ret == SQLITE_ROW)
    {
        if (obj != NULL)
        {
            la_storage_rev_t *rev = (la_storage_rev_t *) sqlite3_column_blob(iterator->stmt, RCOLUMN_REV);
            la_storage_rev_t *oldrev = (la_storage_rev_t *) sqlite3_column_blob(iterator->stmt, RCOLUMN_OLDREVS);
            size_t oldrev_count = sqlite3_column_bytes(iterator->stmt, RCOLUMN_OLDREVS);
            *obj = malloc(sizeof(la_storage_object));
            (*obj)->key = strdup((const char *) sqlite3_column_text(iterator->stmt, RCOLUMN_ID));
            (*obj)->data_length = sqlite3_column_bytes(iterator->stmt, RCOLUMN_DOC);
            (*obj)->header = malloc(sizeof(la_storage_object_header) + (oldrev_count * sizeof(la_storage_rev_t)) + (*obj)->data_length);
            (*obj)->header->deleted = sqlite3_column_int(iterator->stmt, RCOLUMN_DELETED);
            (*obj)->header->seq = sqlite3_column_int64(iterator->stmt, RCOLUMN_SEQ);
            (*obj)->header->doc_seq = sqlite3_column_int64(iterator->stmt, RCOLUMN_DOCSEQ);
            (*obj)->header->rev_count = oldrev_count;
            memcpy(&(*obj)->header->rev, rev, sizeof(la_storage_rev_t));
            memcpy((*obj)->header->revs_data, oldrev, oldrev_count * sizeof(la_storage_rev_t));
            memcpy(la_storage_object_get_data(*obj), sqlite3_column_blob(iterator->stmt, RCOLUMN_DOC), (*obj)->data_length);
        }
        return LA_STORAGE_OBJECT_ITERATOR_GOT_NEXT;
    }
    return LA_STORAGE_OBJECT_ITERATOR_ERROR;
}

void la_storage_iterator_close(la_storage_object_iterator *iterator)
{
    sqlite3_finalize(iterator->stmt);
    free(iterator);
}

void la_storage_close(la_storage_object_store *store)
{
    sqlite3_close(store->db);
    free(store);
}
