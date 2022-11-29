#include <stdio.h>
#include <stdlib.h>
#include <tm.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

#define fail(ln) printf(ln"\n"); exit(2);
atomic_bool flag = false;
atomic_int counter = 0;


void *thread(void *arg) {
    int my_id = atomic_fetch_add(&counter, 1);
    shared_t shared = (shared_t)arg;
    while (!atomic_load(&flag)) {}
    // for (int i=0; i<25000; i++) {
    bool success = false;
    while (!success) {
        tx_t tx = tm_begin(shared, false);
        if (tx == invalid_tx) {
            printf("%d: tm_begin failed", my_id);
            continue;
        }

        char* source = malloc(64);
        for (int i=0; i<64; i++) {
            source[i] = i;
        }

        if (!tm_write(shared, tx, source, 64, tm_start(shared))) {
            printf("%d: tm_write failed\n", my_id);
            continue;
        }

        if (!tm_end(shared, tx)) {
            printf("%d: tm_end failed\n", my_id);
            continue;
        }

        printf("%d: success\n", my_id);

        success = true;
    }
    // }
    return NULL;
}

#define NUM_THREADS 8

int main() {
    shared_t shared = tm_create(1024, 8);

    pthread_t ts[NUM_THREADS];
    for (int i=0; i<NUM_THREADS; i++) {
        pthread_create(&ts[i], NULL, thread, shared);
    }

    atomic_store(&flag, true);

    for (int i=0; i<NUM_THREADS; i++) {
        pthread_join(ts[i], NULL);
    }

    tm_destroy(shared);
}