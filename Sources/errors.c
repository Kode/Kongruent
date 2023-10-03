#include "errors.h"

#include "log.h"

#include <stdlib.h>
#include <string.h>

void error_args(int column, int line, const char *message, va_list args) {
	char buffer[4096];
	int offset = sprintf(buffer, "In column %i at line %i: ", column + 1, line + 1);
	vsnprintf(&buffer[offset], 4090, message, args);

	kong_log_args(LOG_LEVEL_ERROR, buffer, args);

	exit(1);
}

void error(int column, int line, const char *message, ...) {
	va_list args;
	va_start(args, message);
	error_args(column, line, message, args);
	va_end(args);
}

void check_args(bool check, int column, int line, const char *message, va_list args) {
	if (!check) {
		error_args(column, line, message, args);
	}
}

void check(bool check, int column, int line, const char *message, ...) {
	if (!check) {
		va_list args;
		va_start(args, message);
		error_args(column, line, message, args);
		va_end(args);
	}
}
