#include <stdio.h>
#include <stdlib.h>
#include <tm.h>
#include <unistd.h>
#include <string.h>

#define println(ln) printf(ln"\n")

int main() {
  shared_t shared = tm_create(1024, 16);

  if (shared == invalid_shared) {
    printf("Failed to allocate shared memory!\n");
    return 1;
  }

  printf("The alignment is %lu\n", tm_align(shared));
  printf("The size is %lu\n", tm_size(shared));

  // Read only transaction
  tx_t tx = tm_begin(shared, true);
  if (tx == invalid_tx) {
    println("Read only transaction failed to start (?)");
  }

  void *space = malloc(80);
  bool success = tm_read(shared, tx, tm_start(shared), 80, space);
  if (!success) {
    println("I couldn't even read!");
  }

  printf("I read this: %d\n", *(int *)space);

  if (!tm_end(shared, tx)) {
    println("Oh come on, this shouldn't do anything bad!");
  }


  // Read-Write transaction
  tx = tm_begin(shared, false);
  if (tx == invalid_tx) {
    println("Read Write transaction failed");
  }

  // let's write 0 to the first object
  void* content = malloc(16);
  memset(content, 'b', 16);
  void* target = malloc(16);
  memset(target, 'a', 16);
  if (!tm_write(shared, tx, content, 16, tm_start(shared))) {
    println("Failed to write");
  }

  if (!tm_read(shared, tx, tm_start(shared), 16, content)) {
    println("Failed to read");
  }
  for (int i=0; i<16; i++) {
    if (((char*)content)[i] != 'b') {
      println("The read returned a wrong value");
    }
  }

  if (!tm_end(shared, tx)) {
    println("Read-Write transaction failed to commit");
  }


  // Read only transaction to check the memory I wrote to
  tx = tm_begin(shared, true);
  if (tx == invalid_tx) {
    println("Read only transaction failed to start (?)");
  }

  space = malloc(16);
  if (!tm_read(shared, tx, tm_start(shared), 16, space)) {
    println("Failed to read in second read-only transaction");
  }

  for (int i=0; i<16; i++) {
    printf("%c", ((char*)space)[i]);
  }
  println("");

  tm_end(shared, tx);


  // let's build a new segment, just for giggles
  tx = tm_begin(shared, false);

  void* va_tm;
  if (tm_alloc(shared, tx, 16*8, &va_tm) != success_alloc) {
    println("Failed to allocate");
  }
}