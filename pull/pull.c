//
//  pull.c
//  LoungeAct
//
//  Created by Casey Marshall on 3/5/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <jansson.h>
#include <ctype.h>

#include "pull.h"
#include "../utils/buffer.h"
#include "../utils/hexdump.h"
#include "../Codec/Codec.h"

#if DEBUG
#define debug(fmt, args...) fprintf(stderr, fmt, ##args)
#else
#define debug(fmt, args...)
#endif

struct la_pull
{
    la_db_t *db;
    la_pull_params_t params;
    CURL *curl;
    char *user;
    char *password;
    int options;
    la_buffer_t *buffer;
    char *urlbase;
    char *url;
    char sep;
    char *last_seq;
    la_pull_conflict_resolver resolver;
    void *resolver_baton;
};

static la_pull_resolve_result_t _resolve_mine(const char *key, const la_codec_value_t *mine,
                                              const la_codec_value_t *theirs, la_codec_value_t **merged,
                                              void *baton)
{
    return LA_PULL_RESOLVE_TAKE_MINE;
}
la_pull_conflict_resolver la_pull_conflict_resolver_mine = _resolve_mine;

static la_pull_resolve_result_t _resolve_theirs(const char *key, const la_codec_value_t *mine,
                                                const la_codec_value_t *theirs, la_codec_value_t **merged,
                                                void *baton)
{
    return LA_PULL_RESOLVE_TAKE_THEIRS;
}
la_pull_conflict_resolver la_pull_conflict_resolver_theirs = _resolve_theirs;

static size_t pull_write_cb(void *ptr, size_t size, size_t nmemb, void *baton)
{
    la_pull_t *pull = (la_pull_t *) baton;

#if DEBUG
    printf("CURL -- read bytes:\n");
    la_hexdump(ptr, size * nmemb);
#endif
    
    if (la_buffer_append(pull->buffer, ptr, size * nmemb) != 0)
        return CURL_WRITEFUNC_PAUSE;
    return nmemb;
}

static char *url_encode(const char *s)
{
    int len = strlen(s);
    la_buffer_t *buffer = la_buffer_new(len);
    char *ret;
    int i;

    for (i = 0; i < len; i++)
    {
        if (s[i] == ' ')
            la_buffer_append(buffer, "+", 1);
        else if (isalnum(s[i]) || s[i] == '.' || s[i] == '-' || s[i] == '_' || s[i] == '~')
            la_buffer_append(buffer, s+i, 1);
        else
        {
            la_buffer_appendf(buffer, "%%%02x", (int) s[i]);
        }
    }
    ret = la_buffer_string(buffer);
    la_buffer_destroy(buffer);
    return ret;
}

static int init_changes_feed(la_pull_t *puller)
{
    puller->curl = curl_easy_init();
    if (puller->curl == NULL)
    {
        return -1;
    }
    if (puller->last_seq != NULL)
    {
        la_buffer_t *b = la_buffer_new(strlen(puller->url) + strlen(puller->last_seq) + 7);
        la_buffer_append(b, puller->url, strlen(puller->url));
        la_buffer_appendf(b, "%csince=%s", puller->sep, puller->last_seq);
        free(puller->url);
        puller->url = la_buffer_string(b);
        la_buffer_destroy(b);
    }
    if (curl_easy_setopt(puller->curl, CURLOPT_URL, puller->url) != CURLE_OK)
    {
        curl_easy_cleanup(puller->curl);
        return -1;
    }
    if (puller->user != NULL)
    {
        if (curl_easy_setopt(puller->curl, CURLOPT_USERNAME, puller->user) != CURLE_OK)
        {
            curl_easy_cleanup(puller->curl);
            return -1;
        }
    }
    if (puller->password != NULL)
    {
        if (curl_easy_setopt(puller->curl, CURLOPT_USERPWD, puller->password) != CURLE_OK)
        {
            curl_easy_cleanup(puller->curl);
            return -1;
        }
    }
    curl_easy_setopt(puller->curl, CURLOPT_WRITEFUNCTION, pull_write_cb);
    curl_easy_setopt(puller->curl, CURLOPT_WRITEDATA, puller);
    puller->buffer = la_buffer_new(1024);
    if (puller->buffer == NULL)
    {
        curl_easy_cleanup(puller->curl);
        return -1;
    }
    
    return 0;
}

