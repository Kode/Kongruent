#include "functions.h"

struct functions all_functions;

void functions_add(function *f) {
	all_functions.f[all_functions.size] = f;
	all_functions.size += 1;
}
