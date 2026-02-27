#include <stdio.h>
#include <string.h>

#include "../src/base64.h"

struct test_case {
    const char *input;
    const char *expected;
};

int main(void) {
    struct test_case tests[] = {
        {"admin:password", "YWRtaW46cGFzc3dvcmQ="},
        {"user:pass123", "dXNlcjpwYXNzMTIz"},
        {"test:test", "dGVzdDp0ZXN0"},
        {NULL, NULL}
    };

    int failures = 0;
    char output[256];

    for (int i = 0; tests[i].input != NULL; i++) {
        int ret = base64_encode((const unsigned char *) tests[i].input,
                                strlen(tests[i].input),
                                output,
                                sizeof(output));
        if (ret != 0 || strcmp(output, tests[i].expected) != 0) {
            failures++;
        }
    }

    if (failures == 0) {
        printf("ALL PASS\n");
        return 0;
    }

    printf("FAILURES: %d\n", failures);
    return 1;
}
