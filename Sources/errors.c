#include "errors.h"

#include <stdio.h>

#ifdef WIN32
#include <Windows.h>
#endif

void error(const char *message) {
#ifdef WIN32
	OutputDebugStringA(message);
	OutputDebugStringA("\n");
#endif
	printf("%s\n", message);
	exit(1);
}
