#pragma once

#include "names.h"

typedef struct member {
	name_id name;
	name_id member_type;
} member;

#define MAX_MEMBERS 256

typedef struct members {
	member m[MAX_MEMBERS];
	size_t size;
} members;

typedef struct structy {
	name_id attribute;
	name_id name;
	members members;
} structy;

typedef struct definition {
	enum { DEFINITION_FUNCTION, DEFINITION_STRUCT } type;

	union {
		struct {
			name_id attribute;
			name_id name;
			name_id return_type_name;
			name_id parameter_name;
			name_id parameter_type_name;
			struct statement *block;
		} function;
		structy *structy;
	};
} definition;

typedef struct structs {
	structy *s;
	size_t size;
} structs;

extern struct structs all_structs;

void init_structs(void);

structy *add_struct(void);
