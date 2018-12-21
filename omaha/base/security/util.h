#ifndef SECURITY_UTIL_LITE_UTIL_H_
#define SECURITY_UTIL_LITE_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/* An implementation of memset that ought not to be optimized away;
 * useful for scrubbing security sensitive buffers. */
void *always_memset(void *s, int c, size_t n);

/* Constant-time memory equality test. Returns 0 to indicate equivalence to
 * behave in a memcmp()-compatible way. */
int ct_memeq(const void* s1, const void* s2, uint32_t n);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SECURITY_UTIL_LITE_UTIL_H_
