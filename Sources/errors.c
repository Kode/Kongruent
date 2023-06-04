#include "errors.h"

#include "log.h"

#include <stdlib.h>

void error(const char *message, int column, int line) {
	kong_log(LOG_LEVEL_ERROR, "%s in column %i at line %i.\n", message, column + 1, line + 1);
	exit(1);
}
