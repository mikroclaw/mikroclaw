#include "allowlist.h"

#include <string.h>

int sender_allowed(const char *allowlist, const char *sender) {
    const char *p;
    const char *q;

    if (!sender || sender[0] == '\0') {
        return 0;
    }
    if (!allowlist) {
        return 1;
    }
    if (strcmp(allowlist, "*") == 0) {
        return 1;
    }
    if (allowlist[0] == '\0') {
        return 0;
    }

    p = allowlist;
    while (*p != '\0') {
        size_t n;
        while (*p == ' ' || *p == '\t' || *p == ',') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        q = p;
        while (*q != '\0' && *q != ',') {
            q++;
        }
        n = (size_t)(q - p);
        while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t')) {
            n--;
        }
        if (strlen(sender) == n && strncmp(sender, p, n) == 0) {
            return 1;
        }
        p = q;
    }

    return 0;
}
