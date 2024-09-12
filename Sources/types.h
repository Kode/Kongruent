#pragma once

#include "names.h"
#include "tokenizer.h"

#include <stdbool.h>
#include <stddef.h>

#define NO_TYPE 0xFFFFFFFF

typedef uint32_t type_id;

typedef struct type_ref {
	name_id name;
	type_id type;
	uint32_t array_size;
} type_ref;

void init_type_ref(type_ref *t, name_id name);

typedef struct member {
	name_id name;
	type_ref type;
	token value;
} member;

#define MAX_MEMBERS 1024

typedef struct members {
	member m[MAX_MEMBERS];
	size_t size;
} members;

typedef struct attribute {
	name_id name;
	double parameters[16];
	uint8_t paramters_count;
} attribute;

typedef struct attributes {
	attribute attributes[64];
	uint8_t attributes_count;
} attribute_list;

static bool has_attribute(attribute_list *attributes, name_id name) {
	for (uint8_t index = 0; index < attributes->attributes_count; ++index) {
		if (attributes->attributes[index].name == name) {
			return true;
		}
	}
	return false;
}

static attribute *find_attribute(attribute_list *attributes, name_id name) {
	for (uint8_t index = 0; index < attributes->attributes_count; ++index) {
		if (attributes->attributes[index].name == name) {
			return &attributes->attributes[index];
		}
	}
	return NULL;
}

typedef struct type {
	attribute_list attributes;
	name_id name;
	members members;
	bool built_in;
} type;

void types_init(void);

type_id add_type(name_id name);

type_id find_type_by_name(name_id name);

type_id find_type_by_ref(type_ref *t);

type *get_type(type_id t);

extern type_id void_id;
extern type_id float_id;
extern type_id float2_id;
extern type_id float3_id;
extern type_id float4_id;
extern type_id float3x3_id;
extern type_id float4x4_id;
extern type_id int_id;
extern type_id int2_id;
extern type_id int3_id;
extern type_id int4_id;
extern type_id uint_id;
extern type_id uint2_id;
extern type_id uint3_id;
extern type_id uint4_id;
extern type_id bool_id;
extern type_id tex2d_type_id;
extern type_id texcube_type_id;
extern type_id sampler_type_id;
extern type_id ray_type_id;
extern type_id bvh_type_id;
