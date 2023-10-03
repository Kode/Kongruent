#include "c.h"

#include "../compiler.h"
#include "../errors.h"
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
	if (type == float4x4_id) {
		return "kinc_matrix4x4_t";
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
	error(0, 0, "Unknown type for vertex structure");
	return "UNKNOWN";
}

static int global_register_indices[512];

void c_export(char *directory) {
	int cbuffer_index = 0;
	int texture_index = 0;
	int sampler_index = 0;

	memset(global_register_indices, 0, sizeof(global_register_indices));

	for (global_id i = 0; get_global(i).type != NO_TYPE; ++i) {
		global g = get_global(i);
		if (g.type == sampler_type_id) {
			global_register_indices[i] = sampler_index;
			sampler_index += 1;
		}
		else if (g.type == tex2d_type_id || g.type == texcube_type_id) {
			global_register_indices[i] = texture_index;
			texture_index += 1;
		}
		else {
			global_register_indices[i] = cbuffer_index;
			cbuffer_index += 1;
		}
	}

	type_id vertex_inputs[256];
	size_t vertex_inputs_size = 0;

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in && t->attribute == add_name("pipe")) {
			name_id vertex_shader_name = NO_NAME;

			for (size_t j = 0; j < t->members.size; ++j) {
				if (t->members.m[j].name == add_name("vertex")) {
					check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, 0, 0, "vertex expects an identifier");
					vertex_shader_name = t->members.m[j].value.identifier;
				}
			}

			check(vertex_shader_name != NO_NAME, 0, 0, "No vertex shader name found");

			type_id vertex_input = NO_TYPE;

			for (function_id i = 0; get_function(i) != NULL; ++i) {
				function *f = get_function(i);
				if (f->name == vertex_shader_name) {
					vertex_input = f->parameter_type.type;
					break;
				}
			}

			check(vertex_input != NO_TYPE, 0, 0, "No vertex input found");

			vertex_inputs[vertex_inputs_size] = vertex_input;
			vertex_inputs_size += 1;
		}
	}

	{
		char filename[512];
		sprintf(filename, "%s/%s", directory, "kong.h");

		FILE *output = fopen(filename, "wb");

		fprintf(output, "#include <kinc/graphics4/constantbuffer.h>\n");
		fprintf(output, "#include <kinc/graphics4/pipeline.h>\n");
		fprintf(output, "#include <kinc/graphics4/vertexbuffer.h>\n");
		fprintf(output, "#include <kinc/math/matrix.h>\n");
		fprintf(output, "#include <kinc/math/vector.h>\n\n");

		for (global_id i = 0; get_global(i).type != NO_TYPE; ++i) {
			global g = get_global(i);
			if (g.type == float_id) {
			}
			else if (g.type == tex2d_type_id || g.type == texcube_type_id || g.type == sampler_type_id) {
				fprintf(output, "extern int %s;\n", get_name(g.name));
			}
			else {
				type *t = get_type(g.type);

				char name[256];
				if (t->name != NO_NAME) {
					strcpy(name, get_name(t->name));
				}
				else {
					strcpy(name, get_name(g.name));
					strcat(name, "_type");
				}

				fprintf(output, "typedef struct %s {\n", name);
				for (size_t j = 0; j < t->members.size; ++j) {
					fprintf(output, "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
				}
				fprintf(output, "} %s;\n\n", name);

				fprintf(output, "typedef struct %s_buffer {\n", name);
				fprintf(output, "\tkinc_g4_constant_buffer buffer;\n");
				fprintf(output, "\t%s *data;\n", name);
				fprintf(output, "} %s_buffer;\n\n", name);

				fprintf(output, "void %s_buffer_init(%s_buffer *buffer);\n", name, name);
				fprintf(output, "void %s_buffer_destroy(%s_buffer *buffer);\n", name, name);
				fprintf(output, "%s *%s_buffer_lock(%s_buffer *buffer);\n", name, name, name);
				fprintf(output, "void %s_buffer_unlock(%s_buffer *buffer);\n", name, name);
				fprintf(output, "void %s_buffer_set(%s_buffer *buffer);\n\n", name, name);
			}
		}

		fprintf(output, "\n");

		for (size_t i = 0; i < vertex_inputs_size; ++i) {
			type *t = get_type(vertex_inputs[i]);

			fprintf(output, "typedef struct %s {\n", get_name(t->name));
			for (size_t j = 0; j < t->members.size; ++j) {
				fprintf(output, "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
			}
			fprintf(output, "} %s;\n\n", get_name(t->name));

			fprintf(output, "extern kinc_g4_vertex_structure_t %s_structure;\n\n", get_name(t->name));
		}

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
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, 0, 0, "vertex expects an identifier");
						fprintf(output, "#include \"kong_%s.h\"\n", get_name(t->members.m[j].value.identifier));
					}
					else if (t->members.m[j].name == add_name("fragment")) {
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, 0, 0, "fragment expects an identifier");
						fprintf(output, "#include \"kong_%s.h\"\n", get_name(t->members.m[j].value.identifier));
					}
				}
			}
		}

		fprintf(output, "\n#include <kinc/graphics4/graphics.h>\n\n");

		for (global_id i = 0; get_global(i).type != NO_TYPE; ++i) {
			global g = get_global(i);
			if (g.type == tex2d_type_id || g.type == texcube_type_id || g.type == sampler_type_id) {
				fprintf(output, "int %s = %i;\n", get_name(g.name), global_register_indices[i]);
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

		for (global_id i = 0; get_global(i).type != NO_TYPE; ++i) {
			global g = get_global(i);
			if (g.type != tex2d_type_id && g.type != texcube_type_id && g.type != sampler_type_id && g.type != float_id) {
				type *t = get_type(g.type);

				char type_name[256];
				if (t->name != NO_NAME) {
					strcpy(type_name, get_name(t->name));
				}
				else {
					strcpy(type_name, get_name(g.name));
					strcat(type_name, "_type");
				}

				fprintf(output, "\nvoid %s_buffer_init(%s_buffer *buffer) {\n", type_name, type_name);
				fprintf(output, "\tbuffer->data = NULL;\n");
				fprintf(output, "\tkinc_g4_constant_buffer_init(&buffer->buffer, sizeof(%s));\n", type_name);
				fprintf(output, "}\n\n");

				fprintf(output, "void %s_buffer_destroy(%s_buffer *buffer) {\n", type_name, type_name);
				fprintf(output, "\tbuffer->data = NULL;\n");
				fprintf(output, "\tkinc_g4_constant_buffer_destroy(&buffer->buffer);\n");
				fprintf(output, "}\n\n");

				fprintf(output, "%s *%s_buffer_lock(%s_buffer *buffer) {\n", type_name, type_name, type_name);
				fprintf(output, "\tbuffer->data = (%s *)kinc_g4_constant_buffer_lock_all(&buffer->buffer);\n", type_name);
				fprintf(output, "\treturn buffer->data;\n");
				fprintf(output, "}\n\n");

				fprintf(output, "void %s_buffer_unlock(%s_buffer *buffer) {\n", type_name, type_name);
				if (true) {
					// transpose
					for (size_t j = 0; j < t->members.size; ++j) {
						if (t->members.m[j].type.type == float4x4_id) {
							fprintf(output, "\tkinc_matrix4x4_transpose(&buffer->data->%s);\n", get_name(t->members.m[j].name));
						}
					}
				}
				fprintf(output, "\tbuffer->data = NULL;\n");
				fprintf(output, "\tkinc_g4_constant_buffer_unlock_all(&buffer->buffer);\n");
				fprintf(output, "}\n\n");

				fprintf(output, "void %s_buffer_set(%s_buffer *buffer) {\n", type_name, type_name);
				fprintf(output, "\tkinc_g4_set_constant_buffer(%i, &buffer->buffer);\n", global_register_indices[i]);
				fprintf(output, "}\n\n");
			}
		}

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && t->attribute == add_name("pipe")) {
				for (size_t j = 0; j < t->members.size; ++j) {
					if (t->members.m[j].name == add_name("vertex") || t->members.m[j].name == add_name("fragment")) {
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, 0, 0, "vertex or fragment expects an identifier");
						fprintf(output, "static kinc_g4_shader_t %s;\n", get_name(t->members.m[j].value.identifier));
					}
				}
			}
		}

		fprintf(output, "\nvoid kong_init(void) {\n");
		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && t->attribute == add_name("pipe")) {
				fprintf(output, "\tkinc_g4_pipeline_init(&%s);\n\n", get_name(t->name));

				name_id vertex_shader_name = NO_NAME;

				for (size_t j = 0; j < t->members.size; ++j) {
					if (t->members.m[j].name == add_name("vertex")) {
						fprintf(output, "\tkinc_g4_shader_init(&%s, %s_code, %s_code_size, KINC_G4_SHADER_TYPE_VERTEX);\n",
						        get_name(t->members.m[j].value.identifier), get_name(t->members.m[j].value.identifier),
						        get_name(t->members.m[j].value.identifier));
						fprintf(output, "\t%s.vertex_shader = &%s;\n\n", get_name(t->name), get_name(t->members.m[j].value.identifier));
						vertex_shader_name = t->members.m[j].value.identifier;
					}
					else if (t->members.m[j].name == add_name("fragment")) {
						fprintf(output, "\tkinc_g4_shader_init(&%s, %s_code, %s_code_size, KINC_G4_SHADER_TYPE_FRAGMENT);\n",
						        get_name(t->members.m[j].value.identifier), get_name(t->members.m[j].value.identifier),
						        get_name(t->members.m[j].value.identifier));
						fprintf(output, "\t%s.fragment_shader = &%s;\n\n", get_name(t->name), get_name(t->members.m[j].value.identifier));
					}
					else if (t->members.m[j].name == add_name("depth_write")) {
						check(t->members.m[j].value.kind == TOKEN_BOOLEAN, 0, 0, "depth_write expects a bool");
						fprintf(output, "\t%s.depth_write = %s;\n\n", get_name(t->name), t->members.m[j].value.boolean ? "true" : "false");
					}
					else if (t->members.m[j].name == add_name("depth_mode")) {
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, 0, 0, "depth_mode expects an identifier");
						global g = find_global(t->members.m[j].value.identifier);
						fprintf(output, "\t%s.depth_mode = %i;\n\n", get_name(t->name), (int)g.value);
					}
					else {
						error(0, 0, "Unsupported pipe member %s", get_name(t->members.m[j].name));
					}
				}

				check(vertex_shader_name != NO_NAME, 0, 0, "No vertex shader name found");

				type_id vertex_input = NO_TYPE;

				for (function_id i = 0; get_function(i) != NULL; ++i) {
					function *f = get_function(i);
					if (f->name == vertex_shader_name) {
						vertex_input = f->parameter_type.type;
						break;
					}
				}

				check(vertex_input != NO_TYPE, 0, 0, "No vertex input found");

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
