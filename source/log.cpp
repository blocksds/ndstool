#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"

void LogMessage(log_level_t level, const char *msg, ...)
{
    if (level == LOG_LEVEL_WARNING)
        printf("WARNING: ");
    else if (level == LOG_LEVEL_FATAL)
        printf("FATAL: ");

    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);

    if (level == LOG_LEVEL_FATAL)
        exit(EXIT_FAILURE);
}
