#pragma once

#include "names.h"

#include <stdbool.h>

#define NO_TYPE 0xFFFFFFFF

typedef uint32_t type_id;

typedef struct type_ref {
	bool resolved;

	union {
		name_id name;
		type_id type;
	};
} type_ref;

typedef struct member {
	name_id name;
	type_ref type;
} member;

#define MAX_MEMBERS 256

typedef struct members {
	member m[MAX_MEMBERS];
	size_t size;
} members;

typedef struct type {
	name_id attribute;
	name_id name;
	members members;
} type;

void types_init(void);

type_id add_type(name_id name);

type_id find_type(name_id name);

type *get_type(type_id t);

extern type_id f32_id;
extern type_id vec2_id;
extern type_id vec3_id;
extern type_id vec4_id;
extern type_id bool_id;
