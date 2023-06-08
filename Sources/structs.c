#include "structs.h"

#include <assert.h>
#include <stdlib.h>

static structy *structs = NULL;
static struct_id structs_size = 1024;
static struct_id next_struct_index = 0;

struct_id f32_id;
struct_id vec2_id;
struct_id vec3_id;
struct_id vec4_id;
struct_id bool_id;

void structs_init(void) {
	structy *new_structs = realloc(structs, structs_size * sizeof(structy));
	assert(new_structs != NULL);
	structs = new_structs;
	next_struct_index = 0;

	f32_id = add_struct(add_name("f32"));
	vec2_id = add_struct(add_name("vec2"));
	vec3_id = add_struct(add_name("vec3"));
	vec4_id = add_struct(add_name("vec4"));
	bool_id = add_struct(add_name("bool"));
}

static void grow_if_needed(uint64_t size) {
	while (size >= structs_size) {
		structs_size *= 2;
		structy *new_structs = realloc(structs, structs_size * sizeof(structy));
		assert(new_structs != NULL);
		structs = new_structs;
	}
}

struct_id add_struct(name_id name) {
	grow_if_needed(next_struct_index + 1);

	struct_id s = next_struct_index;
	++next_struct_index;

	structs[s].name = name;
	structs[s].attribute = NO_NAME;
	structs[s].members.size = 0;

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
