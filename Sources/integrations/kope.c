#include "kinc.h"

#include "../backends/util.h"
#include "../compiler.h"
#include "../errors.h"
#include "../functions.h"
#include "../parser.h"
#include "../sets.h"
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
	if (type == float3x3_id) {
		return "kinc_matrix3x3_t";
	}
	if (type == float4x4_id) {
		return "kinc_matrix4x4_t";
	}
	return get_name(get_type(type)->name);
}

static const char *structure_type(type_id type) {
	if (type == float_id) {
		return "KOPE_D3D12_VERTEX_FORMAT_FLOAT32";
	}
	if (type == float2_id) {
		return "KOPE_D3D12_VERTEX_FORMAT_FLOAT32X2";
	}
	if (type == float3_id) {
		return "KOPE_D3D12_VERTEX_FORMAT_FLOAT32X3";
	}
	if (type == float4_id) {
		return "KOPE_D3D12_VERTEX_FORMAT_FLOAT32X4";
	}
	debug_context context = {0};
	error(context, "Unknown type for vertex structure");
	return "UNKNOWN";
}

static uint32_t base_type_size(type_id type) {
	if (type == float_id) {
		return 4;
	}
	if (type == float2_id) {
		return 4 * 2;
	}
	if (type == float3_id) {
		return 4 * 3;
	}
	if (type == float4_id) {
		return 4 * 4;
	}
	if (type == float4x4_id) {
		return 4 * 4 * 4;
	}
	if (type == float3x3_id) {
		return 3 * 4 * 4;
	}

	debug_context context = {0};
	error(context, "Unknown type %s for structure", get_name(get_type(type)->name));
	return 1;
}

static uint32_t struct_size(type_id id) {
	uint32_t size = 0;
	type *t = get_type(id);
	for (size_t member_index = 0; member_index < t->members.size; ++member_index) {
		size += base_type_size(t->members.m[member_index].type.type);
	}
	return size;
}

