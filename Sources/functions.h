#pragma once

#include "names.h"

struct statement;

typedef struct function {
	name_id attribute;
	name_id name;
	name_id return_type_name;
	name_id parameter_name;
	name_id parameter_type_name;
	struct statement *block;
} function;

typedef struct functions {
	function *f;
	size_t size;
} functions;

extern struct functions all_functions;

void functions_init(void);

function *add_function(void);
