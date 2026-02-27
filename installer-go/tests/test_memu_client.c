#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/memu_client.h"

int main(void) {
    char out[512];

    setenv("MEMU_API_KEY", "test-key", 1);
    setenv("MEMU_MOCK_RETRIEVE_TEXT", "Rust", 1);
    setenv("MEMU_MOCK_CATEGORIES", "[\"preferences\",\"context\"]", 1);

    assert(memu_client_configure(getenv("MEMU_API_KEY"), "https://api.memu.so") == 0);
    assert(memu_memorize("User likes Rust", "conversation", "user_123") == 0);

    memset(out, 0, sizeof(out));
    assert(memu_retrieve("What does user like?", "rag", out, sizeof(out)) == 0);
    assert(strstr(out, "Rust") != NULL);

    memset(out, 0, sizeof(out));
    assert(memu_categories(out, sizeof(out)) == 0);
    assert(strstr(out, "preferences") != NULL);

    unsetenv("MEMU_MOCK_RETRIEVE_TEXT");
    unsetenv("MEMU_MOCK_CATEGORIES");
    unsetenv("MEMU_API_KEY");

    assert(memu_client_configure(NULL, "https://api.memu.so") != 0);

    printf("ALL PASS: memu client\n");
    return 0;
}
