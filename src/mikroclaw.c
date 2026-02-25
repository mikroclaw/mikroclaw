/*
 * MikroClaw - Main loop and orchestration
 */

#include "mikroclaw.h"
#include "routeros.h"
#include "llm.h"
#include "channels/telegram.h"
#include "gateway.h"
#include "gateway_auth.h"
#include "functions.h"
#include "memu_client.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum reply_target {
    REPLY_TELEGRAM = 1,
    REPLY_SLACK = 2,
    REPLY_DISCORD = 3,
    REPLY_GATEWAY = 4,
};

static void build_session_id(char *dst, size_t dst_len, const char *chat_id) {
    const char *prefix = "telegram:";
    size_t prefix_len = strlen(prefix);
    size_t max_chat_len;

    if (dst_len == 0) {
        return;
    }
    if (!chat_id) {
        chat_id = "";
    }
    if (prefix_len + 1 >= dst_len) {
        dst[0] = '\0';
        return;
    }

    memcpy(dst, prefix, prefix_len);
    max_chat_len = dst_len - prefix_len - 1;
    strncpy(dst + prefix_len, chat_id, max_chat_len);
    dst[prefix_len + max_chat_len] = '\0';
}

static const char *http_body_from_request(const char *request) {
    const char *body;

    if (!request) {
        return NULL;
    }
    body = strstr(request, "\r\n\r\n");
    if (body) {
        return body + 4;
    }
    return request;
}

static void build_http_text_response(const char *body, char *response, size_t response_len) {
    size_t body_len;

    if (!response || response_len == 0) {
        return;
    }
    if (!body) {
        body = "";
    }

    body_len = strlen(body);
    snprintf(response, response_len,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/plain; charset=utf-8\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             body_len, body);
}

static void build_http_json_response(int status, const char *reason,
                                     const char *body, char *response,
                                     size_t response_len) {
    size_t body_len;

    if (!response || response_len == 0) {
        return;
    }
    if (!reason) {
        reason = "OK";
    }
    if (!body) {
        body = "{}";
    }

    body_len = strlen(body);
    snprintf(response, response_len,
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             status, reason, body_len, body);
}

static int parse_request_line(const char *request, char *method, size_t method_len,
                              char *path, size_t path_len) {
    if (!request || !method || !path) {
        return -1;
    }
    method[0] = '\0';
    path[0] = '\0';
    if (sscanf(request, "%15s %255s", method, path) != 2) {
        return -1;
    }
    (void)method_len;
    (void)path_len;
    return 0;
}

static void send_reply(struct mikroclaw_ctx *ctx, enum reply_target target,
                       const char *chat_id, int gateway_client_fd,
                       const char *message) {
    if (!ctx || !message) {
        return;
    }

#ifdef CHANNEL_TELEGRAM
    if (target == REPLY_TELEGRAM && ctx->telegram && chat_id) {
        telegram_send(ctx->telegram, chat_id, message);
    }
#endif
#ifdef CHANNEL_DISCORD
    if (target == REPLY_DISCORD && ctx->discord) {
        (void)discord_send(ctx->discord, message);
    }
#endif
#ifdef CHANNEL_SLACK
    if (target == REPLY_SLACK && ctx->slack) {
        (void)slack_send(ctx->slack, message);
    }
#endif
#ifdef ENABLE_GATEWAY
    if (target == REPLY_GATEWAY && gateway_client_fd >= 0) {
        char response[6144];
        build_http_text_response(message, response, sizeof(response));
        gateway_respond(gateway_client_fd, response);
    }
#else
    (void)gateway_client_fd;
#endif
}

static void memu_store_turn(const char *session_id, const char *role, const char *content) {
    char payload[1400];

    if (!role || !content) {
        return;
    }
    if (!session_id || session_id[0] == '\0') {
        session_id = "default-session";
    }

    snprintf(payload, sizeof(payload), "%s|%s|%s", session_id, role, content);
    (void)memu_memorize(payload, "conversation", session_id);
}

