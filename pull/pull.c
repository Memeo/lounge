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
    int options;
    la_buffer_t *buffer;
    char *url;
    char *last_seq;
};

static size_t pull_write_cb(void *ptr, size_t size, size_t nmemb, void *baton)
{
    la_pull_t *pull = (la_pull_t *) baton;

    if (la_buffer_append(pull->buffer, ptr, size * nmemb) != 0)
        return CURL_WRITEFUNC_PAUSE;
    if ((pull->options & LA_PULL_CONTINUOUS) != 0)
    {
        void *p = memchr(la_buffer_data(pull->buffer), '\n', la_buffer_size(pull->buffer));
        if (p != NULL)
        {
            size_t len = p - la_buffer_data(pull->buffer);
            if (len > 0)
            {
                json_error_t error;
                json_t *json = json_loadb(la_buffer_data(pull->buffer), len, 0, &error);
                if (json == NULL)
                    return CURL_WRITEFUNC_PAUSE;
                la_codec_value_t *value = la_codec_from_json(json);
                if (la_codec_is_object(value))
                {
                    la_codec_value_t *id = la_codec_object_get(value, LA_API_KEY_NAME);
                    la_codec_value_t *rev = la_codec_object_get(value, LA_API_REV_NAME);
                    if (id == NULL || rev == NULL || 
                }
            }
            la_buffer_remove(pull->buffer, 0, len + 1);
        }
    }
    return nmemb;
}

la_pull_t *la_pull_create(la_db_t *db, la_pull_params_t *params)
{
    la_pull_t *puller = (la_pull_t *) malloc(sizeof(struct la_pull));
    la_buffer_t *buffer;
    
    if (puller == NULL)
    {
        return NULL;
    }
    buffer = la_buffer_new(128);
    if (buffer == NULL)
    {
        free(puller);
        return NULL;
    }
    
    la_buffer_appendf(buffer, "http%s://%s:%u/%s/_changes/?include_docs=true",
                      (params->options & LA_PULL_SECURE) != 0 ? "s" : "",
                      params->host, params->port, params->dbname);
    if (params->filter != NULL)
    {
        la_buffer_appendf(buffer, "&filter=%s", params->filter);
        any_params = 1;
    }
    if ((params->options & LA_PULL_CONTINUOUS) != 0)
    {
        if ((params->options & LA_PULL_CONTINUOUS_WITH_LONGPOLL) != 0)
        {
            la_buffer_appendf("&feed=longpoll");
        }
        else
        {
            la_buffer_appendf("&feed=continuous");
        }
    }
    puller->url = la_buffer_string(buffer);
    la_buffer_destroy(buffer);
    
    puller->curl = curl_easy_init();
    if (puller->curl == NULL)
    {
        free(puller);
        return NULL;
    }
    if (curl_easy_setopt(puller->curl, CURLOPT_URL, puller->url) != CURLE_OK)
    {
        curl_easy_cleanup(puller->curl);
        free(puller);
    }
    if (params->user != NULL)
    {
        if (curl_easy_setopt(puller->curl, CURLOPT_USERNAME, params->user) != CURLE_OK)
        {
            curl_easy_cleanup(puller->curl);
            free(puller);
        }
    }
    if (params->password != NULL)
    {
        if (curl_easy_setopt(puller->curl, CURLOPT_USERPWD, params->password) != CURLE_OK)
        {
            curl_easy_cleanup(puller->curl);
            free(puller);
        }
    }
    curl_easy_setopt(puller->curl, CURLOPT_WRITEFUNCTION, pull_write_cb);
    curl_easy_setopt(puller->curl, CURLOPT_WRITEDATA, puller);
    puller->buffer = la_buffer_new(1024);
    if (puller->buffer == NULL)
    {
        curl_easy_cleanup(puller->curl);
        free(puller);
    }
    puller->options = params->options;
    
    return puller;
}

int la_pull_start(la_pull_t *puller)
{
    CURLCode ret;
    int keepgoing = 1;
    
    do
    {
        long status;
        ret = curl_easy_perform(puller->curl);
        if (ret != CURLE_OK)
        {
            return -1;
        }
        
        if (curl_easy_getinfo(puller->curl, CURLINFO_RESPONSE_CODE, &status) != CURLE_OK)
            return -1;
        
        if (status != 200)
            return -1;
        
        curl_easy_reset(puller->curl);
        if ((puller->options & (LA_PULL_CONTINUOUS|LA_PULL_CONTINUOUS_WITH_LONGPOLL)) == LA_PULL_CONTINUOUS|LA_PULL_CONTINUOUS_WITH_LONGPOLL)
        {
            if (puller->last_seq != NULL)
            {
                la_buffer_t *buf = la_buffer_new(128);
                la_buffer_append(buf, puller->url, strlen(puller->url));
                la_buffer_appendf(buf, "&since=%s", puller->last_seq);
                char *url = la_buffer_string(buf);
                curl_easy_setopt(puller->curl, CURLOPT_URL, url);
                la_buffer_destroy(buf);
                free(url);
            }
        }
        else
            keepgoing = 0;
    } while (keepgoing);
    return 0;
}

int la_pull_pause(la_pull_t *puller)
{
    
}

void la_pull_destroy(la_pulL_t *puller)
{
    
}