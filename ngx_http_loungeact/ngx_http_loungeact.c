//
//  ngx_http_loungeact.c
//  LoungeAct
//
//  Created by Casey Marshall on 4/2/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <api/LoungeAct.h>
#include <compress-lz4/compress-lz4.h>

static char *welcome = NULL;

static ngx_int_t ngx_http_loungeact_handler(ngx_http_request_t *request);
static char *ngx_http_loungeact_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
void *ngx_http_loungeact_create_loc_conf(ngx_conf_t *cf);
char *ngx_http_loungeact_merge_loc_conf(ngx_conf_t *cf, void *prev, void *conf);
static ngx_int_t ngx_http_loungeact_preconf(ngx_conf_t *cf);

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
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t
ngx_http_loungeact_preconf(ngx_conf_t *cf)
{
    if (welcome == NULL)
    {
        la_codec_value_t *obj = la_codec_object();
        la_codec_object_set_new(obj, "couchdb", la_codec_string("Welcome"));
        la_codec_object_set_new(obj, "version", la_codec_string("1.1.1"));
        la_codec_object_set_new(obj, "loungeact", la_codec_string("0.0.1"));
        welcome = la_codec_dumps(obj, 0);
        ngx_log_error(NGX_LOG_ERR, cf->log, 0, "welcome inited: %s", welcome);
    }
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

static ngx_str_t uri_root = ngx_string("/");

static ngx_int_t
ngx_http_loungeact_handler(ngx_http_request_t *request)
{
    ngx_buf_t *buf;
    ngx_chain_t out;
    ngx_http_loungeact_state_t *la_conf;
    
    la_conf = ngx_http_get_module_loc_conf(request, ngx_http_loungeact);
    
    if (request->uri.len == uri_root.len && ngx_strncmp(request->uri.data, uri_root.data, uri_root.len) == 0)
    {
        ngx_log_error(NGX_LOG_NOTICE, request->connection->log, 0, "request for /");
        request->headers_out.status = NGX_HTTP_OK;
        request->headers_out.content_length_n = strlen(welcome);
        request->headers_out.content_type.len = strlen("application/json");
        request->headers_out.content_type.data = (u_char *) "application/json";
        ngx_http_send_header(request);
        
        if (!request->header_only)
        {
            buf = ngx_calloc_buf(request->pool);
            if (buf == NULL)
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            memset(buf, 0, sizeof(ngx_buf_t));
            buf->start = buf->pos = (u_char *) welcome;
            buf->end = buf->last = (u_char *) (welcome + strlen(welcome));
            buf->memory = 1;
            buf->last_buf = 1;
            buf->last_in_chain = 1;
            out.buf = buf;
            out.next = NULL;
        }
    }
    else
    {
        u_char *end = ngx_strnstr(request->uri.data + 1, "/", request->uri.len);
        size_t len;
        
        if (end != NULL)
            len = request->uri.data - end;
        else
            len = request->uri.len;
        char *dbname = ngx_pcalloc(request->pool, len);
        strncpy(dbname, (const char *) (request->uri.data + 1), len);
        
        switch (request->method)
        {
            case NGX_HTTP_HEAD:
            {
            }
                break;
                
            case NGX_HTTP_GET:
                break;
        }
        
        return NGX_HTTP_NOT_FOUND;
    }
    
    return ngx_http_output_filter(request, &out);
}
