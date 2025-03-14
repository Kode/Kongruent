#include "errors.h"

#include "log.h"

#include <stdlib.h>
#include <string.h>

void error_args(debug_context context, const char *message, va_list args) {
	char buffer[4096];
	int  offset;
	if (context.filename != NULL) {
		offset = sprintf(buffer, "In column %i at line %i in %s: ", context.column + 1, context.line + 1, context.filename);
	}
	else {
		offset = sprintf(buffer, "In column %i at line %i: ", context.column + 1, context.line + 1);
	}
	vsnprintf(&buffer[offset], 4090, message, args);

	kong_log_args(LOG_LEVEL_ERROR, buffer, args);

	exit(1);
}

void error(debug_context context, const char *message, ...) {
	va_list args;
	va_start(args, message);
	error_args(context, message, args);
	va_end(args);
}

void check_args(bool test, debug_context context, const char *message, va_list args) {
	if (!test) {
		error_args(context, message, args);
	}
}

void check_function(bool test, debug_context context, const char *message, ...) {
	if (!test) {
		va_list args;
		va_start(args, message);
		error_args(context, message, args);
		va_end(args);
	}
}
