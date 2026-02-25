#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../src/channels/telegram.h"

int telegram_parse_message(const char *json_response, struct telegram_message *msg);

int main(void) {
    const char *json =
        "{\"ok\":true,\"result\":[{\"update_id\":12345,\"message\":{"
        "\"from\":{\"username\":\"alice\"},"
        "\"chat\":{\"id\":67890},"
        "\"text\":\"Hello bot\"}}]}";

    struct telegram_message msg;
    memset(&msg, 0, sizeof(msg));

    setenv("TELEGRAM_ALLOWLIST", "alice", 1);
    int rc = telegram_parse_message(json, &msg);
    if (rc != 1) {
        printf("FAIL: parse function did not return message\n");
        return 1;
    }

    if (msg.update_id != 12345) {
        printf("FAIL: update_id mismatch (%d)\n", msg.update_id);
        return 1;
    }
    if (strcmp(msg.chat_id, "67890") != 0) {
        printf("FAIL: chat_id mismatch (%s)\n", msg.chat_id);
        return 1;
    }
    if (strcmp(msg.text, "Hello bot") != 0) {
        printf("FAIL: text mismatch (%s)\n", msg.text);
        return 1;
    }

    setenv("TELEGRAM_ALLOWLIST", "bob", 1);
    memset(&msg, 0, sizeof(msg));
    rc = telegram_parse_message(json, &msg);
    if (rc != 0) {
        printf("FAIL: allowlist should block sender\n");
        return 1;
    }

    printf("ALL PASS\n");
    return 0;
}
