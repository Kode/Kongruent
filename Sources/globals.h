#pragma once

#include "names.h"
#include "types.h"

typedef uint32_t global_id;

typedef struct global {
	name_id name;
	type_id type;
	uint64_t var_index;
	float value;
} global;

void globals_init(void);

global_id add_global(type_id type, name_id name);
global_id add_global_with_value(type_id type, name_id name, float value);

global find_global(name_id name);

global get_global(global_id id);

void assign_global_var(global_id id, uint64_t var_index);
