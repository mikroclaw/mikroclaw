/* MikroClaw - JSON Escape Test Suite */
#include <stdio.h>
#include <string.h>
#include "../src/json.h"

#define CHECK(ID, EXPR) \
    do { \
        if ((EXPR)) { \
            printf("Test %d: PASS\n", (ID)); \
        } else { \
            printf("Test %d: FAIL\n", (ID)); \
            failed++; \
        } \
    } while (0)

int main(void) {
    int failed = 0;
    char out[256];
    int r;

    printf("Testing json_escape...\n");

    r = json_escape("hello world", out, sizeof(out));
    CHECK(1, r == 0 && strcmp(out, "hello world") == 0);

    r = json_escape("hello \"world\"", out, sizeof(out));
    CHECK(2, r == 0 && strcmp(out, "hello \\\"world\\\"") == 0);

    r = json_escape("C:\\path", out, sizeof(out));
    CHECK(3, r == 0 && strcmp(out, "C:\\\\path") == 0);

    r = json_escape("line1\nline2", out, sizeof(out));
    CHECK(4, r == 0 && strcmp(out, "line1\\nline2") == 0);

    r = json_escape("col1\tcol2", out, sizeof(out));
    CHECK(5, r == 0 && strcmp(out, "col1\\tcol2") == 0);

    r = json_escape("\"\\\b\f\n\r\t", out, sizeof(out));
    CHECK(6, r == 0 && strcmp(out, "\\\"\\\\\\b\\f\\n\\r\\t") == 0);

    r = json_escape("", out, sizeof(out));
    CHECK(7, r == 0 && strcmp(out, "") == 0);

    r = json_escape("hello\"world", out, 10);
    CHECK(8, r == -1);

    r = json_escape("\"", out, 3);
    CHECK(9, r == 0 && strcmp(out, "\\\"") == 0);

    r = json_escape("\"", out, 2);
    CHECK(10, r == -1);

    r = json_escape(NULL, out, sizeof(out));
    CHECK(11, r == -1);

    r = json_escape("x", NULL, sizeof(out));
    CHECK(12, r == -1);

    if (failed != 0) {
        printf("\nFAIL: %d json_escape test(s) failed\n", failed);
        return 1;
    }

    printf("\nALL PASS: json_escape\n");
    return 0;
}
