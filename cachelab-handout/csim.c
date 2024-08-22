#include "cachelab.h"
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

typedef struct cache_line
{
    int valid;
    unsigned int tag;
    int time_stamp;
} Cache_line;
typedef struct cache_set
{
    int S;
    int E;
    int B;
    Cache_line **line;
} Cache;

int hit_count = 0,miss_count = 0,eviction_count = 0;
int verbose = 0; // -v argument
char t[1000]; //trace filename

// cache emulator
Cache *cache = NULL;

static void Init_Cache(int s,int E,int b);
static void get_trace(int s,int E,int b);
static void free_Cache();
static void print_help();
static int get_index(unsigned int tag,unsigned int set_num);
static int is_full(unsigned int set_num);
static void update_info(unsigned int tag,unsigned int set_num);
static int find_LRU(unsigned int set_num);
static void update(int i,unsigned int set_num,unsigned int tag);

int main(int argc,char **argv)
{
    char opt;
    int s,E,b;

    while((opt = getopt(argc,argv,"hvs:E:b:t:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            print_help();
            exit(0);
        case 'v':
            verbose = 1;
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            strcpy(t,optarg);
            break;
        default:
            print_help();
            exit(-1);
        }
    }
    Init_Cache(s,E,b);
    get_trace(s,E,b);
    free_Cache();
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}

static void Init_Cache(int s,int E,int b)
{
    int S = 1 << s;
    int B = 1 << b;
    cache = (Cache *)malloc(sizeof(Cache));
    cache->S = S;
    cache->E = E;
    cache->B = B;
    cache->line = (Cache_line **)malloc(sizeof(Cache_line *) * S);
    int i,j;
    for(i = 0;i < S;i++) {
        cache->line[i] = (Cache_line *)malloc(sizeof(Cache_line) * E);
        for(j = 0;j < E;j++) {
            cache->line[i][j].valid = 0;
            cache->line[i][j].tag = -1;
            cache->line[i][j].time_stamp = 0;
        }
    }
}

static void get_trace(int s,int E,int b)
{
    FILE *trace_file;
    trace_file = fopen(t,"r");
    if(trace_file == NULL) {
        printf("No such file: %s\n",t);
        exit(-1);
    }
    char identifier;
    unsigned int address;
    size_t size;

    while(fscanf(trace_file," %c %x,%zu",&identifier,&address,&size) > 0)
    {
        unsigned int tag = address >> (s + b);
        unsigned int set_num = (address >> b) & ((unsigned int)(-1) >> (32 - s));
        switch (identifier)
        {
            case 'M':
                update_info(tag,set_num);
                /*
                    modify cache 
                */
                update_info(tag,set_num);
                break;
            case 'L':
                update_info(tag,set_num);
                break;
            case 'S':
                update_info(tag,set_num);
                break;
        }
    }
    fclose(trace_file);
}

static void update_info(unsigned int tag,unsigned int set_num)
{
    int index = get_index(tag,set_num);
    if(index == -1) {
        miss_count ++;
        if(verbose) {
            printf("miss ");
        }
        int pos = is_full(set_num);
        if(pos == -1) {
            eviction_count ++;
            if(verbose) printf("eviction");
            pos = find_LRU(set_num);
        }
        update(pos,set_num,tag);
    }
    else {
        hit_count ++;
        if(verbose) {
            printf("hit");
        }
        update(index,set_num,tag);
    }
}
static int get_index(unsigned int tag,unsigned int set_num)
{
    int i;
    int set_size = cache->E;
    for(i = 0;i < set_size;i++) {
        if(cache->line[set_num][i].valid && cache->line[set_num][i].tag == tag) {
            return i;
        }
    }
    return -1;
}
static int is_full(unsigned int set_num)
{
    int i;
    int set_size = cache->E;
    for(i = 0;i < set_size;i++) {
        if(cache->line[set_num][i].valid == 0) {
            return i;
        }
    }
    return -1;
}
static int find_LRU(unsigned int set_num)
{
    int max_index = 0,max_stamp = 0,set_size = cache->E;
    int i;
    for(i = 0;i < set_size;i++) {
        if(cache->line[set_num][i].time_stamp > max_stamp) {
            max_stamp = cache->line[set_num][i].time_stamp;
            max_index = i;
        }
    }
    return max_index;
}
static void update(int i,unsigned int set_num,unsigned int tag)
{
    cache->line[set_num][i].valid = 1;
    cache->line[set_num][i].tag = tag;
    int j,set_size = cache->E;
    for(j = 0;j < set_size;j++) {
        if(cache->line[set_num][j].valid == 1) {
            cache->line[set_num][j].time_stamp ++;
        }
    }
    cache->line[set_num][i].time_stamp = 0;
}
static void free_Cache()
{
    int S = cache->S,i;
    for(i = 0;i < S;i++) {
        free(cache->line[i]);
    }
    free(cache->line);
    free(cache);
}

static void print_help()
{
    printf("** A Cache Simulator by Deconx\n");
    printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("-h         Print this help message.\n");
    printf("-v         Optional verbose flag.\n");
    printf("-s <num>   Number of set index bits.\n");
    printf("-E <num>   Number of lines per set.\n");
    printf("-b <num>   Number of block offset bits.\n");
    printf("-t <file>  Trace file.\n\n\n");
    printf("Examples:\n");
    printf("linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}