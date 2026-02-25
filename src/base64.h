#ifndef BASE64_H
#define BASE64_H

#include <stddef.h>

int base64_encode(const unsigned char *input, size_t input_len,
                  char *output, size_t output_len);

#endif
