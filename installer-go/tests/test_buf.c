#include <assert.h>
#include <string.h>

#include "../src/buf.h"

int main(void) {
    char out[16];
    char dst[10];

    assert(safe_snprintf(out, sizeof(out), "hello") == 0);
    assert(strcmp(out, "hello") == 0);

    assert(safe_snprintf(out, sizeof(out), "%s", "0123456789") == 0);
    assert(strcmp(out, "0123456789") == 0);

    assert(safe_snprintf(out, 10, "%s", "0123456789A") == -1);

    dst[0] = 'a';
    dst[1] = '\0';
    assert(buf_append(dst, sizeof(dst), "bc") == 0);
    assert(strcmp(dst, "abc") == 0);

    dst[0] = 'x';
    dst[1] = '\0';
    assert(buf_append(dst, sizeof(dst), "") == 0);
    assert(strcmp(dst, "x") == 0);

    dst[0] = 'x';
    dst[1] = '\0';
    assert(buf_append(dst, sizeof(dst), "1234567890") == -1);

    assert(buf_append(NULL, sizeof(dst), "x") == -1);
    assert(safe_snprintf(NULL, sizeof(out), "x") == -1);

    return 0;
}
