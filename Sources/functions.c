#include "functions.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static function *functions = NULL;
static function_id functions_size = 1024;
static function_id next_function_index = 0;

function_id sample_id;

void functions_init(void) {
	function *new_functions = realloc(functions, functions_size * sizeof(function));
	assert(new_functions != NULL);
	functions = new_functions;
	next_function_index = 0;

	sample_id = add_function(add_name("sample"));

	function *f = get_function(sample_id);
	f->return_type.resolved = true;
	f->return_type.name = add_name("float4");
	f->parameter_name = add_name("tex_coord");
	f->parameter_type.resolved = false;
	f->parameter_type.name = add_name("float2");
	f->block = NULL;
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
	memset(functions[f].code.o, 0, sizeof(functions[f].code.o));
	functions[f].code.size = 0;

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
