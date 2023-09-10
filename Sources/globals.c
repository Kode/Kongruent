#include "globals.h"

#include <assert.h>

static global globals[1024];
static global_id globals_size = 0;

void globals_init(void) {}

global_id add_global(type_id type, name_id name) {
	uint32_t index = globals_size;
	globals[index].name = name;
	globals[index].type = type;
	globals[index].var_index = 0;
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
	assert(id < globals_size);
	globals[id].var_index = var_index;
}
