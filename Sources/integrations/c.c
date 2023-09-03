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
	{
		char filename[512];
		sprintf(filename, "%s/%s", directory, "kong.h");

		FILE *output = fopen(filename, "wb");

		fprintf(output, "#include <kinc/graphics4/pipeline.h>\n");
		fprintf(output, "#include <kinc/graphics4/vertexbuffer.h>\n");
		fprintf(output, "#include <kinc/math/vector.h>\n\n");

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && t->attribute != add_name("pipe")) {
				fprintf(output, "typedef struct %s {\n", get_name(t->name));
				for (size_t j = 0; j < t->members.size; ++j) {
					fprintf(output, "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
				}
				fprintf(output, "} %s;\n\n", get_name(t->name));

				fprintf(output, "extern kinc_g4_vertex_structure_t %s_structure;\n\n", get_name(t->name));
			}
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
		fprintf(output, "#include \"vert.h\"\n");
		fprintf(output, "#include \"frag.h\"\n\n");

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && t->attribute == add_name("pipe")) {
				fprintf(output, "kinc_g4_pipeline_t %s;\n\n", get_name(t->name));
			}
		}

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && t->attribute != add_name("pipe")) {
				fprintf(output, "kinc_g4_vertex_structure_t %s_structure;\n", get_name(t->name));
			}
		}

		fprintf(output, "\nvoid kong_init(void) {\n");
		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && t->attribute == add_name("pipe")) {
				fprintf(output, "\tkinc_g4_pipeline_init(&%s);\n\n", get_name(t->name));

				for (size_t j = 0; j < t->members.size; ++j) {
					fprintf(output, "\tkinc_g4_shader_t %s;\n", get_name(t->members.m[j].value));
					if (t->members.m[j].name == add_name("vertex")) {
						fprintf(output, "\tkinc_g4_shader_init(&%s, kong_vert_code, kong_vert_code_size, KINC_G4_SHADER_TYPE_VERTEX);\n",
						        get_name(t->members.m[j].value));
					}
					else if (t->members.m[j].name == add_name("fragment")) {
						fprintf(output, "\tkinc_g4_shader_init(&%s, kong_frag_code, kong_frag_code_size, KINC_G4_SHADER_TYPE_FRAGMENT);\n",
						        get_name(t->members.m[j].value));
					}
					else {
						assert(false);
					}
					fprintf(output, "\t%s.%s = &%s;\n\n", get_name(t->name), get_name(t->members.m[j].name), get_name(t->members.m[j].value));
				}

				for (type_id i = 0; get_type(i) != NULL; ++i) {
					type *t = get_type(i);
					if (!t->built_in && t->attribute != add_name("pipe")) {
						fprintf(output, "\tkinc_g4_vertex_structure_init(&%s_structure);\n", get_name(t->name));
						for (size_t j = 0; j < t->members.size; ++j) {
							fprintf(output, "\tkinc_g4_vertex_structure_add(&%s_structure, \"%s\", %s);\n", get_name(t->name), get_name(t->members.m[j].name),
							        structure_type(t->members.m[j].type.type));
						}
						fprintf(output, "\n");
					}
				}

				type_id vertex_input = NO_TYPE;

				for (function_id i = 0; get_function(i) != NULL; ++i) {
					function *f = get_function(i);
					if (f->attribute == add_name("vertex")) {
						vertex_input = f->parameter_type.type;
						break;
					}
				}

				assert(vertex_input != NO_TYPE);

				fprintf(output, "\t%s.input_layout[0] = &%s_structure;\n", get_name(t->name), get_name(get_type(vertex_input)->name));
				fprintf(output, "\t%s.input_layout[1] = NULL;\n\n", get_name(t->name));

				fprintf(output, "\tkinc_g4_pipeline_compile(&%s);\n", get_name(t->name));
			}
		}
		fprintf(output, "}\n");

		fclose(output);
	}
}
