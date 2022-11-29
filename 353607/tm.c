/**
 * @file   tm.c
 * @author [...]
 *
 * @section LICENSE
 *
 * [...]
 *
 * @section DESCRIPTION
 *
 * Implementation of your own transaction manager.
 * You can completely rewrite this file (and create more files) as you wish.
 * Only the interface (i.e. exported symbols and semantic) must be preserved.
 **/

// Requested features
#include <assert.h>
#include <malloc/_malloc.h>
#include <sys/_pthread/_pthread_mutex_t.h>
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#ifdef __STDC_NO_ATOMICS__
#error Current C11 compiler does not support atomic operations
#endif

// External headers
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Internal headers
#include <tm.h>

#include "macros.h"
#include "versioned_lock.h"

#define VA_SIZE 65536

/* *********************************** *
 * STRUCTURES FOR TRANSACTIONAL MEMORY *
 * *********************************** */

typedef struct segment_s {
    size_t size;   // Is this really needed? yes
    int num_words; // might be redundant
    versioned_lock_t *locks;
    void *data;
} *segment_t;

typedef struct empty_spot_s {
    struct empty_spot_s *next;
    int index;
} *empty_spot_t;

typedef struct mb_s {
    void* data;
    size_t pos;
    size_t size;
} *mb_t;

typedef struct shared_s {
    size_t align;

    pthread_rwlock_t cleanup_lock;

    // virtual memory array
    pthread_mutex_t virtual_memory_lock; // Taken to write, enough if the va is big enough

    empty_spot_t empty_spots;

    // virtual address array, to get segment from virtual address
    segment_t *va_arr;
    int va_n;

    // global version flag
    atomic_int global_version;

    // to_free buffer
    pthread_mutex_t to_free_lock;
    int* to_free;
    int to_free_n;
    int to_free_sz;

    // source of all memory
    mb_t* memory_blocks;
    int memory_blocks_n;
} *tm_t;

/* ************************** *
 * STRUCTURES FOR TRANSACTION *
 * ************************** */

typedef versioned_lock_t* rs_item_t;

typedef struct ws_item_s {
    void *addr;  // virtual address
    void *value; // content written
    void *raw_addr;
    versioned_lock_t *versioned_lock;
} ws_item_t;

int ws_item_cmp(const void *a, const void *b) {
    return ((ws_item_t *) a)->addr - ((ws_item_t *) b)->addr;
}

typedef struct tx_s {
    int rv;
    bool is_ro;
    ws_item_t *ws;
    int ws_sz;
    int ws_n;
    rs_item_t *rs;
    int rs_sz;
    int rs_n;
    int *to_free;
    int to_free_sz;
    int *allocated; // to free if allocating transaction fails
    int allocated_sz;
} *transaction_t;

bool ro_transaction_read(tm_t tm, transaction_t transaction, void const *source, size_t size, void *target);

// Segment function signatures
inline int allocate_segment(tm_t tm);

inline int segment_init(segment_t *s, size_t size, size_t align, void* data);

inline segment_t segment_at(segment_t ****virtual_memory, int idx);

inline segment_t *segment_at_p(segment_t ****virtual_memory, int idx);

// Cleanup functions, do frees
void segment_cleanup(segment_t segment) {
    free(segment->data);
    free(segment->locks);
    free(segment);
}

void tm_cleanup(tm_t tm) {
    for (int i = 0; i < tm->va_n; i++) {
        if (tm->va_arr[i] != NULL)
            segment_cleanup(tm->va_arr[i]);
    }
    while (tm->empty_spots != NULL) {
        empty_spot_t tmp = tm->empty_spots;
        tm->empty_spots = tm->empty_spots->next;
        free(tmp);
    }
    free(tm->va_arr);
    pthread_rwlock_destroy(&tm->cleanup_lock);
    pthread_mutex_destroy(&tm->virtual_memory_lock);
    free(tm);
}

// Massive block functions

#define DEFAULT_BLOCK_SIZE 103600

