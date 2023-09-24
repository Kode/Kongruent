#include "types.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static type *types = NULL;
static type_id types_size = 1024;
static type_id next_type_index = 0;

type_id float_id;
type_id float2_id;
type_id float3_id;
type_id float4_id;
type_id float4x4_id;
type_id bool_id;
type_id function_type_id;
type_id tex2d_type_id;
type_id texcube_type_id;
type_id sampler_type_id;

typedef struct prefix {
	char str[5];
	size_t size;
} prefix;

static void permute_for_real(const char *set, prefix p, int n, int k, void (*found)(char *)) {
	if (k == 0) {
		found(p.str);
		return;
	}

	for (int i = 0; i < n; ++i) {
		prefix newPrefix = p;
		newPrefix.str[newPrefix.size] = set[i];
		++newPrefix.size;
		permute_for_real(set, newPrefix, n, k - 1, found);
	}
}

static void permute(const char *set, int n, int k, void (*found)(char *)) {
	prefix prefix;
	memset(prefix.str, 0, sizeof(prefix.str));
	prefix.size = 0;
	permute_for_real(set, prefix, n, k, found);
}

static void vec2_found_f32(char *permutation) {
	type *t = get_type(float2_id);
	assert(t->members.size < MAX_MEMBERS);
	t->members.m[t->members.size].name = add_name(permutation);
	t->members.m[t->members.size].type.type = float_id;
	t->members.m[t->members.size].type.array_size = 0;
	++t->members.size;
}

static void vec2_found_vec2(char *permutation) {
	type *t = get_type(float2_id);
	assert(t->members.size < MAX_MEMBERS);
	t->members.m[t->members.size].name = add_name(permutation);
	t->members.m[t->members.size].type.type = float2_id;
	t->members.m[t->members.size].type.array_size = 0;
	++t->members.size;
}

static void vec3_found_f32(char *permutation) {
	type *t = get_type(float3_id);
	assert(t->members.size < MAX_MEMBERS);
	t->members.m[t->members.size].name = add_name(permutation);
	t->members.m[t->members.size].type.type = float_id;
	t->members.m[t->members.size].type.array_size = 0;
	++t->members.size;
}

static void vec3_found_vec2(char *permutation) {
	type *t = get_type(float3_id);
	assert(t->members.size < MAX_MEMBERS);
	t->members.m[t->members.size].name = add_name(permutation);
	t->members.m[t->members.size].type.type = float2_id;
	t->members.m[t->members.size].type.array_size = 0;
	++t->members.size;
}

static void vec3_found_vec3(char *permutation) {
	type *t = get_type(float3_id);
	assert(t->members.size < MAX_MEMBERS);
	t->members.m[t->members.size].name = add_name(permutation);
	t->members.m[t->members.size].type.type = float3_id;
	t->members.m[t->members.size].type.array_size = 0;
	++t->members.size;
}

static void vec4_found_f32(char *permutation) {
	type *t = get_type(float4_id);
	assert(t->members.size < MAX_MEMBERS);
	t->members.m[t->members.size].name = add_name(permutation);
	t->members.m[t->members.size].type.type = float_id;
	t->members.m[t->members.size].type.array_size = 0;
	++t->members.size;
}

static void vec4_found_vec2(char *permutation) {
	type *t = get_type(float4_id);
	assert(t->members.size < MAX_MEMBERS);
	t->members.m[t->members.size].name = add_name(permutation);
	t->members.m[t->members.size].type.type = float2_id;
	t->members.m[t->members.size].type.array_size = 0;
	++t->members.size;
}

static void vec4_found_vec3(char *permutation) {
	type *t = get_type(float4_id);
	assert(t->members.size < MAX_MEMBERS);
	t->members.m[t->members.size].name = add_name(permutation);
	t->members.m[t->members.size].type.type = float3_id;
	t->members.m[t->members.size].type.array_size = 0;
	++t->members.size;
}

