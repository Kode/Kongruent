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
	if (type == int_id) {
		return "int";
	}
	if (type == int2_id) {
		return "kope_int2";
	}
	if (type == int3_id) {
		return "kope_int3";
	}
	if (type == int4_id) {
		return "kope_int4";
	}
	if (type == uint_id) {
		return "unsigned";
	}
	if (type == uint2_id) {
		return "kope_uint2";
	}
	if (type == uint3_id) {
		return "kope_uint3";
	}
	if (type == uint4_id) {
		return "kope_uint4";
	}
	return get_name(get_type(type)->name);
}

static const char *structure_type(type_id type, api_kind api) {
	if (api == API_DIRECT3D12) {
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
	}
	else if (api == API_METAL) {
		if (type == float_id) {
			return "KOPE_METAL_VERTEX_FORMAT_FLOAT32";
		}
		if (type == float2_id) {
			return "KOPE_METAL_VERTEX_FORMAT_FLOAT32X2";
		}
		if (type == float3_id) {
			return "KOPE_METAL_VERTEX_FORMAT_FLOAT32X3";
		}
		if (type == float4_id) {
			return "KOPE_METAL_VERTEX_FORMAT_FLOAT32X4";
		}
	}
	else if (api == API_VULKAN) {
		if (type == float_id) {
			return "KOPE_VULKAN_VERTEX_FORMAT_FLOAT32";
		}
		if (type == float2_id) {
			return "KOPE_VULKAN_VERTEX_FORMAT_FLOAT32X2";
		}
		if (type == float3_id) {
			return "KOPE_VULKAN_VERTEX_FORMAT_FLOAT32X3";
		}
		if (type == float4_id) {
			return "KOPE_VULKAN_VERTEX_FORMAT_FLOAT32X4";
		}
	}
	debug_context context = {0};
	error(context, "Unknown type for vertex structure");
	return "UNKNOWN";
}

static const char *convert_compare_mode(int mode) {
	switch (mode) {
	case 0:
		return "KOPE_G5_COMPARE_FUNCTION_ALWAYS";
	case 1:
		return "KOPE_G5_COMPARE_FUNCTION_NEVER";
	case 2:
		return "KOPE_G5_COMPARE_FUNCTION_EQUAL";
	case 3:
		return "KOPE_G5_COMPARE_FUNCTION_NOT_EQUAL";
	case 4:
		return "KOPE_G5_COMPARE_FUNCTION_LESS";
	case 5:
		return "KOPE_G5_COMPARE_FUNCTION_LESS_EQUAL";
	case 6:
		return "KOPE_G5_COMPARE_FUNCTION_GREATER";
	case 7:
		return "KOPE_G5_COMPARE_FUNCTION_GREATER_EQUAL";
	default: {
		debug_context context = {0};
		error(context, "Unknown compare mode");
		return "UNKNOWN";
	}
	}
}

static const char *convert_texture_format(int format) {
	switch (format) {
	case 0:
		return "KOPE_G5_TEXTURE_FORMAT_R8_UNORM";
	case 1:
		return "KOPE_G5_TEXTURE_FORMAT_R8_SNORM";
	case 2:
		return "KOPE_G5_TEXTURE_FORMAT_R8_UINT";
	case 3:
		return "KOPE_G5_TEXTURE_FORMAT_R8_SINT";
	case 4:
		return "KOPE_G5_TEXTURE_FORMAT_R16_UINT";
	case 5:
		return "KOPE_G5_TEXTURE_FORMAT_R16_SINT";
	case 6:
		return "KOPE_G5_TEXTURE_FORMAT_R16_FLOAT";
	case 7:
		return "KOPE_G5_TEXTURE_FORMAT_RG8_UNORM";
	case 8:
		return "KOPE_G5_TEXTURE_FORMAT_RG8_SNORM";
	case 9:
		return "KOPE_G5_TEXTURE_FORMAT_RG8_UINT";
	case 10:
		return "KOPE_G5_TEXTURE_FORMAT_RG8_SINT";
	case 11:
		return "KOPE_G5_TEXTURE_FORMAT_R32_UINT";
	case 12:
		return "KOPE_G5_TEXTURE_FORMAT_R32_SINT";
	case 13:
		return "KOPE_G5_TEXTURE_FORMAT_R32_FLOAT";
	case 14:
		return "KOPE_G5_TEXTURE_FORMAT_RG16_UINT";
	case 15:
		return "KOPE_G5_TEXTURE_FORMAT_RG16_SINT";
	case 16:
		return "KOPE_G5_TEXTURE_FORMAT_RG16_FLOAT";
	case 17:
		return "KOPE_G5_TEXTURE_FORMAT_RGBA8_UNORM";
	case 18:
		return "KOPE_G5_TEXTURE_FORMAT_RGBA8_UNORM_SRGB";
	case 19:
		return "KOPE_G5_TEXTURE_FORMAT_RGBA8_SNORM";
	case 20:
		return "KOPE_G5_TEXTURE_FORMAT_RGBA8_UINT";
	case 21:
		return "KOPE_G5_TEXTURE_FORMAT_RGBA8_SINT";
	case 22:
		return "KOPE_G5_TEXTURE_FORMAT_BGRA8_UNORM";
	case 23:
		return "KOPE_G5_TEXTURE_FORMAT_BGRA8_UNORM_SRGB";
	case 24:
		return "KOPE_G5_TEXTURE_FORMAT_RGB9E5U_FLOAT";
	case 25:
		return "KOPE_G5_TEXTURE_FORMAT_RGB10A2_UINT";
	case 26:
		return "KOPE_G5_TEXTURE_FORMAT_RGB10A2_UNORM";
	case 27:
		return "KOPE_G5_TEXTURE_FORMAT_RG11B10U_FLOAT";
	case 28:
		return "KOPE_G5_TEXTURE_FORMAT_RG32_UINT";
	case 29:
		return "KOPE_G5_TEXTURE_FORMAT_RG32_SINT";
	case 30:
		return "KOPE_G5_TEXTURE_FORMAT_RG32_FLOAT";
	case 31:
		return "KOPE_G5_TEXTURE_FORMAT_RGBA16_UINT";
	case 32:
		return "KOPE_G5_TEXTURE_FORMAT_RGBA16_SINT";
	case 33:
		return "KOPE_G5_TEXTURE_FORMAT_RGBA16_FLOAT";
	case 34:
		return "KOPE_G5_TEXTURE_FORMAT_RGBA32_UINT";
	case 35:
		return "KOPE_G5_TEXTURE_FORMAT_RGBA32_SINT";
	case 36:
		return "KOPE_G5_TEXTURE_FORMAT_RGBA32_FLOAT";
	case 37:
		return "KOPE_G5_TEXTURE_FORMAT_DEPTH16_UNORM";
	case 38:
		return "KOPE_G5_TEXTURE_FORMAT_DEPTH24PLUS_NOTHING8";
	case 39:
		return "KOPE_G5_TEXTURE_FORMAT_DEPTH24PLUS_STENCIL8";
	case 40:
		return "KOPE_G5_TEXTURE_FORMAT_DEPTH32FLOAT";
	case 41:
		return "KOPE_G5_TEXTURE_FORMAT_DEPTH32FLOAT_STENCIL8_NOTHING24";
	default: {
		debug_context context = {0};
		error(context, "Unknown texture format");
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

bool is_texture(type_id t) {
	return t == tex2d_type_id || t == tex2darray_type_id || t == texcube_type_id;
}

#define CASE_TEXTURE                                                                                                                                           \
	case DEFINITION_TEX2D:                                                                                                                                     \
	case DEFINITION_TEX2DARRAY:                                                                                                                                \
	case DEFINITION_TEXCUBE

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

static void write_root_signature(FILE *output, descriptor_set *all_descriptor_sets[256], size_t all_descriptor_sets_count) {
	uint32_t cbv_index = 0;
	uint32_t srv_index = 0;
	uint32_t uav_index = 0;
	uint32_t sampler_index = 0;

	uint32_t table_count = 0;

	for (size_t set_index = 0; set_index < all_descriptor_sets_count; ++set_index) {
		descriptor_set *set = all_descriptor_sets[set_index];

		bool has_sampler = false;
		bool has_other = false;

		for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
			definition *def = &set->definitions[definition_index];

			switch (def->kind) {
			case DEFINITION_CONST_CUSTOM:
			CASE_TEXTURE:
			case DEFINITION_BVH:
				has_other = true;
				break;
			case DEFINITION_SAMPLER:
				has_sampler = true;
				break;
			default:
				assert(false);
				break;
			}
		}

		if (has_other && has_sampler) {
			table_count += 2;
		}
		else if (has_other || has_sampler) {
			table_count += 1;
		}
	}

	fprintf(output, "\tD3D12_ROOT_PARAMETER params[%i] = {};\n", table_count);

	uint32_t table_index = 0;

	for (size_t set_count = 0; set_count < all_descriptor_sets_count; ++set_count) {
		descriptor_set *set = all_descriptor_sets[set_count];

		bool has_sampler = false;
		bool has_other = false;

		for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
			definition *def = &set->definitions[definition_index];

			switch (def->kind) {
			case DEFINITION_CONST_CUSTOM:
			CASE_TEXTURE:
			case DEFINITION_BVH:
				has_other = true;
				break;
			case DEFINITION_SAMPLER:
				has_sampler = true;
				break;
			default:
				assert(false);
				break;
			}
		}

		if (has_other) {
			size_t count = 0;

			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition *def = &set->definitions[definition_index];

				switch (def->kind) {
				case DEFINITION_CONST_CUSTOM:
				CASE_TEXTURE:
				case DEFINITION_BVH: {
					count += 1;
					break;
				}
				default:
					assert(false);
					break;
				}
			}

			fprintf(output, "\n\tD3D12_DESCRIPTOR_RANGE ranges%i[%zu] = {};\n", table_index, count);

			size_t range_index = 0;
			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition *def = &set->definitions[definition_index];

				switch (def->kind) {
				case DEFINITION_CONST_CUSTOM:
					fprintf(output, "\tranges%i[%zu].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;\n", table_index, range_index);
					fprintf(output, "\tranges%i[%zu].BaseShaderRegister = %i;\n", table_index, range_index, cbv_index);
					fprintf(output, "\tranges%i[%zu].NumDescriptors = 1;\n", table_index, range_index);
					fprintf(output, "\tranges%i[%zu].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;\n", table_index, range_index);

					cbv_index += 1;

					range_index += 1;

					break;
				CASE_TEXTURE: {
					attribute *write_attribute = find_attribute(&get_global(def->global)->attributes, add_name("write"));

					if (write_attribute != NULL) {
						fprintf(output, "\tranges%i[%zu].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;\n", table_index, range_index);
						fprintf(output, "\tranges%i[%zu].BaseShaderRegister = %i;\n", table_index, range_index, uav_index);
						fprintf(output, "\tranges%i[%zu].NumDescriptors = 1;\n", table_index, range_index);
						fprintf(output, "\tranges%i[%zu].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;\n", table_index,
						        range_index);

						uav_index += 1;
					}
					else {
						fprintf(output, "\tranges%i[%zu].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;\n", table_index, range_index);
						fprintf(output, "\tranges%i[%zu].BaseShaderRegister = %i;\n", table_index, range_index, srv_index);
						fprintf(output, "\tranges%i[%zu].NumDescriptors = 1;\n", table_index, range_index);
						fprintf(output, "\tranges%i[%zu].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;\n", table_index,
						        range_index);

						srv_index += 1;
					}

					range_index += 1;

					break;
				default:
					assert(false);
					break;
				}
				case DEFINITION_BVH:
					fprintf(output, "\tranges%i[%zu].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;\n", table_index, range_index);
					fprintf(output, "\tranges%i[%zu].BaseShaderRegister = %i;\n", table_index, range_index, srv_index);
					fprintf(output, "\tranges%i[%zu].NumDescriptors = 1;\n", table_index, range_index);
					fprintf(output, "\tranges%i[%zu].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;\n", table_index, range_index);

					srv_index += 1;

					range_index += 1;

					break;
				}
			}

			fprintf(output, "\n\tparams[%i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;\n", table_index);
			fprintf(output, "\tparams[%i].DescriptorTable.NumDescriptorRanges = %zu;\n", table_index, count);
			fprintf(output, "\tparams[%i].DescriptorTable.pDescriptorRanges = ranges%i;\n", table_index, table_index);

			table_index += 1;
		}

		if (has_sampler) {
			size_t count = 0;

			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition *def = &set->definitions[definition_index];

				switch (def->kind) {
				case DEFINITION_SAMPLER: {
					count += 1;
					break;
				}
				default:
					assert(false);
					break;
				}
			}

			fprintf(output, "\n\tD3D12_DESCRIPTOR_RANGE ranges%i[%zu] = {};\n", table_index, count);

			size_t range_index = 0;
			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition *def = &set->definitions[definition_index];

				switch (def->kind) {
				case DEFINITION_SAMPLER:
					fprintf(output, "\tranges%i[%zu].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;\n", table_index, range_index);
					fprintf(output, "\tranges%i[%zu].BaseShaderRegister = %i;\n", table_index, range_index, sampler_index);
					fprintf(output, "\tranges%i[%zu].NumDescriptors = 1;\n", table_index, range_index);
					fprintf(output, "\tranges%i[%zu].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;\n", table_index, range_index);
					sampler_index += 1;
					break;
				default:
					assert(false);
					break;
				}
			}

			fprintf(output, "\n\tparams[%i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;\n", table_index);
			fprintf(output, "\tparams[%i].DescriptorTable.NumDescriptorRanges = %zu;\n", table_index, count);
			fprintf(output, "\tparams[%i].DescriptorTable.pDescriptorRanges = ranges%i;\n", table_index, table_index);

			table_index += 1;
		}
	}

	fprintf(output, "\n\tD3D12_ROOT_SIGNATURE_DESC desc = {};\n");
	fprintf(output, "\tdesc.NumParameters = %i;\n", table_count);
	fprintf(output, "\tdesc.pParameters = params;\n");

	fprintf(output, "\n\tID3D12RootSignature* root_signature;\n");
	fprintf(output, "\tID3DBlob *blob;\n");
	fprintf(output, "\tD3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, nullptr);\n");
	fprintf(output, "\tdevice->d3d12.device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root_signature));\n");
	fprintf(output, "\tblob->Release();\n");

	fprintf(output, "\n\treturn root_signature;\n");
}

