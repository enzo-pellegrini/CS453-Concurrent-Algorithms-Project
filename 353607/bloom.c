#include "bloom.h"
#include <stdint.h>

#define BITSET_SZ 64
#define bit_get(data, pos) (data & (1 << pos))
#define bit_set(data, pos) (data | (1 << pos))
#define NUM_HASHES 5

void bf_init(bf_t *bf, int align) {
    bf->bitset = 0;
    for (int i = 0; i<16; i++)
        if ((1 << i) >= align) {
            bf->align = i;
            break;
        }
}

int hash(uintptr_t data, int align, int seed) {
    data = (data >> align) + seed;
    return (data * 1607) >> 5;
}

bool bf_in(bf_t bf, const void *x) {
    for (int i = 0; i < NUM_HASHES; i++) {
        int pos = hash((uintptr_t)x, bf.align, i * 31) % BITSET_SZ;
        if (!bit_get(bf.bitset, pos)) {
            return false;
        }
    }

    return true;
}

void bf_add(bf_t *bf, const void *x) {
    for (int i = 0; i < NUM_HASHES; i++) {
        int pos = hash((uintptr_t)x, bf->align, i * 31) % BITSET_SZ;
        bf->bitset = bit_set(bf->bitset, pos);
    }
}