static void vec4_found_vec4(char *permutation) {
	type *t = get_type(float4_id);
	assert(t->members.size < MAX_MEMBERS);
	t->members.m[t->members.size].name = add_name(permutation);
	t->members.m[t->members.size].type.type = float4_id;
	t->members.m[t->members.size].type.array_size = 0;
	++t->members.size;
}

void types_init(void) {
	type *new_types = realloc(types, types_size * sizeof(type));
	assert(new_types != NULL);
	types = new_types;
	next_type_index = 0;

	sampler_type_id = add_type(add_name("sampler"));
	get_type(sampler_type_id)->built_in = true;
	tex2d_type_id = add_type(add_name("tex2d"));
	get_type(tex2d_type_id)->built_in = true;
	texcube_type_id = add_type(add_name("texcube"));
	get_type(texcube_type_id)->built_in = true;

	bool_id = add_type(add_name("bool"));
	get_type(bool_id)->built_in = true;
	float_id = add_type(add_name("float"));
	get_type(float_id)->built_in = true;

	{
		float2_id = add_type(add_name("float2"));
		get_type(float2_id)->built_in = true;
		const char *letters = "xy";
		permute(letters, (int)strlen(letters), 1, vec2_found_f32);
		permute(letters, (int)strlen(letters), 2, vec2_found_vec2);
		letters = "rg";
		permute(letters, (int)strlen(letters), 1, vec2_found_f32);
		permute(letters, (int)strlen(letters), 2, vec2_found_vec2);
	}

	{
		float3_id = add_type(add_name("float3"));
		get_type(float3_id)->built_in = true;
		const char *letters = "xyz";
		permute(letters, (int)strlen(letters), 1, vec3_found_f32);
		permute(letters, (int)strlen(letters), 2, vec3_found_vec2);
		permute(letters, (int)strlen(letters), 3, vec3_found_vec3);
		letters = "rgb";
		permute(letters, (int)strlen(letters), 1, vec3_found_f32);
		permute(letters, (int)strlen(letters), 2, vec3_found_vec2);
		permute(letters, (int)strlen(letters), 3, vec3_found_vec3);
	}

	{
		float4_id = add_type(add_name("float4"));
		get_type(float4_id)->built_in = true;
		const char *letters = "xyzw";
		permute(letters, (int)strlen(letters), 1, vec4_found_f32);
		permute(letters, (int)strlen(letters), 2, vec4_found_vec2);
		permute(letters, (int)strlen(letters), 3, vec4_found_vec3);
		permute(letters, (int)strlen(letters), 3, vec4_found_vec4);
		letters = "rgba";
		permute(letters, (int)strlen(letters), 1, vec4_found_f32);
		permute(letters, (int)strlen(letters), 2, vec4_found_vec2);
		permute(letters, (int)strlen(letters), 3, vec4_found_vec3);
		permute(letters, (int)strlen(letters), 3, vec4_found_vec4);
	}

	{
		float4x4_id = add_type(add_name("float4x4"));
		get_type(float4x4_id)->built_in = true;
	}

	{
		function_type_id = add_type(add_name("fun"));
		get_type(function_type_id)->built_in = true;
	}
}

static void grow_if_needed(uint64_t size) {
	while (size >= types_size) {
		types_size *= 2;
		type *new_types = realloc(types, types_size * sizeof(type));
		assert(new_types != NULL);
		types = new_types;
	}
}

type_id add_type(name_id name) {
	grow_if_needed(next_type_index + 1);

	type_id s = next_type_index;
	++next_type_index;

	types[s].name = name;
	types[s].attribute = NO_NAME;
	types[s].members.size = 0;
	types[s].built_in = false;

	return s;
}

type_id find_type(name_id name) {
	assert(name != NO_NAME);
	for (type_id i = 0; i < next_type_index; ++i) {
		if (types[i].name == name) {
			return i;
		}
	}

	return NO_TYPE;
}

type *get_type(type_id s) {
	if (s >= next_type_index) {
		return NULL;
	}
	return &types[s];
}
