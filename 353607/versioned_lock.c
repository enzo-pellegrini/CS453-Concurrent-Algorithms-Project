#include "versioned_lock.h"

#include <stdio.h>

void vl_init(versioned_lock_t *vl) { atomic_store(vl, 0); }

bool vl_try_lock(versioned_lock_t *vl, int rv, int last_updater) {
    int prev = atomic_load(vl);
    if (prev & 0x1 || should_abort(prev, rv, last_updater)) {
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
    if (read & 0x1) {
        return -1;
    }
    return read >> 1;
}

void vl_unlock(versioned_lock_t *vl) { atomic_fetch_sub(vl, 1); }

void vl_unlock_update(versioned_lock_t *vl, int version) { atomic_store(vl, version << 1); }

int version_to_clock(int version) { return version & 0x7FFFFF; }

int version_to_ti(int version) { return (version >> 23) & 0xFF; }

int build_version(int clock, int ti) { return ((ti & 0xFF) << 23) | (clock & 0x7FFFFF); }

bool should_abort(int version_read, int rv, int ti_latest) {
    int clock = version_to_clock(version_read);
    int ti = version_to_ti(version_read);
    // printf("Read clock: %d, ti: %d\n", clock, ti);
    if (clock > rv || (clock == rv && ti != ti_latest)) {
        return true;
    }
    return false;
}
