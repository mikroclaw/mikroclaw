#ifndef MIKROCLAW_IDENTITY_H
#define MIKROCLAW_IDENTITY_H

#include <stddef.h>

int identity_get(char *out, size_t out_len);
int identity_rotate(char *out, size_t out_len);

#endif
