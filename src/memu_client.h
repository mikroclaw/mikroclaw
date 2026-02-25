#ifndef MIKROCLAW_MEMU_CLIENT_H
#define MIKROCLAW_MEMU_CLIENT_H

#include <stddef.h>

int memu_client_configure(const char *api_key, const char *base_url);
int memu_memorize(const char *content, const char *modality, const char *user_id);
int memu_retrieve(const char *query, const char *method, char *out, size_t out_len);
int memu_categories(char *out, size_t out_len);
int memu_forget(const char *key);

#endif
