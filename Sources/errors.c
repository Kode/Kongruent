#include "errors.h"

#include <stdio.h>

#ifdef WIN32
#include <Windows.h>
#endif

void error(const char *message, int column, int line) {
#ifdef WIN32
	char output[1024];
	sprintf(output, "%s in column %i at line %i.\n", message, column + 1, line + 1);
	OutputDebugStringA(output);
#endif
	printf("%s in column %i at line %i.\n", message, column + 1, line + 1);
	exit(1);
}
