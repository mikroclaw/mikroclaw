#include <assert.h>
#include <stdio.h>

#include "../src/channels/allowlist.h"

int main(void) {
    assert(sender_allowed("*", "alice") == 1);
    assert(sender_allowed("alice,bob", "alice") == 1);
    assert(sender_allowed("alice,bob", "carol") == 0);
    assert(sender_allowed("", "alice") == 0);
    assert(sender_allowed(NULL, "alice") == 1);

    printf("ALL PASS: allowlist\n");
    return 0;
}