la_pull_t *la_pull_create(la_db_t *db, la_pull_params_t *params)
{
    la_pull_t *puller = (la_pull_t *) malloc(sizeof(struct la_pull));
    la_buffer_t *buffer;
    
    if (puller == NULL)
    {
        return NULL;
    }
    memset(puller, 0, sizeof(struct la_pull));
    buffer = la_buffer_new(128);
    if (buffer == NULL)
    {
        free(puller);
        return NULL;
    }
    puller->db = db;
    debug("params options:%x, host:%s, port:%d, dbname:%s\n", params->options,
          params->host, params->port, params->dbname);

    debug("http%s://%s:%u/%s",
          (params->options & LA_PULL_SECURE) != 0 ? "s" : "",
          params->host, params->port, params->dbname);
    la_buffer_appendf(buffer, "http%s://%s:%u/%s",
                      (params->options & LA_PULL_SECURE) != 0 ? "s" : "",
                      params->host, params->port, params->dbname);
    puller->urlbase = la_buffer_string(buffer);
    la_buffer_append(buffer, "/_changes", strlen("/_changes"));
    puller->sep = '?';
    if (params->filter != NULL)
    {
        la_buffer_appendf(buffer, "%cfilter=%s", puller->sep, params->filter);
        puller->sep = '&';
    }
    puller->url = la_buffer_string(buffer);
    la_buffer_destroy(buffer);
    
    debug("using URL %s\n", puller->url);
    
    if (params->user)
    {
        puller->user = strdup(params->user);
        if (puller->user == NULL)
        {
            free(puller->url);
            free(puller);
            return NULL;
        }
    }
    if (params->password)
    {
        puller->password = strdup(params->password);
        if (puller->password == NULL)
        {
            if (puller->user)
                free(puller->user);
            free(puller->url);
            free(puller);
            return NULL;
        }
    }
    puller->options = params->options;
    puller->resolver = params->resolver;
    if (puller->resolver == NULL)
        puller->resolver = la_pull_conflict_resolver_mine;
    puller->resolver_baton = params->baton;
    
    return puller;
}

