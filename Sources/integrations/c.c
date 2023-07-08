#include "c.h"

#include "../compiler.h"
#include "../functions.h"
#include "../parser.h"
#include "../types.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

static char *type_string(type_id type) {
	if (type == f32_id) {
		return "float";
	}
	if (type == vec2_id) {
		return "kinc_vector2";
	}
	if (type == vec3_id) {
		return "kinc_vector3";
	}
	if (type == vec4_id) {
		return "kinc_vector4";
	}
	return get_name(get_type(type)->name);
}

void c_export(void) {
	{
		FILE *output = fopen("test.h", "wb");

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && t->attribute != add_name("pipe")) {
				fprintf(output, "struct %s {\n", get_name(t->name));
				for (size_t j = 0; j < t->members.size; ++j) {
					fprintf(output, "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
				}
				fprintf(output, "};\n\n");

				fprintf(output, "%s *kong_lock_(kinc_g4_vertex_buffer *buffer);\n\n", get_name(t->name));
			}
		}

		fclose(output);
	}

	{
		FILE *output = fopen("test.c", "wb");

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && t->attribute != add_name("pipe")) {
				fprintf(output, "%s *kong_lock_(kinc_g4_vertex_buffer *buffer) {\n\treturn (%s *)kinc_vertex_buffer_lock_all();\n}\n\n", get_name(t->name),
				        get_name(t->name));
			}
		}

		fclose(output);
	}
}
