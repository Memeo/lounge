//
//  main.c
//  test-pull-bdb-json
//
//  Created by Casey Marshall on 3/16/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#include <stdio.h>
#include <ftw.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "../api/LoungeAct.h"
#include "../pull/pull.h"

int cb1(const char *path, const struct stat *ptr, int flag, struct FTW *ftw);
int cb1(const char *path, const struct stat *ptr, int flag, struct FTW *ftw)
{
    printf("cb1 %s %x\n", path, flag);
    if (flag == FTW_F)
    {
        if (unlink(path) != 0)
            fprintf(stderr, "failed to unlink %s: %s\n", path, strerror(errno));
    }
    return 0;
}

int cb2(const char *path, const struct stat *ptr, int flag, struct FTW *ftw);
int cb2(const char *path, const struct stat *ptr, int flag, struct FTW *ftw)
{
    printf("cb2 %s %x\n", path, flag);
    if (flag == FTW_DP)
    {
        if (rmdir(path) != 0)
            fprintf(stderr, "failed to rmdir %s: %s\n", path, strerror(errno));
    }
    return 0;
}

static la_host_t *host = NULL;
static la_db_t *db = NULL;
static la_pull_t *puller = NULL;

void cleanup(void);
void cleanup(void)
{
    printf("cleaning up...\n");
    if (db != NULL)
        la_db_close(db);
    if (host != NULL)
        la_host_close(host);
    if (puller != NULL)
        la_pull_destroy(puller);
}

#define OK() printf("OK\n")
#define FAIL(fmt,args...) do { printf("FAIL" fmt "\n", ##args); return -1; } while (0)
#define FAIL0() FAIL("")

int main(int argc, const char * argv[])
{
    if (nftw("/tmp/apitest", cb1, 10, FTW_DEPTH | FTW_PHYS) == 0)
        nftw("/tmp/apitest", cb2, 10, FTW_DEPTH | FTW_PHYS);
    
    atexit(cleanup);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("opening host... ");
    if ((host = la_host_open("/tmp/apitest")) == NULL)
    {
        FAIL0();
    }
    OK();
    
    printf("opening db... ");
    if ((db = la_db_open(host, "apitest")) == NULL)
    {
        FAIL0();
    }
    OK();
    
    la_pull_params_t params;
    params.dbname = "agentdb-bea301e655f30ce24d9405dad97cf2b0";
    params.filter = NULL;
    params.host = "localhost";
    params.port = 5984;
    params.options = 0;
    params.user = NULL;
    params.password = NULL;
    params.resolver = NULL;
    params.baton = NULL;
    puller = la_pull_create(db, &params);
    printf("pulling changes... ");
    if (la_pull_run(puller) != 0)
        FAIL0();
    la_pull_destroy(puller);
    OK();
    
    la_view_iterator_t *it = la_db_view(db, NULL, NULL, NULL, NULL);
    la_view_iterator_result itresult;
    do
    {
        la_codec_value_t *value = NULL;
        la_codec_error_t error;
        itresult = la_view_iterator_next(it, &value, &error);
        if (itresult == LA_VIEW_ITERATOR_GOT_NEXT)
            printf("got object %s\n", la_codec_dumps(value, 0));
        printf("%d %d\n", itresult, itresult == LA_VIEW_ITERATOR_GOT_NEXT);
    } while (itresult == LA_VIEW_ITERATOR_GOT_NEXT);
    if (itresult != LA_VIEW_ITERATOR_END)
        FAIL0();
    OK();
    
    printf("pulling again... ");
    puller = la_pull_create(db, &params);
    if (la_pull_run(puller) != 0)
        FAIL0();
    la_pull_destroy(puller);
    OK();
    
    return 0;
}