int la_pull_run(la_pull_t *puller)
{
    CURLcode ret;
    json_t *changes;
    json_error_t json_error;
    long status;
    int i, len;

    if (init_changes_feed(puller) != 0)
        return -1;
    
    ret = curl_easy_perform(puller->curl);
    if (ret != CURLE_OK)
    {
        debug("curl_easy_perform %d\n", ret);
        return -1;
    }
    
    if (curl_easy_getinfo(puller->curl, CURLINFO_RESPONSE_CODE, &status) != CURLE_OK)
    {
        return -1;
    }
    curl_easy_cleanup(puller->curl);
       
    if (status != 200)
    {
        debug("got response code %ld\n", status);
        return -1;
    }
        
    changes = json_loadb(la_buffer_data(puller->buffer), la_buffer_size(puller->buffer), 0, &json_error);
    la_buffer_clear(puller->buffer);
    if (changes == NULL || !json_is_object(changes))
        return -1;
    
    if (puller->last_seq != NULL)
        free(puller->last_seq);
    puller->last_seq = NULL;
    json_t *last_seq = json_object_get(changes, "last_seq");
    if (last_seq != NULL)
        puller->last_seq = json_dumps(last_seq, 0);
    
    json_t *results = json_object_get(changes, "results");
    if (results == NULL || !json_is_array(results))
    {
        json_decref(changes);
        return -1;
    }

    len = json_array_size(results);
    debug("got %d changes\n", len);
    for (i = 0; i < len; i++)
    {
        la_codec_error_t codec_error;
        la_db_get_result ret;
        la_rev_t rev, thatrev;
        json_t *key, *changes_array, *change_obj, *revobj, *seq;
        json_t *change = json_array_get(results, i);
        if (change == NULL || !json_is_object(change))
            continue;
        key = json_object_get(change, "id");
        if (key == NULL || !json_is_string(key))
            continue;
        debug("handling document %s\n", json_string_value(key));
        seq = json_object_get(change, "seq");
        if (seq == NULL || (!json_is_integer(seq) && !json_is_string(seq)))
            continue;
        changes_array = json_object_get(change, "changes");
        if (changes_array == NULL || !json_is_array(changes_array) || json_array_size(changes_array) == 0)
            continue;
        change_obj = json_array_get(changes_array, 0);
        if (change_obj == NULL || !json_is_object(change_obj))
            continue;
        revobj = json_object_get(change_obj, "rev");
        if (revobj == NULL || !json_is_string(revobj))
            continue;
        debug("changed revision %s\n", json_string_value(revobj));
        la_rev_scan(json_string_value(revobj), &thatrev);
        ret = la_db_get(puller->db, json_string_value(key), NULL, NULL, &rev, &codec_error);
        if (ret == LA_DB_GET_ERROR)
            continue;
        debug("get local result %d rev %s\n", ret, ret == LA_DB_GET_OK ? la_rev_string(rev) : "");
        if (ret == LA_DB_GET_NOT_FOUND || memcmp(&rev.rev.rev, &thatrev.rev.rev, LA_OBJECT_REVISION_LEN) != 0)
        {
            // Rev is missing, fetch it.
            la_codec_error_t codec_error;
            la_buffer_t *urlbuf = la_buffer_new(256);
            char *path = url_encode(json_string_value(key));
            la_buffer_appendf(urlbuf, "%s/%s?revs=true&rev=%s", puller->urlbase, path, json_string_value(revobj));
            char *url = la_buffer_string(urlbuf);
            free(path);
            
            debug("fetching %s\n", url);
            la_buffer_destroy(urlbuf);
            CURL *fetch = curl_easy_init();
            la_storage_rev_t *remote_revs = NULL;
            la_storage_rev_t *local_revs = NULL;
            uint64_t mystart;
            int j, revslen;
            
            if (fetch == NULL)
            {
                free(url);
                continue;
            }
            
            if (curl_easy_setopt(fetch, CURLOPT_URL, url) != CURLE_OK)
            {
                free(url);
                curl_easy_cleanup(fetch);
                continue;
            }
            free(url);
            if (puller->user != NULL)
            {
                if (curl_easy_setopt(fetch, CURLOPT_USERNAME, puller->user) != CURLE_OK)
                {
                    curl_easy_cleanup(fetch);
                    continue;
                }
            }
            if (puller->password != NULL)
            {
                if (curl_easy_setopt(fetch, CURLOPT_USERPWD, puller->password) != CURLE_OK)
                {
                    curl_easy_cleanup(fetch);
                    continue;
                }
            }
            curl_easy_setopt(fetch, CURLOPT_WRITEFUNCTION, pull_write_cb);
            curl_easy_setopt(fetch, CURLOPT_WRITEDATA, puller);
            if (curl_easy_perform(fetch) != CURLE_OK)
            {
                curl_easy_cleanup(fetch);
                continue;
            }
            curl_easy_cleanup(fetch);
            
            json_t *object = json_loadb(la_buffer_data(puller->buffer), la_buffer_size(puller->buffer), 0, &json_error);
            la_buffer_clear(puller->buffer);
            if (object == NULL || !json_is_object(object))
            {
                debug("didn't get object back from server %p %d\n", object, object ? json_typeof(object) : -1);
                debug("%s", json_error.text);
                continue;
            }
            
            json_t *_revisions = json_object_get(object, "_revisions");
            if (_revisions == NULL || !json_is_object(_revisions))
            {
                debug("didn't find _revisions\n");
                json_decref(object);
                continue;
            }
            json_incref(_revisions);
            json_object_del(object, "_revisions");
            
            json_t *_revisions_start = json_object_get(_revisions, "start");
            if (_revisions_start == NULL || !json_is_integer(_revisions_start))
            {
                debug("didn't find revisions/start\n");
                json_decref(_revisions);
                json_decref(object);
                continue;
            }
            
            json_t *_revisions_ids = json_object_get(_revisions, "ids");
            if (_revisions_ids == NULL || !json_is_array(_revisions_ids))
            {
                debug("didn't find revisions/ids\n");
                json_decref(_revisions);
                json_decref(object);
                continue;
            }
            
            revslen = json_array_size(_revisions_ids);
            debug("%d remote revisions\n", revslen);
            remote_revs = malloc(sizeof(la_storage_rev_t) * revslen);
            if (remote_revs == NULL)
            {
                json_decref(_revisions);
                json_decref(object);
                continue;
            }
            for (j = 0; j < revslen; j++)
            {
                la_storage_scan_rev(json_string_value(json_array_get(_revisions_ids, j)), &remote_revs[j]);
            }

            // Fetch local revisions of the object, if it exists.
            int myrevcount = 0;
            if (ret == LA_DB_GET_OK)
            {
                myrevcount = la_db_get_allrevs(puller->db, json_string_value(key), &mystart, &local_revs);
                
                if (myrevcount > 0 && local_revs != NULL)
                {
                    la_storage_rev_t *r = realloc(local_revs, (myrevcount + 1) * sizeof(la_storage_rev_t));
                    if (r == NULL)
                    {
                        json_decref(_revisions);
                        json_decref(object);
                        free(local_revs);
                        continue;
                    }
                    local_revs = r;
                    memmove(&local_revs[1], local_revs, sizeof(la_storage_rev_t) * myrevcount);
                    myrevcount++;
                }
                else
                {
                    local_revs = malloc(sizeof(la_storage_rev_t));
                    if (local_revs == NULL)
                    {
                        json_decref(object);
                        continue;
                    }
                    myrevcount = 1;
                }
                memcpy(&local_revs[0], &rev.rev, sizeof(la_storage_rev_t));
            }
            else
            {
                // If it doesn't exist, just set it.
                debug("value never there, just adding it\n");
                la_codec_value_t *value = la_codec_from_json(object);
                la_db_put_result putresult = la_db_replace(puller->db, json_string_value(key), &thatrev, value, &remote_revs[1], revslen - 1);
                if (putresult != LA_DB_PUT_OK)
                {
                    debug("failed to add the doc %d\n", putresult
                          );
                }
                la_codec_decref(value);
                json_decref(object);
                json_decref(_revisions);
                continue;
            }
            
            // There will be three cases:
            //
            //  1. Our current version is part of the history of the remote version (we are behind).
            //     To resolve, take the remote version and the history.
            //
            //  2. Our history contains (some of) the remote history (we are ahead).
            //     To resolve, do nothing.
            //
            //  3. Our history diverges (we have a fork).
            //     To resolve, invoke the conflict handler.
            
            if (la_storage_revs_overlap(local_revs, myrevcount, remote_revs, revslen))
            {
                debug("overwriting local version\n");
                free(local_revs);
                // Just insert the document and revision history.
                la_codec_value_t *value = la_codec_from_json(object);
                la_db_replace(puller->db, json_string_value(key), &thatrev, value, &remote_revs[1], revslen - 1);
                la_codec_decref(value);
            }
            else if (!la_storage_revs_overlap(remote_revs, revslen, local_revs, myrevcount))
            {
                debug("handling conflict...\n");
                free(local_revs);
                // We are conflicting.
                la_codec_value_t *myvalue;
                if (la_db_get(puller->db, json_string_value(key), NULL, &myvalue, NULL, &codec_error) != LA_DB_GET_OK)
                {
                    la_codec_decref(myvalue);
                    json_decref(_revisions);
                    json_decref(object);
                    continue;
                }
                la_codec_value_t *theirvalue = la_codec_from_json(object);
                la_codec_value_t *mergedvalue = NULL;
                
                switch (puller->resolver(json_string_value(key), myvalue, theirvalue, &mergedvalue, puller->resolver_baton))
                {
                    case LA_PULL_RESOLVE_TAKE_MINE:
                        break; // Nothing
                        
                    case LA_PULL_RESOLVE_TAKE_THEIRS:
                        la_db_replace(puller->db, json_string_value(key), &thatrev, theirvalue, &remote_revs[1], revslen - 1);
                        break;
                        
                    case LA_PULL_RESOLVE_TAKE_MERGED:
                        la_db_put(puller->db, json_string_value(key), &rev, mergedvalue, NULL);
                        break;
                }
                
                la_codec_decref(myvalue);
                la_codec_decref(theirvalue);
                if (mergedvalue != NULL)
                    la_codec_decref(mergedvalue);
                free(remote_revs);
            }
            else
            {
                debug("doing nothing, we are ahead of the server");
            }
            // Otherwise, we are ahead. Do nothing.
        }
    }
    json_decref(changes);
 
    if ((puller->options & LA_PULL_CONTINUOUS) != 0)
    {
        init_changes_feed(puller);
    }
    return 0;
}

int la_pull_pause(la_pull_t *puller)
{
    return 0;
}

void la_pull_destroy(la_pull_t *puller)
{
    
}