void mb_init(mb_t* mbp, size_t align) {
    *mbp = malloc(sizeof(struct mb_s));
    mb_t mb = *mbp;

    mb->size = DEFAULT_BLOCK_SIZE;
    mb->pos = 0;

    int err = posix_memalign(&mb->data, align, DEFAULT_BLOCK_SIZE);
    if (err != 0) {
        // printf("size: %d, align: %zu\n", DEFAULT_BLOCK_SIZE, align);
        perror("I can't allocate mb");
        exit(1);
    }
    memset(mb->data, 0, mb->size);
}

void* mb_get_chunk(mb_t mb, size_t size) {
    if (mb->size - mb->pos < size) {
        // There's not enough space left in this chunk
        return NULL;
    }
    void* start = mb->data + mb->pos;
    mb->pos += size;
    return start;
}

void transaction_cleanup(tm_t tm, transaction_t transaction, bool failed) {
    if (failed) {
        pthread_rwlock_unlock(&tm->cleanup_lock);
    }
    if (!transaction->is_ro) {
        free(transaction->ws);
        free(transaction->rs);
        free(transaction->to_free);
        free(transaction->allocated);
    }
    free(transaction);
}

/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
 **/
shared_t tm_create(size_t size, size_t align) {
    tm_t tm = malloc(sizeof(struct shared_s));

    if (tm == NULL) {
        return invalid_shared;
    }

    tm->align = align;

    atomic_store(&tm->global_version, 0);

    unlikely(pthread_rwlock_init(&tm->cleanup_lock, NULL));
    unlikely(pthread_mutex_init(&tm->virtual_memory_lock, NULL));
    unlikely(pthread_mutex_init(&tm->to_free_lock, NULL));

    // memory blocks
    tm->memory_blocks_n = 1;
    tm->memory_blocks = malloc(tm->memory_blocks_n * sizeof(mb_t));
    mb_init(&tm->memory_blocks[0], align);

    // virtual memory
    tm->va_arr = malloc(VA_SIZE * sizeof(segment_t));
    tm->va_n = 0;
    tm->empty_spots = NULL;

    // free batching
    tm->to_free_n = 0;
    tm->to_free_sz = 128;
    tm->to_free = malloc(tm->to_free_sz*sizeof(int));

    void* first_seg_data = mb_get_chunk(tm->memory_blocks[0], size);
    int allocation_err = segment_init(&tm->va_arr[0], size, align, first_seg_data);
    if (allocation_err != 0) {
        tm_cleanup(tm);
        return invalid_shared;
    }

    tm->va_n++;

    return tm;
}


/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
 **/
void tm_destroy(shared_t shared) {
    // TODO: implement freeing
    tm_t tm = (tm_t) shared;

    tm_cleanup(tm);
}

/** [thread-safe] Return the start address of the first allocated segment in the shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
 **/
void *tm_start(shared_t unused(shared)) { return va_from_index(0, 0); }

/** [thread-safe] Return the size (in bytes) of the first allocated segment
of
 *the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
 **/
