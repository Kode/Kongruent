#include "kinc.h"

#include "../backends/util.h"
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
	debug_context context = {0};
	error(context, "Unknown type for vertex structure");
	return "UNKNOWN";
}

static const char *convert_compare_mode(int mode) {
	switch (mode) {
	case 0:
		return "KINC_G4_COMPARE_ALWAYS";
	case 1:
		return "KINC_G4_COMPARE_NEVER";
	case 2:
		return "KINC_G4_COMPARE_EQUAL";
	case 3:
		return "KINC_G4_COMPARE_NOT_EQUAL";
	case 4:
		return "KINC_G4_COMPARE_LESS";
	case 5:
		return "KINC_G4_COMPARE_LESS_EQUAL";
	case 6:
		return "KINC_G4_COMPARE_GREATER";
	case 7:
		return "KINC_G4_COMPARE_GREATER_EQUAL";
	default: {
		debug_context context = {0};
		error(context, "Unknown compare mode");
		return "UNKNOWN";
	}
	}
}

static const char *convert_blend_mode(int mode) {
	switch (mode) {
	case 0:
		return "KINC_G4_BLEND_ONE";
	case 1:
		return "KINC_G4_BLEND_ZERO";
	case 2:
		return "KINC_G4_BLEND_SOURCE_ALPHA";
	case 3:
		return "KINC_G4_BLEND_DEST_ALPHA";
	case 4:
		return "KINC_G4_BLEND_INV_SOURCE_ALPHA";
	case 5:
		return "KINC_G4_BLEND_INV_DEST_ALPHA";
	case 6:
		return "KINC_G4_BLEND_SOURCE_COLOR";
	case 7:
		return "KINC_G4_BLEND_DEST_COLOR";
	case 8:
		return "KINC_G4_BLEND_INV_SOURCE_COLOR";
	case 9:
		return "KINC_G4_BLEND_INV_DEST_COLOR";
	case 10:
		return "KINC_G4_BLEND_CONSTANT";
	case 11:
		return "KINC_G4_BLEND_INV_CONSTANT";
	default: {
		debug_context context = {0};
		error(context, "Unknown blend mode");
		return "UNKNOWN";
	}
	}
}

static const char *convert_blend_op(int op) {
	switch (op) {
	case 0:
		return "KINC_G4_BLENDOP_ADD";
	case 1:
		return "KINC_G4_BLENDOP_SUBTRACT";
	case 2:
		return "KINC_G4_BLENDOP_REVERSE_SUBTRACT";
	case 3:
		return "KINC_G4_BLENDOP_MIN";
	case 4:
		return "KINC_G4_BLENDOP_MAX";
	default: {
		debug_context context = {0};
		error(context, "Unknown blend op");
		return "UNKNOWN";
	}
	}
}

static int global_register_indices[512];

