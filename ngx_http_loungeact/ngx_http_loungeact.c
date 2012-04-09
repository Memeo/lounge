//
//  ngx_http_loungeact.c
//  LoungeAct
//
//  Created by Casey Marshall on 4/2/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <syslog.h>

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <api/LoungeAct.h>
#include <compress-lz4/compress-lz4.h>
#include <db_cache.h>
#include <utils/stringutils.h>

static char *welcome = NULL;
static db_cache_t cache;

static ngx_int_t ngx_http_loungeact_handler(ngx_http_request_t *request);
static char *ngx_http_loungeact_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
void *ngx_http_loungeact_create_loc_conf(ngx_conf_t *cf);
char *ngx_http_loungeact_merge_loc_conf(ngx_conf_t *cf, void *prev, void *conf);
static ngx_int_t ngx_http_loungeact_preconf(ngx_conf_t *cf);
static ngx_int_t ngx_http_loungeact_init_module(ngx_cycle_t *cycle);

typedef struct
{
    ngx_str_t la_home;
    la_host_t *la_host;
} ngx_http_loungeact_state_t;

static ngx_command_t ngx_http_loungeact_commands[] = {
    {
        ngx_string("loungeact"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_http_loungeact_command,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },
    ngx_null_command
};

ngx_http_module_t ngx_http_loungeact_module_ctx = {
    ngx_http_loungeact_preconf,            /* preconfiguration */
    NULL,                                  /* postconfiguration */
    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */
    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */
    ngx_http_loungeact_create_loc_conf,    /* create location configration */
    NULL                                   /* merge location configration */
};

ngx_module_t ngx_http_loungeact = {
    NGX_MODULE_V1,
    &ngx_http_loungeact_module_ctx,
    ngx_http_loungeact_commands,
    NGX_HTTP_MODULE,
    NULL,                                  /* init master */
    ngx_http_loungeact_init_module,        /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t
ngx_http_loungeact_init_module(ngx_cycle_t *cycle)
{
    db_cache_init(&cache, 1024);
    la_codec_value_t *obj = la_codec_object();
    la_codec_object_set_new(obj, "couchdb", la_codec_string("Welcome"));
    la_codec_object_set_new(obj, "version", la_codec_string("1.1.1"));
    la_codec_object_set_new(obj, "loungeact", la_codec_string("0.0.1"));
    char *s = la_codec_dumps(obj, LA_CODEC_COMPACT);
    welcome = s;
    openlog("loungeact", LOG_CONS|LOG_PID, LOG_USER);
    syslog(LOG_NOTICE, "module welcome inited: %s", welcome);
    return 0;
}

static ngx_int_t
ngx_http_loungeact_preconf(ngx_conf_t *cf)
{
    return 0;
}

void *ngx_http_loungeact_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_loungeact_state_t *state;
    
    state = ngx_pcalloc(cf->pool, sizeof(ngx_http_loungeact_state_t));
    if (state == NULL)
    {
        return NGX_CONF_ERROR;
    }
    ngx_str_null(&state->la_home);
    state->la_host = NGX_CONF_UNSET_PTR;
    return state;
}

static char *
ngx_http_loungeact_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t *value;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_loungeact_state_t *cglcf = conf;
    char *home;
    
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_loungeact_handler;
    
    value = cf->args->elts;
    cglcf->la_home = value[1];
    home = ngx_pcalloc(cf->pool, cglcf->la_home.len + 1);
    if (home == NULL)
        return "failed malloc";
    memset(home, 0, cglcf->la_home.len + 1);
    strncpy(home, (const char *) cglcf->la_home.data, cglcf->la_home.len);
    cglcf->la_host = la_host_open(home);
    ngx_pfree(cf->pool, home);
    if (cglcf->la_host == NULL)
        return "la_host_open failed";
    la_host_configure_compressor(cglcf->la_host, la_lz4_compressor);
    
    return NGX_CONF_OK;
}

static const char *newline = "\n";
static ngx_str_t uri_root = ngx_string("/");

