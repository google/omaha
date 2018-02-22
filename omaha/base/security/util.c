#include "util.h"

static void always_memset_impl(volatile char *s, int c, size_t n) {
  while (n--)
    *s++ = c;
}

void *always_memset(void *s, int c, size_t n) {
  always_memset_impl((char*) s, c, n);
  return s;
}
