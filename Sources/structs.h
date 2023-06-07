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

#define NO_STRUCT 0xFFFFFFFF

typedef uint32_t struct_id;

typedef struct structy {
	name_id attribute;
	name_id name;
	members members;
} structy;

void structs_init(void);

struct_id add_struct(name_id name);

struct_id find_struct(name_id name);

structy *get_struct(struct_id s);
