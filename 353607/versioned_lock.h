//
// Created by Vincenzo Pellegrini on 21/11/22.
//

#ifndef CS453_CONCURRENT_ALGORITHMS_PROJECT_VERSIONED_LOCK_H
#define CS453_CONCURRENT_ALGORITHMS_PROJECT_VERSIONED_LOCK_H

#include <stdatomic.h>
#include <stdbool.h>

typedef atomic_int versioned_lock_t;

void vl_init(versioned_lock_t *vl);

bool vl_try_lock(versioned_lock_t *vl, int rv);

/** Read version
 * @param vl versioned flag
 * @returns version read, -1 if locked
 */
int vl_read_version(versioned_lock_t *vl);

void vl_unlock(versioned_lock_t *vl);

void vl_unlock_update(versioned_lock_t *vl, int version);

#endif // CS453_CONCURRENT_ALGORITHMS_PROJECT_VERSIONED_LOCK_H
