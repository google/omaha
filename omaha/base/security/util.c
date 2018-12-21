#include "util.h"

static void always_memset_impl(volatile char *s, int c, size_t n) {
  while (n--)
    *s++ = c;
}

void *always_memset(void *s, int c, size_t n) {
  always_memset_impl((char*) s, c, n);
  return s;
}

int ct_memeq(const void* s1, const void* s2, uint32_t n) {
  uint8_t diff = 0;
  const uint8_t* ps1 = s1;
  const uint8_t* ps2 = s2;

  while (n--) {
    diff |= *ps1 ^ *ps2;
    ps1++;
    ps2++;
  }

  // Counter-intuitive, but we can't just return diff because we don't want to
  // leak the xor of non-equal bytes
  return 0 != diff;
}
