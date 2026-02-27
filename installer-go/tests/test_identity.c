#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/identity.h"

int main(void) {
    char id1[128];
    char id2[128];

    assert(identity_get(id1, sizeof(id1)) == 0);
    assert(id1[0] != '\0');
    assert(identity_rotate(id2, sizeof(id2)) == 0);
    assert(id2[0] != '\0');
    assert(strcmp(id1, id2) != 0);

    printf("ALL PASS: identity\n");
    return 0;
}
