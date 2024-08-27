#include "globals.h"

#include "errors.h"

#include <assert.h>

static global globals[1024];
static global_id globals_size = 0;

void globals_init(void) {
	global_value int_value;
	int_value.kind = GLOBAL_VALUE_INT;

	attribute_list attributes = {0};

	int_value.value.ints[0] = 0;
	add_global_with_value(float_id, attributes, add_name("COMPARE_ALWAYS"), int_value);
	int_value.value.ints[0] = 1;
	add_global_with_value(float_id, attributes, add_name("COMPARE_NEVER"), int_value);
	int_value.value.ints[0] = 2;
	add_global_with_value(float_id, attributes, add_name("COMPARE_EQUAL"), int_value);
	int_value.value.ints[0] = 3;
	add_global_with_value(float_id, attributes, add_name("COMPARE_NOT_EQUAL"), int_value);
	int_value.value.ints[0] = 4;
	add_global_with_value(float_id, attributes, add_name("COMPARE_LESS"), int_value);
	int_value.value.ints[0] = 5;
	add_global_with_value(float_id, attributes, add_name("COMPARE_LESS_EQUAL"), int_value);
	int_value.value.ints[0] = 6;
	add_global_with_value(float_id, attributes, add_name("COMPARE_GREATER"), int_value);
	int_value.value.ints[0] = 7;
	add_global_with_value(float_id, attributes, add_name("COMPARE_GREATER_EQUAL"), int_value);

	int_value.value.ints[0] = 0;
	add_global_with_value(float_id, attributes, add_name("BLEND_ONE"), int_value);
	int_value.value.ints[0] = 1;
	add_global_with_value(float_id, attributes, add_name("BLEND_ZERO"), int_value);
	int_value.value.ints[0] = 2;
	add_global_with_value(float_id, attributes, add_name("BLEND_SOURCE_ALPHA"), int_value);
	int_value.value.ints[0] = 3;
	add_global_with_value(float_id, attributes, add_name("BLEND_DEST_ALPHA"), int_value);
	int_value.value.ints[0] = 4;
	add_global_with_value(float_id, attributes, add_name("BLEND_INV_SOURCE_ALPHA"), int_value);
	int_value.value.ints[0] = 5;
	add_global_with_value(float_id, attributes, add_name("BLEND_INV_DEST_ALPHA"), int_value);
	int_value.value.ints[0] = 6;
	add_global_with_value(float_id, attributes, add_name("BLEND_SOURCE_COLOR"), int_value);
	int_value.value.ints[0] = 7;
	add_global_with_value(float_id, attributes, add_name("BLEND_DEST_COLOR"), int_value);
	int_value.value.ints[0] = 8;
	add_global_with_value(float_id, attributes, add_name("BLEND_INV_SOURCE_COLOR"), int_value);
	int_value.value.ints[0] = 9;
	add_global_with_value(float_id, attributes, add_name("BLEND_INV_DEST_COLOR"), int_value);
	int_value.value.ints[0] = 10;
	add_global_with_value(float_id, attributes, add_name("BLEND_CONSTANT"), int_value);
	int_value.value.ints[0] = 11;
	add_global_with_value(float_id, attributes, add_name("BLEND_INV_CONSTANT"), int_value);

	int_value.value.ints[0] = 0;
	add_global_with_value(float_id, attributes, add_name("BLENDOP_ADD"), int_value);
	int_value.value.ints[0] = 1;
	add_global_with_value(float_id, attributes, add_name("BLENDOP_SUBTRACT"), int_value);
	int_value.value.ints[0] = 2;
	add_global_with_value(float_id, attributes, add_name("BLENDOP_REVERSE_SUBTRACT"), int_value);
	int_value.value.ints[0] = 3;
	add_global_with_value(float_id, attributes, add_name("BLENDOP_MIN"), int_value);
	int_value.value.ints[0] = 4;
	add_global_with_value(float_id, attributes, add_name("BLENDOP_MAX"), int_value);
}

global_id add_global(type_id type, attribute_list attributes, name_id name) {
	uint32_t index = globals_size;
	globals[index].name = name;
	globals[index].type = type;
	globals[index].var_index = 0;
	globals[index].value.kind = GLOBAL_VALUE_NONE;
	globals[index].attributes = attributes;
	globals_size += 1;
	return index;
}

global_id add_global_with_value(type_id type, attribute_list attributes, name_id name, global_value value) {
	uint32_t index = globals_size;
	globals[index].name = name;
	globals[index].type = type;
	globals[index].var_index = 0;
	globals[index].value = value;
	globals[index].attributes = attributes;
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
