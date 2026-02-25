#include "base64.h"

static const char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64_encode(const unsigned char *input, size_t input_len,
                  char *output, size_t output_len) {
    size_t out_len = 4 * ((input_len + 2) / 3);
    if (!input || !output || output_len < out_len + 1) {
        return -1;
    }

    size_t i = 0;
    size_t j = 0;
    while (i < input_len) {
        size_t rem = input_len - i;
        unsigned int octet_a = input[i++];
        unsigned int octet_b = (rem > 1) ? input[i++] : 0;
        unsigned int octet_c = (rem > 2) ? input[i++] : 0;
        unsigned int triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        output[j++] = BASE64_CHARS[(triple >> 18) & 0x3F];
        output[j++] = BASE64_CHARS[(triple >> 12) & 0x3F];
        output[j++] = (rem > 1) ? BASE64_CHARS[(triple >> 6) & 0x3F] : '=';
        output[j++] = (rem > 2) ? BASE64_CHARS[triple & 0x3F] : '=';
    }

    output[j] = '\0';
    return 0;
}
