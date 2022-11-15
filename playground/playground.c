#include <stdio.h>
#include <stdlib.h>
#include <tm.h>
#include <unistd.h>

int main() {
  shared_t s = tm_create(1024, 16);

  if (s == invalid_shared) {
    printf("Failed to allocate shared memory!\n");
    return 1;
  }

  printf("The alignment is %lu\n", tm_align(s));
  printf("The size is %lu\n", tm_size(s));

  tx_t tx = tm_begin(s, true);

  void *space = malloc(80);
  bool success = tm_read(s, tx, tm_start(s), 80, space);
  if (!success) {
    printf("I couldn't even read!");
  }

  printf("I read this: %d\n", *(int *)space);
}