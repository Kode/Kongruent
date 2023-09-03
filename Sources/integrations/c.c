#include "c.h"

#include "../compiler.h"
#include "../functions.h"
#include "../parser.h"
#include "../types.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static char *type_string(type_id type) {
	if (type == float_id) {
		return "float";
	}
	if (type == float2_id) {
		return "kinc_vector2_t";
	}
	if (type == float3_id) {
		return "kinc_vector3_t";
	}
	if (type == float4_id) {
		return "kinc_vector4_t";
	}
	return get_name(get_type(type)->name);
}

static const char *structure_type(type_id type) {
	if (type == float_id) {
		return "KINC_G4_VERTEX_DATA_F32_1X";
	}
	if (type == float2_id) {
		return "KINC_G4_VERTEX_DATA_F32_2X";
	}
	if (type == float3_id) {
		return "KINC_G4_VERTEX_DATA_F32_3X";
	}
	if (type == float4_id) {
		return "KINC_G4_VERTEX_DATA_F32_4X";
	}
	assert(false);
	return "UNKNOWN";
}

void c_export(char *directory) {
	type_id vertex_inputs[256];
	size_t vertex_inputs_size = 0;

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in && t->attribute == add_name("pipe")) {
			name_id vertex_shader_name = NO_NAME;

			for (size_t j = 0; j < t->members.size; ++j) {
				if (t->members.m[j].name == add_name("vertex")) {
					vertex_shader_name = t->members.m[j].value;
				}
			}

			assert(vertex_shader_name != NO_NAME);

			type_id vertex_input = NO_TYPE;

			for (function_id i = 0; get_function(i) != NULL; ++i) {
				function *f = get_function(i);
				if (f->name == vertex_shader_name) {
					vertex_input = f->parameter_type.type;
					break;
				}
			}

			assert(vertex_input != NO_TYPE);

			vertex_inputs[vertex_inputs_size] = vertex_input;
			vertex_inputs_size += 1;
		}
	}

	{
		char filename[512];
		sprintf(filename, "%s/%s", directory, "kong.h");

		FILE *output = fopen(filename, "wb");

		fprintf(output, "#include <kinc/graphics4/pipeline.h>\n");
		fprintf(output, "#include <kinc/graphics4/vertexbuffer.h>\n");
		fprintf(output, "#include <kinc/math/vector.h>\n\n");

		for (size_t i = 0; i < vertex_inputs_size; ++i) {
			type *t = get_type(vertex_inputs[i]);

			fprintf(output, "typedef struct %s {\n", get_name(t->name));
			for (size_t j = 0; j < t->members.size; ++j) {
				fprintf(output, "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
			}
			fprintf(output, "} %s;\n\n", get_name(t->name));

			fprintf(output, "extern kinc_g4_vertex_structure_t %s_structure;\n\n", get_name(t->name));
		}

		fprintf(output, "void kong_init(void);\n\n");

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && t->attribute == add_name("pipe")) {
				fprintf(output, "extern kinc_g4_pipeline_t %s;\n\n", get_name(t->name));
			}
		}

		fclose(output);
	}

	{
		char filename[512];
		sprintf(filename, "%s/%s", directory, "kong.c");

		FILE *output = fopen(filename, "wb");

		fprintf(output, "#include \"kong.h\"\n\n");

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && t->attribute == add_name("pipe")) {
				for (size_t j = 0; j < t->members.size; ++j) {
					if (t->members.m[j].name == add_name("vertex")) {
						fprintf(output, "#include \"kong_%s.h\"\n", get_name(t->members.m[j].value));
					}
					else if (t->members.m[j].name == add_name("fragment")) {
						fprintf(output, "#include \"kong_%s.h\"\n", get_name(t->members.m[j].value));
					}
					else {
						assert(false);
					}
				}
			}
		}

		fprintf(output, "\n");

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && t->attribute == add_name("pipe")) {
				fprintf(output, "kinc_g4_pipeline_t %s;\n\n", get_name(t->name));
			}
		}

		for (size_t i = 0; i < vertex_inputs_size; ++i) {
			type *t = get_type(vertex_inputs[i]);
			fprintf(output, "kinc_g4_vertex_structure_t %s_structure;\n", get_name(t->name));
		}

		fprintf(output, "\nvoid kong_init(void) {\n");
		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && t->attribute == add_name("pipe")) {
				fprintf(output, "\tkinc_g4_pipeline_init(&%s);\n\n", get_name(t->name));

				name_id vertex_shader_name = NO_NAME;

				for (size_t j = 0; j < t->members.size; ++j) {
					fprintf(output, "\tkinc_g4_shader_t %s;\n", get_name(t->members.m[j].value));
					if (t->members.m[j].name == add_name("vertex")) {
						fprintf(output, "\tkinc_g4_shader_init(&%s, %s_code, %s_code_size, KINC_G4_SHADER_TYPE_VERTEX);\n", get_name(t->members.m[j].value),
						        get_name(t->members.m[j].value), get_name(t->members.m[j].value));
						vertex_shader_name = t->members.m[j].value;
					}
					else if (t->members.m[j].name == add_name("fragment")) {
						fprintf(output, "\tkinc_g4_shader_init(&%s, %s_code, %s_code_size, KINC_G4_SHADER_TYPE_FRAGMENT);\n", get_name(t->members.m[j].value),
						        get_name(t->members.m[j].value), get_name(t->members.m[j].value));
					}
					else {
						assert(false);
					}

					char *member_name = "unknown";
					if (strcmp(get_name(t->members.m[j].name), "vertex") == 0) {
						member_name = "vertex_shader";
					}
					else if (strcmp(get_name(t->members.m[j].name), "fragment") == 0) {
						member_name = "fragment_shader";
					}

					fprintf(output, "\t%s.%s = &%s;\n\n", get_name(t->name), member_name, get_name(t->members.m[j].value));
				}

				assert(vertex_shader_name != NO_NAME);

				type_id vertex_input = NO_TYPE;

				for (function_id i = 0; get_function(i) != NULL; ++i) {
					function *f = get_function(i);
					if (f->name == vertex_shader_name) {
						vertex_input = f->parameter_type.type;
						break;
					}
				}

				assert(vertex_input != NO_TYPE);

				for (type_id i = 0; get_type(i) != NULL; ++i) {
					if (i == vertex_input) {
						type *t = get_type(i);
						fprintf(output, "\tkinc_g4_vertex_structure_init(&%s_structure);\n", get_name(t->name));
						for (size_t j = 0; j < t->members.size; ++j) {
							fprintf(output, "\tkinc_g4_vertex_structure_add(&%s_structure, \"%s\", %s);\n", get_name(t->name), get_name(t->members.m[j].name),
							        structure_type(t->members.m[j].type.type));
						}
						fprintf(output, "\n");
					}
				}

				fprintf(output, "\t%s.input_layout[0] = &%s_structure;\n", get_name(t->name), get_name(get_type(vertex_input)->name));
				fprintf(output, "\t%s.input_layout[1] = NULL;\n\n", get_name(t->name));

				fprintf(output, "\tkinc_g4_pipeline_compile(&%s);\n\n", get_name(t->name));
			}
		}
		fprintf(output, "}\n");

		fclose(output);
	}
}
