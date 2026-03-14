
#pragma once

typedef enum {
    LOG_LEVEL_VERBOSE,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_FATAL,
} log_level_t;

void LogMessage(log_level_t level, const char *msg, ...);

#define LogVerbose(m, ...)  LogMessage(LOG_LEVEL_VERBOSE, m __VA_OPT__(,) __VA_ARGS__)
#define LogInfo(m, ...)     LogMessage(LOG_LEVEL_INFO, m __VA_OPT__(,) __VA_ARGS__)
#define LogWarning(m, ...)  LogMessage(LOG_LEVEL_WARNING, m __VA_OPT__(,) __VA_ARGS__)
#define LogFatal(m, ...)    LogMessage(LOG_LEVEL_FATAL, m __VA_OPT__(,) __VA_ARGS__)
