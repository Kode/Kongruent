#pragma once

#include "names.h"
#include "types.h"

#define NO_FUNCTION 0xFFFFFFFF

typedef uint32_t function_id;

struct statement;

typedef struct function {
	name_id attribute;
	name_id name;
	type_ref return_type;
	name_id parameter_name;
	type_ref parameter_type;
	struct statement *block;
} function;

void functions_init(void);

function_id add_function(name_id name);

function_id find_function(name_id name);

function *get_function(function_id function);
