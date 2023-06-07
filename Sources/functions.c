#include "functions.h"

#include <assert.h>
#include <stdlib.h>

static function *functions = NULL;
static function_id functions_size = 1024;
static function_id next_function_index = 0;

void functions_init(void) {
	free(functions);
	functions = (function *)malloc(functions_size * sizeof(function));
	next_function_index = 0;
}

static void grow_if_needed(uint64_t size) {
	while (size >= functions_size) {
		functions_size *= 2;
		function *new_functions = realloc(functions, functions_size * sizeof(function));
		assert(new_functions != NULL);
		functions = new_functions;
	}
}

function_id add_function(void) {
	grow_if_needed(next_function_index + 1);

	function_id f = next_function_index;
	++next_function_index;
	return f;
}

function_id find_function(name_id name) {
	for (function_id i = 0; i < next_function_index; ++i) {
		if (functions[i].name == name) {
			return i;
		}
	}

	return NO_FUNCTION;
}

function *get_function(function_id function) {
	if (function >= next_function_index) {
		return NULL;
	}
	return &functions[function];
}
