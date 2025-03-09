#pragma once

#include "compiler.h"
#include "names.h"
#include "types.h"

#define NO_FUNCTION 0xFFFFFFFF

typedef uint32_t function_id;

struct statement;

typedef struct function {
	attribute_list attributes;
	name_id name;
	type_ref return_type;
	name_id parameter_names[256];
	type_ref parameter_types[256];
	name_id parameter_attributes[256];
	uint8_t parameters_size;
	struct statement *block;

	uint32_t descriptor_set_group_index;

	opcodes code;
} function;

void functions_init(void);

function_id add_function(name_id name);

function_id find_function(name_id name);

function *get_function(function_id function);
