#include "functions.h"

#include <assert.h>
#include <stdlib.h>

struct functions all_functions = {0};

#define MAX_FUNCTIONS 1024 * 64

void functions_init(void) {
	free(all_functions.f);
	all_functions.f = (function *)malloc(MAX_FUNCTIONS * sizeof(function));
	all_functions.size = 0;
}

function *add_function(void) {
	assert(all_functions.size < MAX_FUNCTIONS);

	function *f = &all_functions.f[all_functions.size];
	all_functions.size += 1;
	return f;
}

function *get_function(name_id name) {
	for (size_t i = 0; i < all_functions.size; ++i) {
		if (all_functions.f[i].name == name) {
			return &all_functions.f[i];
		}
	}

	return NULL;
}
