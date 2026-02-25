#ifndef BUF_H
#define BUF_H

#include <stddef.h>

#ifdef __GNUC__
#define BUF_PRINTF_ATTR(fmt, first) __attribute__((format(printf, fmt, first)))
#else
#define BUF_PRINTF_ATTR(fmt, first)
#endif

/* Safe snprintf: returns 0 on success, -1 when truncation would occur */
int safe_snprintf(char *buf, size_t size, const char *fmt, ...) BUF_PRINTF_ATTR(3, 4);

/* Append src to dst with bounds checks. Returns 0 on success, -1 on overflow */
int buf_append(char *dst, size_t dst_size, const char *src);

#endif
