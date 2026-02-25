#include "memu_client.h"

#ifndef USE_MEMU_CLOUD

#include <stddef.h>

int memu_client_configure(const char *api_key, const char *base_url) {
    (void)api_key;
    (void)base_url;
    return -1;
}

int memu_memorize(const char *content, const char *modality, const char *user_id) {
    (void)content;
    (void)modality;
    (void)user_id;
    return -1;
}

int memu_retrieve(const char *query, const char *method, char *out, size_t out_len) {
    (void)query;
    (void)method;
    if (out && out_len > 0) {
        out[0] = '\0';
    }
    return -1;
}

int memu_categories(char *out, size_t out_len) {
    if (out && out_len > 0) {
        out[0] = '\0';
    }
    return -1;
}

int memu_forget(const char *key) {
    (void)key;
    return -1;
}

#endif
