#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_WORKERS 256 / 16
#define NUM_WRITES 1024 * 4

#define NUM_LOOPS 100

/**
 * Multi level array
 * Not the hero that we wanted but the hero that we needed
 */

typedef struct {
  void *****p;
  short next;
} array_t;

array_t arr;

void array_init() {
  arr.p = calloc(17, sizeof(void *));
  arr.p[16] = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init((pthread_mutex_t*)arr.p[16], NULL);
  arr.next = 0;
}

unsigned short array_add(void *val) {
  pthread_mutex_t* lock = (pthread_mutex_t*) arr.p[16];

  pthread_mutex_lock(lock);
  int curr = arr.next++;
  unsigned char positions[4];
  for (int i = 3; i >= 0; i--) {
    positions[i] = (curr >> (4 * i)) & 0xF;
  }
  void **p = (void **)arr.p;
  for (int i = 0; i < 3; i++) {
    if (p[positions[i]] == NULL) {
      p[positions[i]] = (i < 2) ? calloc(17, sizeof(void *)) : malloc(16*sizeof(void*));
      p = p[positions[i]];
      p[16] = malloc(sizeof(pthread_mutex_t));
      pthread_mutex_init(((void**)p)[16], NULL);
    } else {
      p = p[positions[i]];
    }
    pthread_mutex_unlock(lock);
    lock = p[16];
    if (i < 2)
      pthread_mutex_lock(lock);
  }

  p[positions[3]] = val;

  return curr;
}

void *array_get(unsigned short index) {
  unsigned char positions[4];
  for (int i = 3; i >= 0; i--) {
    positions[i] = (index >> (4 * i)) & 0xF;
  }

  return arr.p[positions[0]][positions[1]][positions[2]][positions[3]];
}

// Time waster
int fib(int n) {
  if (n == 0)
    return 0;
  if (n == 1)
    return 1;
  return fib(n - 1) + fib(n - 2);
}

static atomic_long counter;
void *writer_worker(void *args) {
  for (int i = 0; i < NUM_WRITES; i++) {
    array_add((void *)(atomic_fetch_add(&counter, 1) % 23));
    // fib(22);
  }
  // printf("A worker wrote\n");

  return NULL;
}

int main() {
  clock_t begin = clock();

  for (int count = 0; count < NUM_LOOPS; count++) {
    // Initialize array
    array_init();

    // start threads inserting stuff
    pthread_t workers[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++) {
      pthread_create(&workers[i], NULL, writer_worker, NULL);
    }

    for (int i = 0; i < NUM_WORKERS; i++) {
      pthread_join(workers[i], NULL);
    }
  }

  clock_t end = clock();

  double time_spent = (double)(end - begin)*(1000000000) / (CLOCKS_PER_SEC * NUM_LOOPS * 65536);

  printf("\nNanoseconds per array write and stuff: %f\n\n", time_spent);
  // printf("arr.next = %d", arr.next);
  // for (int i=0; i<NUM_WORKERS*NUM_WRITES; i++) {
  // 	printf("%ld\n", (long int)array_get(i));
  // }
}