static ngx_int_t
ngx_http_loungeact_handler(ngx_http_request_t *request)
{
    ngx_buf_t *buf, *buf2;
    ngx_chain_t out, out2;
    ngx_http_loungeact_state_t *la_conf;
    
    out.buf = NULL;
    out.next = NULL;
    
    la_conf = ngx_http_get_module_loc_conf(request, ngx_http_loungeact);
    
    if (request->uri.len == uri_root.len && ngx_strncmp(request->uri.data, uri_root.data, uri_root.len) == 0)
    {
        syslog(LOG_NOTICE, "request for /");
        request->headers_out.status = NGX_HTTP_OK;
        request->headers_out.content_length_n = strlen(welcome) + 1;
        request->headers_out.content_type.len = strlen("application/json");
        request->headers_out.content_type.data = (u_char *) "application/json";
        ngx_http_send_header(request);
        
        if (!request->header_only)
        {
            buf = ngx_calloc_buf(request->pool);
            buf2 = ngx_calloc_buf(request->pool);
            if (buf == NULL)
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            if (buf2 == NULL)
            {
                ngx_pfree(request->pool, buf);
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            memset(buf, 0, sizeof(ngx_buf_t));
            buf->start = buf->pos = (u_char *) welcome;
            buf->end = buf->last = (u_char *) (welcome + strlen(welcome));
            buf->memory = 1;
            buf->last_buf = 1;
            buf->last_in_chain = 1;
            out.buf = buf;
            out.next = &out2;
            
            buf2->start = buf2->pos = (u_char *) newline;
            buf2->end = buf2->last = buf2->start + 1;
            buf2->memory = 1;
            buf2->last_buf = 1;
            buf2->last_in_chain = 1;
            out2.buf = buf2;
            out2.next = NULL;
        }
    }
    else
    {
        u_char *slash = ngx_strnstr(request->uri.data + 1, "/", request->uri.len);
        size_t len;
        
        if (slash != NULL)
            len = request->uri.data - slash;
        else
            len = request->uri.len;
        char *dbname = ngx_pcalloc(request->pool, len);
        strncpy(dbname, (const char *) (request->uri.data + 1), len - 1);
        char *docname = NULL;
        if (len <= request->uri.len - 1)
        {
            docname = ngx_pcalloc(request->pool, request->uri.len - len );
            strncpy(docname, (char *) slash + 1, request->uri.len - len - 1);
        }
        
        syslog(LOG_NOTICE, "dbname: %s, docname: %s", dbname, docname);
        
        if (docname == NULL)
        {
            switch (request->method)
            {
                case NGX_HTTP_HEAD:
                case NGX_HTTP_GET:
                {
                    la_db_t *db;
                    la_db_open_result_t result = db_cache_open(&cache, la_conf->la_host, dbname, 0, &db);
                    la_codec_value_t *reply = NULL;
                    syslog(LOG_NOTICE, "HEAD/GET db = %p", db);
                    
                    switch (result)
                    {
                        case LA_DB_OPEN_OK:
                            request->headers_out.status = NGX_HTTP_OK;
                            if (!request->header_only && request->method != NGX_HTTP_HEAD)
                            {
                                la_storage_stat_t st;
                                la_db_stat(db, &st);
                                reply = la_codec_object();
                                la_codec_object_set_new(reply, "db_name", la_codec_string(dbname));
                                la_codec_object_set_new(reply, "update_seq", la_codec_integer((la_codec_int_t) la_db_last_seq(db)));
                                la_codec_object_set_new(reply, "doc_count", la_codec_integer((la_codec_int_t) st.numkeys));
                                la_codec_object_set_new(reply, "disk_size", la_codec_integer((la_codec_int_t) st.size));
                            }
                            break;
                            
                        case LA_DB_OPEN_NOT_FOUND:
                            request->headers_out.status = NGX_HTTP_NOT_FOUND;
                            if (!request->header_only && request->method != NGX_HTTP_HEAD)
                            {
                                reply = la_codec_object();
                                la_codec_object_set_new(reply, "error", la_codec_string("not_found"));
                                la_codec_object_set_new(reply, "reason", la_codec_string("missing"));
                            }
                            break;
                            
                        default:
                            request->headers_out.status = NGX_HTTP_INTERNAL_SERVER_ERROR;
                            if (!request->header_only && request->method != NGX_HTTP_HEAD)
                            {
                                reply = la_codec_object();
                                la_codec_object_set_new(reply, "error", la_codec_string("internal_error"));
                                la_codec_object_set_new(reply, "reason", la_codec_string("failed"));
                            }
                            break;
                    }
                    
                    if (reply != NULL)
                    {
                        char *s = la_codec_dumps(reply, LA_CODEC_COMPACT);
                        char *ss = ngx_pcalloc(request->pool, strlen(s) + 1);
                        strcpy(ss, s);
                        free(s);
                        la_codec_decref(reply);
                        syslog(LOG_NOTICE, "PUT request %s :: %s", dbname, ss);
                        request->headers_out.content_length_n = strlen(ss) + 1;
                        buf = ngx_calloc_buf(request->pool);
                        buf->start = buf->pos = (u_char *) ss;
                        buf->end = buf->last = buf->start + strlen(ss);
                        buf->memory = 1;
                        buf->last_buf = 1;
                        buf->last_in_chain = 1;
                        out.buf = buf;
                        out.next = &out2;
                        
                        buf2 = ngx_calloc_buf(request->pool);
                        buf2->start = buf2->pos = (u_char *) newline;
                        buf2->end = buf2->last = buf2->start + 1;
                        buf2->memory = 1;
                        buf2->last_buf = 1;
                        buf2->last_in_chain = 1;
                        out2.buf = buf2;
                        out2.next = NULL;
                    }
                    ngx_http_send_header(request);
                    return ngx_http_output_filter(request, &out);
                }
                    
                case NGX_HTTP_PUT:
                {
                    // Create the database.
                    la_db_t *db;
                    la_db_open_result_t result = db_cache_open(&cache, la_conf->la_host, dbname, LA_DB_OPEN_FLAG_CREATE|LA_DB_OPEN_FLAG_EXCL, &db);
                    la_codec_value_t *reply = NULL;
                    
                    switch (result)
                    {
                        case LA_DB_OPEN_CREATED:
                        {
                            request->headers_out.status = NGX_HTTP_CREATED;
                            reply = la_codec_object();
                            la_codec_object_set_new(reply, "ok", la_codec_true());
                        }
                            break;
                            
                        case LA_DB_OPEN_EXISTS:
                        {
                            request->headers_out.status = NGX_HTTP_PRECONDITION_FAILED;
                            reply = la_codec_object();
                            la_codec_object_set_new(reply, "error", la_codec_string("file_exists"));
                            la_codec_object_set_new(reply, "reason", la_codec_string("The database could not be created, the file already exists."));
                        }
                            break;
                            
                        default:
                        {
                            request->headers_out.status = NGX_HTTP_INTERNAL_SERVER_ERROR;
                            const char *err = strerror(errno);
                            reply = la_codec_object();
                            la_codec_object_set_new(reply, "error", la_codec_string("internal_error"));
                            la_codec_object_set_new(reply, "reason", la_codec_string(err));
                        }
                            break;
                    }
                    
                    char *s = la_codec_dumps(reply, LA_CODEC_COMPACT);
                    char *ss = ngx_pcalloc(request->pool, strlen(s) + 1);
                    strcpy(ss, s);
                    free(s);
                    la_codec_decref(reply);
                    syslog(LOG_NOTICE, "PUT request %s :: %s", dbname, ss);
                    request->headers_out.content_length_n = strlen(ss) + 1;
                    buf = ngx_calloc_buf(request->pool);
                    buf->start = buf->pos = (u_char *) ss;
                    buf->end = buf->last = buf->start + strlen(ss);
                    buf->memory = 1;
                    buf->last_buf = 1;
                    buf->last_in_chain = 1;
                    out.buf = buf;
                    out.next = &out2;
                    
                    buf2 = ngx_calloc_buf(request->pool);
                    buf2->start = buf2->pos = (u_char *) newline;
                    buf2->end = buf2->last = buf2->start + 1;
                    buf2->memory = 1;
                    buf2->last_buf = 1;
                    buf2->last_in_chain = 1;
                    out2.buf = buf2;
                    out2.next = NULL;
                    ngx_http_send_header(request);
                }
                return ngx_http_output_filter(request, &out);
                    
                case NGX_HTTP_DELETE:
                {
                    la_db_t *db;
                    la_db_open_result_t result = db_cache_open(&cache, la_conf->la_host, dbname, 0, &db);
                    la_codec_value_t *reply = NULL;

                    switch (result)
                    {
                        case LA_DB_OPEN_OK:
                        {
                            if (la_db_delete_db(db) == 0)
                            {
                                request->headers_out.status = NGX_HTTP_OK;
                                reply = la_codec_object();
                                la_codec_object_set_new(reply, "ok", la_codec_true());
                            }
                            else
                            {
                                request->headers_out.status = NGX_HTTP_CONFLICT;
                                reply = la_codec_object();
                                la_codec_object_set_new(reply, "error", la_codec_string("conflict"));
                                la_codec_object_set_new(reply, "reason", la_codec_string("Database is busy."));
                            }
                        }
                            break;
                            
                        case LA_DB_OPEN_NOT_FOUND:
                        {
                            request->headers_out.status = NGX_HTTP_NOT_FOUND;
                            reply = la_codec_object();
                            la_codec_object_set_new(reply, "error", la_codec_string("not_found"));
                            la_codec_object_set_new(reply, "reason", la_codec_string("missing"));
                        }
                            break;
                            
                        default:
                        {
                            const char *err = strerror(errno);
                            request->headers_out.status = NGX_HTTP_INTERNAL_SERVER_ERROR;
                            reply = la_codec_object();
                            la_codec_object_set_new(reply, "error", la_codec_string("internal_error"));
                            la_codec_object_set_new(reply, "reason", la_codec_string(err));
                            break;
                        }
                    }
                    
                    char *s = la_codec_dumps(reply, LA_CODEC_COMPACT);
                    char *ss = ngx_pcalloc(request->pool, strlen(s) + 1);
                    strcpy(ss, s);
                    free(s);
                    la_codec_decref(reply);
                    buf = ngx_calloc_buf(request->pool);
                    buf->start = buf->pos = (u_char *) ss;
                    buf->end = buf->last = (u_char *) ss + strlen(ss);
                    buf->memory = 1;
                    buf->last_buf = 1;
                    buf->last_in_chain = 1;
                    out.buf = buf;
                    out.next = &out2;
                    
                    buf2 = ngx_calloc_buf(request->pool);
                    buf2->start = buf2->pos = (u_char *) newline;
                    buf2->end = buf2->last = buf2->start + 1;
                    buf2->memory = 1;
                    buf2->last_buf = 1;
                    buf2->last_in_chain = 1;
                    out2.buf = buf2;
                    out2.next = NULL;
                    ngx_http_send_header(request);
                    return ngx_http_output_filter(request, &out);
                }

                default:
                    return NGX_HTTP_NOT_ALLOWED;
            }
        }
    }
    
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
}
