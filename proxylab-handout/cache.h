#ifndef __CACHE_H__
#define __CACHE_H__

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_CACHE 10

typedef struct
{
    char obj[MAX_OBJECT_SIZE];
    char url[MAXLINE];
    int LRU;
    int isEmpty;

    int read_cnt; // number of reader
    sem_t w;// write lock
    sem_t mutex;
} block;

typedef struct
{
    block data[MAX_CACHE];
    int num;
} Cache;

void init_Cache(Cache *cache);
int get_Cache(Cache *cache,char *url);
int get_Index(Cache *cache);
void update_LRU(Cache *cache,int index);
void write_Cache(Cache *cache,char *url,char *buf);

#endif