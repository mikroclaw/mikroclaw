/* MikroClaw - JSON Escape Test Suite */
#include <stdio.h>
#include <string.h>
#include "../src/json.h"

int main() {
    printf("Testing json_escape...\n");
    
    char out[256];
    int r;
    
    /* Test 1: Normal string */
    r = json_escape("hello world", out, sizeof(out));
    printf("Test 1: %s\n", r == 0 && strcmp(out, "hello world") == 0 ? "PASS" : "FAIL");
    
    /* Test 2: Quotes */
    r = json_escape("hello \"world\"", out, sizeof(out));
    printf("Test 2: %s\n", r == 0 && strcmp(out, "hello \\\"world\\\"") == 0 ? "PASS" : "FAIL");
    
    /* Test 3: Backslash */
    r = json_escape("C:\\path", out, sizeof(out));
    printf("Test 3: %s\n", r == 0 && strcmp(out, "C:\\\\path") == 0 ? "PASS" : "FAIL");
    
    /* Test 4: Newline */
    r = json_escape("line1\nline2", out, sizeof(out));
    printf("Test 4: %s\n", r == 0 && strcmp(out, "line1\\nline2") == 0 ? "PASS" : "FAIL");
    
    /* Test 5: Tab */
    r = json_escape("col1\tcol2", out, sizeof(out));
    printf("Test 5: %s\n", r == 0 && strcmp(out, "col1\\tcol2") == 0 ? "PASS" : "FAIL");
    
    /* Test 6: All special */
    r = json_escape("\"\\\b\f\n\r\t", out, sizeof(out));
    printf("Test 6: %s\n", r == 0 && strcmp(out, "\\\"\\\\\\b\\f\\n\\r\\t") == 0 ? "PASS" : "FAIL");
    
    /* Test 7: Empty */
    r = json_escape("", out, sizeof(out));
    printf("Test 7: %s\n", r == 0 && strcmp(out, "") == 0 ? "PASS" : "FAIL");
    
    /* Test 8: Buffer too small */
    r = json_escape("hello\"world", out, 10);
    printf("Test 8: %s\n", r == -1 ? "PASS" : "FAIL");
    
    printf("\nAll tests complete.\n");
    return 0;
}
