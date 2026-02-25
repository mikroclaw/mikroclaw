/*
 * MikroClaw - RouterOS REST API Client
 */

#ifndef ROUTEROS_H
#define ROUTEROS_H

#include <stdbool.h>
#include <stddef.h>

struct routeros_config {
    char host[256];
    int port;
    char user[64];
    char pass[64];
};

struct routeros_ctx;

/* Initialize RouterOS connection */
struct routeros_ctx *routeros_init(const char *host, int port, 
                                   const char *user, const char *pass);

/* Cleanup */
void routeros_destroy(struct routeros_ctx *ctx);

/* Execute command */
int routeros_execute(struct routeros_ctx *ctx, const char *command,
                     char *output, size_t max_output);

/* Get data from REST API */
int routeros_get(struct routeros_ctx *ctx, const char *path,
                 char *output, size_t max_output);

/* Post data to REST API */
int routeros_post(struct routeros_ctx *ctx, const char *path,
                  const char *data, char *output, size_t max_output);

int routeros_firewall_allow_subnets(struct routeros_ctx *ctx, const char *comment,
                                    const char *subnets_csv, int port);
int routeros_firewall_remove_comment(struct routeros_ctx *ctx, const char *comment);
int routeros_script_run_inline(struct routeros_ctx *ctx, const char *script, char *output, size_t max_output);
int routeros_scheduler_add(struct routeros_ctx *ctx, const char *name,
                           const char *interval, const char *on_event,
                           char *output, size_t max_output);
int routeros_scheduler_remove(struct routeros_ctx *ctx, const char *name,
                              char *output, size_t max_output);

#endif /* ROUTEROS_H */
