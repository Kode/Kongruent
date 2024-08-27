#pragma once

#include "names.h"
#include "types.h"

typedef uint32_t global_id;

typedef struct global_value {
	enum {
		GLOBAL_VALUE_FLOAT,
		GLOBAL_VALUE_FLOAT2,
		GLOBAL_VALUE_FLOAT3,
		GLOBAL_VALUE_FLOAT4,
		GLOBAL_VALUE_INT,
		GLOBAL_VALUE_INT2,
		GLOBAL_VALUE_INT3,
		GLOBAL_VALUE_INT4,
		GLOBAL_VALUE_BOOL,
		GLOBAL_VALUE_NONE
	} kind;
	union {
		float floats[4];
		int ints[4];
		bool b;
	} value;
} global_value;

typedef struct global {
	name_id name;
	type_id type;
	uint64_t var_index;
	global_value value;
	attribute_list attributes;
} global;

void globals_init(void);

global_id add_global(type_id type, attribute_list attributes, name_id name);
global_id add_global_with_value(type_id type, attribute_list attributes, name_id name, global_value value);

global find_global(name_id name);

global get_global(global_id id);

void assign_global_var(global_id id, uint64_t var_index);
