#pragma once

#include <stdarg.h>

typedef enum { LOG_LEVEL_INFO, LOG_LEVEL_WARNING, LOG_LEVEL_ERROR } log_level_t;

void kong_log(log_level_t log_level, const char *format, ...);

void kong_log_args(log_level_t log_level, const char *format, va_list args);
