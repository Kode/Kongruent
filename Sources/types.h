#pragma once

#include "names.h"

#include <stdbool.h>

#define NO_TYPE 0xFFFFFFFF

typedef uint32_t type_id;

typedef struct type_ref {
	name_id name;
	type_id type;
} type_ref;

typedef struct member {
	name_id name;
	type_ref type;
	name_id value;
} member;

#define MAX_MEMBERS 1024

typedef struct members {
	member m[MAX_MEMBERS];
	size_t size;
} members;

typedef struct type {
	name_id attribute;
	name_id name;
	members members;
	bool built_in;
} type;

void types_init(void);

type_id add_type(name_id name);

type_id find_type(name_id name);

type *get_type(type_id t);

extern type_id float_id;
extern type_id float2_id;
extern type_id float3_id;
extern type_id float4_id;
extern type_id bool_id;
