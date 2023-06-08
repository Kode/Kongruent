#pragma once

#include "names.h"

#include <stdbool.h>

#define NO_STRUCT 0xFFFFFFFF

typedef uint32_t struct_id;

typedef struct type_ref {
	bool resolved;

	union {
		name_id name;
		struct_id type;
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

typedef struct structy {
	name_id attribute;
	name_id name;
	members members;
} structy;

void structs_init(void);

struct_id add_struct(name_id name);

struct_id find_struct(name_id name);

structy *get_struct(struct_id s);

extern struct_id f32_id;
extern struct_id vec2_id;
extern struct_id vec3_id;
extern struct_id vec4_id;
extern struct_id bool_id;
