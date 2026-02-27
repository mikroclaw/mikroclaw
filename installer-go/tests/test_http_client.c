#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/http_client.h"

int main(void) {
    curl_http_client *client = curl_http_client_create();
    assert(client != NULL);

    curl_http_response response;
    int ret = curl_http_get(client, "https://httpbin.org/get", &response);
    assert(ret == 0);
    assert(response.status_code == 200);
    assert(response.body != NULL);
    assert(strstr(response.body, "\"url\"") != NULL);
    curl_http_response_free(&response);

    ret = curl_http_post(client, "https://httpbin.org/post", "{\"hello\":\"world\"}", &response);
    assert(ret == 0);
    assert(response.status_code == 200);
    assert(response.body != NULL);
    assert(strstr(response.body, "\"hello\": \"world\"") != NULL);
    curl_http_response_free(&response);

    curl_http_client_destroy(client);

    printf("ALL PASS: http_client create/destroy/get\n");
    return 0;
}
