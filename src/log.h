#ifndef MIKROCLAW_LOG_H
#define MIKROCLAW_LOG_H

enum log_level {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_DEBUG = 3
};

void log_set_level_from_env(void);
void log_emit(enum log_level level, const char *component, const char *message);

#endif
