#include "globals.h"

#include "errors.h"

#include <assert.h>

static global globals[1024];
static global_id globals_size = 0;

void globals_init(void) {
	add_global_with_value(float_id, add_name("COMPARE_ALWAYS"), 0);
	add_global_with_value(float_id, add_name("COMPARE_NEVER"), 1);
	add_global_with_value(float_id, add_name("COMPARE_EQUAL"), 2);
	add_global_with_value(float_id, add_name("COMPARE_NOT_EQUAL"), 3);
	add_global_with_value(float_id, add_name("COMPARE_LESS"), 4);
	add_global_with_value(float_id, add_name("COMPARE_LESS_EQUAL"), 5);
	add_global_with_value(float_id, add_name("COMPARE_GREATER"), 6);
	add_global_with_value(float_id, add_name("COMPARE_GREATER_EQUAL"), 7);
}

global_id add_global(type_id type, name_id name) {
	uint32_t index = globals_size;
	globals[index].name = name;
	globals[index].type = type;
	globals[index].var_index = 0;
	globals[index].value = 0;
	globals_size += 1;
	return index;
}

global_id add_global_with_value(type_id type, name_id name, float value) {
	uint32_t index = globals_size;
	globals[index].name = name;
	globals[index].type = type;
	globals[index].var_index = 0;
	globals[index].value = value;
	globals_size += 1;
	return index;
}

global find_global(name_id name) {
	for (uint32_t i = 0; i < globals_size; ++i) {
		if (globals[i].name == name) {
			return globals[i];
		}
	}

	global g;
	g.type = NO_TYPE;
	g.name = NO_NAME;
	return g;
}

global get_global(global_id id) {
	if (id >= globals_size) {
		global g;
		g.type = NO_TYPE;
		g.name = NO_NAME;
		return g;
	}

	return globals[id];
}

void assign_global_var(global_id id, uint64_t var_index) {
	check(id < globals_size, 0, 0, "Encountered a global with a weird id");
	globals[id].var_index = var_index;
}
