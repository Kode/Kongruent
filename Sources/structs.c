#include "structs.h"

#include <assert.h>
#include <stdlib.h>

structy *structs = NULL;
static struct_id structs_size = 1024;
static struct_id next_struct_index = 0;

void structs_init(void) {
	structy *new_structs = realloc(structs, structs_size * sizeof(structy));
	assert(new_structs != NULL);
	structs = new_structs;
	next_struct_index = 0;
}

static void grow_if_needed(uint64_t size) {
	while (size >= structs_size) {
		structs_size *= 2;
		structy *new_structs = realloc(structs, structs_size * sizeof(structy));
		assert(new_structs != NULL);
		structs = new_structs;
	}
}

struct_id add_struct(void) {
	grow_if_needed(next_struct_index + 1);

	struct_id s = next_struct_index;
	++next_struct_index;
	return s;
}

struct_id find_struct(name_id name) {
	for (struct_id i = 0; i < next_struct_index; ++i) {
		if (structs[i].name == name) {
			return i;
		}
	}

	return NO_STRUCT;
}

structy *get_struct(struct_id s) {
	if (s >= next_struct_index) {
		return NULL;
	}
	return &structs[s];
}
