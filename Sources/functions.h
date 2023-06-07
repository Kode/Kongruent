#pragma once

#include "names.h"

#define NO_FUNCTION 0xFFFFFFFF

typedef uint32_t function_id;

struct statement;

typedef struct function {
	name_id attribute;
	name_id name;
	name_id return_type_name;
	name_id parameter_name;
	name_id parameter_type_name;
	struct statement *block;
} function;

void functions_init(void);

function_id add_function(name_id name);

function_id find_function(name_id name);

function *get_function(function_id function);