static void to_upper(char *from, char *to) {
	size_t from_size = strlen(from);
	for (size_t i = 0; i < from_size; ++i) {
		if (from[i] >= 'a' && from[i] <= 'z') {
			to[i] = from[i] + ('A' - 'a');
		}
		else {
			to[i] = from[i];
		}
	}
	to[from_size] = 0;
}

static int global_register_indices[512];

void kope_export(char *directory, api_kind api) {
	memset(global_register_indices, 0, sizeof(global_register_indices));

	char *api_short = NULL;
	char *api_long = NULL;
	char *api_caps = NULL;
	switch (api) {
	case API_DIRECT3D9:
		api_short = "d3d9";
		api_long = "direct3d9";
		api_caps = "D3D9";
		break;
	case API_DIRECT3D11:
		api_short = "d3d11";
		api_long = "direct3d11";
		api_caps = "D3D11";
		break;
	case API_DIRECT3D12:
		api_short = "d3d12";
		api_long = "direct3d12";
		api_caps = "D3D12";
		break;
	case API_METAL:
		api_short = "metal";
		api_long = "metal";
		api_caps = "METAL";
		break;
	case API_OPENGL:
		api_short = "gl";
		api_long = "opengl";
		api_caps = "GL";
		break;
	case API_VULKAN:
		api_short = "vulkan";
		api_long = "vulkan";
		api_caps = "VULKAN";
		break;
	case API_WEBGPU:
		api_short = "webgpu";
		api_long = "webgpu";
		api_caps = "WEBGPU";
		break;
	default:
		assert(false);
		break;
	}

	if (api == API_WEBGPU) {
		int binding_index = 0;

		for (global_id i = 0; get_global(i)->type != NO_TYPE; ++i) {
			global *g = get_global(i);
			if (g->type == sampler_type_id) {
				global_register_indices[i] = binding_index;
				binding_index += 1;
			}
			else if (is_texture(g->type)) {
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
			else if (is_texture(g->type)) {
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

	type_id vertex_inputs[256] = {0};
	size_t vertex_input_slots[256] = {0};
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
				for (function_id i = 0; get_function(i) != NULL; ++i) {
					function *f = get_function(i);
					if (f->name == vertex_shader_name) {
						check(f->parameters_size > 0, context, "Vertex function requires at least one parameter");

						for (size_t input_index = 0; input_index < f->parameters_size; ++input_index) {
							vertex_inputs[vertex_inputs_size] = f->parameter_types[input_index].type;
							vertex_input_slots[vertex_inputs_size] = input_index;
							vertex_inputs_size += 1;
						}

						break;
					}
				}
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
		fprintf(output, "#include <kope/%s/descriptorset_structs.h>\n", api_long);
		fprintf(output, "#include <kope/%s/pipeline_structs.h>\n", api_long);
		fprintf(output, "#include <kinc/math/matrix.h>\n");
		fprintf(output, "#include <kinc/math/vector.h>\n\n");

		fprintf(output, "#ifdef __cplusplus\n");
		fprintf(output, "extern \"C\" {\n");
		fprintf(output, "#endif\n");

		fprintf(output, "\nvoid kong_init(kope_g5_device *device);\n\n");

		for (global_id i = 0; get_global(i) != NULL && get_global(i)->type != NO_TYPE; ++i) {
			global *g = get_global(i);

			type_id base_type = get_type(g->type)->kind == TYPE_ARRAY ? get_type(g->type)->array.base : g->type;

			if (is_texture(base_type) || base_type == sampler_type_id) {
			}
			else if (!get_type(base_type)->built_in) {
				type *t = get_type(base_type);

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
						fprintf(output, "\tfloat pad%zu[3];\n", j);
					}
				}
				fprintf(output, "} %s;\n\n", name);

				if (g->set != NULL && g->set->name == add_name("root_constants")) {
					fprintf(output, "void kong_set_root_constants_%s(kope_g5_command_list *list, %s *constants);\n", get_name(g->name), name);
				}
				else {
					fprintf(output, "void %s_buffer_create(kope_g5_device *device, kope_g5_buffer *buffer, uint32_t count);\n", name);
					fprintf(output, "void %s_buffer_destroy(kope_g5_buffer *buffer);\n", name);
					fprintf(output, "%s *%s_buffer_lock(kope_g5_buffer *buffer, uint32_t index, uint32_t count);\n", name, name);
					fprintf(output, "%s *%s_buffer_try_to_lock(kope_g5_buffer *buffer, uint32_t index, uint32_t count);\n", name, name);
					fprintf(output, "void %s_buffer_unlock(kope_g5_buffer *buffer);\n", name);
				}
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

			if (set->name == add_name("root_constants")) {
				continue;
			}

			fprintf(output, "typedef struct %s_parameters {\n", get_name(set->name));
			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition d = set->definitions[definition_index];
				switch (d.kind) {
				case DEFINITION_CONST_CUSTOM:
					fprintf(output, "\tkope_g5_buffer *%s;\n", get_name(get_global(d.global)->name));
					break;
				CASE_TEXTURE: {
					type *t = get_type(get_global(d.global)->type);
					if (t->kind == TYPE_ARRAY && t->array.array_size == -1) {
						fprintf(output, "\tkope_g5_texture_view *%s;\n", get_name(get_global(d.global)->name));
						fprintf(output, "\tsize_t %s_count;\n", get_name(get_global(d.global)->name));
					}
					else {
						fprintf(output, "\tkope_g5_texture_view %s;\n", get_name(get_global(d.global)->name));
					}
					break;
				}
				case DEFINITION_SAMPLER:
					fprintf(output, "\tkope_g5_sampler *%s;\n", get_name(get_global(d.global)->name));
					break;
				case DEFINITION_BVH:
					fprintf(output, "\tkope_g5_raytracing_hierarchy *%s;\n", get_name(get_global(d.global)->name));
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
			fprintf(output, "\tkope_%s_descriptor_set set;\n\n", api_short);
			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition d = set->definitions[definition_index];
				switch (d.kind) {
				case DEFINITION_CONST_CUSTOM:
					fprintf(output, "\tkope_g5_buffer *%s;\n", get_name(get_global(d.global)->name));
					break;
				case DEFINITION_BVH:
					fprintf(output, "\tkope_g5_raytracing_hierarchy *%s;\n", get_name(get_global(d.global)->name));
					break;
				CASE_TEXTURE: {
					type *t = get_type(get_global(d.global)->type);
					if (t->kind == TYPE_ARRAY && t->array.array_size == -1) {
						fprintf(output, "\tkope_g5_texture_view *%s;\n", get_name(get_global(d.global)->name));
						fprintf(output, "\tsize_t %s_count;\n", get_name(get_global(d.global)->name));
					}
					else {
						fprintf(output, "\tkope_g5_texture_view %s;\n", get_name(get_global(d.global)->name));
					}
					break;
				}
				default:
					break;
				}
			}
			fprintf(output, "} %s_set;\n\n", get_name(set->name));

			fprintf(output, "void kong_create_%s_set(kope_g5_device *device, const %s_parameters *parameters, %s_set *set);\n", get_name(set->name),
			        get_name(set->name), get_name(set->name));

			fprintf(output, "void kong_set_descriptor_set_%s(kope_g5_command_list *list, %s_set *set", get_name(set->name), get_name(set->name));
			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition d = set->definitions[definition_index];

				switch (d.kind) {
				case DEFINITION_CONST_CUSTOM:
					if (has_attribute(&get_global(d.global)->attributes, add_name("indexed"))) {
						fprintf(output, ", uint32_t %s_index", get_name(get_global(d.global)->name));
					}
					break;
				default:
					break;
				}
			}
			fprintf(output, ");\n\n");

			char upper_set_name[256];
			to_upper(get_name(set->name), upper_set_name);

			fprintf(output, "typedef struct %s_set_update {\n", get_name(set->name));
			fprintf(output, "\tenum {\n");
			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition d = set->definitions[definition_index];
				char upper_definition_name[256];
				to_upper(get_name(get_global(d.global)->name), upper_definition_name);

				switch (d.kind) {
				case DEFINITION_CONST_CUSTOM:
					fprintf(output, "\t\t%s_SET_UPDATE_%s,\n", upper_set_name, upper_definition_name);
					break;
				case DEFINITION_BVH:
					fprintf(output, "\t\t%s_SET_UPDATE_%s,\n", upper_set_name, upper_definition_name);
					break;
				CASE_TEXTURE: {
					// type *t = get_type(get_global(d.global)->type);
					fprintf(output, "\t\t%s_SET_UPDATE_%s,\n", upper_set_name, upper_definition_name);
					break;
				}
				default:
					break;
				}
			}
			fprintf(output, "\t} kind;\n");

			fprintf(output, "\tunion {\n");
			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition d = set->definitions[definition_index];
				switch (d.kind) {
				case DEFINITION_CONST_CUSTOM:
					fprintf(output, "\t\tkope_g5_buffer *%s;\n", get_name(get_global(d.global)->name));
					break;
				case DEFINITION_BVH:
					fprintf(output, "\t\tkope_g5_raytracing_hierarchy *%s;\n", get_name(get_global(d.global)->name));
					break;
				CASE_TEXTURE: {
					type *t = get_type(get_global(d.global)->type);
					if (t->kind == TYPE_ARRAY && t->array.array_size == -1) {
						fprintf(output, "\t\tstruct {\n");
						fprintf(output, "\t\t\tkope_g5_texture_view *%s;\n", get_name(get_global(d.global)->name));
						fprintf(output, "\t\t\tuint32_t *%s_indices;\n", get_name(get_global(d.global)->name));
						fprintf(output, "\t\t\tsize_t %s_count;\n", get_name(get_global(d.global)->name));
						fprintf(output, "\t\t} %s;\n", get_name(get_global(d.global)->name));
					}
					else {
						fprintf(output, "\t\tkope_g5_texture_view %s;\n", get_name(get_global(d.global)->name));
					}
					break;
				}
				default:
					break;
				}
			}
			fprintf(output, "\t};\n");

			fprintf(output, "} %s_set_update;\n\n", get_name(set->name));

			fprintf(output, "void kong_update_%s_set(%s_set *set, %s_set_update *updates, uint32_t updates_count);\n", get_name(set->name), get_name(set->name),
			        get_name(set->name));
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
			fprintf(output, "%s *kong_%s_buffer_try_to_lock(%s_buffer *buffer);\n", get_name(t->name), get_name(t->name), get_name(t->name));
			fprintf(output, "void kong_%s_buffer_unlock(%s_buffer *buffer);\n", get_name(t->name), get_name(t->name));
			fprintf(output, "void kong_set_vertex_buffer_%s(kope_g5_command_list *list, %s_buffer *buffer);\n\n", get_name(t->name), get_name(t->name));
		}

		fprintf(output, "void kong_set_render_pipeline(kope_g5_command_list *list, kope_%s_render_pipeline *pipeline);\n\n", api_short);

		fprintf(output, "void kong_set_compute_pipeline(kope_g5_command_list *list, kope_%s_compute_pipeline *pipeline);\n\n", api_short);

		fprintf(output, "void kong_set_ray_pipeline(kope_g5_command_list *list, kope_%s_ray_pipeline *pipeline);\n\n", api_short);

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
				fprintf(output, "extern kope_%s_render_pipeline %s;\n\n", api_short, get_name(t->name));
			}
		}

		for (function_id i = 0; get_function(i) != NULL; ++i) {
			function *f = get_function(i);
			if (has_attribute(&f->attributes, add_name("compute"))) {
				fprintf(output, "extern kope_%s_compute_pipeline %s;\n\n", api_short, get_name(f->name));
			}
		}

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && has_attribute(&t->attributes, add_name("raypipe"))) {
				fprintf(output, "extern kope_%s_ray_pipeline %s;\n\n", api_short, get_name(t->name));
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

		fprintf(output, "\n#include <kope/%s/buffer_functions.h>\n", api_long);
		fprintf(output, "#include <kope/%s/commandlist_functions.h>\n", api_long);
		fprintf(output, "#include <kope/%s/device_functions.h>\n", api_long);
		fprintf(output, "#include <kope/%s/descriptorset_functions.h>\n", api_long);
		fprintf(output, "#include <kope/%s/pipeline_functions.h>\n", api_long);
		fprintf(output, "#include <kope/util/align.h>\n\n");
		fprintf(output, "#include <assert.h>\n");
		fprintf(output, "#include <stdlib.h>\n\n");

		if (api == API_VULKAN) {
			fprintf(output, "#include <kope/vulkan/vulkanunit.h>\n\n");
		}

		for (size_t i = 0; i < vertex_inputs_size; ++i) {
			type *t = get_type(vertex_inputs[i]);

			fprintf(output, "void kong_create_buffer_%s(kope_g5_device * device, size_t count, %s_buffer *buffer) {\n", get_name(t->name), get_name(t->name));
			fprintf(output, "\tkope_g5_buffer_parameters parameters;\n");
			fprintf(output, "\tparameters.size = count * sizeof(%s);\n", get_name(t->name));
			fprintf(output, "\tparameters.usage_flags = KOPE_G5_BUFFER_USAGE_VERTEX | KOPE_G5_BUFFER_USAGE_CPU_WRITE;\n");
			fprintf(output, "\tkope_g5_device_create_buffer(device, &parameters, &buffer->buffer);\n");
			fprintf(output, "\tbuffer->count = count;\n");
			fprintf(output, "}\n\n");

			fprintf(output, "%s *kong_%s_buffer_lock(%s_buffer *buffer) {\n", get_name(t->name), get_name(t->name), get_name(t->name));
			fprintf(output, "\treturn (%s *)kope_%s_buffer_lock_all(&buffer->buffer);\n", get_name(t->name), api_short);
			fprintf(output, "}\n\n");

			fprintf(output, "%s *kong_%s_buffer_try_to_lock(%s_buffer *buffer) {\n", get_name(t->name), get_name(t->name), get_name(t->name));
			fprintf(output, "\treturn (%s *)kope_%s_buffer_try_to_lock_all(&buffer->buffer);\n", get_name(t->name), api_short);
			fprintf(output, "}\n\n");

			fprintf(output, "void kong_%s_buffer_unlock(%s_buffer *buffer) {\n", get_name(t->name), get_name(t->name));
			fprintf(output, "\tkope_%s_buffer_unlock(&buffer->buffer);\n", api_short);
			fprintf(output, "}\n\n");

			fprintf(output, "void kong_set_vertex_buffer_%s(kope_g5_command_list *list, %s_buffer *buffer) {\n", get_name(t->name), get_name(t->name));
			fprintf(output, "\tkope_%s_command_list_set_vertex_buffer(list, %zu, &buffer->buffer.%s, 0, buffer->count * sizeof(%s), sizeof(%s));\n", api_short,
			        vertex_input_slots[i], api_short, get_name(t->name), get_name(t->name));
			fprintf(output, "}\n\n");
		}

		fprintf(output, "void kong_set_render_pipeline(kope_g5_command_list *list, kope_%s_render_pipeline *pipeline) {\n", api_short);
		fprintf(output, "\tkope_%s_command_list_set_render_pipeline(list, pipeline);\n", api_short);
		fprintf(output, "}\n\n");

		fprintf(output, "void kong_set_compute_pipeline(kope_g5_command_list *list, kope_%s_compute_pipeline *pipeline) {\n", api_short);
		fprintf(output, "\tkope_%s_command_list_set_compute_pipeline(list, pipeline);\n", api_short);
		fprintf(output, "}\n\n");

		fprintf(output, "void kong_set_ray_pipeline(kope_g5_command_list *list, kope_%s_ray_pipeline *pipeline) {\n", api_short);
		fprintf(output, "\tkope_%s_command_list_set_ray_pipeline(list, pipeline);\n", api_short);
		fprintf(output, "}\n\n");

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
				fprintf(output, "kope_%s_render_pipeline %s;\n\n", api_short, get_name(t->name));
			}
		}

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && has_attribute(&t->attributes, add_name("raypipe"))) {
				fprintf(output, "kope_%s_ray_pipeline %s;\n\n", api_short, get_name(t->name));
			}
		}

		global *root_constants_global = NULL;
		char root_constants_type_name[256] = {0};

		for (global_id i = 0; get_global(i) != NULL && get_global(i)->type != NO_TYPE; ++i) {
			global *g = get_global(i);

			type_id base_type = get_type(g->type)->kind == TYPE_ARRAY ? get_type(g->type)->array.base : g->type;

			if (!get_type(base_type)->built_in) {
				type *t = get_type(g->type);

				char type_name[256];
				if (t->name != NO_NAME) {
					strcpy(type_name, get_name(t->name));
				}
				else {
					strcpy(type_name, get_name(g->name));
					strcat(type_name, "_type");
				}

				if (g->set != NULL && g->set->name == add_name("root_constants")) {
					root_constants_global = g;
					strcpy(root_constants_type_name, type_name);
				}
				else {
					fprintf(output, "\nvoid %s_buffer_create(kope_g5_device *device, kope_g5_buffer *buffer, uint32_t count) {\n", type_name);
					fprintf(output, "\tkope_g5_buffer_parameters parameters;\n");
					fprintf(output, "\tparameters.size = align_pow2(%i, 256) * count;\n", struct_size(g->type));
					fprintf(output, "\tparameters.usage_flags = KOPE_G5_BUFFER_USAGE_CPU_WRITE;\n");
					fprintf(output, "\tkope_g5_device_create_buffer(device, &parameters, buffer);\n");
					fprintf(output, "}\n\n");

					fprintf(output, "void %s_buffer_destroy(kope_g5_buffer *buffer) {\n", type_name);
					fprintf(output, "\tkope_g5_buffer_destroy(buffer);\n");
					fprintf(output, "}\n\n");

					fprintf(output, "%s *%s_buffer_lock(kope_g5_buffer *buffer, uint32_t index, uint32_t count) {\n", type_name, type_name);
					fprintf(output,
					        "\treturn (%s *)kope_g5_buffer_lock(buffer, index * align_pow2((int)sizeof(%s), 256), count * align_pow2((int)sizeof(%s), 256));\n",
					        type_name, type_name, type_name);
					fprintf(output, "}\n\n");

					fprintf(output, "%s *%s_buffer_try_to_lock(kope_g5_buffer *buffer, uint32_t index, uint32_t count) {\n", type_name, type_name);
					fprintf(output,
					        "\treturn (%s *)kope_g5_buffer_try_to_lock(buffer, index * align_pow2((int)sizeof(%s), 256), count * align_pow2((int)sizeof(%s), "
					        "256));\n",
					        type_name, type_name, type_name);
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
							fprintf(output, "\t%s *data = (%s *)buffer->%s.locked_data;\n", type_name, type_name, api_short);
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
						}
					}
					fprintf(output, "\tkope_g5_buffer_unlock(buffer);\n");
					fprintf(output, "}\n\n");
				}
			}
		}

		uint32_t descriptor_table_index = 0;
		for (size_t set_index = 0; set_index < sets_count; ++set_index) {
			descriptor_set *set = sets[set_index];

			if (set->name == add_name("root_constants")) {
				assert(root_constants_global != NULL);

				fprintf(output, "void kong_set_root_constants_%s(kope_g5_command_list *list, %s *constants) {\n", get_name(root_constants_global->name),
				        root_constants_type_name);
				fprintf(output, "\tkope_%s_command_list_set_root_constants(list, %i, constants, %i);\n", api_short, descriptor_table_index,
				        struct_size(root_constants_global->type));
				fprintf(output, "}\n\n");

				descriptor_table_index += 1;
				continue;
			}

			if (api == API_VULKAN) {
				fprintf(output, "extern VkDescriptorSetLayout %s_set_layout;\n\n", get_name(set->name));
			}

			fprintf(output, "void kong_create_%s_set(kope_g5_device *device, const %s_parameters *parameters, %s_set *set) {\n", get_name(set->name),
			        get_name(set->name), get_name(set->name));

			size_t other_count = 0;
			size_t sampler_count = 0;
			size_t dynamic_count = 0;
			size_t bindless_count = 0;

			for (size_t descriptor_index = 0; descriptor_index < set->definitions_count; ++descriptor_index) {
				definition d = set->definitions[descriptor_index];

				switch (d.kind) {
				case DEFINITION_CONST_CUSTOM:
					if (has_attribute(&get_global(d.global)->attributes, add_name("indexed"))) {
						dynamic_count += 1;
					}
					else {
						other_count += 1;
					}
					break;
				CASE_TEXTURE: {
					type *t = get_type(get_global(d.global)->type);
					if (t->kind == TYPE_ARRAY && t->array.array_size == -1) {
						bindless_count += 1;
					}
					else {
						other_count += 1;
					}
					break;
				default:
					assert(false);
					break;
				}
				case DEFINITION_SAMPLER:
					sampler_count += 1;
					break;
				}
			}

			if (api == API_VULKAN) {
				fprintf(output, "\tkope_vulkan_device_create_descriptor_set(device, &%s_set_layout, &set->set);\n", get_name(set->name));
			}
			else {
				fprintf(output, "\tkope_%s_device_create_descriptor_set(device, %zu, %zu, %zu, %zu, &set->set);\n", api_short, other_count, dynamic_count,
				        bindless_count, sampler_count);
			}

			size_t other_index = 0;
			size_t sampler_index = 0;

			for (size_t descriptor_index = 0; descriptor_index < set->definitions_count; ++descriptor_index) {
				definition d = set->definitions[descriptor_index];

				switch (d.kind) {
				case DEFINITION_CONST_CUSTOM:
					if (!has_attribute(&get_global(d.global)->attributes, add_name("indexed"))) {
						fprintf(output, "\tkope_%s_descriptor_set_set_buffer_view_cbv(device, &set->set, parameters->%s, %zu);\n", api_short,
						        get_name(get_global(d.global)->name), other_index);
						other_index += 1;
					}
					fprintf(output, "\tset->%s = parameters->%s;\n", get_name(get_global(d.global)->name), get_name(get_global(d.global)->name));
					break;
				case DEFINITION_BVH:
					fprintf(output, "\tkope_%s_descriptor_set_set_bvh_view_srv(device, &set->set, parameters->%s, %zu);\n", api_short,
					        get_name(get_global(d.global)->name), other_index);
					fprintf(output, "\tset->%s = parameters->%s;\n", get_name(get_global(d.global)->name), get_name(get_global(d.global)->name));
					other_index += 1;
					break;
				case DEFINITION_TEX2D: {
					type *t = get_type(get_global(d.global)->type);
					if (t->kind == TYPE_ARRAY && t->array.array_size == -1) {
						fprintf(output, "\tset->%s = (kope_g5_texture_view *)malloc(sizeof(kope_g5_texture_view) * parameters->textures_count);\n",
						        get_name(get_global(d.global)->name));
						fprintf(output, "\tassert(set->%s != NULL);\n", get_name(get_global(d.global)->name));
						fprintf(output, "\tfor (size_t index = 0; index < parameters->textures_count; ++index) {\n");
						fprintf(output,
						        "\t\tkope_%s_descriptor_set_set_texture_view_srv(device, set->set.bindless_descriptor_allocation.offset + (uint32_t)index, "
						        "&parameters->%s[index]);\n",
						        api_short, get_name(get_global(d.global)->name));
						fprintf(output, "\t\tset->%s[index] = parameters->%s[index];\n", get_name(get_global(d.global)->name),
						        get_name(get_global(d.global)->name));
						fprintf(output, "\t}\n");

						fprintf(output, "\tset->%s_count = parameters->%s_count;\n", get_name(get_global(d.global)->name),
						        get_name(get_global(d.global)->name));
					}
					else {
						attribute *write_attribute = find_attribute(&get_global(d.global)->attributes, add_name("write"));
						if (write_attribute != NULL) {
							fprintf(output, "\tkope_%s_descriptor_set_set_texture_view_uav(device, &set->set, &parameters->%s, %zu);\n", api_short,
							        get_name(get_global(d.global)->name), other_index);
						}
						else {
							if (api == API_VULKAN) {
								fprintf(output, "\tkope_vulkan_descriptor_set_set_texture_view(device, &set->set, &parameters->%s, %zu);\n",
								        get_name(get_global(d.global)->name), descriptor_index);
							}
							else {
								fprintf(
								    output,
								    "\tkope_%s_descriptor_set_set_texture_view_srv(device, set->set.descriptor_allocation.offset + %zu, &parameters->%s);\n",
								    api_short, other_index, get_name(get_global(d.global)->name));
							}
						}

						fprintf(output, "\tset->%s = parameters->%s;\n", get_name(get_global(d.global)->name), get_name(get_global(d.global)->name));

						other_index += 1;
					}

					break;
				}
				case DEFINITION_TEX2DARRAY: {
					attribute *write_attribute = find_attribute(&get_global(d.global)->attributes, add_name("write"));
					if (write_attribute != NULL) {
						debug_context context = {0};
						error(context, "Texture arrays can not be writable");
					}

					fprintf(output, "\tkope_%s_descriptor_set_set_texture_array_view_srv(device, &set->set, &parameters->%s, %zu);\n", api_short,
					        get_name(get_global(d.global)->name), other_index);

					fprintf(output, "\tset->%s = parameters->%s;\n", get_name(get_global(d.global)->name), get_name(get_global(d.global)->name));
					other_index += 1;
					break;
				}
				case DEFINITION_TEXCUBE: {
					attribute *write_attribute = find_attribute(&get_global(d.global)->attributes, add_name("write"));
					if (write_attribute != NULL) {
						debug_context context = {0};
						error(context, "Cube maps can not be writable");
					}
					fprintf(output, "\tkope_%s_descriptor_set_set_texture_cube_view_srv(device, &set->set, &parameters->%s, %zu);\n", api_short,
					        get_name(get_global(d.global)->name), other_index);

					fprintf(output, "\tset->%s = parameters->%s;\n", get_name(get_global(d.global)->name), get_name(get_global(d.global)->name));
					other_index += 1;
					break;
				}
				case DEFINITION_SAMPLER:
					fprintf(output, "\tkope_%s_descriptor_set_set_sampler(device, &set->set, parameters->%s, %zu);\n", api_short,
					        get_name(get_global(d.global)->name), sampler_index);
					sampler_index += 1;
					break;
				default:
					assert(false);
					break;
				}
			}
			fprintf(output, "}\n\n");

			fprintf(output, "void kong_update_%s_set(%s_set *set, %s_set_update *updates, uint32_t updates_count) {\n", get_name(set->name),
			        get_name(set->name), get_name(set->name));
			fprintf(output, "}\n\n");

			fprintf(output, "void kong_set_descriptor_set_%s(kope_g5_command_list *list, %s_set *set", get_name(set->name), get_name(set->name));
			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition d = set->definitions[definition_index];
				switch (d.kind) {
				case DEFINITION_CONST_CUSTOM:
					if (has_attribute(&get_global(d.global)->attributes, add_name("indexed"))) {
						fprintf(output, ", uint32_t %s_index", get_name(get_global(d.global)->name));
					}
					break;
				default:
					break;
				}
			}
			fprintf(output, ") {\n");
			for (size_t descriptor_index = 0; descriptor_index < set->definitions_count; ++descriptor_index) {
				definition d = set->definitions[descriptor_index];

				switch (d.kind) {
				case DEFINITION_CONST_CUSTOM:
					if (has_attribute(&get_global(d.global)->attributes, add_name("indexed"))) {
						if (api == API_VULKAN) {
							fprintf(output, "\tkope_vulkan_descriptor_set_prepare_buffer(list, set->%s);\n", get_name(get_global(d.global)->name));
						}
						else {
							fprintf(
							    output,
							    "\tkope_%s_descriptor_set_prepare_cbv_buffer(list, set->%s, %s_index * align_pow2((int)%i, 256), align_pow2((int)%i, 256));\n",
							    api_short, get_name(get_global(d.global)->name), get_name(get_global(d.global)->name), struct_size(get_global(d.global)->type),
							    struct_size(get_global(d.global)->type));
						}
					}
					else {
						fprintf(output, "\tkope_%s_descriptor_set_prepare_cbv_buffer(list, set->%s, 0, UINT32_MAX);\n", api_short,
						        get_name(get_global(d.global)->name));
					}
					break;
				CASE_TEXTURE: {
					type *t = get_type(get_global(d.global)->type);
					if (t->kind == TYPE_ARRAY && t->array.array_size == -1) {
						fprintf(output, "\tfor (size_t index = 0; index < set->%s_count; ++index) {\n", get_name(get_global(d.global)->name));
						fprintf(output, "\t\tkope_%s_descriptor_set_prepare_srv_texture(list, &set->%s[index]);\n", api_short,
						        get_name(get_global(d.global)->name));
						fprintf(output, "\t}\n");
					}
					else {
						attribute *write_attribute = find_attribute(&get_global(d.global)->attributes, add_name("write"));
						if (write_attribute != NULL) {
							fprintf(output, "\tkope_%s_descriptor_set_prepare_uav_texture(list, &set->%s);\n", api_short, get_name(get_global(d.global)->name));
						}
						else {
							if (api == API_VULKAN) {
								fprintf(output, "\tkope_vulkan_descriptor_set_prepare_texture(list, &set->%s);\n", get_name(get_global(d.global)->name));
							}
							else {
								fprintf(output, "\tkope_%s_descriptor_set_prepare_srv_texture(list, &set->%s);\n", api_short,
								        get_name(get_global(d.global)->name));
							}
						}
					}
					break;
				}
				default:
					break;
				}
			}

			fprintf(output, "\n");

			{
				uint32_t dynamic_count = 0;
				for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
					definition d = set->definitions[definition_index];
					switch (d.kind) {
					case DEFINITION_CONST_CUSTOM:
						if (has_attribute(&get_global(d.global)->attributes, add_name("indexed"))) {
							dynamic_count += 1;
						}
						break;
					default:
						break;
					}
				}

				if (dynamic_count > 0) {
					fprintf(output, "\tkope_g5_buffer *dynamic_buffers[%i];\n", dynamic_count);

					fprintf(output, "\tuint32_t dynamic_offsets[%i];\n", dynamic_count);

					fprintf(output, "\tuint32_t dynamic_sizes[%i];\n", dynamic_count);

					uint32_t dynamic_index = 0;
					for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
						definition d = set->definitions[definition_index];
						switch (d.kind) {
						case DEFINITION_CONST_CUSTOM:
							if (has_attribute(&get_global(d.global)->attributes, add_name("indexed"))) {
								fprintf(output, "\tdynamic_buffers[%i] = set->%s;\n", dynamic_index, get_name(get_global(d.global)->name));
								fprintf(output, "\tdynamic_offsets[%i] = %s_index * align_pow2((int)%i, 256);\n", dynamic_index,
								        get_name(get_global(d.global)->name), struct_size(get_global(d.global)->type));
								fprintf(output, "\tdynamic_sizes[%i] = align_pow2((int)%i, 256);\n", dynamic_index, struct_size(get_global(d.global)->type));
								dynamic_index += 1;
							}
							break;
						default:
							break;
						}
					}
				}

				fprintf(output, "\n\tkope_%s_command_list_set_descriptor_table(list, %i, &set->set", api_short, descriptor_table_index);
				if (dynamic_count > 0) {
					fprintf(output, ", dynamic_buffers, dynamic_offsets, dynamic_sizes");
				}
				else {
					fprintf(output, ", NULL, NULL, NULL");
				}
				fprintf(output, ");\n");
				fprintf(output, "}\n\n");
			}

			descriptor_table_index += (sampler_count > 0) ? 2 : 1; // TODO
		}

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
				for (size_t j = 0; j < t->members.size; ++j) {
					if (t->members.m[j].name == add_name("vertex") || t->members.m[j].name == add_name("fragment")) {
						debug_context context = {0};
						check(t->members.m[j].value.kind == TOKEN_IDENTIFIER, context, "vertex or fragment expects an identifier");
						fprintf(output, "static kope_%s_shader %s;\n", api_short, get_name(t->members.m[j].value.identifier));
					}
				}
			}
		}

		for (function_id i = 0; get_function(i) != NULL; ++i) {
			function *f = get_function(i);
			if (has_attribute(&f->attributes, add_name("compute"))) {
				fprintf(output, "kope_%s_compute_pipeline %s;\n", api_short, get_name(f->name));
			}
		}

		if (api == API_WEBGPU) {
			fprintf(output, "\nvoid kinc_g5_internal_webgpu_create_shader_module(const void *source, size_t length);\n");
		}

		if (api == API_OPENGL) {
			fprintf(output, "\nvoid kinc_g4_internal_opengl_setup_uniform_block(unsigned program, const char *name, unsigned binding);\n");
		}

		if (api == API_DIRECT3D12) {
			fprintf(output, "struct ID3D12RootSignature;");

			for (type_id i = 0; get_type(i) != NULL; ++i) {
				type *t = get_type(i);
				if (!t->built_in && has_attribute(&t->attributes, add_name("raypipe"))) {
					fprintf(output, "struct ID3D12RootSignature *kong_create_%s_root_signature(kope_g5_device *device);", get_name(t->name));
				}
			}
		}

		fprintf(output, "\nvoid kong_init(kope_g5_device *device) {\n");

		if (api == API_WEBGPU) {
			fprintf(output, "\tkinc_g5_internal_webgpu_create_shader_module(wgsl, wgsl_size);\n\n");
		}

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
				fprintf(output, "\tkope_%s_render_pipeline_parameters %s_parameters = {0};\n\n", api_short, get_name(t->name));

				name_id vertex_shader_name = NO_NAME;
				name_id amplification_shader_name = NO_NAME;
				name_id mesh_shader_name = NO_NAME;
				name_id fragment_shader_name = NO_NAME;

				for (size_t j = 0; j < t->members.size; ++j) {
					if (t->members.m[j].name == add_name("vertex")) {
						if (api == API_METAL) {
							fprintf(output, "\t%s_parameters.vertex.shader.function_name = \"%s\";\n", get_name(t->name),
							        get_name(t->members.m[j].value.identifier));
						}
						else {
							fprintf(output, "\t%s_parameters.vertex.shader.data = %s_code;\n", get_name(t->name), get_name(t->members.m[j].value.identifier));
							fprintf(output, "\t%s_parameters.vertex.shader.size = %s_code_size;\n\n", get_name(t->name),
							        get_name(t->members.m[j].value.identifier));
						}
						vertex_shader_name = t->members.m[j].value.identifier;
					}
					else if (t->members.m[j].name == add_name("amplification")) {
						amplification_shader_name = t->members.m[j].value.identifier;
					}
					else if (t->members.m[j].name == add_name("mesh")) {
						mesh_shader_name = t->members.m[j].value.identifier;
					}
					else if (t->members.m[j].name == add_name("fragment")) {
						if (api == API_METAL) {
							fprintf(output, "\t%s_parameters.fragment.shader.function_name = \"%s\";\n", get_name(t->name),
							        get_name(t->members.m[j].value.identifier));
						}
						else {
							fprintf(output, "\t%s_parameters.fragment.shader.data = %s_code;\n", get_name(t->name), get_name(t->members.m[j].value.identifier));
							fprintf(output, "\t%s_parameters.fragment.shader.size = %s_code_size;\n\n", get_name(t->name),
							        get_name(t->members.m[j].value.identifier));
						}
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

				type_id vertex_inputs[64] = {0};
				bool instanced[64] = {0};
				size_t vertex_inputs_count = 0;

				if (vertex_shader_name != NO_NAME) {
					for (function_id i = 0; get_function(i) != NULL; ++i) {
						function *f = get_function(i);
						if (f->name == vertex_shader_name) {
							vertex_function = f;
							debug_context context = {0};
							check(f->parameters_size > 0, context, "Vertex function requires at least one parameter");
							for (size_t input_index = 0; input_index < f->parameters_size; ++input_index) {
								vertex_inputs[input_index] = f->parameter_types[input_index].type;
								if (f->parameter_attributes[input_index] == add_name("per_instance")) {
									instanced[input_index] = true;
								}
							}
							vertex_inputs_count = f->parameters_size;
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
					check(vertex_function == NULL || (vertex_inputs_count > 0 && vertex_inputs[0] != NO_TYPE), context, "No vertex input found");
				}

				if (vertex_function != NULL) {
					size_t location = 0;

					for (size_t input_index = 0; input_index < vertex_inputs_count; ++input_index) {
						type *vertex_type = get_type(vertex_inputs[input_index]);
						size_t offset = 0;

						for (size_t j = 0; j < vertex_type->members.size; ++j) {
							if (api == API_OPENGL) {
								fprintf(output, "\tkinc_g4_vertex_structure_add(&%s_structure, \"%s_%s\", %s);\n", get_name(t->name),
								        get_name(vertex_type->name), get_name(vertex_type->members.m[j].name),
								        structure_type(vertex_type->members.m[j].type.type, api));
							}
							else {
								fprintf(output, "\t%s_parameters.vertex.buffers[%zu].attributes[%zu].format = %s;\n", get_name(t->name), input_index, j,
								        structure_type(vertex_type->members.m[j].type.type, api));
								fprintf(output, "\t%s_parameters.vertex.buffers[%zu].attributes[%zu].offset = %zu;\n", get_name(t->name), input_index, j,
								        offset);
								fprintf(output, "\t%s_parameters.vertex.buffers[%zu].attributes[%zu].shader_location = %zu;\n", get_name(t->name), input_index,
								        j, location);
							}

							offset += base_type_size(vertex_type->members.m[j].type.type);
							location += 1;
						}
						fprintf(output, "\t%s_parameters.vertex.buffers[%zu].attributes_count = %zu;\n", get_name(t->name), input_index,
						        vertex_type->members.size);
						fprintf(output, "\t%s_parameters.vertex.buffers[%zu].array_stride = %zu;\n", get_name(t->name), input_index, offset);

						char step_mode[64];
						if (instanced[input_index]) {
							sprintf(step_mode, "KOPE_%s_VERTEX_STEP_MODE_INSTANCE", api_caps);
						}
						else {
							sprintf(step_mode, "KOPE_%s_VERTEX_STEP_MODE_VERTEX", api_caps);
						}
						fprintf(output, "\t%s_parameters.vertex.buffers[%zu].step_mode = %s;\n", get_name(t->name), input_index, step_mode);
					}
					fprintf(output, "\t%s_parameters.vertex.buffers_count = %zu;\n\n", get_name(t->name), vertex_inputs_count);
				}

				fprintf(output, "\t%s_parameters.primitive.topology = KOPE_%s_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;\n", get_name(t->name), api_caps);
				fprintf(output, "\t%s_parameters.primitive.strip_index_format = KOPE_G5_INDEX_FORMAT_UINT16;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.primitive.front_face = KOPE_%s_FRONT_FACE_CW;\n", get_name(t->name), api_caps);
				fprintf(output, "\t%s_parameters.primitive.cull_mode = KOPE_%s_CULL_MODE_NONE;\n", get_name(t->name), api_caps);
				fprintf(output, "\t%s_parameters.primitive.unclipped_depth = false;\n\n", get_name(t->name));

				member *depth_stencil_format = find_member(t, "depth_stencil_format");
				if (depth_stencil_format != NULL) {
					debug_context context = {0};
					check(depth_stencil_format->value.kind == TOKEN_IDENTIFIER, context, "depth_stencil_format expects an identifier");
					global *g = find_global(depth_stencil_format->value.identifier);
					fprintf(output, "\t%s_parameters.depth_stencil.format = %s;\n", get_name(t->name), convert_texture_format(g->value.value.ints[0]));
				}
				else {
					fprintf(output, "\t%s_parameters.depth_stencil.format = KOPE_G5_TEXTURE_FORMAT_DEPTH32FLOAT;\n", get_name(t->name));
				}

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
				fprintf(output, "\t%s_parameters.depth_stencil.stencil_front.fail_op = KOPE_%s_STENCIL_OPERATION_KEEP;\n", get_name(t->name), api_caps);
				fprintf(output, "\t%s_parameters.depth_stencil.stencil_front.depth_fail_op = KOPE_%s_STENCIL_OPERATION_KEEP;\n", get_name(t->name), api_caps);
				fprintf(output, "\t%s_parameters.depth_stencil.stencil_front.pass_op = KOPE_%s_STENCIL_OPERATION_KEEP;\n", get_name(t->name), api_caps);

				fprintf(output, "\t%s_parameters.depth_stencil.stencil_back.compare = KOPE_G5_COMPARE_FUNCTION_ALWAYS;\n", get_name(t->name));
				fprintf(output, "\t%s_parameters.depth_stencil.stencil_back.fail_op = KOPE_%s_STENCIL_OPERATION_KEEP;\n", get_name(t->name), api_caps);
				fprintf(output, "\t%s_parameters.depth_stencil.stencil_back.depth_fail_op = KOPE_%s_STENCIL_OPERATION_KEEP;\n", get_name(t->name), api_caps);
				fprintf(output, "\t%s_parameters.depth_stencil.stencil_back.pass_op = KOPE_%s_STENCIL_OPERATION_KEEP;\n", get_name(t->name), api_caps);

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
						char format_name[32];
						sprintf(format_name, "format%i", i);
						member *format = find_member(t, format_name);
						if (format == NULL && i == 0) {
							format = find_member(t, "format");
						}
						if (format != NULL) {
							debug_context context = {0};
							check(format->value.kind == TOKEN_IDENTIFIER, context, "format expects an identifier");
							global *g = find_global(format->value.identifier);
							fprintf(output, "\t%s_parameters.fragment.targets[%i].format = %s;\n", get_name(t->name), i,
							        convert_texture_format(g->value.value.ints[0]));
						}
						else {
							fprintf(output, "\t%s_parameters.fragment.targets[%i].format = KOPE_G5_TEXTURE_FORMAT_RGBA8_UNORM;\n", get_name(t->name), i);
						}

						fprintf(output, "\t%s_parameters.fragment.targets[%i].blend.color.operation = KOPE_%s_BLEND_OPERATION_ADD;\n", get_name(t->name), i,
						        api_caps);
						fprintf(output, "\t%s_parameters.fragment.targets[%i].blend.color.src_factor = KOPE_%s_BLEND_FACTOR_ONE;\n", get_name(t->name), i,
						        api_caps);
						fprintf(output, "\t%s_parameters.fragment.targets[%i].blend.color.dst_factor = KOPE_%s_BLEND_FACTOR_ZERO;\n", get_name(t->name), i,
						        api_caps);

						fprintf(output, "\t%s_parameters.fragment.targets[%i].blend.alpha.operation = KOPE_%s_BLEND_OPERATION_ADD;\n", get_name(t->name), i,
						        api_caps);
						fprintf(output, "\t%s_parameters.fragment.targets[%i].blend.alpha.src_factor = KOPE_%s_BLEND_FACTOR_ONE;\n", get_name(t->name), i,
						        api_caps);
						fprintf(output, "\t%s_parameters.fragment.targets[%i].blend.alpha.dst_factor = KOPE_%s_BLEND_FACTOR_ZERO;\n", get_name(t->name), i,
						        api_caps);

						fprintf(output, "\t%s_parameters.fragment.targets[%i].write_mask = 0xf;\n\n", get_name(t->name), i);
					}
					fprintf(output, "\n");
				}
				else {
					fprintf(output, "\t%s_parameters.fragment.targets_count = 1;\n", get_name(t->name));

					member *format = find_member(t, "format");
					if (format == NULL) {
						format = find_member(t, "format0");
					}
					if (format != NULL) {
						debug_context context = {0};
						check(format->value.kind == TOKEN_IDENTIFIER, context, "format expects an identifier");

						if (strcmp(get_name(format->value.identifier), "framebuffer_format") == 0) {
							fprintf(output, "\t%s_parameters.fragment.targets[0].format = kope_g5_device_framebuffer_format(device);\n", get_name(t->name));
						}
						else {
							global *g = find_global(format->value.identifier);
							fprintf(output, "\t%s_parameters.fragment.targets[0].format = %s;\n", get_name(t->name),
							        convert_texture_format(g->value.value.ints[0]));
						}
					}
					else {
						fprintf(output, "\t%s_parameters.fragment.targets[0].format = KOPE_G5_TEXTURE_FORMAT_RGBA8_UNORM;\n", get_name(t->name));
					}

					fprintf(output, "\t%s_parameters.fragment.targets[0].blend.color.operation = KOPE_%s_BLEND_OPERATION_ADD;\n", get_name(t->name), api_caps);
					fprintf(output, "\t%s_parameters.fragment.targets[0].blend.color.src_factor = KOPE_%s_BLEND_FACTOR_ONE;\n", get_name(t->name), api_caps);
					fprintf(output, "\t%s_parameters.fragment.targets[0].blend.color.dst_factor = KOPE_%s_BLEND_FACTOR_ZERO;\n", get_name(t->name), api_caps);

					fprintf(output, "\t%s_parameters.fragment.targets[0].blend.alpha.operation = KOPE_%s_BLEND_OPERATION_ADD;\n", get_name(t->name), api_caps);
					fprintf(output, "\t%s_parameters.fragment.targets[0].blend.alpha.src_factor = KOPE_%s_BLEND_FACTOR_ONE;\n", get_name(t->name), api_caps);
					fprintf(output, "\t%s_parameters.fragment.targets[0].blend.alpha.dst_factor = KOPE_%s_BLEND_FACTOR_ZERO;\n", get_name(t->name), api_caps);

					fprintf(output, "\t%s_parameters.fragment.targets[0].write_mask = 0xf;\n\n", get_name(t->name));
				}

				fprintf(output, "\tkope_%s_render_pipeline_init(&device->%s, &%s, &%s_parameters);\n\n", api_short, api_short, get_name(t->name),
				        get_name(t->name));

				if (api == API_OPENGL) {
					global_id globals[256];
					size_t globals_size = 0;
					find_referenced_globals(vertex_function, globals, &globals_size);
					find_referenced_globals(fragment_function, globals, &globals_size);

					for (global_id i = 0; i < globals_size; ++i) {
						global *g = get_global(globals[i]);
						if (g->type == sampler_type_id) {
						}
						else if (is_texture(g->type)) {
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
				fprintf(output, "\tkope_%s_compute_pipeline_parameters %s_parameters;\n", api_short, get_name(f->name));
				fprintf(output, "\t%s_parameters.shader.data = %s_code;\n", get_name(f->name), get_name(f->name));
				fprintf(output, "\t%s_parameters.shader.size = %s_code_size;\n", get_name(f->name), get_name(f->name));
				fprintf(output, "\tkope_%s_compute_pipeline_init(&device->%s, &%s, &%s_parameters);\n", api_short, api_short, get_name(f->name),
				        get_name(f->name));
			}
		}

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && has_attribute(&t->attributes, add_name("raypipe"))) {
				fprintf(output, "\tkope_%s_ray_pipeline_parameters %s_parameters = {0};\n\n", api_short, get_name(t->name));

				name_id gen_shader_name = NO_NAME;
				name_id miss_shader_name = NO_NAME;
				name_id closest_shader_name = NO_NAME;
				name_id intersection_shader_name = NO_NAME;
				name_id any_shader_name = NO_NAME;

				for (size_t j = 0; j < t->members.size; ++j) {
					if (t->members.m[j].name == add_name("gen")) {
						gen_shader_name = t->members.m[j].value.identifier;
					}
					else if (t->members.m[j].name == add_name("miss")) {
						miss_shader_name = t->members.m[j].value.identifier;
					}
					else if (t->members.m[j].name == add_name("closest")) {
						closest_shader_name = t->members.m[j].value.identifier;
					}
					else if (t->members.m[j].name == add_name("intersection")) {
						intersection_shader_name = t->members.m[j].value.identifier;
					}
					else if (t->members.m[j].name == add_name("any")) {
						any_shader_name = t->members.m[j].value.identifier;
					}
				}

				{
					debug_context context = {0};
					check(gen_shader_name != NO_NAME, context, "No ray gen shader name found");
					check(miss_shader_name != NO_NAME, context, "No miss shader name found");
					check(closest_shader_name != NO_NAME, context, "No closest hit shader name found");
				}

				fprintf(output, "\t%s_parameters.gen_shader_name = \"%s\";\n", get_name(t->name), get_name(gen_shader_name));
				fprintf(output, "\t%s_parameters.miss_shader_name = \"%s\";\n", get_name(t->name), get_name(miss_shader_name));
				fprintf(output, "\t%s_parameters.closest_shader_name = \"%s\";\n", get_name(t->name), get_name(closest_shader_name));
				if (intersection_shader_name != NO_NAME) {
					fprintf(output, "\t%s_parameters.intersection_shader_name = \"%s\";\n", get_name(t->name), get_name(intersection_shader_name));
				}
				if (any_shader_name != NO_NAME) {
					fprintf(output, "\t%s_parameters.any_shader_name = \"%s\";\n", get_name(t->name), get_name(any_shader_name));
				}

				fprintf(output, "\n\tkope_%s_ray_pipeline_init(device, &%s, &%s_parameters, kong_create_%s_root_signature(device));\n\n", api_short,
				        get_name(t->name), get_name(t->name), get_name(t->name));
			}
		}

		fprintf(output, "}\n");

		fclose(output);
	}

	if (api == API_DIRECT3D12) {
		char filename[512];
		sprintf(filename, "%s/%s", directory, "kong_ray_root_signatures.cpp");

		FILE *output = fopen(filename, "wb");

		fprintf(output, "#include <kope/direct3d12/d3d12unit.h>\n");
		fprintf(output, "#include <kope/graphics5/device.h>\n\n");

		for (type_id i = 0; get_type(i) != NULL; ++i) {
			type *t = get_type(i);
			if (!t->built_in && has_attribute(&t->attributes, add_name("raypipe"))) {
				fprintf(output, "extern \"C\" ID3D12RootSignature *kong_create_%s_root_signature(kope_g5_device *device) {\n", get_name(t->name));
				write_root_signature(output, sets, sets_count);
				fprintf(output, "}\n");
			}
		}

		fclose(output);
	}
	else if (api == API_VULKAN) {
		char filename[512];
		sprintf(filename, "%s/%s", directory, "kong_descriptor_sets.c");

		FILE *output = fopen(filename, "wb");

		fprintf(output, "#include <kope/vulkan/vulkanunit.h>\n");
		fprintf(output, "#include <kope/graphics5/device.h>\n\n");

		fprintf(output, "#include <assert.h>\n\n");

		for (size_t set_index = 0; set_index < sets_count; ++set_index) {
			descriptor_set *set = sets[set_index];

			fprintf(output, "VkDescriptorSetLayout %s_set_layout;\n", get_name(set->name));
		}

		fprintf(output, "\n");

		fprintf(output, "void create_descriptor_set_layouts(kope_g5_device *device) {\n");

		for (size_t set_index = 0; set_index < sets_count; ++set_index) {
			descriptor_set *set = sets[set_index];

			fprintf(output, "\t{\n");

			fprintf(output, "\t\tVkDescriptorSetLayoutBinding layout_bindings[%zu] = {\n", set->definitions_count);

			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition def = set->definitions[definition_index];

				switch (def.kind) {
				case DEFINITION_FUNCTION:
					break;
				case DEFINITION_STRUCT:
					break;
				case DEFINITION_TEX2D:
					fprintf(output, "\t\t\t{\n");
					fprintf(output, "\t\t\t\t.binding = %zu,\n", definition_index);
					if (has_attribute(&get_global(def.global)->attributes, add_name("write"))) {
						fprintf(output, "\t\t\t\t.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,\n");
					}
					else {
						fprintf(output, "\t\t\t\t.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,\n");
					}
					fprintf(output, "\t\t\t\t.descriptorCount = 1,\n");
					fprintf(output, "\t\t\t\t.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,\n");
					fprintf(output, "\t\t\t\t.pImmutableSamplers = NULL,\n");
					fprintf(output, "\t\t\t},\n");
					break;
				case DEFINITION_TEX2DARRAY:
					break;
				case DEFINITION_TEXCUBE:
					break;
				case DEFINITION_SAMPLER:
					fprintf(output, "\t\t\t{\n");
					fprintf(output, "\t\t\t\t.binding = %zu,\n", definition_index);
					fprintf(output, "\t\t\t\t.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,\n");
					fprintf(output, "\t\t\t\t.descriptorCount = 1,\n");
					fprintf(output, "\t\t\t\t.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,\n");
					fprintf(output, "\t\t\t\t.pImmutableSamplers = NULL,\n");
					fprintf(output, "\t\t\t},\n");
					break;
				case DEFINITION_CONST_CUSTOM: {
					fprintf(output, "\t\t\t{\n");
					fprintf(output, "\t\t\t\t.binding = %zu,\n", definition_index);
					if (has_attribute(&get_global(def.global)->attributes, add_name("write"))) {
						if (has_attribute(&get_global(def.global)->attributes, add_name("indexed"))) {
							fprintf(output, "\t\t\t\t.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,\n");
						}
						else {
							fprintf(output, "\t\t\t\t.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,\n");
						}
					}
					else {
						if (has_attribute(&get_global(def.global)->attributes, add_name("indexed"))) {
							fprintf(output, "\t\t\t\t.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,\n");
						}
						else {
							fprintf(output, "\t\t\t\t.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,\n");
						}
					}
					fprintf(output, "\t\t\t\t.descriptorCount = 1,\n");
					fprintf(output, "\t\t\t\t.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,\n");
					fprintf(output, "\t\t\t\t.pImmutableSamplers = NULL,\n");
					fprintf(output, "\t\t\t},\n");
					break;
				}
				case DEFINITION_CONST_BASIC:
					break;
				case DEFINITION_BVH:
					break;
				}
			}

			fprintf(output, "\t\t};\n");

			fprintf(output, "\n");

			fprintf(output, "\t\tVkDescriptorSetLayoutCreateInfo layout_create_info = {\n");
			fprintf(output, "\t\t\t.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,\n");
			fprintf(output, "\t\t\t.pNext = NULL,\n");
			fprintf(output, "\t\t\t.bindingCount = %zu,\n", set->definitions_count);
			fprintf(output, "\t\t\t.pBindings = layout_bindings,\n");
			fprintf(output, "\t\t};\n");

			fprintf(output, "\t\tVkResult result = vkCreateDescriptorSetLayout(device->vulkan.device, &layout_create_info, NULL, &%s_set_layout);\n",
			        get_name(set->name));
			fprintf(output, "\t\tassert(result == VK_SUCCESS);\n");

			fprintf(output, "\t}\n");
		}

		fprintf(output, "}\n");

		fclose(output);
	}
}
