#include "cache.h"

void init_Cache(Cache *cache)
{
    cache->num = 0;
    int i;
    for(i = 0;i < MAX_CACHE;i++) {
        cache->data[i].LRU = 0;
        cache->data[i].isEmpty = 1;
        // w and mutex lock initialize to 1
        Sem_init(&(cache->data[i].w),0,1);
        Sem_init(&(cache->data[i].mutex),0,1);
        cache->data[i].read_cnt = 0;
    }
}

int get_Cache(Cache *cache,char *url)
{
    int i;
    for(i = 0;i < MAX_CACHE;i++) {
        P(&(cache->data[i].mutex));
        cache->data[i].read_cnt ++;
        if(cache->data[i].read_cnt == 1) {
            P(&(cache->data[i].w));
        }
        V(&(cache->data[i].mutex));

        if((cache->data[i].isEmpty == 0) && !strcmp(url,cache->data[i].url))
            break;
        
        P(&(cache->data[i].mutex));
        cache->data[i].read_cnt --;
        if(cache->data[i].read_cnt == 0) {
            V(&(cache->data[i].w));
        }
        V(&(cache->data[i].mutex));
    }
    if(i >= MAX_CACHE) {
        return -1;
    }
    return i;
}

int get_Index(Cache *cache)
{
    int min = __INT_MAX__;
    int min_index = 0;
    int i;
    for(i = 0;i < MAX_CACHE;i++) {
        P(&(cache->data[i].mutex));
        cache->data[i].read_cnt ++;
        if(cache->data[i].read_cnt == 1) {
            P(&(cache->data[i].w));
        }
        V(&(cache->data[i].mutex));

        if(cache->data[i].isEmpty == 1) {
            min_index = i;
            P(&(cache->data[i].mutex));
            cache->data[i].read_cnt --;
            if(cache->data[i].read_cnt == 0) {
                V(&(cache->data[i].w));
            }
            V(&(cache->data[i].mutex));
            break;
        }
        if(cache->data[i].LRU < min) {
            min_index = i;
            P(&(cache->data[i].mutex));
            cache->data[i].read_cnt --;
            if(cache->data[i].read_cnt == 0) {
                V(&(cache->data[i].w));
            }
            V(&(cache->data[i].mutex));
            continue;
        }

        P(&(cache->data[i].mutex));
        cache->data[i].read_cnt --;
        if(cache->data[i].read_cnt == 0) {
            V(&(cache->data[i].w));
        }
        V(&(cache->data[i].mutex));
    }

    return min_index;
}

void update_LRU(Cache *cache,int index)
{
    for(int i = 0;i < MAX_CACHE;i++) {
        if(cache->data[i].isEmpty == 0 && i != index) {
            P(&(cache->data[i].w));
            cache->data[i].LRU --;
            V(&(cache->data[i].w));
        }
    }
}

void write_Cache(Cache *cache,char *url,char *buf)
{
    int i = get_Index(cache);
    
    P(&(cache->data[i].w));

    strcpy(cache->data[i].obj,buf);
    strcpy(cache->data[i].url,url);
    cache->data[i].isEmpty = 0;
    cache->data[i].LRU = __INT_MAX__;
    update_LRU(cache,i);

    V(&(cache->data[i].w));
}