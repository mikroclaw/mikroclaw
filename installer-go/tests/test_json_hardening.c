#include <stdio.h>
#include <string.h>

int telegram_build_send_body(const char *chat_id, const char *message,
                             char *body, size_t body_len);
int cron_build_add_body(const char *name, const char *schedule, const char *command,
                        char *body, size_t body_len);

static int test_telegram_body_escaping(void) {
    char body[1024];
    const char *chat_id = "12345";
    const char *message = "hello\",\"evil\":\"data";

    if (telegram_build_send_body(chat_id, message, body, sizeof(body)) != 0) {
        printf("FAIL: telegram_build_send_body returned error\n");
        return 1;
    }

    if (strstr(body, "\"evil\":\"data\"") != NULL) {
        printf("FAIL: telegram body injection not escaped\n");
        return 1;
    }

    if (strstr(body, "\\\"evil\\\":\\\"data") == NULL) {
        printf("FAIL: telegram body missing escaped payload\n");
        return 1;
    }

    return 0;
}

static int test_cron_body_escaping(void) {
    char body[1024];
    const char *name = "job\",\"interval\":\"*/1 * * * *\",\"x\":\"y";
    const char *schedule = "1m";
    const char *command = "/interface/print";

    if (cron_build_add_body(name, schedule, command, body, sizeof(body)) != 0) {
        printf("FAIL: cron_build_add_body returned error\n");
        return 1;
    }

    if (strstr(body, "\"interval\":\"*/1") != NULL) {
        printf("FAIL: cron body injection not escaped\n");
        return 1;
    }

    return 0;
}

int main(void) {
    int failures = 0;

    failures += test_telegram_body_escaping();
    failures += test_cron_body_escaping();

    if (failures == 0) {
        printf("ALL PASS\n");
        return 0;
    }

    printf("FAILURES: %d\n", failures);
    return 1;
}
