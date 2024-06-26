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

	add_global_with_value(float_id, add_name("BLEND_ONE"), 0);
	add_global_with_value(float_id, add_name("BLEND_ZERO"), 1);
	add_global_with_value(float_id, add_name("BLEND_SOURCE_ALPHA"), 2);
	add_global_with_value(float_id, add_name("BLEND_DEST_ALPHA"), 3);
	add_global_with_value(float_id, add_name("BLEND_INV_SOURCE_ALPHA"), 4);
	add_global_with_value(float_id, add_name("BLEND_INV_DEST_ALPHA"), 5);
	add_global_with_value(float_id, add_name("BLEND_SOURCE_COLOR"), 6);
	add_global_with_value(float_id, add_name("BLEND_DEST_COLOR"), 7);
	add_global_with_value(float_id, add_name("BLEND_INV_SOURCE_COLOR"), 8);
	add_global_with_value(float_id, add_name("BLEND_INV_DEST_COLOR"), 9);
	add_global_with_value(float_id, add_name("BLEND_CONSTANT"), 10);
	add_global_with_value(float_id, add_name("BLEND_INV_CONSTANT"), 11);

	add_global_with_value(float_id, add_name("BLENDOP_ADD"), 0);
	add_global_with_value(float_id, add_name("BLENDOP_SUBTRACT"), 1);
	add_global_with_value(float_id, add_name("BLENDOP_REVERSE_SUBTRACT"), 2);
	add_global_with_value(float_id, add_name("BLENDOP_MIN"), 3);
	add_global_with_value(float_id, add_name("BLENDOP_MAX"), 4);
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
	debug_context context = {0};
	check(id < globals_size, context, "Encountered a global with a weird id");
	globals[id].var_index = var_index;
}
