#include "memu_client.h"

#include "json.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_api_key[256];
static char g_base_url[256] = "https://api.memu.so";

struct memu_buf {
    char *data;
    size_t len;
};

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t chunk = size * nmemb;
    struct memu_buf *buf = (struct memu_buf *)userp;
    char *next = realloc(buf->data, buf->len + chunk + 1);

    if (!next) {
        return 0;
    }
    buf->data = next;
    memcpy(buf->data + buf->len, contents, chunk);
    buf->len += chunk;
    buf->data[buf->len] = '\0';
    return chunk;
}

static int post_json(const char *path, const char *json_body, struct memu_buf *out) {
    CURL *curl;
    CURLcode rc;
    long code = 0;
    struct curl_slist *headers = NULL;
    char auth[320];
    char url[512];

    if (!path || !json_body || !out || g_api_key[0] == '\0') {
        return -1;
    }

    snprintf(url, sizeof(url), "%s%s", g_base_url, path);
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", g_api_key);

    curl = curl_easy_init();
    if (!curl) {
        return -1;
    }

    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth);

    out->data = NULL;
    out->len = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(json_body));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 30000L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    rc = curl_easy_perform(curl);
    if (rc == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK || code < 200 || code >= 300) {
        free(out->data);
        out->data = NULL;
        out->len = 0;
        return -1;
    }
    return 0;
}

int memu_client_configure(const char *api_key, const char *base_url) {
    if (!api_key || api_key[0] == '\0') {
        g_api_key[0] = '\0';
        return -1;
    }

    snprintf(g_api_key, sizeof(g_api_key), "%s", api_key);
    if (base_url && base_url[0] != '\0') {
        snprintf(g_base_url, sizeof(g_base_url), "%s", base_url);
    }
    return 0;
}

int memu_memorize(const char *content, const char *modality, const char *user_id) {
    char esc[2048];
    char body[3072];
    struct memu_buf out;

    if (!content || content[0] == '\0') {
        return -1;
    }
    if (getenv("MEMU_MOCK_RETRIEVE_TEXT")) {
        return 0;
    }

    if (json_escape(content, esc, sizeof(esc)) != 0) {
        return -1;
    }

    snprintf(body, sizeof(body),
             "{\"resource_url\":\"inline\",\"modality\":\"%s\",\"content\":\"%s\",\"user\":{\"user_id\":\"%s\"}}",
             (modality && modality[0]) ? modality : "conversation",
             esc,
             (user_id && user_id[0]) ? user_id : "default-user");

    if (post_json("/api/v3/memory/memorize", body, &out) != 0) {
        return -1;
    }
    free(out.data);
    return 0;
}

int memu_retrieve(const char *query, const char *method, char *out, size_t out_len) {
    char esc[1024];
    char body[2048];
    struct memu_buf buf;

    if (!query || !out || out_len == 0) {
        return -1;
    }

    if (getenv("MEMU_MOCK_RETRIEVE_TEXT")) {
        snprintf(out, out_len, "%s", getenv("MEMU_MOCK_RETRIEVE_TEXT"));
        return 0;
    }

    if (json_escape(query, esc, sizeof(esc)) != 0) {
        return -1;
    }

    snprintf(body, sizeof(body),
             "{\"queries\":[{\"role\":\"user\",\"content\":{\"text\":\"%s\"}}],\"method\":\"%s\"}",
             esc,
             (method && method[0]) ? method : "rag");

    if (post_json("/api/v3/memory/retrieve", body, &buf) != 0) {
        return -1;
    }

    snprintf(out, out_len, "%s", buf.data ? buf.data : "");
    free(buf.data);
    return 0;
}

int memu_categories(char *out, size_t out_len) {
    struct memu_buf buf;

    if (!out || out_len == 0) {
        return -1;
    }

    if (getenv("MEMU_MOCK_CATEGORIES")) {
        snprintf(out, out_len, "%s", getenv("MEMU_MOCK_CATEGORIES"));
        return 0;
    }

    if (post_json("/api/v3/memory/categories", "{}", &buf) != 0) {
        return -1;
    }

    snprintf(out, out_len, "%s", buf.data ? buf.data : "");
    free(buf.data);
    return 0;
}

int memu_forget(const char *key) {
    char esc[1024];
    char body[1536];
    struct memu_buf out;

    if (!key || key[0] == '\0') {
        return -1;
    }
    if (getenv("MEMU_MOCK_RETRIEVE_TEXT")) {
        return 0;
    }
    if (json_escape(key, esc, sizeof(esc)) != 0) {
        return -1;
    }

    snprintf(body, sizeof(body), "{\"key\":\"%s\"}", esc);
    if (post_json("/api/v3/memory/forget", body, &out) != 0) {
        return -1;
    }
    free(out.data);
    return 0;
}
