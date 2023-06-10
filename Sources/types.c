#include "types.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static type *types = NULL;
static type_id types_size = 1024;
static type_id next_type_index = 0;

type_id f32_id;
type_id vec2_id;
type_id vec3_id;
type_id vec4_id;
type_id bool_id;

typedef struct prefix {
	char str[5];
	size_t size;
} prefix;

static void permute_for_real(const char *set, prefix p, int n, int k, void (*found)(char *)) {
	if (k == 0) {
		found(p.str);
		return;
	}

	for (int i = 0; i < n; i++) {
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
	type *s = get_type(vec2_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = f32_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

static void vec2_found_vec2(char *permutation) {
	type *s = get_type(vec2_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = vec2_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

static void vec3_found_f32(char *permutation) {
	type *s = get_type(vec3_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = f32_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

static void vec3_found_vec2(char *permutation) {
	type *s = get_type(vec3_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = vec2_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

static void vec3_found_vec3(char *permutation) {
	type *s = get_type(vec3_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = vec3_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

static void vec4_found_f32(char *permutation) {
	type *s = get_type(vec4_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = f32_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

static void vec4_found_vec2(char *permutation) {
	type *s = get_type(vec4_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = vec2_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

static void vec4_found_vec3(char *permutation) {
	type *s = get_type(vec4_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = vec3_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

static void vec4_found_vec4(char *permutation) {
	type *s = get_type(vec4_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = vec4_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

void types_init(void) {
	type *new_types = realloc(types, types_size * sizeof(type));
	assert(new_types != NULL);
	types = new_types;
	next_type_index = 0;

	bool_id = add_type(add_name("bool"));
	f32_id = add_type(add_name("f32"));

	{
		vec2_id = add_type(add_name("vec2"));
		const char *letters = "xy";
		permute(letters, (int)strlen(letters), 1, vec2_found_f32);
		permute(letters, (int)strlen(letters), 2, vec2_found_vec2);
	}

	{
		vec3_id = add_type(add_name("vec3"));
		const char *letters = "xyz";
		permute(letters, (int)strlen(letters), 1, vec3_found_f32);
		permute(letters, (int)strlen(letters), 2, vec3_found_vec2);
		permute(letters, (int)strlen(letters), 3, vec3_found_vec3);
	}

	{
		vec4_id = add_type(add_name("vec4"));
		const char *letters = "xyzw";
		permute(letters, (int)strlen(letters), 1, vec4_found_f32);
		permute(letters, (int)strlen(letters), 2, vec4_found_vec2);
		permute(letters, (int)strlen(letters), 3, vec4_found_vec3);
		permute(letters, (int)strlen(letters), 3, vec4_found_vec4);
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

	return s;
}

type_id find_type(name_id name) {
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