static const char *convert_compare_mode(int mode) {
	switch (mode) {
	case 0:
		return "KOPE_D3D12_COMPARE_FUNCTION_ALWAYS";
	case 1:
		return "KOPE_D3D12_COMPARE_FUNCTION_NEVER";
	case 2:
		return "KOPE_D3D12_COMPARE_FUNCTION_EQUAL";
	case 3:
		return "KOPE_D3D12_COMPARE_FUNCTION_NOT_EQUAL";
	case 4:
		return "KOPE_D3D12_COMPARE_FUNCTION_LESS";
	case 5:
		return "KOPE_D3D12_COMPARE_FUNCTION_LESS_EQUAL";
	case 6:
		return "KOPE_D3D12_COMPARE_FUNCTION_GREATER";
	case 7:
		return "KOPE_D3D12_COMPARE_FUNCTION_GREATER_EQUAL";
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

static member *find_member(type *t, char *name) {
	for (size_t j = 0; j < t->members.size; ++j) {
		if (t->members.m[j].name == add_name(name)) {
			return &t->members.m[j];
		}
	}

	return NULL;
}

static int global_register_indices[512];

void kope_export(char *directory, api_kind api) {
	memset(global_register_indices, 0, sizeof(global_register_indices));

	if (api == API_WEBGPU) {
		int binding_index = 0;

		for (global_id i = 0; get_global(i)->type != NO_TYPE; ++i) {
			global *g = get_global(i);
			if (g->type == sampler_type_id) {
				global_register_indices[i] = binding_index;
				binding_index += 1;
			}
			else if (g->type == tex2d_type_id || g->type == texcube_type_id) {
				global_register_indices[i] = binding_index;
				binding_index += 1;
			}
			else if (g->type == float_id) {
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

		for (global_id i = 0; get_global(i) != NULL && get_global(i)->type != NO_TYPE; ++i) {
			global *g = get_global(i);
			if (g->type == sampler_type_id) {
				global_register_indices[i] = sampler_index;
				sampler_index += 1;
			}
			else if (g->type == tex2d_type_id || g->type == texcube_type_id) {
				global_register_indices[i] = texture_index;
				texture_index += 1;
			}
			else if (g->type == float_id) {
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
			name_id mesh_shader_name = NO_NAME;

			for (size_t j = 0; j < t->members.size; ++j) {
				if (t->members.m[j].name == add_name("vertex")) {
					debug_context context = {0};
					check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "vertex expects an identifier");
					vertex_shader_name = t->members.m[j].value.identifier;
				}
				if (t->members.m[j].name == add_name("mesh")) {
					debug_context context = {0};
					check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "mesh expects an identifier");
					mesh_shader_name = t->members.m[j].value.identifier;
				}
			}

			debug_context context = {0};
			check(vertex_shader_name != NO_NAME || mesh_shader_name != NO_NAME, context, "No vertex or mesh shader name found");

			if (vertex_shader_name != NO_NAME) {

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
	}

	descriptor_set *sets[256];
	size_t sets_count = 0;

	{
		char filename[512];
		sprintf(filename, "%s/%s", directory, "kong.h");

		FILE *output = fopen(filename, "wb");

		fprintf(output, "#ifndef KONG_INTEGRATION_HEADER\n");
		fprintf(output, "#define KONG_INTEGRATION_HEADER\n\n");

		fprintf(output, "#include <kope/graphics5/device.h>\n");
		fprintf(output, "#include <kope/graphics5/sampler.h>\n");
		fprintf(output, "#include <kope/direct3d12/descriptorset_structs.h>\n");
		fprintf(output, "#include <kope/direct3d12/pipeline_structs.h>\n");
		fprintf(output, "#include <kinc/math/matrix.h>\n");
		fprintf(output, "#include <kinc/math/vector.h>\n\n");

		fprintf(output, "#ifdef __cplusplus\n");
		fprintf(output, "extern \"C\" {\n");
		fprintf(output, "#endif\n");

		fprintf(output, "\nvoid kong_init(kope_g5_device *device);\n\n");

		for (global_id i = 0; get_global(i) != NULL && get_global(i)->type != NO_TYPE; ++i) {
			global *g = get_global(i);
			if (g->type == tex2d_type_id || g->type == texcube_type_id || g->type == sampler_type_id) {
				fprintf(output, "extern int %s;\n", get_name(g->name));
			}
			else if (!get_type(g->type)->built_in) {
				type *t = get_type(g->type);

				char name[256];
				if (t->name != NO_NAME) {
					strcpy(name, get_name(t->name));
				}
				else {
					strcpy(name, get_name(g->name));
					strcat(name, "_type");
				}

				fprintf(output, "typedef struct %s {\n", name);
				for (size_t j = 0; j < t->members.size; ++j) {
					fprintf(output, "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
					if (t->members.m[j].type.type == float3x3_id) {
						fprintf(output, "\tfloat pad%" PRIu64 "[3];\n", j);
					}
				}
				fprintf(output, "} %s;\n\n", name);

				fprintf(output, "void %s_buffer_create(kope_g5_device *device, kope_g5_buffer *buffer);\n", name);
				fprintf(output, "void %s_buffer_destroy(kope_g5_buffer *buffer);\n", name);
				fprintf(output, "%s *%s_buffer_lock(kope_g5_buffer *buffer);\n", name, name);
				fprintf(output, "void %s_buffer_unlock(kope_g5_buffer *buffer);\n", name);
			}
		}

		fprintf(output, "\n");

		for (global_id i = 0; get_global(i) != NULL && get_global(i)->type != NO_TYPE; ++i) {
			global *g = get_global(i);
			descriptor_set *set = g->set;

			if (set == NULL) {
				continue;
			}

			bool found = false;
			for (size_t set_index = 0; set_index < sets_count; ++set_index) {
				if (sets[set_index] == set) {
					found = true;
					break;
				}
			}

			if (!found) {
				sets[sets_count] = set;
				sets_count += 1;
			}
		}

		for (size_t set_index = 0; set_index < sets_count; ++set_index) {
			descriptor_set *set = sets[set_index];

			fprintf(output, "typedef struct %s_parameters {\n", get_name(set->name));
			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition d = set->definitions[definition_index];
				switch (d.kind) {
				case DEFINITION_CONST_CUSTOM:
					fprintf(output, "\tkope_g5_buffer *%s;\n", get_name(get_global(d.global)->name));
					break;
				case DEFINITION_TEX2D:
					fprintf(output, "\tkope_g5_texture *%s;\n", get_name(get_global(d.global)->name));
					break;
				case DEFINITION_SAMPLER:
					fprintf(output, "\tkope_g5_sampler *%s;\n", get_name(get_global(d.global)->name));
					break;
				default: {
					debug_context context = {0};
					error(context, "Unexpected kind of definition");
					break;
				}
				}
			}
			fprintf(output, "} %s_parameters;\n\n", get_name(set->name));

			fprintf(output, "typedef struct %s_set {\n", get_name(set->name));
			fprintf(output, "\tkope_d3d12_descriptor_set set;\n\n");
			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition d = set->definitions[definition_index];
				switch (d.kind) {
				case DEFINITION_CONST_CUSTOM:
					fprintf(output, "\tkope_g5_buffer *%s;\n", get_name(get_global(d.global)->name));
					break;
				case DEFINITION_TEX2D:
					fprintf(output, "\tkope_g5_texture *%s;\n", get_name(get_global(d.global)->name));
					break;
				}
			}
			fprintf(output, "} %s_set;\n\n", get_name(set->name));

			fprintf(output, "void kong_create_%s_set(kope_g5_device *device, const %s_parameters *parameters, %s_set *set);\n", get_name(set->name),
			        get_name(set->name), get_name(set->name));
			fprintf(output, "void kong_set_descriptor_set_%s(kope_g5_command_list *list, %s_set *set);\n\n", get_name(set->name), get_name(set->name));
		}

		fprintf(output, "\n");

		for (size_t i = 0; i < vertex_inputs_size; ++i) {
			type *t = get_type(vertex_inputs[i]);

			fprintf(output, "typedef struct %s {\n", get_name(t->name));
			for (size_t j = 0; j < t->members.size; ++j) {
				fprintf(output, "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
			}
			fprintf(output, "} %s;\n\n", get_name(t->name));

			fprintf(output, "typedef struct %s_buffer {\n", get_name(t->name));
			fprintf(output, "\tkope_g5_buffer buffer;\n");
			fprintf(output, "\tsize_t count;\n");
			fprintf(output, "} %s_buffer;\n\n", get_name(t->name));

			fprintf(output, "void kong_create_buffer_%s(kope_g5_device * device, size_t count, %s_buffer *buffer);\n", get_name(t->name), get_name(t->name));
			fprintf(output, "%s *kong_%s_buffer_lock(%s_buffer *buffer);\n", get_name(t->name), get_name(t->name), get_name(t->name));
			fprintf(output, "void kong_%s_buffer_unlock(%s_buffer *buffer);\n", get_name(t->name), get_name(t->name));
			fprintf(output, "void kong_set_vertex_buffer_%s(kope_g5_command_list *list, %s_buffer *buffer);\n\n", get_name(t->name), get_name(t->name));
		}

		fprintf(output, "void kong_set_render_pipeline(kope_g5_command_list *list, kope_d3d12_render_pipeline *pipeline);\n\n");

		fprintf(output, "void kong_set_compute_pipeline(kope_g5_command_list *list, kope_d3d12_compute_pipeline *pipeline);\n\n");

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
				fprintf(output, "extern kope_d3d12_render_pipeline %s;\n\n", get_name(t->name));
			}
		}

		for (function_id i = 0; get_function(i) != NULL; ++i) {
			function *f = get_function(i);
			if (has_attribute(&f->attributes, add_name("compute"))) {
				fprintf(output, "extern kope_d3d12_compute_pipeline %s;\n\n", get_name(f->name));
			}
		}

		fprintf(output, "#ifdef __cplusplus\n");
		fprintf(output, "}\n");
		fprintf(output, "#endif\n\n");

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

		fprintf(output, "\n#include <kope/direct3d12/buffer_functions.h>\n");
		fprintf(output, "#include <kope/direct3d12/commandlist_functions.h>\n");
		fprintf(output, "#include <kope/direct3d12/device_functions.h>\n");
		fprintf(output, "#include <kope/direct3d12/descriptorset_functions.h>\n");
		fprintf(output, "#include <kope/direct3d12/pipeline_functions.h>\n\n");

		for (global_id i = 0; get_global(i) != NULL && get_global(i)->type != NO_TYPE; ++i) {
			global *g = get_global(i);
			if (g->type == tex2d_type_id || g->type == texcube_type_id || g->type == sampler_type_id) {
				fprintf(output, "int %s = %i;\n", get_name(g->name), global_register_indices[i]);
			}
		}

		fprintf(output, "\n");

		for (size_t i = 0; i < vertex_inputs_size; ++i) {
			type *t = get_type(vertex_inputs[i]);

			fprintf(output, "void kong_create_buffer_%s(kope_g5_device * device, size_t count, %s_buffer *buffer) {\n", get_name(t->name), get_name(t->name));
			fprintf(output, "\tkope_g5_buffer_parameters parameters;\n");
			fprintf(output, "\tparameters.size = count * sizeof(%s);\n", get_name(t->name));
			fprintf(output, "\tparameters.usage_flags = KOPE_G5_BUFFER_USAGE_CPU_WRITE;\n");
			fprintf(output, "\tkope_g5_device_create_buffer(device, &parameters, &buffer->buffer);\n");
			fprintf(output, "\tbuffer->count = count;\n");
			fprintf(output, "}\n\n");

			fprintf(output, "%s *kong_%s_buffer_lock(%s_buffer *buffer) {\n", get_name(t->name), get_name(t->name), get_name(t->name));
			fprintf(output, "\treturn (%s *)kope_d3d12_buffer_lock(&buffer->buffer);\n", get_name(t->name));
			fprintf(output, "}\n\n");

			fprintf(output, "void kong_%s_buffer_unlock(%s_buffer *buffer) {\n", get_name(t->name), get_name(t->name));
			fprintf(output, "\tkope_d3d12_buffer_unlock(&buffer->buffer);\n");
			fprintf(output, "}\n\n");

			fprintf(output, "void kong_set_vertex_buffer_%s(kope_g5_command_list *list, %s_buffer *buffer) {\n", get_name(t->name), get_name(t->name));
			fprintf(output,
			        "\tkope_d3d12_command_list_set_vertex_buffer(list, %" PRIu64 ", &buffer->buffer.d3d12, 0, buffer->count * sizeof(%s), sizeof(%s));\n", i,
			        get_name(t->name), get_name(t->name));
			fprintf(output, "}\n\n");
		}

		fprintf(output, "void kong_set_render_pipeline(kope_g5_command_list *list, kope_d3d12_render_pipeline *pipeline) {\n");
		fprintf(output, "\tkope_d3d12_command_list_set_render_pipeline(list, pipeline);\n");
		fprintf(output, "}\n\n");

		fprintf(output, "void kong_set_compute_pipeline(kope_g5_command_list *list, kope_d3d12_compute_pipeline *pipeline) {\n");
		fprintf(output, "\tkope_d3d12_command_list_set_compute_pipeline(list, pipeline);\n");
		fprintf(output, "}\n\n");

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
				fprintf(output, "kope_d3d12_render_pipeline %s;\n\n", get_name(t->name));
			}
		}

		for (global_id i = 0; get_global(i) != NULL && get_global(i)->type != NO_TYPE; ++i) {
			global *g = get_global(i);
			if (g->type != tex2d_type_id && g->type != texcube_type_id && g->type != sampler_type_id && !get_type(g->type)->built_in) {
				type *t = get_type(g->type);

				char type_name[256];
				if (t->name != NO_NAME) {
					strcpy(type_name, get_name(t->name));
				}
				else {
					strcpy(type_name, get_name(g->name));
					strcat(type_name, "_type");
				}

				fprintf(output, "\nvoid %s_buffer_create(kope_g5_device *device, kope_g5_buffer *buffer) {\n", type_name);
				fprintf(output, "\tkope_g5_buffer_parameters parameters;\n");
				fprintf(output, "\tparameters.size = %i;\n", struct_size(g->type));
				fprintf(output, "\tparameters.usage_flags = KOPE_G5_BUFFER_USAGE_CPU_WRITE;\n");
				fprintf(output, "\tkope_g5_device_create_buffer(device, &parameters, buffer);\n");
				fprintf(output, "}\n\n");

				fprintf(output, "void %s_buffer_destroy(kope_g5_buffer *buffer) {\n", type_name);
				fprintf(output, "\tkope_g5_buffer_destroy(buffer);\n");
				fprintf(output, "}\n\n");

				fprintf(output, "%s *%s_buffer_lock(kope_g5_buffer *buffer) {\n", type_name, type_name);
				fprintf(output, "\treturn (%s *)kope_g5_buffer_lock(buffer);\n", type_name);
				fprintf(output, "}\n\n");

				fprintf(output, "void %s_buffer_unlock(kope_g5_buffer *buffer) {\n", type_name);
				if (api != API_OPENGL) {
					bool has_matrices = false;
					for (size_t j = 0; j < t->members.size; ++j) {
						if (t->members.m[j].type.type == float4x4_id || t->members.m[j].type.type == float3x3_id) {
							has_matrices = true;
							break;
						}
					}

					if (has_matrices) {
						fprintf(output, "\t%s *data = (%s *)kope_g5_buffer_lock(buffer);\n", type_name, type_name);
						// adjust matrices
						for (size_t j = 0; j < t->members.size; ++j) {
							if (t->members.m[j].type.type == float4x4_id) {
								fprintf(output, "\tkinc_matrix4x4_transpose(&data->%s);\n", get_name(t->members.m[j].name));
							}
							else if (t->members.m[j].type.type == float3x3_id) {
								// fprintf(output, "\tkinc_matrix3x3_transpose(&data->%s);\n", get_name(t->members.m[j].name));
								fprintf(output, "\t{\n");
								fprintf(output, "\t\tkinc_matrix3x3_t m = data->%s;\n", get_name(t->members.m[j].name));
								fprintf(output, "\t\tfloat *m_data = (float *)&data->%s;\n", get_name(t->members.m[j].name));
								fprintf(output, "\t\tm_data[0] = m.m[0];\n");
								fprintf(output, "\t\tm_data[1] = m.m[1];\n");
								fprintf(output, "\t\tm_data[2] = m.m[2];\n");
								fprintf(output, "\t\tm_data[3] = 0.0f;\n");
								fprintf(output, "\t\tm_data[4] = m.m[3];\n");
								fprintf(output, "\t\tm_data[5] = m.m[4];\n");
								fprintf(output, "\t\tm_data[6] = m.m[5];\n");
								fprintf(output, "\t\tm_data[7] = 0.0f;\n");
								fprintf(output, "\t\tm_data[8] = m.m[6];\n");
								fprintf(output, "\t\tm_data[9] = m.m[7];\n");
								fprintf(output, "\t\tm_data[10] = m.m[8];\n");
								fprintf(output, "\t\tm_data[11] = 0.0f;\n");
								fprintf(output, "\t}\n");
							}
						}
						fprintf(output, "\tkope_g5_buffer_unlock(buffer);\n");
					}
				}
				fprintf(output, "\tkope_g5_buffer_unlock(buffer);\n");
				fprintf(output, "}\n\n");
			}
		}

		uint32_t descriptor_table_index = 0;
		for (size_t set_index = 0; set_index < sets_count; ++set_index) {
			descriptor_set *set = sets[set_index];

			fprintf(output, "void kong_create_%s_set(kope_g5_device *device, const %s_parameters *parameters, %s_set *set) {\n", get_name(set->name),
			        get_name(set->name), get_name(set->name));

			size_t other_count = 0;
			size_t sampler_count = 0;

			for (size_t descriptor_index = 0; descriptor_index < set->definitions_count; ++descriptor_index) {
				definition d = set->definitions[descriptor_index];

				switch (d.kind) {
				case DEFINITION_CONST_CUSTOM:
					other_count += 1;
					break;
				case DEFINITION_TEX2D:
					other_count += 1;
					break;
				case DEFINITION_SAMPLER:
					sampler_count += 1;
					break;
				}
			}

			fprintf(output, "\tkope_d3d12_device_create_descriptor_set(device, %" PRIu64 ", %" PRIu64 ", &set->set);\n", other_count, sampler_count);

			size_t other_index = 0;
			size_t sampler_index = 0;

			for (size_t descriptor_index = 0; descriptor_index < set->definitions_count; ++descriptor_index) {
				definition d = set->definitions[descriptor_index];

				switch (d.kind) {
				case DEFINITION_CONST_CUSTOM:
					fprintf(output, "\tkope_d3d12_descriptor_set_set_buffer_view_cbv(device, &set->set, parameters->%s, %" PRIu64 ");\n",
					        get_name(get_global(d.global)->name), other_index);
					fprintf(output, "\tset->%s = parameters->%s;\n", get_name(get_global(d.global)->name), get_name(get_global(d.global)->name));
					other_index += 1;
					break;
				case DEFINITION_TEX2D: {
					attribute *write_attribute = find_attribute(&get_global(d.global)->attributes, add_name("write"));
					if (write_attribute != NULL) {
						fprintf(output, "\tkope_d3d12_descriptor_set_set_texture_view_uav(device, &set->set, parameters->%s, %" PRIu64 ");\n",
						        get_name(get_global(d.global)->name), other_index);
					}
					else {
						fprintf(output, "\tkope_d3d12_descriptor_set_set_texture_view_srv(device, &set->set, parameters->%s, %" PRIu64 ");\n",
						        get_name(get_global(d.global)->name), other_index);
					}
					fprintf(output, "\tset->%s = parameters->%s;\n", get_name(get_global(d.global)->name), get_name(get_global(d.global)->name));
					other_index += 1;
					break;
				}
				case DEFINITION_SAMPLER:
					fprintf(output, "\tkope_d3d12_descriptor_set_set_sampler(device, &set->set, parameters->%s, %" PRIu64 ");\n",
					        get_name(get_global(d.global)->name), sampler_index);
					sampler_index += 1;
					break;
				}
			}
			fprintf(output, "}\n\n");

			fprintf(output, "void kong_set_descriptor_set_%s(kope_g5_command_list *list, %s_set *set) {\n", get_name(set->name), get_name(set->name));
			for (size_t descriptor_index = 0; descriptor_index < set->definitions_count; ++descriptor_index) {
				definition d = set->definitions[descriptor_index];

				switch (d.kind) {
				case DEFINITION_CONST_CUSTOM:
					fprintf(output, "\tkope_d3d12_descriptor_set_prepare_cbv_buffer(list, set->%s);\n", get_name(get_global(d.global)->name));
					break;
				case DEFINITION_TEX2D: {
					attribute *write_attribute = find_attribute(&get_global(d.global)->attributes, add_name("write"));
					if (write_attribute != NULL) {
						fprintf(output, "\tkope_d3d12_descriptor_set_prepare_uav_texture(list, set->%s);\n", get_name(get_global(d.global)->name));
					}
					else {
						fprintf(output, "\tkope_d3d12_descriptor_set_prepare_srv_texture(list, set->%s);\n", get_name(get_global(d.global)->name));
					}
					break;
				}
				}
			}
			fprintf(output, "\n\tkope_d3d12_command_list_set_descriptor_table(list, %i, &set->set);\n", descriptor_table_index);
			fprintf(output, "}\n\n");

			descriptor_table_index += (sampler_count > 0) ? 2 : 1;
		}

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
				for (size_t j = 0; j < t->members.size; ++j) {
					if (t->members.m[j].name == add_name("vertex") || t->members.m[j].name == add_name("fragment")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "vertex or fragment expects an identifier");
						fprintf(output, "static kope_d3d12_shader %s;\n", get_name(t->members.m[j].value.identifier));
					}
				}
			}
		}

		for (function_id i = 0; get_function(i) != NULL; ++i) {
			function *f = get_function(i);
			if (has_attribute(&f->attributes, add_name("compute"))) {
				fprintf(output, "kope_d3d12_compute_pipeline %s;\n", get_name(f->name));
			}
		}

		if (api == API_WEBGPU) {
			fprintf(output, "\nvoid kinc_g5_internal_webgpu_create_shader_module(const void *source, size_t length);\n");
		}

		if (api == API_OPENGL) {
			fprintf(output, "\nvoid kinc_g4_internal_opengl_setup_uniform_block(unsigned program, const char *name, unsigned binding);\n");
		}

		fprintf(output, "\nvoid kong_init(kope_g5_device *device) {\n");

		if (api == API_WEBGPU) {
			fprintf(output, "\tkinc_g5_internal_webgpu_create_shader_module(wgsl, wgsl_size);\n\n");
		}

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
				fprintf(output, "\tkope_d3d12_render_pipeline_parameters %s_parameters = {0};\n\n", get_name(t->name));

				name_id vertex_shader_name = NO_NAME;
				name_id amplification_shader_name = NO_NAME;
				name_id mesh_shader_name = NO_NAME;
				name_id fragment_shader_name = NO_NAME;

				for (size_t j = 0; j < t->members.size; ++j) {
					if (t->members.m[j].name == add_name("vertex")) {
						fprintf(output, "\t%s_parameters.vertex.shader.data = %s_code;\n", get_name(t->name), get_name(t->members.m[j].value.identifier));
						fprintf(output, "\t%s_parameters.vertex.shader.size = %s_code_size;\n\n", get_name(t->name),
						        get_name(t->members.m[j].value.identifier));
						vertex_shader_name = t->members.m[j].value.identifier;
					}
					else if (t->members.m[j].name == add_name("amplification")) {
						amplification_shader_name = t->members.m[j].value.identifier;
					}
					else if (t->members.m[j].name == add_name("mesh")) {
						mesh_shader_name = t->members.m[j].value.identifier;
					}
					else if (t->members.m[j].name == add_name("fragment")) {
						fprintf(output, "\t%s_parameters.fragment.shader.data = %s_code;\n", get_name(t->name), get_name(t->members.m[j].value.identifier));
						fprintf(output, "\t%s_parameters.fragment.shader.size = %s_code_size;\n\n", get_name(t->name),
						        get_name(t->members.m[j].value.identifier));
						fragment_shader_name = t->members.m[j].value.identifier;
					}
					// else if (t->members.m[j].name == add_name("depth_write")) {
					//	debug_context context = {0};
					//	check(t->members.m[j].value.kind == TOKEN_BOOLEAN, context, "depth_write expects a bool");
					//	fprintf(output, "\t%s.depth_write = %s;\n\n", get_name(t->name), t->members.m[j].value.boolean ? "true" : "false");
					// }
					// else if (t->members.m[j].name == add_name("depth_mode")) {
					//	debug_context context = {0};
					//	check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "depth_mode expects an identifier");
					//	global *g = find_global(t->members.m[j].value.identifier);
					//	fprintf(output, "\t%s.depth_mode = %s;\n\n", get_name(t->name), convert_compare_mode(g->value.value.ints[0]));
					//}
					else if (t->members.m[j].name == add_name("blend_source")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "blend_source expects an identifier");
						global *g = find_global(t->members.m[j].value.identifier);
						fprintf(output, "\t%s.blend_source = %s;\n\n", get_name(t->name), convert_blend_mode(g->value.value.ints[0]));
					}
					else if (t->members.m[j].name == add_name("blend_destination")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "blend_destination expects an identifier");
						global *g = find_global(t->members.m[j].value.identifier);
						fprintf(output, "\t%s.blend_destination = %s;\n\n", get_name(t->name), convert_blend_mode(g->value.value.ints[0]));
					}
					else if (t->members.m[j].name == add_name("blend_operation")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "blend_operation expects an identifier");
						global *g = find_global(t->members.m[j].value.identifier);
						fprintf(output, "\t%s.blend_operation = %s;\n\n", get_name(t->name), convert_blend_op(g->value.value.ints[0]));
					}
					else if (t->members.m[j].name == add_name("alpha_blend_source")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "alpha_blend_source expects an identifier");
						global *g = find_global(t->members.m[j].value.identifier);
						fprintf(output, "\t%s.alpha_blend_source = %s;\n\n", get_name(t->name), convert_blend_mode(g->value.value.ints[0]));
					}
					else if (t->members.m[j].name == add_name("alpha_blend_destination")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "alpha_blend_destination expects an identifier");
						global *g = find_global(t->members.m[j].value.identifier);
						fprintf(output, "\t%s.alpha_blend_destination = %s;\n\n", get_name(t->name), convert_blend_mode(g->value.value.ints[0]));
					}
					else if (t->members.m[j].name == add_name("alpha_blend_operation")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "alpha_blend_operation expects an identifier");
						global *g = find_global(t->members.m[j].value.identifier);
						fprintf(output, "\t%s.alpha_blend_operation = %s;\n\n", get_name(t->name), convert_blend_op(g->value.value.ints[0]));
					}
					// else {
					//	debug_context context = {0};
					//	error(context, "Unsupported pipe member %s", get_name(t->members.m[j].name));
					// }
				}

				{
					debug_context context = {0};
					check(vertex_shader_name != NO_NAME || mesh_shader_name != NO_NAME, context, "No vertex or mesh shader name found");
					check(fragment_shader_name != NO_NAME, context, "No fragment shader name found");
				}

				function *vertex_function = NULL;
				function *fragment_function = NULL;

				type_id vertex_input = NO_TYPE;

				if (vertex_shader_name != NO_NAME) {
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
					check(vertex_shader_name == NO_NAME || vertex_function != NULL, context, "Vertex function not found");
					check(fragment_function != NULL, context, "Fragment function not found");
					check(vertex_function == NULL || vertex_input != NO_TYPE, context, "No vertex input found");
				}

				if (vertex_function != NULL) {
					for (type_id i = 0; get_type(i) != NULL; ++i) {
						if (i == vertex_input) {
							type *vertex_type = get_type(i);
							size_t offset = 0;

							for (size_t j = 0; j < vertex_type->members.size; ++j) {
								if (api == API_OPENGL) {
									fprintf(output, "\tkinc_g4_vertex_structure_add(&%s_structure, \"%s_%s\", %s);\n", get_name(t->name),
									        get_name(vertex_type->name), get_name(vertex_type->members.m[j].name),
									        structure_type(vertex_type->members.m[j].type.type));
								}
								else {
									fprintf(output, "\t%s_parameters.vertex.buffers[0].attributes[%" PRIu64 "].format = %s;\n", get_name(t->name), j,
									        structure_type(vertex_type->members.m[j].type.type));
									fprintf(output, "\t%s_parameters.vertex.buffers[0].attributes[%" PRIu64 "].offset = %" PRIu64 ";\n", get_name(t->name), j,
									        offset);
									fprintf(output, "\t%s_parameters.vertex.buffers[0].attributes[%" PRIu64 "].shader_location = %" PRIu64 ";\n",
									        get_name(t->name), j, j);
								}

								offset += base_type_size(vertex_type->members.m[j].type.type);
							}
							fprintf(output, "\t%s_parameters.vertex.buffers[0].attributes_count = %" PRIu64 ";\n", get_name(t->name),
							        vertex_type->members.size);
							fprintf(output, "\t%s_parameters.vertex.buffers[0].array_stride = %" PRIu64 ";\n", get_name(t->name), offset);
							fprintf(output, "\t%s_parameters.vertex.buffers[0].step_mode = KOPE_D3D12_VERTEX_STEP_MODE_VERTEX;\n", get_name(t->name));
							fprintf(output, "\t%s_parameters.vertex.buffers_count = 1;\n\n", get_name(t->name));
						}
					}
				}

				fprintf(output, "\t%s_parameters.primitive.topology = KOPE_D3D12_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.primitive.strip_index_format = KOPE_G5_INDEX_FORMAT_UINT16;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.primitive.front_face = KOPE_D3D12_FRONT_FACE_CW;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.primitive.cull_mode = KOPE_D3D12_CULL_MODE_NONE;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.primitive.unclipped_depth = false;\n\n", get_name(t->name));

				fprintf(output, "\t%s_parameters.depth_stencil.format = KOPE_G5_TEXTURE_FORMAT_DEPTH32FLOAT;\n", get_name(t->name));
				member *depth_write = find_member(t, "depth_write");
				if (depth_write != NULL) {
					debug_context context = {0};
					check(depth_write->value.kind == TOKEN_BOOLEAN, context, "depth_write expects a bool");
					fprintf(output, "\t%s_parameters.depth_stencil.depth_write_enabled = %s;\n", get_name(t->name),
					        depth_write->value.boolean ? "true" : "false");
				}
				else {
					fprintf(output, "\t%s_parameters.depth_stencil.depth_write_enabled = false;\n", get_name(t->name));
				}

				member *depth_compare = find_member(t, "depth_compare");
				if (depth_compare != NULL) {
					debug_context context = {0};
					check(depth_compare->value.kind == TOKEN_IDENTIFIER, context, "depth_compare expects an identifier");
					global *g = find_global(depth_compare->value.identifier);
					fprintf(output, "\t%s_parameters.depth_stencil.depth_compare = %s;\n", get_name(t->name), convert_compare_mode(g->value.value.ints[0]));
				}
				else {
					fprintf(output, "\t%s_parameters.depth_stencil.depth_compare = KOPE_G5_COMPARE_FUNCTION_ALWAYS;\n", get_name(t->name));
				}

				fprintf(output, "\t%s_parameters.depth_stencil.stencil_front.compare = KOPE_G5_COMPARE_FUNCTION_ALWAYS;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.depth_stencil.stencil_front.fail_op = KOPE_D3D12_STENCIL_OPERATION_KEEP;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.depth_stencil.stencil_front.depth_fail_op = KOPE_D3D12_STENCIL_OPERATION_KEEP;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.depth_stencil.stencil_front.pass_op = KOPE_D3D12_STENCIL_OPERATION_KEEP;\n", get_name(t->name));

				fprintf(output, "\t%s_parameters.depth_stencil.stencil_back.compare = KOPE_G5_COMPARE_FUNCTION_ALWAYS;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.depth_stencil.stencil_back.fail_op = KOPE_D3D12_STENCIL_OPERATION_KEEP;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.depth_stencil.stencil_back.depth_fail_op = KOPE_D3D12_STENCIL_OPERATION_KEEP;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.depth_stencil.stencil_back.pass_op = KOPE_D3D12_STENCIL_OPERATION_KEEP;\n", get_name(t->name));

				fprintf(output, "\t%s_parameters.depth_stencil.stencil_read_mask = 0xffffffff;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.depth_stencil.stencil_write_mask = 0xffffffff;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.depth_stencil.depth_bias = 0;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.depth_stencil.depth_bias_slope_scale = 0.0f;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.depth_stencil.depth_bias_clamp = 0.0f;\n\n", get_name(t->name));

				fprintf(output, "\t%s_parameters.multisample.count = 1;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.multisample.mask = 0xffffffff;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.multisample.alpha_to_coverage_enabled = false;\n\n", get_name(t->name));

				if (fragment_function->return_type.array_size > 0) {
					fprintf(output, "\t%s_parameters.fragment.targets_count = %i;\n", get_name(t->name), fragment_function->return_type.array_size);
					for (uint32_t i = 0; i < fragment_function->return_type.array_size; ++i) {
						fprintf(output, "\t%s.color_attachment[%i] = KINC_G4_RENDER_TARGET_FORMAT_32BIT;\n", get_name(t->name), i);
					}
					fprintf(output, "\n");
				}
				else {
					fprintf(output, "\t%s_parameters.fragment.targets_count = 1;\n", get_name(t->name));
					fprintf(output, "\t%s_parameters.fragment.targets[0].format = KOPE_G5_TEXTURE_FORMAT_RGBA8_UNORM;\n", get_name(t->name));

					fprintf(output, "\t%s_parameters.fragment.targets[0].blend.color.operation = KOPE_D3D12_BLEND_OPERATION_ADD;\n", get_name(t->name));
					fprintf(output, "\t%s_parameters.fragment.targets[0].blend.color.src_factor = KOPE_D3D12_BLEND_FACTOR_ONE;\n", get_name(t->name));
					fprintf(output, "\t%s_parameters.fragment.targets[0].blend.color.dst_factor = KOPE_D3D12_BLEND_FACTOR_ZERO;\n", get_name(t->name));

					fprintf(output, "\t%s_parameters.fragment.targets[0].blend.alpha.operation = KOPE_D3D12_BLEND_OPERATION_ADD;\n", get_name(t->name));
					fprintf(output, "\t%s_parameters.fragment.targets[0].blend.alpha.src_factor = KOPE_D3D12_BLEND_FACTOR_ONE;\n", get_name(t->name));
					fprintf(output, "\t%s_parameters.fragment.targets[0].blend.alpha.dst_factor = KOPE_D3D12_BLEND_FACTOR_ZERO;\n", get_name(t->name));

					fprintf(output, "\t%s_parameters.fragment.targets[0].write_mask = 0xf;\n\n", get_name(t->name));
				}

				fprintf(output, "\tkope_d3d12_render_pipeline_init(&device->d3d12, &%s, &%s_parameters);\n\n", get_name(t->name), get_name(t->name));

				if (api == API_OPENGL) {
					global_id globals[256];
					size_t globals_size = 0;
					find_referenced_globals(vertex_function, globals, &globals_size);
					find_referenced_globals(fragment_function, globals, &globals_size);

					for (global_id i = 0; i < globals_size; ++i) {
						global *g = get_global(globals[i]);
						if (g->type == sampler_type_id) {
						}
						else if (g->type == tex2d_type_id || g->type == texcube_type_id) {
						}
						else if (g->type == float_id) {
						}
						else {
							fprintf(output, "\tkinc_g4_internal_opengl_setup_uniform_block(%s.impl.programId, \"_%" PRIu64 "\", %i);\n\n", get_name(t->name),
							        g->var_index, global_register_indices[i]);
						}
					}
				}
			}
		}

		for (function_id i = 0; get_function(i) != NULL; ++i) {
			function *f = get_function(i);
			if (has_attribute(&f->attributes, add_name("compute"))) {
				fprintf(output, "\tkope_d3d12_compute_pipeline_parameters %s_parameters;\n", get_name(f->name));
				fprintf(output, "\t%s_parameters.shader.data = %s_code;\n", get_name(f->name), get_name(f->name));
				fprintf(output, "\t%s_parameters.shader.size = %s_code_size;\n", get_name(f->name), get_name(f->name));
				fprintf(output, "\tkope_d3d12_compute_pipeline_init(&device->d3d12, &%s, &%s_parameters);\n", get_name(f->name), get_name(f->name));
			}
		}

		fprintf(output, "}\n");

		fclose(output);
	}
}
