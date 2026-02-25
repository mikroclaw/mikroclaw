/*
 * MikroClaw - Cron/Scheduler Header
 */

#ifndef CRON_H
#define CRON_H

#include <stddef.h>

struct routeros_ctx;

int cron_add(struct routeros_ctx *router, const char *name, 
             const char *schedule, const char *command);
int cron_build_add_body(const char *name, const char *schedule, const char *command,
                        char *body, size_t body_len);
int cron_list(struct routeros_ctx *router, char *out, size_t max_len);
int cron_remove(struct routeros_ctx *router, const char *name);
int cron_run(struct routeros_ctx *router, const char *name);

#endif /* CRON_H */