static void supervisor_tick(struct mikroclaw_ctx *ctx) {
    if (!ctx) {
        return;
    }

#ifdef CHANNEL_TELEGRAM
    if (ctx->telegram) {
        if (telegram_health_check(ctx->telegram)) {
            channel_supervisor_record_success(&ctx->supervisor.telegram);
        } else {
            channel_supervisor_record_failure(&ctx->supervisor.telegram);
        }
    }
#endif

#ifdef CHANNEL_DISCORD
    if (ctx->discord) {
        if (discord_health_check(ctx->discord)) {
            channel_supervisor_record_success(&ctx->supervisor.discord);
        } else if (channel_supervisor_should_retry(&ctx->supervisor.discord)) {
            const char *url = getenv("DISCORD_WEBHOOK_URL");
            struct discord_config cfg;
            memset(&cfg, 0, sizeof(cfg));
            if (url && url[0]) {
                snprintf(cfg.webhook_url, sizeof(cfg.webhook_url), "%s", url);
                discord_destroy(ctx->discord);
                ctx->discord = discord_init(&cfg);
                if (ctx->discord) {
                    channel_supervisor_record_success(&ctx->supervisor.discord);
                } else {
                    channel_supervisor_record_failure(&ctx->supervisor.discord);
                }
            }
        }
    }
#endif

#ifdef CHANNEL_SLACK
    if (ctx->slack) {
        if (slack_health_check(ctx->slack)) {
            channel_supervisor_record_success(&ctx->supervisor.slack);
        } else if (channel_supervisor_should_retry(&ctx->supervisor.slack)) {
            const char *url = getenv("SLACK_WEBHOOK_URL");
            struct slack_config cfg;
            memset(&cfg, 0, sizeof(cfg));
            if (url && url[0]) {
                snprintf(cfg.webhook_url, sizeof(cfg.webhook_url), "%s", url);
                slack_destroy(ctx->slack);
                ctx->slack = slack_init(&cfg);
                if (ctx->slack) {
                    channel_supervisor_record_success(&ctx->supervisor.slack);
                } else {
                    channel_supervisor_record_failure(&ctx->supervisor.slack);
                }
            }
        }
    }
#endif
}

