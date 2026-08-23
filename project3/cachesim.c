#include "cachesim.h"

cache_t *cache_create(int s, int E, int delay, cache_t *nextlevel) {
    cache_t *cache = malloc(sizeof(cache_t)); // allocate cache

    cache->cache_s = s;
    cache->cache_E = E;
    cache->cache_delay = delay;
    cache->cache_next = nextlevel;

    cache->cache_sets = malloc(sizeof(cacheset_t) * (1 << s)); // allocate sets
    for (int i = 0; i < (1 << s); i++) { // for each set
        cache->cache_sets[i].lines = malloc(sizeof(cacheline_t) * E); // allocate lines
        cache->cache_sets[i].useinfo = malloc(sizeof(int) * E); // allocate lru array

        int* useinf = cache->cache_sets[i].useinfo; // interpret useinfo as int array
        for (int j = 0; j < E; j++) { // for each line
            cache->cache_sets[i].lines[j].block = malloc(1 << LOGBSIZE); // allocate block
            useinf[j] = j; // initialize index in useinfo
        }
    }
    return cache;
}

int cache_access(cache_t *c, addr_t addr, void *value, int size, int iswrite) {
    int byte_offset = addr & ((1 << LOGBSIZE) - 1); // last LOGBSIZE bits are byte offset
    int set_index = (addr >> LOGBSIZE) & ((1 << c->cache_s) - 1); // next c->cache_s bits are set index
    addr_t tag = addr >> (LOGBSIZE + c->cache_s); // remaining bits are tag

    cacheset_t* set = c->cache_sets + set_index;
    for (int i = 0; i < c->cache_E; i++) { // iterate through set
        cacheline_t* line = set->lines + i;
        if (line->valid && line->tag == tag) { // hit
            if (iswrite) {
                memcpy(line->block + byte_offset, value, size);
                line->dirty = 1;
            }
            else {
                memcpy(value, line->block + byte_offset, size);
            }
            
            int *lru = set->useinfo; // interpret useinfo as int array
            int j;
            for (j = 0; j < c->cache_E; j++) {
                if (lru[j] == i) { // find index of hit line
                    break;
                }
            }

            for (j += 1; j < c->cache_E; j++) {
                lru[j-1] = lru[j]; // shift left
            }

            lru[c->cache_E - 1] = i; // put index as mru
            return c->cache_delay;
        }
    }

    int* useinf = set->useinfo;
    int total_delay = c->cache_delay;
    cacheline_t* victim = set->lines + useinf[0];
    if (victim->valid && victim->dirty) { // write back victim if dirty
        if (c->cache_next == NULL) {
            total_delay += mem_access((victim->tag << (LOGBSIZE + c->cache_s)) | (set_index << LOGBSIZE), victim->block, 1 << LOGBSIZE, 1); // address without block offset is tag then set_index
        }
        else {
            total_delay += cache_access(c->cache_next, (victim->tag << (LOGBSIZE + c->cache_s)) | (set_index << LOGBSIZE), victim->block, 1 << LOGBSIZE, 1);
        }
    }

    int lru = useinf[0]; // index of victim
    for (int i = 1; i < c->cache_E; i++) { // shift left
        useinf[i-1] = useinf[i];
    }
    useinf[c->cache_E - 1] = lru; // put index as mru

    victim->tag = tag;
    victim->valid = 1;
    victim->dirty = 0;

    if (c->cache_next == NULL) {
        total_delay += mem_access(addr & ~((1 << LOGBSIZE) - 1), victim->block, 1 << LOGBSIZE, 0);
    } else {
        total_delay += cache_access(c->cache_next, addr & ~((1 << LOGBSIZE) - 1), victim->block, 1 << LOGBSIZE, 0); // read from lower level
    }

    if (iswrite) {
        memcpy(victim->block + byte_offset, value, size);
        victim->dirty = 1;
    }
    else {
        memcpy(value, victim->block + byte_offset, size);
    }
    return total_delay;
}

void cache_flush(cache_t *c) {
    for (int i = 0; i < (1 << c->cache_s); i++) { // for each set
        cacheset_t* set = &c->cache_sets[i];
        for (int j = 0; j < c->cache_E; j++) { // for each line
            cacheline_t* line = &set->lines[j];
            if (line->dirty) { // write back if dirty
                if (c->cache_next == NULL) {
                    mem_access((line->tag << (LOGBSIZE + c->cache_s)) | (i << LOGBSIZE), line->block, 1 << LOGBSIZE, 1);
                } else {
                    cache_access(c->cache_next, (line->tag << (LOGBSIZE + c->cache_s)) | (i << LOGBSIZE), line->block, 1 << LOGBSIZE, 1);
                }
                line->dirty = 0;
            }
            line->valid = 0;
        }
    }
}