void kinc_export(char *directory, api_kind api) {
	memset(global_register_indices, 0, sizeof(global_register_indices));

	if (api == API_WEBGPU) {
		int binding_index = 0;

		for (global_id i = 0; get_global(i).type != NO_TYPE; ++i) {
			global g = get_global(i);
			if (g.type == sampler_type_id) {
				global_register_indices[i] = binding_index;
				binding_index += 1;
			}
			else if (g.type == tex2d_type_id || g.type == texcube_type_id) {
				global_register_indices[i] = binding_index;
				binding_index += 1;
			}
			else if (g.type == float_id) {
			}
			else {
				global_register_indices[i] = binding_index;
				binding_index += 1;
			}
		}
	}
	else {
		int cbuffer_index = 0;
		int texture_index = 0;
		int sampler_index = 0;

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
			else if (g.type == float_id) {
			}
			else {
				global_register_indices[i] = cbuffer_index;
				cbuffer_index += 1;
			}
		}
	}

	type_id vertex_inputs[256];
	size_t vertex_inputs_size = 0;

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
			name_id vertex_shader_name = NO_NAME;

			for (size_t j = 0; j < t->members.size; ++j) {
				if (t->members.m[j].name == add_name("vertex")) {
					debug_context context = {0};
					check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "vertex expects an identifier");
					vertex_shader_name = t->members.m[j].value.identifier;
				}
			}

			debug_context context = {0};
			check(vertex_shader_name != NO_NAME, context, "No vertex shader name found");

			type_id vertex_input = NO_TYPE;

			for (function_id i = 0; get_function(i) != NULL; ++i) {
				function *f = get_function(i);
				if (f->name == vertex_shader_name) {
					check(f->parameters_size > 0, context, "Vertex function requires at least one parameter");
					vertex_input = f->parameter_types[0].type;
					break;
				}
			}

			check(vertex_input != NO_TYPE, context, "No vertex input found");

			vertex_inputs[vertex_inputs_size] = vertex_input;
			vertex_inputs_size += 1;
		}
	}

	{
		char filename[512];
		sprintf(filename, "%s/%s", directory, "kong.h");

		FILE *output = fopen(filename, "wb");

		fprintf(output, "#ifndef KONG_INTEGRATION_HEADER\n");
		fprintf(output, "#define KONG_INTEGRATION_HEADER\n\n");

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
			if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
				fprintf(output, "extern kinc_g4_pipeline_t %s;\n\n", get_name(t->name));
			}
		}

		for (function_id i = 0; get_function(i) != NULL; ++i) {
			function *f = get_function(i);
			if (has_attribute(&f->attributes, add_name("compute"))) {
				fprintf(output, "extern kinc_g4_compute_shader %s;\n\n", get_name(f->name));
			}
		}

		fprintf(output, "#endif\n");

		fclose(output);
	}

	{
		char filename[512];
		sprintf(filename, "%s/%s", directory, "kong.c");

		FILE *output = fopen(filename, "wb");

		fprintf(output, "#include \"kong.h\"\n\n");

		if (api == API_METAL) {
			// Code is added directly to the Xcode project instead
		}
		else if (api == API_WEBGPU) {
			fprintf(output, "#include \"wgsl.h\"\n");
		}
		else {
			for (type_id i = 0; get_type(i) != NULL; ++i) {
				type *t = get_type(i);
				if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
					for (size_t j = 0; j < t->members.size; ++j) {
						debug_context context = {0};
						if (t->members.m[j].name == add_name("vertex")) {
							check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "vertex expects an identifier");
							fprintf(output, "#include \"kong_%s.h\"\n", get_name(t->members.m[j].value.identifier));
						}
						else if (t->members.m[j].name == add_name("fragment")) {
							check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "fragment expects an identifier");
							fprintf(output, "#include \"kong_%s.h\"\n", get_name(t->members.m[j].value.identifier));
						}
					}
				}
			}

			for (function_id i = 0; get_function(i) != NULL; ++i) {
				function *f = get_function(i);
				if (has_attribute(&f->attributes, add_name("compute"))) {
					fprintf(output, "#include \"kong_%s.h\"\n", get_name(f->name));
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
			if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
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
				if (api != API_OPENGL) {
					// transpose matrices
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
			if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
				for (size_t j = 0; j < t->members.size; ++j) {
					if (t->members.m[j].name == add_name("vertex") || t->members.m[j].name == add_name("fragment")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "vertex or fragment expects an identifier");
						fprintf(output, "static kinc_g4_shader_t %s;\n", get_name(t->members.m[j].value.identifier));
					}
				}
			}
		}

		for (function_id i = 0; get_function(i) != NULL; ++i) {
			function *f = get_function(i);
			if (has_attribute(&f->attributes, add_name("compute"))) {
				fprintf(output, "kinc_g4_compute_shader %s;\n", get_name(f->name));
			}
		}

		if (api == API_WEBGPU) {
			fprintf(output, "\nvoid kinc_g5_internal_webgpu_create_shader_module(const void *source, size_t length);\n");
		}

		if (api == API_OPENGL) {
			fprintf(output, "\nvoid kinc_g4_internal_opengl_setup_uniform_block(unsigned program, const char *name, unsigned binding);\n");
		}

		fprintf(output, "\nvoid kong_init(void) {\n");

		if (api == API_WEBGPU) {
			fprintf(output, "\tkinc_g5_internal_webgpu_create_shader_module(wgsl, wgsl_size);\n\n");
		}

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
				fprintf(output, "\tkinc_g4_pipeline_init(&%s);\n\n", get_name(t->name));

				name_id vertex_shader_name = NO_NAME;
				name_id fragment_shader_name = NO_NAME;

				for (size_t j = 0; j < t->members.size; ++j) {
					if (t->members.m[j].name == add_name("vertex")) {
						if (api == API_METAL || api == API_WEBGPU) {
							fprintf(output, "\tkinc_g4_shader_init(&%s, \"%s\", 0, KINC_G4_SHADER_TYPE_VERTEX);\n", get_name(t->members.m[j].value.identifier),
							        get_name(t->members.m[j].value.identifier));
						}
						else {
							fprintf(output, "\tkinc_g4_shader_init(&%s, %s_code, %s_code_size, KINC_G4_SHADER_TYPE_VERTEX);\n",
							        get_name(t->members.m[j].value.identifier), get_name(t->members.m[j].value.identifier),
							        get_name(t->members.m[j].value.identifier));
						}
						fprintf(output, "\t%s.vertex_shader = &%s;\n\n", get_name(t->name), get_name(t->members.m[j].value.identifier));
						vertex_shader_name = t->members.m[j].value.identifier;
					}
					else if (t->members.m[j].name == add_name("fragment")) {
						if (api == API_METAL || api == API_WEBGPU) {
							fprintf(output, "\tkinc_g4_shader_init(&%s, \"%s\", 0, KINC_G4_SHADER_TYPE_FRAGMENT);\n",
							        get_name(t->members.m[j].value.identifier), get_name(t->members.m[j].value.identifier));
						}
						else {
							fprintf(output, "\tkinc_g4_shader_init(&%s, %s_code, %s_code_size, KINC_G4_SHADER_TYPE_FRAGMENT);\n",
							        get_name(t->members.m[j].value.identifier), get_name(t->members.m[j].value.identifier),
							        get_name(t->members.m[j].value.identifier));
						}
						fprintf(output, "\t%s.fragment_shader = &%s;\n\n", get_name(t->name), get_name(t->members.m[j].value.identifier));
						fragment_shader_name = t->members.m[j].value.identifier;
					}
					else if (t->members.m[j].name == add_name("depth_write")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_BOOLEAN, context, "depth_write expects a bool");
						fprintf(output, "\t%s.depth_write = %s;\n\n", get_name(t->name), t->members.m[j].value.boolean ? "true" : "false");
					}
					else if (t->members.m[j].name == add_name("depth_mode")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "depth_mode expects an identifier");
						global g = find_global(t->members.m[j].value.identifier);
						fprintf(output, "\t%s.depth_mode = %s;\n\n", get_name(t->name), convert_compare_mode(g.value.value.ints[0]));
					}
					else if (t->members.m[j].name == add_name("blend_source")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "blend_source expects an identifier");
						global g = find_global(t->members.m[j].value.identifier);
						fprintf(output, "\t%s.blend_source = %s;\n\n", get_name(t->name), convert_blend_mode(g.value.value.ints[0]));
					}
					else if (t->members.m[j].name == add_name("blend_destination")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "blend_destination expects an identifier");
						global g = find_global(t->members.m[j].value.identifier);
						fprintf(output, "\t%s.blend_destination = %s;\n\n", get_name(t->name), convert_blend_mode(g.value.value.ints[0]));
					}
					else if (t->members.m[j].name == add_name("blend_operation")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "blend_operation expects an identifier");
						global g = find_global(t->members.m[j].value.identifier);
						fprintf(output, "\t%s.blend_operation = %s;\n\n", get_name(t->name), convert_blend_op(g.value.value.ints[0]));
					}
					else if (t->members.m[j].name == add_name("alpha_blend_source")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "alpha_blend_source expects an identifier");
						global g = find_global(t->members.m[j].value.identifier);
						fprintf(output, "\t%s.alpha_blend_source = %s;\n\n", get_name(t->name), convert_blend_mode(g.value.value.ints[0]));
					}
					else if (t->members.m[j].name == add_name("alpha_blend_destination")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "alpha_blend_destination expects an identifier");
						global g = find_global(t->members.m[j].value.identifier);
						fprintf(output, "\t%s.alpha_blend_destination = %s;\n\n", get_name(t->name), convert_blend_mode(g.value.value.ints[0]));
					}
					else if (t->members.m[j].name == add_name("alpha_blend_operation")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "alpha_blend_operation expects an identifier");
						global g = find_global(t->members.m[j].value.identifier);
						fprintf(output, "\t%s.alpha_blend_operation = %s;\n\n", get_name(t->name), convert_blend_op(g.value.value.ints[0]));
					}
					else {
						debug_context context = {0};
						error(context, "Unsupported pipe member %s", get_name(t->members.m[j].name));
					}
				}

				{
					debug_context context = {0};
					check(vertex_shader_name != NO_NAME, context, "No vertex shader name found");
					check(fragment_shader_name != NO_NAME, context, "No fragment shader name found");
				}

				function *vertex_function = NULL;
				function *fragment_function = NULL;

				type_id vertex_input = NO_TYPE;

				for (function_id i = 0; get_function(i) != NULL; ++i) {
					function *f = get_function(i);
					if (f->name == vertex_shader_name) {
						vertex_function = f;
						debug_context context = {0};
						check(f->parameters_size > 0, context, "Vertex function requires at least one parameter");
						vertex_input = f->parameter_types[0].type;
						break;
					}
				}

				for (function_id i = 0; get_function(i) != NULL; ++i) {
					function *f = get_function(i);
					if (f->name == fragment_shader_name) {
						fragment_function = f;
						break;
					}
				}

				{
					debug_context context = {0};
					check(vertex_function != NULL, context, "Vertex function not found");
					check(fragment_function != NULL, context, "Fragment function not found");
					check(vertex_input != NO_TYPE, context, "No vertex input found");
				}

				for (type_id i = 0; get_type(i) != NULL; ++i) {
					if (i == vertex_input) {
						type *t = get_type(i);
						fprintf(output, "\tkinc_g4_vertex_structure_init(&%s_structure);\n", get_name(t->name));
						for (size_t j = 0; j < t->members.size; ++j) {
							if (api == API_OPENGL) {
								fprintf(output, "\tkinc_g4_vertex_structure_add(&%s_structure, \"%s_%s\", %s);\n", get_name(t->name), get_name(t->name),
								        get_name(t->members.m[j].name), structure_type(t->members.m[j].type.type));
							}
							else {
								fprintf(output, "\tkinc_g4_vertex_structure_add(&%s_structure, \"%s\", %s);\n", get_name(t->name),
								        get_name(t->members.m[j].name), structure_type(t->members.m[j].type.type));
							}
						}
						fprintf(output, "\n");
					}
				}

				fprintf(output, "\t%s.input_layout[0] = &%s_structure;\n", get_name(t->name), get_name(get_type(vertex_input)->name));
				fprintf(output, "\t%s.input_layout[1] = NULL;\n\n", get_name(t->name));

				if (fragment_function->return_type.array_size > 0) {
					fprintf(output, "\t%s.color_attachment_count = %i;\n", get_name(t->name), fragment_function->return_type.array_size);
					for (uint32_t i = 0; i < fragment_function->return_type.array_size; ++i) {
						fprintf(output, "\t%s.color_attachment[%i] = KINC_G4_RENDER_TARGET_FORMAT_32BIT;\n", get_name(t->name), i);
					}
					fprintf(output, "\n");
				}

				fprintf(output, "\tkinc_g4_pipeline_compile(&%s);\n\n", get_name(t->name));

				if (api == API_OPENGL) {
					global_id globals[256];
					size_t globals_size = 0;
					find_referenced_globals(vertex_function, globals, &globals_size);
					find_referenced_globals(fragment_function, globals, &globals_size);

					for (global_id i = 0; i < globals_size; ++i) {
						global g = get_global(globals[i]);
						if (g.type == sampler_type_id) {
						}
						else if (g.type == tex2d_type_id || g.type == texcube_type_id) {
						}
						else if (g.type == float_id) {
						}
						else {
							fprintf(output, "\tkinc_g4_internal_opengl_setup_uniform_block(%s.impl.programId, \"_%" PRIu64 "\", %i);\n\n", get_name(t->name),
							        g.var_index, global_register_indices[i]);
						}
					}
				}
			}
		}

		for (function_id i = 0; get_function(i) != NULL; ++i) {
			function *f = get_function(i);
			if (has_attribute(&f->attributes, add_name("compute"))) {
				fprintf(output, "\tkinc_g4_compute_shader_init(&%s, %s_code, %s_code_size);\n", get_name(f->name), get_name(f->name), get_name(f->name));
			}
		}

		fprintf(output, "}\n");

		fclose(output);
	}
}
