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
    for (int i=0; i<25000; i++) {
        bool success = false;
        while (!success) {
            tx_t tx = tm_begin(shared, false);
            if (tx == invalid_tx) {
//                printf("%d: tm_begin failed", i);
            }
            void *loc;
//        if (tm_alloc(shared, tx, 32, &loc) != success_alloc) {
//            fail("tm_alloc failed");
//        }
            loc = tm_start(shared);

            char *str = strdup("0123456789abcde");
            if (tm_write(shared, tx, str, 16, loc) != true) {
//                printf("%d@%d: tm_write failed\n", i, my_id);
                continue;
            }
            if (tm_write(shared, tx, str, 16, loc+16) != true) {
//                printf("%d@%d: tm_write failed\n", i, my_id);
                continue;
            }
            if (tm_write(shared, tx, str, 16, loc+32) != true) {
//                printf("%d@%d: tm_write failed\n", i, my_id);
                continue;
            }

            if (tm_read(shared, tx, loc, 16, str) != true) {
//                printf("%d@%d: tm_read failed\n", i, my_id);
                continue;
            }
            if (tm_read(shared, tx, loc+32, 16, str) != true) {
//                printf("%d@%d: tm_read failed\n", i, my_id);
                continue;
            }

            if (strcmp(str, "0123456789abcde") != 0) {
//                printf("%d@%d: tm_read failed\n", i, my_id);
                continue;
            }

            if (!tm_end(shared, tx)) {
//                printf("%d@%d: tm_end failed\n", i, my_id);
                continue;
            }

            if (my_id % 4 == 0)
                usleep(100);
            else
                usleep(10);

            success = true;
        }
    }
    return NULL;
}

#define NUM_THREADS 8

int main() {
    shared_t shared = tm_create(1024, 16);

    pthread_t ts[NUM_THREADS];
    for (int i=0; i<NUM_THREADS; i++) {
        pthread_create(&ts[i], NULL, thread, shared);
    }

    atomic_store(&flag, true);

    for (int i=0; i<NUM_THREADS; i++) {
        pthread_join(ts[i], NULL);
    }

    tm_destroy(shared);

//    if (shared == invalid_shared) {
//        printf("Failed to allocate shared memory!\n");
//        return 1;
//    }
//
//    printf("The alignment is %lu\n", tm_align(shared));
//    printf("The size is %lu\n", tm_size(shared));
//
//    for (int c = 0; c < 100000000; c++) {
//        // Read-Write transaction
//        tx_t tx = tm_begin(shared, false);
//        if (tx == invalid_tx) {
//            fail("Read Write transaction failed");
//        }
//
//        // let's write 0 to the first object
//        void *content = malloc(48);
//        memset(content, 'b', 48);
//        void *target = malloc(48);
//        memset(target, 'a', 48);
//        if (!tm_write(shared, tx, content, 48, tm_start(shared))) {
//            fail("Failed to write");
//        }
//
//        if (!tm_read(shared, tx, tm_start(shared), 16, content)) {
//            fail("Failed to read");
//        }
//        for (int i = 0; i < 16; i++) {
//            if (((char *) content)[i] != 'b') {
//                fail("The read returned a wrong value");
//            }
//        }
//
//        if (!tm_end(shared, tx)) {
//            fail("Read-Write transaction failed to commit");
//        }
//
//        // Read only transaction to check the memory I wrote to
//        tx = tm_begin(shared, true);
//        if (tx == invalid_tx) {
//            fail("Read only transaction failed to start (?)");
//        }
//
//        void *space = malloc(16);
//        if (!tm_read(shared, tx, tm_start(shared), 16, space)) {
//            fail("Failed to read in second read-only transaction");
//        }
//
//        for (int i = 0; i < 16; i++) {
////            printf("%c", ((char *) space)[i]);
//        }
////        printf("\n");
//
//        tm_end(shared, tx);
//
//        // let's build a new segment, just for giggles
////    tx = tm_begin(shared, false);
//
////    void *va_tm;
////    if (tm_alloc(shared, tx, 16 * 8, &va_tm) != success_alloc) {
////      fail("Failed to allocate");
////    }
//    }
}