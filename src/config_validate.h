#ifndef MIKROCLAW_CONFIG_VALIDATE_H
#define MIKROCLAW_CONFIG_VALIDATE_H

#include <stddef.h>

int config_validate_required(char *err_out, size_t err_len);
int config_dump_redacted(char *out, size_t out_len);

#endif
