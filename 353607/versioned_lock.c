#include "versioned_lock.h"

void vl_init(versioned_lock_t *vl) { atomic_store(vl, 0); }

bool vl_try_lock(versioned_lock_t *vl, int rv) {
  int prev = atomic_load(vl);
  if (prev & 0x1 || (prev >> 1 & (~0b1)) > rv) {
    return false;
  }
  bool success = atomic_compare_exchange_strong(vl, &prev, prev | 0x1);
  return success;
}

/** Read version
 * @param vl versioned flag
 * @returns version read, -1 if locked
 */
int vl_read_version(versioned_lock_t *vl) {
  int read = atomic_load(vl);
  if (read & 0b1) {
    return -1;
  }
  return read >> 1 & (~0x1);
}

void vl_unlock(versioned_lock_t *vl) { atomic_fetch_sub(vl, 1); }

void vl_unlock_update(versioned_lock_t *vl, int version) {
  atomic_store(vl, version << 1);
}

