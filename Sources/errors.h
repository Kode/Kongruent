#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct debug_context {
	const char *filename;
	uint32_t column;
	uint32_t line;
} debug_context;

void error(debug_context context, const char *message, ...);
void error_args(debug_context context, const char *message, va_list args);
void check(bool check, debug_context context, const char *message, ...);
void check_args(bool check, debug_context context, const char *message, va_list args);
