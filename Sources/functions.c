#include "functions.h"

#include <assert.h>
#include <stdlib.h>

static function *functions = NULL;
static function_id functions_size = 1024;
static function_id next_function_index = 0;

void functions_init(void) {
	function *new_functions = realloc(functions, functions_size * sizeof(function));
	assert(new_functions != NULL);
	functions = new_functions;
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

function_id add_function(name_id name) {
	grow_if_needed(next_function_index + 1);

	function_id f = next_function_index;
	++next_function_index;

	functions[f].name = name;
	functions[f].attribute = NO_NAME;
	functions[f].return_type.resolved = false;
	functions[f].return_type.name = NO_NAME;
	functions[f].parameter_name = NO_NAME;
	functions[f].parameter_type.resolved = false;
	functions[f].parameter_type.name = NO_NAME;
	functions[f].block = NULL;

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
