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
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#ifdef __STDC_NO_ATOMICS__
#error Current C11 compiler does not support atomic operations
#endif

// External headers
#include <stdatomic.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

// Internal headers
#include <tm.h>

#include "macros.h"

/* *********************************** *
 * STRUCTURES FOR TRANSACTIONAL MEMORY *
 * *********************************** */

typedef struct versioned_lock_s {
  pthread_mutex_t write_lock;
  atomic_int versioned_lock;
} versioned_lock_t;

typedef struct segment_s {
  size_t size;   // Is this really needed?
  int num_words; // might be redundant
  versioned_lock_t *locks;
  void *data;
} * segment_t;

typedef struct shared_s {
  size_t align;

  // virtual memory array
  // TODO: more stuff needed for deallocation?
  pthread_mutex_t
      virtual_memory_lock; // Taken only to write (? can I do this? maybe yes)
  segment_t ****virtual_memory; // 4 level list
  atomic_int next_spot;
  int alloced_spots;

  // global version lock
  pthread_mutex_t global_version_lock;
  atomic_int global_version;
} * shared_t;

/* ************************** *
 * STRUCTURES FOR TRANSACTION *
 * ************************** */

// TODO: use a hasmap with segment as key instead

typedef struct ws_item_s {
  segment_t segment; // this item refers to this segment
  void *addr;        // addr in raw memory
  void *value;       // content written
} ws_item_t;

typedef struct rs_item_s {
  segment_t segment;
  void *addr; // addr in raw memory
} rs_item_t;

typedef struct tx_s {
  int rv;
  bool is_ro;
  ws_item_t *ws;
  int ws_sz;
  rs_item_t *rs;
  int rs_sz;
} * tx_t;

// Segment function signatures
inline int segment_init(segment_t s, size_t size, size_t align);
inline void segment_destroy(segment_t);
inline segment_t segment_at(segment_t ****virtual_memory, int idx);

/** Create (i.e. allocate + init) a new shared memory region, with one first
 *non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in
 *bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared
 *memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
 **/
shared_t tm_create(size_t size, size_t align) {
  shared_t tm = malloc(sizeof(struct shared_s));

  if (tm == NULL) {
    return invalid_shared;
  }

  tm->align = align;

  unlinkely(pthread_mutex_init(&tm->global_version_lock, NULL));
  atomic_store(&tm->global_version, 0);

  unlinkely(pthread_mutex_init(&tm->virtual_memory_lock, NULL));
  tm->virtual_memory = calloc(16, sizeof(void *));
  tm->virtual_memory[0] = calloc(16, sizeof(void *));
  tm->virtual_memory[0][0] = calloc(16, sizeof(void *));
  tm->virtual_memory[0][0][0] =
      malloc(16 * sizeof(segment_t)); // Does not need to be null initialized
  atomic_store(&tm->next_spot, 0);

  int allocation_err = segment_init(tm->virtual_memory[0][0][0][0], align, size);
  if (allocation_err != 0) {
    free(tm->virtual_memory[0][0][0]);
    free(tm->virtual_memory[0][0]);
    free(tm->virtual_memory[0]);
    free(tm->virtual_memory);
    free(tm);

    return invalid_shared;
  }

  tm->next_spot++;

  return tm;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
 **/
void tm_destroy(shared_t tm) {
  // TODO: implement freeing

  for (int i = 0; i < tm->next_spot; i++) {
    segment_destroy(segment_at(tm->virtual_memory, i));
  }
}

/** [thread-safe] Return the start address of the first allocated segment in the
 *shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
 **/
void *tm_start(shared_t unused(shared)) {
  return 1 << 48;
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of
 *the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
 **/
size_t tm_size(shared_t unused(shared)) {
  // TODO: tm_size(shared_t)
  return 0;
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the
 *given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
 **/
size_t tm_align(shared_t unused(shared)) {
  // TODO: tm_align(shared_t)
  return 0;
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
 **/
tx_t tm_begin(shared_t unused(shared), bool unused(is_ro)) {
  // TODO: tm_begin(shared_t)
  return invalid_tx;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
 **/
bool tm_end(shared_t unused(shared), tx_t unused(tx)) {
  // TODO: tm_end(shared_t, tx_t)
  return false;
}

/** [thread-safe] Read operation in the given transaction, source in the shared
 *region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the
 *alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
 **/
bool tm_read(shared_t unused(shared), tx_t unused(tx),
             void const *unused(source), size_t unused(size),
             void *unused(target)) {
  // TODO: tm_read(shared_t, tx_t, void const*, size_t, void*)
  return false;
}

/** [thread-safe] Write operation in the given transaction, source in a private
 *region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the
 *alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
 **/
bool tm_write(shared_t unused(shared), tx_t unused(tx),
              void const *unused(source), size_t unused(size),
              void *unused(target)) {
  if (tx->is_ro) {
    // TODO: do I need to check this?
    return false; // invalid operation 
  }
  // TODO: tm_write(shared_t, tx_t, void const*, size_t, void*)
  return false;
}

/** [thread-safe] Memory allocation in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param size   Allocation requested size (in bytes), must be a positive
 *multiple of the alignment
 * @param target Pointer in private memory receiving the address of the first
 *byte of the newly allocated, aligned segment
 * @return Whether the whole transaction can continue (success/nomem), or not
 *(abort_alloc)
 **/
alloc_t tm_alloc(shared_t tm, tx_t unused(tx), size_t unused(size),
                 void **unused(target)) {
  // TODO: tm_alloc(shared_t, tx_t, size_t, void**)
  return abort_alloc;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment
 *to deallocate
 * @return Whether the whole transaction can continue
 **/
bool tm_free(shared_t unused(shared), tx_t unused(tx), void *unused(target)) {
  // TODO: tm_free(shared_t, tx_t, void*)
  return false;
}

/* ************************************ *
 * IMPLEMENTATION OF INTERNAL FUNCTIONS *
 * ************************************ */

inline int segment_init(segment_t s, size_t size, size_t align) {
  int err = posix_memalign(&s->data, align, size);
  if (err != 0) {
    return err;
  }

  s->num_words = size / align;
  s->locks = malloc((size / align) * sizeof(versioned_lock_t));
  if (s->locks == NULL) {
    free(s->data);

    return -1;
  }
  for (int i = 0; i < s->num_words; i++) {
    unlinkedly(pthread_mutex_init(&s->locks[i], NULL));
  }

  return 0;
}

inline segment_t segment_at(segment_t ****virtual_memory, int idx);