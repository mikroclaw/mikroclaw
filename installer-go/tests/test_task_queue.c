#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/task_queue.h"

int main(void) {
    struct task_queue_ctx *q = task_queue_init(4);
    char id[64];
    struct task_item *it;
    char list[512];

    assert(q != NULL);
    assert(task_queue_add(q, "analyze", "{}", id, sizeof(id)) == 0);
    assert(id[0] != '\0');

    it = task_queue_get(q, id);
    assert(it != NULL);
    assert(strcmp(it->type, "analyze") == 0);
    assert(it->status == TASK_QUEUED);

    assert(task_queue_list_json(q, list, sizeof(list)) == 0);
    assert(strstr(list, id) != NULL);

    task_queue_destroy(q);
    printf("ALL PASS: task queue\n");
    return 0;
}
