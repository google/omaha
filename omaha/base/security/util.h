#ifndef SECURITY_UTIL_LITE_UTIL_H_
#define SECURITY_UTIL_LITE_UTIL_H_

#include <stddef.h>

/* An implementation of memset that ought not to be optimized away;
 * useful for scrubbing security sensitive buffers. */
void *always_memset(void *s, int c, size_t n);

#endif  // SECURITY_UTIL_LITE_UTIL_H_