int mikroclaw_run(struct mikroclaw_ctx *ctx) {
    char llm_response[LLM_MAX_RESPONSE] = {0};
    int ret;
    
    /* Check Telegram for messages */
#ifdef CHANNEL_TELEGRAM
    if (ctx->telegram) {
        struct telegram_message msg = {0};
        ret = telegram_poll(ctx->telegram, &msg);
        if (ret == 1) {
            printf("Telegram: %s\n", msg.text);

            {
                char session_id[64];
                build_session_id(session_id, sizeof(session_id), msg.chat_id);
                memu_store_turn(session_id, "user", msg.text);
            }

            if (strncmp(msg.text, "/fn ", 4) == 0) {
                const char *payload = msg.text + 4;
                const char *space = strchr(payload, ' ');
                char fn_name[64];
                char fn_args[1024];
                char fn_result[2048];
                if (!space) {
                    send_reply(ctx, REPLY_TELEGRAM, msg.chat_id, -1,
                               "Usage: /fn <name> <json>");
                    return MC_OK;
                }

                size_t fn_len = (size_t)(space - payload);
                if (fn_len >= sizeof(fn_name)) {
                    send_reply(ctx, REPLY_TELEGRAM, msg.chat_id, -1,
                               "Function name too long");
                    return MC_OK;
                }
                memcpy(fn_name, payload, fn_len);
                fn_name[fn_len] = '\0';
                snprintf(fn_args, sizeof(fn_args), "%s", space + 1);

                if (function_call(fn_name, fn_args, fn_result, sizeof(fn_result)) == 0) {
                    send_reply(ctx, REPLY_TELEGRAM, msg.chat_id, -1, fn_result);
                    {
                        char session_id[64];
                        build_session_id(session_id, sizeof(session_id), msg.chat_id);
                        memu_store_turn(session_id, "assistant", fn_result);
                    }
                } else {
                    send_reply(ctx, REPLY_TELEGRAM, msg.chat_id, -1, fn_result);
                }
                return MC_OK;
            }
            
            /* Query LLM */
            const char *system_prompt = 
                "You are MikroClaw, an AI assistant running on a MikroTik router. "
                "Respond with valid RouterOS commands when appropriate, "
                "or helpful explanations. "
                "Keep responses concise. "
                "Format: Start with ### if providing RouterOS commands to execute.";
            
            ret = llm_chat_reliable(ctx->llm, system_prompt, msg.text,
                          llm_response, sizeof(llm_response));
            if (ret == 0) {
                /* Check if response contains RouterOS commands */
                char *cmds = strstr(llm_response, "###");
                if (cmds) {
                    /* Execute commands on RouterOS */
                    cmds += 3;
                    while (*cmds == ' ' || *cmds == '\n') cmds++;
                    
                    char result[4096];
                    ret = routeros_execute(ctx->ros, cmds, result, sizeof(result));
                    
                    /* Send result back */
                    char reply[TELEGRAM_MAX_MESSAGE];
                    if (ret == 0) {
                        const char *prefix = "Executed:\n```\n";
                        const char *middle = "\n```\nResult:\n";
                        size_t cmds_len = strlen(cmds);
                        size_t result_len = strlen(result);
                        size_t needed = strlen(prefix) + cmds_len + strlen(middle) + result_len + 1;
                        if (needed > sizeof(reply)) {
                            snprintf(reply, sizeof(reply), "Output truncated.\nResult:\n%.1000s", result);
                        } else {
                            size_t pos = 0;
                            memcpy(reply + pos, prefix, strlen(prefix));
                            pos += strlen(prefix);
                            memcpy(reply + pos, cmds, cmds_len);
                            pos += cmds_len;
                            memcpy(reply + pos, middle, strlen(middle));
                            pos += strlen(middle);
                            memcpy(reply + pos, result, result_len);
                            pos += result_len;
                            reply[pos] = '\0';
                        }
                    } else {
                        snprintf(reply, sizeof(reply),
                            "Failed to execute:\n```\n%s\n```",
                            cmds);
                    }
                    send_reply(ctx, REPLY_TELEGRAM, msg.chat_id, -1, reply);
                } else {
                    /* Just send text response */
                    send_reply(ctx, REPLY_TELEGRAM, msg.chat_id, -1, llm_response);
                }
                {
                    char session_id[64];
                    build_session_id(session_id, sizeof(session_id), msg.chat_id);
                    memu_store_turn(session_id, "assistant", llm_response);
                }
            } else {
                send_reply(ctx, REPLY_TELEGRAM, msg.chat_id, -1,
                           "Error querying LLM. Check configuration.");
            }
        }
    }
#endif
    
    /* Check Gateway for requests */
#ifdef ENABLE_GATEWAY
    if (ctx->gateway) {
        char message[4096] = {0};
        char client_ip[64] = {0};
#if defined(CHANNEL_SLACK) || defined(CHANNEL_DISCORD)
        char inbound_text[2048] = {0};
#endif
        enum reply_target gateway_target = REPLY_GATEWAY;
        int client_fd = -1;
        if (ctx->subagent) {
            subagent_poll(ctx->subagent);
        }

        ret = gateway_poll(ctx->gateway, message, sizeof(message), &client_fd, 1000,
                           client_ip, sizeof(client_ip));
        if (ret == 1 && message[0]) {
            char method[16];
            char path[256];
            const char *system_prompt =
                "You are MikroClaw, an AI assistant running on a MikroTik router. "
                "Respond with valid RouterOS commands when appropriate, "
                "or helpful explanations. "
                "Keep responses concise. "
                "Format: Start with ### if providing RouterOS commands to execute.";
            const char *gateway_prompt = http_body_from_request(message);

            if (parse_request_line(message, method, sizeof(method), path, sizeof(path)) != 0) {
                send_reply(ctx, REPLY_GATEWAY, NULL, client_fd, "bad request");
                return MC_OK;
            }

            if (strcmp(method, "GET") == 0 && strcmp(path, "/health") == 0) {
                char body[256];
                char response[512];
                snprintf(body, sizeof(body),
                         "{\"status\":\"ok\",\"components\":{\"llm\":%s,\"gateway\":true,\"routeros\":%s,\"memu\":true}}",
                         ctx->llm ? "true" : "false",
                         ctx->ros ? "true" : "false");
                build_http_json_response(200, "OK", body, response, sizeof(response));
                gateway_respond(client_fd, response);
                return MC_OK;
            }

            if (strcmp(method, "GET") == 0 && strcmp(path, "/health/heartbeat") == 0) {
                char response[512];
                build_http_json_response(200, "OK", "{\"heartbeat\":\"ok\"}", response, sizeof(response));
                gateway_respond(client_fd, response);
                return MC_OK;
            }

            if (ctx->rate_limit && !rate_limit_allow_request(ctx->rate_limit, client_ip)) {
                char response[256];
                build_http_json_response(429, "Too Many Requests",
                                         "{\"error\":\"rate limit exceeded\"}",
                                         response, sizeof(response));
                gateway_respond(client_fd, response);
                return MC_OK;
            }

            if (ctx->rate_limit) {
                int retry_after = 0;
                if (rate_limit_is_locked(ctx->rate_limit, client_ip, &retry_after)) {
                    char body[256];
                    char response[512];
                    snprintf(body, sizeof(body),
                             "{\"error\":\"auth locked\",\"retry_after\":%d}",
                             retry_after);
                    build_http_json_response(429, "Too Many Requests", body,
                                             response, sizeof(response));
                    gateway_respond(client_fd, response);
                    return MC_OK;
                }
            }

            if (strcmp(method, "POST") == 0 && strcmp(path, "/pair") == 0) {
                char pairing_code[64];
                char token[128];
                char body[256];
                char response[512];
                if (!ctx->gateway_auth ||
                    gateway_auth_extract_header(message, "X-Pairing-Code", pairing_code, sizeof(pairing_code)) != 0 ||
                    gateway_auth_exchange_pairing_code(ctx->gateway_auth, pairing_code, token, sizeof(token)) != 0) {
                    if (ctx->rate_limit) {
                        rate_limit_record_auth_failure(ctx->rate_limit, client_ip);
                    }
                    build_http_json_response(403, "Forbidden", "{\"error\":\"invalid pairing code\"}",
                                             response, sizeof(response));
                    gateway_respond(client_fd, response);
                    return MC_OK;
                }
                if (ctx->rate_limit) {
                    rate_limit_record_auth_success(ctx->rate_limit, client_ip);
                }
                snprintf(body, sizeof(body), "{\"paired\":true,\"token\":\"%s\"}", token);
                build_http_json_response(200, "OK", body, response, sizeof(response));
                gateway_respond(client_fd, response);
                return MC_OK;
            }

            if (ctx->subagent && strcmp(method, "POST") == 0 && strcmp(path, "/tasks") == 0) {
                char type[64];
                char task_id[64];
                char response[512];
                const char *body = http_body_from_request(message);
                if (extract_json_string(body, "type", type, sizeof(type)) != 0) {
                    build_http_json_response(400, "Bad Request", "{\"error\":\"missing type\"}",
                                             response, sizeof(response));
                    gateway_respond(client_fd, response);
                    return MC_OK;
                }
                if (subagent_submit(ctx->subagent, type, body ? body : "{}", task_id, sizeof(task_id)) != 0) {
                    build_http_json_response(503, "Service Unavailable", "{\"error\":\"queue full\"}",
                                             response, sizeof(response));
                    gateway_respond(client_fd, response);
                    return MC_OK;
                }
                snprintf(response, sizeof(response),
                         "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"task_id\":\"%s\",\"status\":\"queued\"}",
                         task_id);
                gateway_respond(client_fd, response);
                return MC_OK;
            }

            if (ctx->subagent && strcmp(method, "GET") == 0 && strcmp(path, "/tasks") == 0) {
                char body[4096];
                char response[4608];
                if (subagent_list_json(ctx->subagent, body, sizeof(body)) != 0) {
                    build_http_json_response(500, "Internal Server Error", "{\"error\":\"list failed\"}",
                                             response, sizeof(response));
                    gateway_respond(client_fd, response);
                    return MC_OK;
                }
                build_http_json_response(200, "OK", body, response, sizeof(response));
                gateway_respond(client_fd, response);
                return MC_OK;
            }

            if (ctx->subagent && strncmp(path, "/tasks/", 7) == 0) {
                const char *task_id = path + 7;
                char body[4096];
                char response[4608];

                if (strcmp(method, "DELETE") == 0) {
                    if (subagent_cancel(ctx->subagent, task_id) != 0) {
                        build_http_json_response(404, "Not Found", "{\"error\":\"task not found\"}",
                                                 response, sizeof(response));
                    } else {
                        build_http_json_response(200, "OK", "{\"status\":\"cancelled\"}",
                                                 response, sizeof(response));
                    }
                    gateway_respond(client_fd, response);
                    return MC_OK;
                }

                if (strcmp(method, "GET") == 0) {
                    if (subagent_get_json(ctx->subagent, task_id, body, sizeof(body)) != 0) {
                        build_http_json_response(404, "Not Found", "{\"error\":\"task not found\"}",
                                                 response, sizeof(response));
                    } else {
                        build_http_json_response(200, "OK", body, response, sizeof(response));
                    }
                    gateway_respond(client_fd, response);
                    return MC_OK;
                }
            }

            {
                const char *pairing_required = getenv("PAIRING_REQUIRED");
                if (pairing_required && strcmp(pairing_required, "1") == 0) {
                    char bearer[128];
                    char response[256];
                    if (strcmp(path, "/health") != 0 && strcmp(path, "/pair") != 0) {
                        if (!ctx->gateway_auth ||
                            gateway_auth_extract_bearer(message, bearer, sizeof(bearer)) != 0 ||
                            gateway_auth_validate_token(ctx->gateway_auth, bearer) != 1) {
                            if (ctx->rate_limit) {
                                rate_limit_record_auth_failure(ctx->rate_limit, client_ip);
                            }
                            build_http_json_response(401, "Unauthorized", "{\"error\":\"unauthorized\"}",
                                                     response, sizeof(response));
                            gateway_respond(client_fd, response);
                            return MC_OK;
                        }
                        if (ctx->rate_limit) {
                            rate_limit_record_auth_success(ctx->rate_limit, client_ip);
                        }
                    }
                }
            }

#ifdef CHANNEL_SLACK
            if (ctx->slack && slack_parse_inbound(message, inbound_text, sizeof(inbound_text)) == 1) {
                gateway_target = REPLY_SLACK;
                gateway_prompt = inbound_text;
            }
#endif
#ifdef CHANNEL_DISCORD
            if (ctx->discord && discord_parse_inbound(message, inbound_text, sizeof(inbound_text)) == 1) {
                gateway_target = REPLY_DISCORD;
                gateway_prompt = inbound_text;
            }
#endif

            if (!gateway_prompt || gateway_prompt[0] == '\0') {
                send_reply(ctx, REPLY_GATEWAY, NULL, client_fd, "empty request");
                return MC_OK;
            }

            printf("Gateway: %s\n", gateway_prompt);

            ret = llm_chat_reliable(ctx->llm, system_prompt, gateway_prompt,
                           llm_response, sizeof(llm_response));
            if (ret != 0) {
                send_reply(ctx, gateway_target, NULL, client_fd,
                           "Error querying LLM. Check configuration.");
                return MC_OK;
            }

            {
                char *cmds = strstr(llm_response, "###");
                if (cmds) {
                    char result[4096];
                    char reply[4096];
                    cmds += 3;
                    while (*cmds == ' ' || *cmds == '\n') {
                        cmds++;
                    }
                    ret = routeros_execute(ctx->ros, cmds, result, sizeof(result));
                    if (ret == 0) {
                        const char *prefix = "Executed:\n```\n";
                        const char *middle = "\n```\nResult:\n";
                        size_t cmds_len = strlen(cmds);
                        size_t result_len = strlen(result);
                        size_t needed = strlen(prefix) + cmds_len + strlen(middle) + result_len + 1;
                        if (needed > sizeof(reply)) {
                            snprintf(reply, sizeof(reply), "Output truncated.\nResult:\n%.1000s", result);
                        } else {
                            size_t pos = 0;
                            memcpy(reply + pos, prefix, strlen(prefix));
                            pos += strlen(prefix);
                            memcpy(reply + pos, cmds, cmds_len);
                            pos += cmds_len;
                            memcpy(reply + pos, middle, strlen(middle));
                            pos += strlen(middle);
                            memcpy(reply + pos, result, result_len);
                            pos += result_len;
                            reply[pos] = '\0';
                        }
                    } else {
                        snprintf(reply, sizeof(reply),
                                 "Failed to execute:\n```\n%s\n```", cmds);
                    }
                    send_reply(ctx, gateway_target, NULL, client_fd, reply);
                } else {
                    send_reply(ctx, gateway_target, NULL, client_fd, llm_response);
                }
            }
        }
    }
#endif
    
    supervisor_tick(ctx);
    return MC_OK;
}
