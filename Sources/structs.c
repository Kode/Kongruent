#include "structs.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static structy *structs = NULL;
static struct_id structs_size = 1024;
static struct_id next_struct_index = 0;

struct_id f32_id;
struct_id vec2_id;
struct_id vec3_id;
struct_id vec4_id;
struct_id bool_id;

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
	structy *s = get_struct(vec2_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = f32_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

static void vec2_found_vec2(char *permutation) {
	structy *s = get_struct(vec2_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = vec2_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

static void vec3_found_f32(char *permutation) {
	structy *s = get_struct(vec3_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = f32_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

static void vec3_found_vec2(char *permutation) {
	structy *s = get_struct(vec3_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = vec2_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

static void vec3_found_vec3(char *permutation) {
	structy *s = get_struct(vec3_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = vec3_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

static void vec4_found_f32(char *permutation) {
	structy *s = get_struct(vec4_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = f32_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

static void vec4_found_vec2(char *permutation) {
	structy *s = get_struct(vec4_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = vec2_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

static void vec4_found_vec3(char *permutation) {
	structy *s = get_struct(vec4_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = vec3_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

static void vec4_found_vec4(char *permutation) {
	structy *s = get_struct(vec4_id);
	s->members.m[s->members.size].name = add_name(permutation);
	s->members.m[s->members.size].type.type = vec4_id;
	s->members.m[s->members.size].type.resolved = true;
	++s->members.size;
}

void structs_init(void) {
	structy *new_structs = realloc(structs, structs_size * sizeof(structy));
	assert(new_structs != NULL);
	structs = new_structs;
	next_struct_index = 0;

	bool_id = add_struct(add_name("bool"));
	f32_id = add_struct(add_name("f32"));

	{
		vec2_id = add_struct(add_name("vec2"));
		const char *letters = "xy";
		permute(letters, (int)strlen(letters), 1, vec2_found_f32);
		permute(letters, (int)strlen(letters), 2, vec2_found_vec2);
	}

	{
		vec3_id = add_struct(add_name("vec3"));
		const char *letters = "xyz";
		permute(letters, (int)strlen(letters), 1, vec3_found_f32);
		permute(letters, (int)strlen(letters), 2, vec3_found_vec2);
		permute(letters, (int)strlen(letters), 3, vec3_found_vec3);
	}

	{
		vec4_id = add_struct(add_name("vec4"));
		const char *letters = "xyzw";
		permute(letters, (int)strlen(letters), 1, vec4_found_f32);
		permute(letters, (int)strlen(letters), 2, vec4_found_vec2);
		permute(letters, (int)strlen(letters), 3, vec4_found_vec3);
		permute(letters, (int)strlen(letters), 3, vec4_found_vec4);
	}
}

static void grow_if_needed(uint64_t size) {
	while (size >= structs_size) {
		structs_size *= 2;
		structy *new_structs = realloc(structs, structs_size * sizeof(structy));
		assert(new_structs != NULL);
		structs = new_structs;
	}
}

struct_id add_struct(name_id name) {
	grow_if_needed(next_struct_index + 1);

	struct_id s = next_struct_index;
	++next_struct_index;

	structs[s].name = name;
	structs[s].attribute = NO_NAME;
	structs[s].members.size = 0;

	return s;
}

struct_id find_struct(name_id name) {
	for (struct_id i = 0; i < next_struct_index; ++i) {
		if (structs[i].name == name) {
			return i;
		}
	}

	return NO_STRUCT;
}

structy *get_struct(struct_id s) {
	if (s >= next_struct_index) {
		return NULL;
	}
	return &structs[s];
}
