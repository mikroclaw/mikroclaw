#include "http_client.h"

#include <stdlib.h>
#include <string.h>

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t chunk_size = size * nmemb;
    curl_http_response *response = (curl_http_response *)userp;

    char *next = realloc(response->body, response->body_len + chunk_size + 1);
    if (!next) {
        return 0;
    }

    response->body = next;
    memcpy(response->body + response->body_len, contents, chunk_size);
    response->body_len += chunk_size;
    response->body[response->body_len] = '\0';
    return chunk_size;
}

curl_http_client *curl_http_client_create(void) {
    curl_http_client *client = calloc(1, sizeof(*client));
    if (!client) {
        return NULL;
    }

    client->curl = curl_easy_init();
    if (!client->curl) {
        free(client);
        return NULL;
    }

    return client;
}

void curl_http_client_destroy(curl_http_client *client) {
    if (!client) {
        return;
    }

    if (client->curl) {
        curl_easy_cleanup(client->curl);
    }
    free(client);
}

int curl_http_get(curl_http_client *client, const char *url, curl_http_response *response) {
    if (!client || !url || !response) {
        return -1;
    }

    response->status_code = 0;
    response->body = NULL;
    response->body_len = 0;

    curl_easy_setopt(client->curl, CURLOPT_URL, url);
    curl_easy_setopt(client->curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(client->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(client->curl, CURLOPT_TIMEOUT_MS, 30000L);
    curl_easy_setopt(client->curl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
    curl_easy_setopt(client->curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(client->curl, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode rc = curl_easy_perform(client->curl);
    if (rc != CURLE_OK) {
        curl_http_response_free(response);
        return -1;
    }

    curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, &response->status_code);
    return 0;
}

int curl_http_post(curl_http_client *client, const char *url, const char *body, curl_http_response *response) {
    if (!client || !url || !body || !response) {
        return -1;
    }

    response->status_code = 0;
    response->body = NULL;
    response->body_len = 0;

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(client->curl, CURLOPT_URL, url);
    curl_easy_setopt(client->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(client->curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(client->curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
    curl_easy_setopt(client->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(client->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(client->curl, CURLOPT_TIMEOUT_MS, 30000L);
    curl_easy_setopt(client->curl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
    curl_easy_setopt(client->curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(client->curl, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode rc = curl_easy_perform(client->curl);
    curl_slist_free_all(headers);
    curl_easy_setopt(client->curl, CURLOPT_HTTPHEADER, NULL);
    curl_easy_setopt(client->curl, CURLOPT_POST, 0L);
    if (rc != CURLE_OK) {
        curl_http_response_free(response);
        return -1;
    }

    curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, &response->status_code);
    return 0;
}

void curl_http_response_free(curl_http_response *response) {
    if (!response) {
        return;
    }

    free(response->body);
    response->body = NULL;
    response->body_len = 0;
    response->status_code = 0;
}