size_t tm_size(shared_t unused(shared)) {
    tm_t tm = (tm_t) shared;
    return tm->va_arr[0]->size;
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on
the
 *given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
 **/
size_t tm_align(shared_t shared) {
    tm_t tm = shared;
    return tm->align;
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
 **/
tx_t tm_begin(shared_t unused(shared), bool is_ro) {
    tm_t tm = (tm_t) shared;

   pthread_rwlock_rdlock(&tm->cleanup_lock);

    transaction_t t = malloc(sizeof(struct tx_s));
    if (t == NULL) {
        return invalid_tx;
    }
    t->rv = atomic_load(&tm->global_version);
    t->is_ro = is_ro;
    // printf("Started transaction with rv: %d\n", t->rv);
    if (is_ro) {
        return (tx_t) t; // I didn't actually need as much space as I allocated
    }

    // Allocate read-set
    t->rs_sz = 8;
    t->rs_n = 0;
    t->rs = malloc(t->rs_sz * sizeof(rs_item_t));
    if (t->rs == NULL) {
        free(t);
        return invalid_tx;
    }

    // Allocate write-set
    t->ws_sz = 8;
    t->ws_n = 0;
    t->ws = malloc(t->ws_sz * sizeof(struct ws_item_s));
    if (t->ws == NULL) {
        free(t->rs);
        free(t);
        return invalid_tx;
    }

    // Init to-free array
    t->to_free_sz = 0;
    t->to_free = NULL;

    // Init allocated array
    t->allocated_sz = 0;
    t->allocated = NULL;

    return (tx_t) t;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
 **/
bool tm_end(shared_t unused(shared), tx_t unused(tx)) {
    tm_t tm = (tm_t) shared;
    transaction_t t = (transaction_t) tx;

    if (t->is_ro) {
        free(t);
        return true;
    }

    // sort the write-set
    qsort(t->ws, t->ws_n, sizeof(ws_item_t), ws_item_cmp);

    // try to lock each item in the write-set
    for (int i = 0; i < t->ws_n; i++) {
        ws_item_t item = t->ws[i];
        // take flag and check version number
        bool success_locking = vl_try_lock(item.versioned_lock, t->rv);
        if (!success_locking) {
            // abort
            for (int j = 0; j < i; j++) {
                vl_unlock(t->ws[j].versioned_lock);
            }

            transaction_cleanup(tm, t, true);

            return false;
        }
    }

    // fetch and increment global version
    int wv = atomic_fetch_add(&tm->global_version, 1) + 1;

    if (t->rv + 1 != wv) {
        // check version number and if it is locked for each item in read-set,
        for (int i = 0; i < t->rs_n; i++) {
            if (t->rs[i] == NULL) continue;
            int version_read = vl_read_version(t->rs[i]);
            if (version_read == -1 || version_read > t->rv) {
                // abort, unlock all locks
                for (int j = 0; j < t->ws_n; j++) {
                    vl_unlock(t->ws[j].versioned_lock);
                }

                transaction_cleanup(tm, t, true);

                return false;
            }
        }
    }


    // for each item in write set, write to memory, set version number to wv and
    // unlock
    for (int i = 0; i < t->ws_n; i++) {
        ws_item_t item = t->ws[i];
        memcpy(item.raw_addr, item.value, tm->align);
        vl_unlock_update(item.versioned_lock, wv);
    }


    pthread_rwlock_unlock(&tm->cleanup_lock);

    if (t->to_free_sz > 0) {
        pthread_mutex_lock(&tm->to_free_lock);

        for (int i=0; i < t->to_free_sz; i++) {
            int spot = t->to_free[i];
        //     segment_cleanup(tm->va_arr[spot]);
        //     empty_spot_t tmp = malloc(sizeof(struct empty_spot_s));
        //     tmp->index = spot;
        //     tmp->next = tm->empty_spots;
        //     tm->empty_spots = tmp;

            // printf("Dealloced segment %d\n", spot);
            if (tm->to_free_n + 1 >= tm->to_free_sz) {
                tm->to_free_sz = 2 * tm->to_free_sz;
                tm->to_free = realloc(tm->to_free, tm->to_free_sz * sizeof(int));
            }
            tm->to_free[tm->to_free_n++] = spot;
        }

        // pthread_rwlock_unlock(&tm->cleanup_lock);
        pthread_mutex_unlock(&tm->to_free_lock);
    }

    // printf("commited transaction with %d reads and %d writes\n", t->rs_n, t->ws_n);

    transaction_cleanup(tm, t, false);
    return true;
}

/** [thread-safe] Read operation in the given transaction, source in the
shared
 *region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of
 the *alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
 **/
bool tm_read(shared_t unused(shared), tx_t unused(tx), void const *source, size_t size, void *target) {
    tm_t tm = shared;
    transaction_t transaction = (void *) tx;

    if (transaction->is_ro) {
        return ro_transaction_read(tm, transaction, source, size, target);
    }

    int align = tm->align;
    segment_t s = tm->va_arr[index_from_va(source)];

    assert(s!=NULL);

    int num_words = size / align;
    int offset = offset_from_va(source);
    int start_word = offset / align;

    for (int i = 0; i < num_words; i++) {
        bool found = false;

        // Check if word is present in the write set
        for (int j = 0; j < transaction->ws_n; j++) {
            if (transaction->ws[j].addr == source + i * align) {
                memcpy(target, transaction->ws[j].value, align);
                found = true;
                break;
            }
        }
        if (found) {
            continue;
        }

        // Read word the same way as for read-only transactions
        versioned_lock_t *version = &s->locks[start_word + i];

        // Check if word is present in the read set
        bool found_in_readset = false;
        for (int j = 0; j < transaction->rs_n; j++) {
            if (transaction->rs[j] == version) {
                found_in_readset = true;
                break;
            }
        }

        int version_read = vl_read_version(version);
        if (version_read == -1 || version_read > transaction->rv) {
            // abort
            transaction_cleanup(tm, transaction, true);

//            printf("aborting transaction because of version number\n");
            return false;
        }
        memcpy(target + i * align, s->data + (start_word + i) * align, align);
        if (vl_read_version(version) != version_read) {
            // version number changed, abort
            transaction_cleanup(tm, transaction, true);
            return false;
        }

        if (found_in_readset) {
            return true;
        }
        // Add word to read set
        if (transaction->rs_n == transaction->rs_sz) {
            transaction->rs_sz *= 2;
            transaction->rs = realloc(transaction->rs, transaction->rs_sz * sizeof(rs_item_t));
            if (transaction->rs == NULL) {
                transaction_cleanup(tm, transaction, true);
                return false;
            }
        }
        transaction->rs[transaction->rs_n++] = (rs_item_t) {version};
    }

    return true;
}

bool ro_transaction_read(tm_t tm, transaction_t transaction, void const *source, size_t size, void *target) {
    segment_t s = tm->va_arr[index_from_va(source)];
    size_t offset = offset_from_va(source);
    int start_idx = offset / tm->align;
    int num_words = size / tm->align;

    for (int i = 0; i < num_words; i++) {
        memcpy(target + i * tm->align, s->data + offset + i * tm->align, tm->align);
        int versionRead = vl_read_version(&s->locks[start_idx + i]);
        if (versionRead == -1 || versionRead > transaction->rv) {
            // word locked, abort
            // printf("ro_read failing, va: %p, read %d\n", source, versionRead);

            transaction_cleanup(tm, transaction, true);
            return false;
        }
    }

    return true;
}

/** [thread-safe] Write operation in the given transaction, source in a
private
 *region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of
 the *alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
 **/
bool tm_write(shared_t shared, tx_t tx, void const *source, size_t size, void *target) {
    tm_t tm = shared;
    transaction_t transaction = (transaction_t) tx;

//    printf("tm_write: %p %p %d", source, target, size);

    unlikely(transaction->is_ro);

    segment_t s = tm->va_arr[index_from_va(target)];
    int idx_start = offset_from_va(target) / tm->align;
    int num_words = size / tm->align;

    for (int i = 0; i < num_words; i++) {
        //    int version = atomic_load(&s->locks[idx_start + i].version);
        versioned_lock_t *version_lock = &s->locks[idx_start + i];
        int version_read = vl_read_version(version_lock);
        if (version_read == -1 || version_read > transaction->rv) {
            // word modified or soon to be modified
            transaction_cleanup(tm, transaction, true);
            return false;
        }

        // Check if word already in write set
        bool found = false;
        for (int j = 0; j < transaction->ws_n; j++) {
            if (transaction->ws[j].addr == target + i * tm->align) {
                found = true;
                memcpy(transaction->ws[j].value, source + i * tm->align, tm->align);
                break;
            }
        }

        if (found) {
            continue;
        }

        // Add word to write set
        if (transaction->ws_n == transaction->ws_sz) {
            transaction->ws_sz *= 2;
            transaction->ws = realloc(transaction->ws, transaction->ws_sz * sizeof(ws_item_t));
            if (transaction->ws == NULL) {
                transaction_cleanup(tm, transaction, true);
//                printf("aborting transaction because of malloc\n");
                return false;
            }
        }
        void *tmp = malloc(tm->align);
        if (tmp == NULL) {
            transaction_cleanup(tm, transaction, true);
            return false;
        }
        memcpy(tmp, source + i * tm->align, tm->align);
        void *raw_address = s->data + (idx_start + i) * tm->align;
        transaction->ws[transaction->ws_n++] = (ws_item_t) {target + i * tm->align, tmp, raw_address, version_lock};

        // remove word from read-set
        for (int i=0; i<transaction->rs_n; i++) {
            if (transaction->rs[i] == version_lock) {
                transaction->rs[i] = NULL;
                break;
            }
        }
    }

    return true;
}

/** [thread-safe] Memory allocation in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param size   Allocation requested size (in bytes), must be a positive multiple of the alignment
 * @param target Pointer in private memory receiving the address of the first byte of the newly allocated, aligned segment
 * @return Whether the whole transaction can continue (success/nomem), or not
 *(abort_alloc)
 **/
alloc_t tm_alloc(shared_t shared, tx_t unused(tx), size_t unused(size), void **unused(target)) {
    tm_t tm = shared;
    transaction_t transaction = (transaction_t) tx;

    // printf("tm_alloc called\n");

    pthread_mutex_lock(&tm->virtual_memory_lock);

    int spot;
    if (tm->empty_spots != NULL) {
        spot = tm->empty_spots->index;
        empty_spot_t tmp = tm->empty_spots;
        tm->empty_spots = tm->empty_spots->next;
        free(tmp);
    } else {
        spot = tm->va_n++;
    }

    // // TODO: Handle (unlikely) size > DEFAULT_BLOCK_SIZE
    void* seg_data;
    for (int i_mb = 0; seg_data == NULL; i_mb++) {
        if (i_mb >= tm->memory_blocks_n) {
            tm->memory_blocks_n++;
            mb_init(&tm->memory_blocks[i_mb], tm->align);
        }

        seg_data = mb_get_chunk(tm->memory_blocks[i_mb], size);
    }

    pthread_mutex_unlock(&tm->virtual_memory_lock);

    segment_t *seg_ptr = &tm->va_arr[spot];
    int mem_err = segment_init(seg_ptr, size, tm->align, seg_data);
    if (mem_err != 0) {
        printf("Chec\n");
        empty_spot_t tmp = malloc(sizeof(struct empty_spot_s));
        unlikely(tmp == NULL);
        tmp->index = spot;
        tmp->next = tm->empty_spots;
        tm->empty_spots = tmp;

        return nomem_alloc;
    }


    *target = va_from_index(spot, 0);
    // printf("Allocated semgment %d, va: %p\n", spot, *target);

    // // save allocated segment in transaction
    // transaction->allocated = realloc(transaction->allocated, (++transaction->allocated_sz) * sizeof(int));
    // transaction->allocated[transaction->allocated_sz] = spot;

    return success_alloc;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated
 segment *to deallocate
 * @return Whether the whole transaction can continue
 **/
bool tm_free(shared_t unused(shared), tx_t tx, void *target) {
    transaction_t transaction = (transaction_t) tx;

    int spot = index_from_va(target);
    // printf("tm_free called with spot=%d\n", spot);

    transaction->to_free = realloc(transaction->to_free, (++transaction->to_free_sz) * sizeof(int));
    transaction->to_free[transaction->to_free_sz-1] = spot;

    return true;
}

/* ************************************ *
 * IMPLEMENTATION OF INTERNAL FUNCTIONS *
 * ************************************ */

int segment_init(segment_t *sp, size_t size, size_t align, void* data) {
    *sp = malloc(sizeof(struct segment_s));
    segment_t s = *sp;
    s->data = data;

    // printf("allocated segment of size: %zu\n", size);

    s->num_words = size / align;
    s->locks = malloc(s->num_words * sizeof(versioned_lock_t));
    if (s->locks == NULL) {
        // free(s->data);

        return -1;
    }
    for (int i = 0; i < s->num_words; i++) {
        vl_init(&s->locks[i]);
    }

    s->size = size;

    return 0;
}
