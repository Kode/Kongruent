#include "functions.h"

#include <stdlib.h>

struct functions all_functions = {0};

void functions_init(void) {
	size_t size = 1024;

	free(all_functions.f);
	all_functions.f = (function *)malloc(size * sizeof(function));
	all_functions.size = 0;
}

function *add_function(void) {
	function *f = &all_functions.f[all_functions.size];
	all_functions.size += 1;
	return f;
}
