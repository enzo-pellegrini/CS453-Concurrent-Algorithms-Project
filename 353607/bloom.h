#ifndef BLOOM_H
#define BLOOM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct bf_s {
    uint64_t bitset;
    int align;
} bf_t;
void bf_init(bf_t *bf, int align);
bool bf_in(bf_t bf, const void* x);
void bf_add(bf_t* bf, const void* x);

#endif