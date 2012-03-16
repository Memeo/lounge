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

#include "pull.h"
#include "../utils/buffer.h"
#include "../Codec/Codec.h"

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

la_codec_value_t *la_pull_conflict_resolver_mine(const char *key, const la_codec_value_t *mine,
                                                 const la_codec_value_t *theirs, void *baton)
{
    return mine;
}

la_codec_value_t *la_pull_conflict_resolver_theirs(const char *key, const la_codec_value_t *mine,
                                                   const la_codec_value_t *theirs, void *baton)
{
    return thiers;
}

static size_t pull_write_cb(void *ptr, size_t size, size_t nmemb, void *baton)
{
    la_pull_t *pull = (la_pull_t *) baton;

    if (la_buffer_append(pull->buffer, ptr, size * nmemb) != 0)
        return CURL_WRITEFUNC_PAUSE;
    return nmemb;
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
    memset(&puller, 0, sizeof(struct la_pull));
    buffer = la_buffer_new(128);
    if (buffer == NULL)
    {
        free(puller);
        return NULL;
    }
    
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
        return -1;
    
    if (curl_easy_getinfo(puller->curl, CURLINFO_RESPONSE_CODE, &status) != CURLE_OK)
        return -1;
    curl_easy_cleanup(puller->curl);
       
    if (status != 200)
        return -1;
        
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
    for (i = 0; i < len; i++)
    {
        la_codec_error_t codec_error;
        la_db_get_result ret;
        la_rev_t rev, thatrev;
        json_t *key, *changes_array, *change_obj, *revobj;
        json_t *change = json_array_get(results, i);
        if (change == NULL || !json_is_object(change))
            continue;
        key = json_object_get(change, "id");
        if (key == NULL || !json_is_string(key))
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
        la_rev_scan(json_string_value(revobj), &thatrev);
        ret = la_db_get(puller->db, json_string_value(key), NULL, NULL, &rev, &codec_error);
        if (ret == LA_DB_GET_ERROR)
            continue;
        if (ret == LA_DB_GET_NOT_FOUND || memcmp(&rev.rev.rev, &thatrev.rev.rev, LA_OBJECT_REVISION_LEN) != 0)
        {
            // Rev is missing, fetch it.
            la_buffer_t *urlbuf = la_buffer_new(256);
            la_buffer_appendf(urlbuf, "%s/%s?revs=true&rev=%s", puller->urlbase, key, json_string_value(revobj));
            char *url = la_buffer_string(urlbuf);
            la_buffer_destroy(urlbuf);
            CURL *fetch = curl_easy_init();
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
                continue;
            
            json_t *_revisions = json_object_get(object, "_revisions");
            if (_revisions == NULL || !json_is_object(_revisions))
            {
                json_decref(object);
                continue;
            }
            json_incref(_revisions);
            json_object_del(object, "_revisions");
            
            json_t *_revisions_ids = json_object_get(_revisions, "ids");
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