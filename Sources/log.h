#pragma once

#include <stdarg.h>

typedef enum { LOG_LEVEL_INFO, LOG_LEVEL_WARNING, LOG_LEVEL_ERROR } log_level_t;

void log(log_level_t log_level, const char *format, ...);

void log_args(log_level_t log_level, const char *format, va_list args);
