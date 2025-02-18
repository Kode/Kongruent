#include "hlsl.h"

#include "../compiler.h"
#include "../errors.h"
#include "../functions.h"
#include "../parser.h"
#include "../sets.h"
#include "../shader_stage.h"
#include "../types.h"
#include "cstyle.h"
#include "d3d11.h"
#include "d3d12.h"
#include "d3d9.h"
#include "util.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *member_string(type *parent_type, name_id member_name) {
	if (parent_type == get_type(ray_type_id)) {
		if (member_name == add_name("origin")) {
			return "Origin";
		}
		else if (member_name == add_name("direction")) {
			return "Direction";
		}
		else if (member_name == add_name("min")) {
			return "TMin";
		}
		else if (member_name == add_name("max")) {
			return "TMax";
		}
		else {
			return get_name(member_name);
		}
	}
	else {
		return get_name(member_name);
	}
}

static char *type_string(type_id type) {
	if (type == float_id) {
		return "float";
	}
	if (type == float2_id) {
		return "float2";
	}
	if (type == float3_id) {
		return "float3";
	}
	if (type == float4_id) {
		return "float4";
	}
	if (type == float4x4_id) {
		return "float4x4";
	}
	if (type == ray_type_id) {
		return "RayDesc";
	}
	if (type == bvh_type_id) {
		return "RaytracingAccelerationStructure";
	}
	if (type == tex2d_type_id) {
		return "Texture2D<float4>";
	}
	return get_name(get_type(type)->name);
}

static void type_arr(type_ref t_ref, char *arr) {
	type *t = get_type(t_ref.type);
	if (t->array_size == 0) {
		arr[0] = 0;
	}
	else if (t->array_size == UINT32_MAX) {
		strcpy(arr, "[]");
	}
	else {
		sprintf(arr, "[%i]", t->array_size);
	}
}

static char *function_string(name_id func) {
	return get_name(func);
}

static void write_bytecode(char *hlsl, char *directory, const char *filename, const char *name, uint8_t *output, size_t output_size) {
	char full_filename[512];

	{
		sprintf(full_filename, "%s/%s.h", directory, filename);
		FILE *file = fopen(full_filename, "wb");

		fprintf(file, "#ifndef KONG_%s_HEADER\n", name);
		fprintf(file, "#define KONG_%s_HEADER\n\n", name);

		fprintf(file, "#include <stddef.h>\n");
		fprintf(file, "#include <stdint.h>\n\n");

		fprintf(file, "#ifdef __cplusplus\n");
		fprintf(file, "extern \"C\" {\n");
		fprintf(file, "#endif\n\n");

		fprintf(file, "extern uint8_t *%s;\n", name);
		fprintf(file, "extern size_t %s_size;\n", name);

		fprintf(file, "\n#ifdef __cplusplus\n");
		fprintf(file, "}\n");
		fprintf(file, "#endif\n\n");

		fprintf(file, "#endif\n");

		fclose(file);
	}

	{
		sprintf(full_filename, "%s/%s.c", directory, filename);

		FILE *file = fopen(full_filename, "wb");
		fprintf(file, "#include \"%s.h\"\n\n", filename);

		fprintf(file, "uint8_t *%s = \"", name);
		for (size_t i = 0; i < output_size; ++i) {
			// based on the encoding described in https://github.com/adobe/bin2c
			if (output[i] == '!' || output[i] == '#' || (output[i] >= '%' && output[i] <= '>') || (output[i] >= 'A' && output[i] <= '[') ||
			    (output[i] >= ']' && output[i] <= '~')) {
				fprintf(file, "%c", output[i]);
			}
			else if (output[i] == '\a') {
				fprintf(file, "\\a");
			}
			else if (output[i] == '\b') {
				fprintf(file, "\\b");
			}
			else if (output[i] == '\t') {
				fprintf(file, "\\t");
			}
			else if (output[i] == '\v') {
				fprintf(file, "\\v");
			}
			else if (output[i] == '\f') {
				fprintf(file, "\\f");
			}
			else if (output[i] == '\r') {
				fprintf(file, "\\r");
			}
			else if (output[i] == '\"') {
				fprintf(file, "\\\"");
			}
			else if (output[i] == '\\') {
				fprintf(file, "\\\\");
			}
			else {
				fprintf(file, "\\%03o", output[i]);
			}
		}
		fprintf(file, "\";\n");

		fprintf(file, "size_t %s_size = %zu;\n\n", name, output_size);

		fprintf(file, "/*\n%s*/\n", hlsl);

		fclose(file);
	}
}

static bool is_input(type_id t, type_id inputs[64], size_t inputs_count) {
	for (size_t input_index = 0; input_index < inputs_count; ++input_index) {
		if (inputs[input_index] == t) {
			return true;
		}
	}
	return false;
}

static void write_types(char *hlsl, size_t *offset, shader_stage stage, type_id inputs[64], size_t inputs_count, type_id output, function *main,
                        function **rayshaders, size_t rayshaders_count) {
	type_id types[256];
	size_t types_size = 0;
	if (main != NULL) {
		find_referenced_types(main, types, &types_size);
	}
	for (size_t rayshader_index = 0; rayshader_index < rayshaders_count; ++rayshader_index) {
		find_referenced_types(rayshaders[rayshader_index], types, &types_size);
	}

	size_t input_offsets[64];
	input_offsets[0] = 0;

	if (inputs_count > 0) {
		for (size_t input_index = 0; input_index < inputs_count - 1; ++input_index) {
			type *t = get_type(inputs[input_index]);
			input_offsets[input_index + 1] = input_offsets[input_index] + t->members.size;
		}
	}

	for (size_t i = 0; i < types_size; ++i) {
		type *t = get_type(types[i]);

		if (!t->built_in && !has_attribute(&t->attributes, add_name("pipe"))) {
			*offset += sprintf(&hlsl[*offset], "struct %s {\n", get_name(t->name));

			if (stage == SHADER_STAGE_VERTEX && is_input(types[i], inputs, inputs_count)) {
				size_t input_offset = 0;
				for (size_t input_index = 0; input_index < inputs_count; ++input_index) {
					if (types[i] == inputs[input_index]) {
						input_offset = input_offsets[input_index];
						break;
					}
				}

				for (size_t j = 0; j < t->members.size; ++j) {
					*offset += sprintf(&hlsl[*offset], "\t%s %s : TEXCOORD%zu;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name),
					                   j + input_offset);
				}
			}
			else if (stage == SHADER_STAGE_VERTEX && types[i] == output) {
				for (size_t j = 0; j < t->members.size; ++j) {
					if (j == 0) {
						*offset += sprintf(&hlsl[*offset], "\t%s %s : SV_POSITION;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
					}
					else {
						*offset +=
						    sprintf(&hlsl[*offset], "\t%s %s : TEXCOORD%zu;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name), j - 1);
					}
				}
			}
			else if (stage == SHADER_STAGE_MESH && types[i] == output) {
				for (size_t j = 0; j < t->members.size; ++j) {
					if (j == 0) {
						*offset += sprintf(&hlsl[*offset], "\t%s %s : SV_POSITION;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
					}
					else {
						*offset +=
						    sprintf(&hlsl[*offset], "\t%s %s : TEXCOORD%zu;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name), j - 1);
					}
				}
			}
			else if (stage == SHADER_STAGE_FRAGMENT && types[i] == inputs[0]) {
				for (size_t j = 0; j < t->members.size; ++j) {
					if (j == 0) {
						*offset += sprintf(&hlsl[*offset], "\t%s %s : SV_POSITION;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
					}
					else {
						*offset +=
						    sprintf(&hlsl[*offset], "\t%s %s : TEXCOORD%zu;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name), j - 1);
					}
				}
			}
			else {
				for (size_t j = 0; j < t->members.size; ++j) {
					*offset += sprintf(&hlsl[*offset], "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
				}
			}
			*offset += sprintf(&hlsl[*offset], "};\n\n");
		}
	}
}

static int global_register_indices[512];

static void write_globals(char *hlsl, size_t *offset, function *main, function **rayshaders, size_t rayshaders_count) {
	global_id globals[256];
	size_t globals_size = 0;
	if (main != NULL) {
		find_referenced_globals(main, globals, &globals_size);
	}
	for (size_t rayshader_index = 0; rayshader_index < rayshaders_count; ++rayshader_index) {
		find_referenced_globals(rayshaders[rayshader_index], globals, &globals_size);
	}

	for (size_t i = 0; i < globals_size; ++i) {
		global *g = get_global(globals[i]);
		int register_index = global_register_indices[globals[i]];

		type *t = get_type(g->type);
		type_id base_type = t->array_size > 0 ? t->base : g->type;

		if (base_type == sampler_type_id) {
			*offset += sprintf(&hlsl[*offset], "SamplerState _%" PRIu64 " : register(s%i);\n\n", g->var_index, register_index);
		}
		else if (base_type == tex2d_type_id) {
			if (has_attribute(&g->attributes, add_name("write"))) {
				*offset += sprintf(&hlsl[*offset], "RWTexture2D<float4> _%" PRIu64 " : register(u%i);\n\n", g->var_index, register_index);
			}
			else {
				if (t->array_size > 0 && t->array_size == UINT32_MAX) {
					*offset += sprintf(&hlsl[*offset], "Texture2D<float4> _%" PRIu64 "[] : register(t%i, space1);\n\n", g->var_index, register_index);
				}
				else {
					*offset += sprintf(&hlsl[*offset], "Texture2D<float4> _%" PRIu64 " : register(t%i);\n\n", g->var_index, register_index);
				}
			}
		}
		else if (base_type == tex2darray_type_id) {
			*offset += sprintf(&hlsl[*offset], "Texture2DArray<float4> _%" PRIu64 " : register(t%i);\n\n", g->var_index, register_index);
		}
		else if (base_type == texcube_type_id) {
			*offset += sprintf(&hlsl[*offset], "TextureCube<float4> _%" PRIu64 " : register(t%i);\n\n", g->var_index, register_index);
		}
		else if (base_type == bvh_type_id) {
			*offset += sprintf(&hlsl[*offset], "RaytracingAccelerationStructure  _%" PRIu64 " : register(t%i);\n\n", g->var_index, register_index);
		}
		else if (base_type == float_id) {
			*offset += sprintf(&hlsl[*offset], "static const float _%" PRIu64 " = %f;\n\n", g->var_index, g->value.value.floats[0]);
		}
		else if (base_type == float2_id) {
			*offset += sprintf(&hlsl[*offset], "static const float2 _%" PRIu64 " = float2(%f, %f);\n\n", g->var_index, g->value.value.floats[0],
			                   g->value.value.floats[1]);
		}
		else if (base_type == float3_id) {
			*offset += sprintf(&hlsl[*offset], "static const float3 _%" PRIu64 " = float3(%f, %f, %f);\n\n", g->var_index, g->value.value.floats[0],
			                   g->value.value.floats[1], g->value.value.floats[2]);
		}
		else if (base_type == float4_id) {
			if (t->array_size > 0) {
				*offset += sprintf(&hlsl[*offset], "struct _%llu_type { float4 data; };\n", g->var_index);
				*offset += sprintf(&hlsl[*offset], "RWStructuredBuffer<_%llu_type> _%llu : register(u%i);\n", g->var_index, g->var_index, register_index);
			}
			else {
				*offset += sprintf(&hlsl[*offset], "static const float4 _%" PRIu64 " = float4(%f, %f, %f, %f);\n\n", g->var_index, g->value.value.floats[0],
				                   g->value.value.floats[1], g->value.value.floats[2], g->value.value.floats[3]);
			}
		}
		else {
			*offset += sprintf(&hlsl[*offset], "cbuffer _%" PRIu64 " : register(b%i) {\n", g->var_index, register_index);
			type *t = get_type(g->type);
			for (size_t i = 0; i < t->members.size; ++i) {
				char arr[16];
				type_arr(t->members.m[i].type, arr);
				*offset += sprintf(&hlsl[*offset], "\t%s _%" PRIu64 "_%s%s;\n", type_string(t->members.m[i].type.type), g->var_index,
				                   get_name(t->members.m[i].name), arr);
			}
			*offset += sprintf(&hlsl[*offset], "}\n\n");
		}
	}
}

static function *raygen_shaders[256];
static size_t raygen_shaders_size = 0;

static function *raymiss_shaders[256];
static size_t raymiss_shaders_size = 0;

static function *rayclosesthit_shaders[256];
static size_t rayclosesthit_shaders_size = 0;

static function *rayintersection_shaders[256];
static size_t rayintersection_shaders_size = 0;

static function *rayanyhit_shaders[256];
static size_t rayanyhit_shaders_size = 0;

static bool is_raygen_shader(function *f) {
	for (size_t rayshader_index = 0; rayshader_index < raygen_shaders_size; ++rayshader_index) {
		if (f == raygen_shaders[rayshader_index]) {
			return true;
		}
	}
	return false;
}

static bool is_raymiss_shader(function *f) {
	for (size_t rayshader_index = 0; rayshader_index < raymiss_shaders_size; ++rayshader_index) {
		if (f == raymiss_shaders[rayshader_index]) {
			return true;
		}
	}
	return false;
}

static bool is_rayclosesthit_shader(function *f) {
	for (size_t rayshader_index = 0; rayshader_index < rayclosesthit_shaders_size; ++rayshader_index) {
		if (f == rayclosesthit_shaders[rayshader_index]) {
			return true;
		}
	}
	return false;
}

static bool is_rayintersection_shader(function *f) {
	for (size_t rayshader_index = 0; rayshader_index < rayintersection_shaders_size; ++rayshader_index) {
		if (f == rayintersection_shaders[rayshader_index]) {
			return true;
		}
	}
	return false;
}

static bool is_rayanyhit_shader(function *f) {
	for (size_t rayshader_index = 0; rayshader_index < rayanyhit_shaders_size; ++rayshader_index) {
		if (f == rayanyhit_shaders[rayshader_index]) {
			return true;
		}
	}
	return false;
}

static descriptor_set *all_descriptor_sets[256];
static size_t all_descriptor_sets_count = 0;

static void write_root_signature(char *hlsl, size_t *offset) {
	uint32_t cbv_index = 0;
	uint32_t srv_index = 0;
	uint32_t uav_index = 0;
	uint32_t sampler_index = 0;

	*offset += sprintf(&hlsl[*offset], "[RootSignature(\"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)");

	for (size_t set_count = 0; set_count < all_descriptor_sets_count; ++set_count) {
		descriptor_set *set = all_descriptor_sets[set_count];

		if (set->name == add_name("root_constants")) {
			if (set->definitions_count != 1) {
				debug_context context = {0};
				error(context, "More than one root constants struct found");
			}

			uint32_t size = 0;
			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition *def = &set->definitions[definition_index];

				switch (def->kind) {
				case DEFINITION_CONST_CUSTOM:
					size += struct_size(get_global(def->global)->type);
					break;
				default: {
					debug_context context = {0};
					error(context, "Unsupported type for a root constant");
					break;
				}
				}
			}

			*offset += sprintf(&hlsl[*offset], "\\\n, RootConstants(num32BitConstants=%i, b%i)", size / 4, cbv_index);
			cbv_index += 1;

			continue;
		}

		bool has_sampler = false;
		bool has_other = false;
		bool has_dynamic = false;
		bool has_boundless = false;

		for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
			definition *def = &set->definitions[definition_index];

			switch (def->kind) {
			case DEFINITION_CONST_CUSTOM: {
				if (has_attribute(&get_global(def->global)->attributes, add_name("indexed"))) {
					has_dynamic = true;
				}
				else {
					has_other = true;
				}
				break;
			}
			case DEFINITION_TEX2D:
			case DEFINITION_TEX2DARRAY:
			case DEFINITION_TEXCUBE: {
				type *t = get_type(get_global(def->global)->type);
				if (t->array_size == UINT32_MAX) {
					has_boundless = true;
				}
				else {
					has_other = true;
				}
				break;
			}
			case DEFINITION_SAMPLER:
				has_sampler = true;
				break;
			case DEFINITION_CONST_BASIC: {
				type *t = get_type(get_global(def->global)->type);
				if (t->array_size > 0) {
					has_other = true;
				}
				break;
			}
			default:
				break;
			}
		}

		if (has_other) {
			*offset += sprintf(&hlsl[*offset], "\\\n, DescriptorTable(");

			bool first = true;
			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition *def = &set->definitions[definition_index];

				switch (def->kind) {
				case DEFINITION_CONST_CUSTOM:
					if (!has_attribute(&get_global(def->global)->attributes, add_name("indexed"))) {
						if (first) {
							first = false;
						}
						else {
							*offset += sprintf(&hlsl[*offset], ", ");
						}

						*offset += sprintf(&hlsl[*offset], "CBV(b%i)", cbv_index);
						cbv_index += 1;
					}
					break;
				case DEFINITION_TEX2D:
				case DEFINITION_TEX2DARRAY:
				case DEFINITION_TEXCUBE:
					if (first) {
						first = false;
					}
					else {
						*offset += sprintf(&hlsl[*offset], ", ");
					}

					attribute *write_attribute = find_attribute(&get_global(def->global)->attributes, add_name("write"));

					if (first) {
						first = false;
					}
					else {
						*offset += sprintf(&hlsl[*offset], ", ");
					}

					if (write_attribute != NULL) {
						*offset += sprintf(&hlsl[*offset], "UAV(u%i)", uav_index);
						uav_index += 1;
					}
					else {
						*offset += sprintf(&hlsl[*offset], "SRV(t%i)", srv_index);
						srv_index += 1;
					}
					break;
				case DEFINITION_CONST_BASIC: {
					type *t = get_type(get_global(def->global)->type);
					if (t->array_size > 0) {
						if (first) {
							first = false;
						}
						else {
							*offset += sprintf(&hlsl[*offset], ", ");
						}

						*offset += sprintf(&hlsl[*offset], "UAV(u%i)", uav_index);
						uav_index += 1;
					}
				}
				default:
					break;
				}
			}

			*offset += sprintf(&hlsl[*offset], ")");
		}

		if (has_dynamic) {
			*offset += sprintf(&hlsl[*offset], "\\\n, DescriptorTable(");

			bool first = true;
			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition *def = &set->definitions[definition_index];

				switch (def->kind) {
				case DEFINITION_CONST_CUSTOM:
					if (has_attribute(&get_global(def->global)->attributes, add_name("indexed"))) {
						if (first) {
							first = false;
						}
						else {
							*offset += sprintf(&hlsl[*offset], ", ");
						}
						*offset += sprintf(&hlsl[*offset], "CBV(b%i)", cbv_index);
						cbv_index += 1;
					}
					break;
				default:
					break;
				}
			}

			*offset += sprintf(&hlsl[*offset], ")");
		}

		if (has_boundless) {
			uint32_t boundless_space = 1;
			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition *def = &set->definitions[definition_index];

				switch (def->kind) {
				case DEFINITION_TEX2D:
				case DEFINITION_TEX2DARRAY:
				case DEFINITION_TEXCUBE: {
					type *t = get_type(get_global(def->global)->type);

					if (t->array_size == UINT32_MAX) {
						*offset += sprintf(&hlsl[*offset], "\\\n, DescriptorTable(SRV(t0, space = %i, numDescriptors = unbounded))", boundless_space);
						boundless_space += 1;
					}

					break;
				}
				default:
					break;
				}
			}
		}

		if (has_sampler) {
			*offset += sprintf(&hlsl[*offset], "\\\n, DescriptorTable(");

			bool first = true;
			for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
				definition *def = &set->definitions[definition_index];

				switch (def->kind) {
				case DEFINITION_SAMPLER:
					if (first) {
						first = false;
					}
					else {
						*offset += sprintf(&hlsl[*offset], ", ");
					}
					*offset += sprintf(&hlsl[*offset], "Sampler(s%i)", sampler_index);
					sampler_index += 1;
					break;
				default:
					break;
				}
			}

			*offset += sprintf(&hlsl[*offset], ")");
		}
	}

	*offset += sprintf(&hlsl[*offset], "\")]\n");
}

static type_id payload_types[256];
static size_t payload_types_count = 0;

static bool is_payload_type(type_id t) {
	for (size_t payload_index = 0; payload_index < payload_types_count; ++payload_index) {
		if (payload_types[payload_index] == t) {
			return true;
		}
	}
	return false;
}

static void write_functions(char *hlsl, size_t *offset, shader_stage stage, function *main, function **rayshaders, size_t rayshaders_count) {
	function *functions[256];
	size_t functions_size = 0;

	if (main != NULL) {
		functions[functions_size] = main;
		functions_size += 1;

		find_referenced_functions(main, functions, &functions_size);
	}

	for (size_t rayshader_index = 0; rayshader_index < rayshaders_count; ++rayshader_index) {
		functions[functions_size] = rayshaders[rayshader_index];
		functions_size += 1;
		find_referenced_functions(rayshaders[rayshader_index], functions, &functions_size);
	}

	// find payloads
	for (size_t i = 0; i < functions_size; ++i) {
		function *f = functions[i];

		uint8_t *data = f->code.o;
		size_t size = f->code.size;

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];
			switch (o->type) {
			case OPCODE_CALL: {
				if (o->op_call.func == add_name("trace_ray")) {
					debug_context context = {0};
					check(o->op_call.parameters_size == 3, context, "trace_ray requires three parameters");

					type_id payload_type = o->op_call.parameters[2].type.type;

					bool found = false;
					for (size_t payload_index = 0; payload_index < payload_types_count; ++payload_index) {
						if (payload_types[payload_index] == payload_type) {
							found = true;
							break;
						}
					}

					if (!found) {
						payload_types[payload_types_count] = payload_type;
						payload_types_count += 1;
					}
				}
			}
			default:
				break;
			}
			index += o->size;
		}
	}

	// function declarations
	for (size_t i = 0; i < functions_size; ++i) {
		function *f = functions[i];

		if (f != main && !is_raygen_shader(f) && !is_raymiss_shader(f) && !is_rayclosesthit_shader(f) && !is_rayintersection_shader(f) &&
		    !is_rayanyhit_shader(f)) {

			uint64_t parameter_ids[256] = {0};
			for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
				for (size_t i = 0; i < f->block->block.vars.size; ++i) {
					if (f->parameter_names[parameter_index] == f->block->block.vars.v[i].name) {
						parameter_ids[parameter_index] = f->block->block.vars.v[i].variable_id;
						break;
					}
				}
			}

			*offset += sprintf(&hlsl[*offset], "%s %s(", type_string(f->return_type.type), get_name(f->name));
			for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
				char *payload_prefix = "";
				if (is_payload_type(f->parameter_types[parameter_index].type)) {
					payload_prefix = "inout ";
				}

				if (parameter_index == 0) {

					*offset += sprintf(&hlsl[*offset], "%s%s _%" PRIu64, payload_prefix, type_string(f->parameter_types[parameter_index].type),
					                   parameter_ids[parameter_index]);
				}
				else {
					*offset += sprintf(&hlsl[*offset], ", %s%s _%" PRIu64, payload_prefix, type_string(f->parameter_types[parameter_index].type),
					                   parameter_ids[parameter_index]);
				}
			}
			*offset += sprintf(&hlsl[*offset], ");\n");
		}
	}

	*offset += sprintf(&hlsl[*offset], "\n");

	for (size_t i = 0; i < functions_size; ++i) {
		function *f = functions[i];
		assert(f != NULL);

		debug_context context = {0};
		check(f->block != NULL, context, "Function block missing");

		uint8_t *data = f->code.o;
		size_t size = f->code.size;

		uint64_t parameter_ids[256] = {0};
		for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
			for (size_t i = 0; i < f->block->block.vars.size; ++i) {
				if (f->parameter_names[parameter_index] == f->block->block.vars.v[i].name) {
					parameter_ids[parameter_index] = f->block->block.vars.v[i].variable_id;
					break;
				}
			}
		}

		for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
			check(parameter_ids[parameter_index] != 0, context, "Parameter not found");
		}

		if (f == main) {
			if (stage == SHADER_STAGE_VERTEX) {
				write_root_signature(hlsl, offset);
				*offset += sprintf(&hlsl[*offset], "%s main(", type_string(f->return_type.type));
				for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
					if (parameter_index == 0) {
						*offset +=
						    sprintf(&hlsl[*offset], "%s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
					}
					else {
						*offset +=
						    sprintf(&hlsl[*offset], ", %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
					}
				}
				*offset += sprintf(&hlsl[*offset], ") {\n");
			}
			else if (stage == SHADER_STAGE_FRAGMENT) {
				if (get_type(f->return_type.type)->array_size > 0) {
					*offset += sprintf(&hlsl[*offset], "struct _kong_colors_out {\n");
					for (uint32_t j = 0; j < get_type(f->return_type.type)->array_size; ++j) {
						*offset += sprintf(&hlsl[*offset], "\t%s _%i : SV_Target%i;\n", type_string(f->return_type.type), j, j);
					}
					*offset += sprintf(&hlsl[*offset], "};\n\n");

					write_root_signature(hlsl, offset);
					*offset += sprintf(&hlsl[*offset], "_kong_colors_out main(");
					for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
						if (parameter_index == 0) {
							*offset +=
							    sprintf(&hlsl[*offset], "%s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
						}
						else {
							*offset +=
							    sprintf(&hlsl[*offset], "%s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
						}
					}
					*offset += sprintf(&hlsl[*offset], ") {\n");
				}
				else {
					write_root_signature(hlsl, offset);
					*offset += sprintf(&hlsl[*offset], "%s main(", type_string(f->return_type.type));
					for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
						if (parameter_index == 0) {
							*offset +=
							    sprintf(&hlsl[*offset], "%s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
						}
						else {
							*offset += sprintf(&hlsl[*offset], ", %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type),
							                   parameter_ids[parameter_index]);
						}
					}
					*offset += sprintf(&hlsl[*offset], ") : SV_Target0 {\n");
				}
			}
			else if (stage == SHADER_STAGE_COMPUTE) {
				attribute *threads_attribute = find_attribute(&f->attributes, add_name("threads"));
				if (threads_attribute == NULL || threads_attribute->paramters_count != 3) {
					debug_context context = {0};
					error(context, "Compute function requires a threads attribute with three parameters");
				}

				write_root_signature(hlsl, offset);
				*offset += sprintf(&hlsl[*offset], "[numthreads(%i, %i, %i)]\n%s main(", (int)threads_attribute->parameters[0],
				                   (int)threads_attribute->parameters[1], (int)threads_attribute->parameters[2], type_string(f->return_type.type));
				for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
					if (parameter_index == 0) {
						*offset +=
						    sprintf(&hlsl[*offset], "%s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
					}
					else {
						*offset +=
						    sprintf(&hlsl[*offset], ", %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
					}
				}
				if (f->parameters_size > 0) {
					*offset += sprintf(&hlsl[*offset], ", ");
				}
				*offset += sprintf(&hlsl[*offset], "in uint3 _kong_group_id : SV_GroupID, in uint3 _kong_group_thread_id : SV_GroupThreadID, in uint3 "
				                                   "_kong_dispatch_thread_id : SV_DispatchThreadID, in uint _kong_group_index : SV_GroupIndex) {\n");
			}
			else if (stage == SHADER_STAGE_AMPLIFICATION) {
				attribute *threads_attribute = find_attribute(&f->attributes, add_name("threads"));
				if (threads_attribute == NULL || threads_attribute->paramters_count != 3) {
					debug_context context = {0};
					error(context, "Compute function requires a threads attribute with three parameters");
				}

				*offset += sprintf(&hlsl[*offset], "[numthreads(%i, %i, %i)] %s main(", (int)threads_attribute->parameters[0],
				                   (int)threads_attribute->parameters[1], (int)threads_attribute->parameters[2], type_string(f->return_type.type));
				for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
					if (parameter_index == 0) {
						*offset +=
						    sprintf(&hlsl[*offset], "%s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
					}
					else {
						*offset +=
						    sprintf(&hlsl[*offset], ", %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
					}
				}
				if (f->parameters_size > 0) {
					*offset += sprintf(&hlsl[*offset], ", ");
				}
				*offset += sprintf(&hlsl[*offset], "in uint3 _kong_group_id : SV_GroupID, in uint3 _kong_group_thread_id : SV_GroupThreadID, in uint3 "
				                                   "_kong_dispatch_thread_id : SV_DispatchThreadID, in uint _kong_group_index : SV_GroupIndex) {\n");
			}
			else if (stage == SHADER_STAGE_MESH) {
				attribute *topology_attribute = find_attribute(&f->attributes, add_name("topology"));
				if (topology_attribute == NULL || topology_attribute->paramters_count != 1 || topology_attribute->parameters[0] != 0) {
					debug_context context = {0};
					error(context, "Mesh function requires a threads attribute with one parameter which has to be \"triangle\"");
				}

				attribute *threads_attribute = find_attribute(&f->attributes, add_name("threads"));
				if (threads_attribute == NULL || threads_attribute->paramters_count != 3) {
					debug_context context = {0};
					error(context, "Mesh function requires a threads attribute with three parameters");
				}

				attribute *tris_attribute = find_attribute(&f->attributes, add_name("tris"));
				if (tris_attribute == NULL || tris_attribute->paramters_count != 1) {
					debug_context context = {0};
					error(context, "Mesh function requires a tris attribute with one parameter");
				}

				attribute *vertices_attribute = find_attribute(&f->attributes, add_name("vertices"));
				if (vertices_attribute == NULL || vertices_attribute->paramters_count != 2) {
					debug_context context = {0};
					error(context, "Mesh function requires a vertices attribute with two parameters");
				}

				type_id vertex_type = (type_id)vertices_attribute->parameters[1];
				char *vertex_name = get_name(get_type(vertex_type)->name);

				*offset += sprintf(&hlsl[*offset], "[outputtopology(\"triangle\")][numthreads(%i, %i, %i)] %s main(", (int)threads_attribute->parameters[0],
				                   (int)threads_attribute->parameters[1], (int)threads_attribute->parameters[2], type_string(f->return_type.type));
				for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
					if (parameter_index == 0) {
						*offset +=
						    sprintf(&hlsl[*offset], "%s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
					}
					else {
						*offset +=
						    sprintf(&hlsl[*offset], ", %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
					}
				}
				if (f->parameters_size > 0) {
					*offset += sprintf(&hlsl[*offset], ", ");
				}
				*offset +=
				    sprintf(&hlsl[*offset],
				            "out indices uint3 _kong_mesh_tris[%i], out vertices %s _kong_mesh_vertices[%i], in uint3 _kong_group_id : SV_GroupID, in uint3 "
				            "_kong_group_thread_id : SV_GroupThreadID, in uint3 "
				            "_kong_dispatch_thread_id : SV_DispatchThreadID, in uint _kong_group_index : SV_GroupIndex) {\n",
				            (int)tris_attribute->parameters[0], vertex_name, (int)vertices_attribute->parameters[0]);
			}
			else {
				debug_context context = {0};
				error(context, "Unsupported shader stage");
			}
		}
		else if (is_raygen_shader(f)) {
			*offset += sprintf(&hlsl[*offset], "[shader(\"raygeneration\")]\n");

			*offset += sprintf(&hlsl[*offset], "%s %s(", type_string(f->return_type.type), get_name(f->name));
			for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
				if (parameter_index == 0) {
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
				else {
					*offset += sprintf(&hlsl[*offset], ", %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
			}
			*offset += sprintf(&hlsl[*offset], ") {\n");
		}
		else if (is_raymiss_shader(f)) {
			*offset += sprintf(&hlsl[*offset], "[shader(\"miss\")]\n");

			*offset += sprintf(&hlsl[*offset], "%s %s(", type_string(f->return_type.type), get_name(f->name));
			for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
				if (parameter_index == 0) {
					*offset +=
					    sprintf(&hlsl[*offset], "inout %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
				else {
					*offset += sprintf(&hlsl[*offset], ", %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
			}
			*offset += sprintf(&hlsl[*offset], ") {\n");
		}
		else if (is_rayclosesthit_shader(f)) {
			debug_context context = {0};
			check(f->parameters_size == 2, context, "rayclosesthit shader requires two arguments");
			check(f->parameter_types[1].type == float2_id, context, "Second parameter of a rayclosesthit shader needs to be a float2");

			*offset += sprintf(&hlsl[*offset], "[shader(\"closesthit\")]\n");

			*offset += sprintf(&hlsl[*offset], "%s %s(", type_string(f->return_type.type), get_name(f->name));
			for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
				if (parameter_index == 0) {
					*offset +=
					    sprintf(&hlsl[*offset], "inout %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
				else if (parameter_index == 1) {
					*offset += sprintf(&hlsl[*offset], ", BuiltInTriangleIntersectionAttributes _kong_triangle_intersection_attributes");
				}
				else {
					*offset += sprintf(&hlsl[*offset], ", %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
			}
			*offset += sprintf(&hlsl[*offset], ") {\n");
			*offset += sprintf(&hlsl[*offset], "\t%s _%" PRIu64 " = _kong_triangle_intersection_attributes.barycentrics;\n",
			                   type_string(f->parameter_types[1].type), parameter_ids[1]);
		}
		else if (is_rayintersection_shader(f)) {
			debug_context context = {0};
			check(f->parameters_size == 0, context, "intersection shader can not have any parameters");

			*offset += sprintf(&hlsl[*offset], "[shader(\"intersection\")]\n");

			*offset += sprintf(&hlsl[*offset], "%s %s() {\n", type_string(f->return_type.type), get_name(f->name));
		}
		else if (is_rayanyhit_shader(f)) {
			debug_context context = {0};
			check(f->parameters_size == 2, context, "anyhit shader requires two arguments");
			check(f->parameter_types[1].type == float2_id, context, "Second parameter of a rayanyhit shader needs to be a float2");

			*offset += sprintf(&hlsl[*offset], "[shader(\"anyhit\")]\n");

			*offset += sprintf(&hlsl[*offset], "%s %s(", type_string(f->return_type.type), get_name(f->name));
			for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
				if (parameter_index == 0) {
					*offset +=
					    sprintf(&hlsl[*offset], "inout %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
				else if (parameter_index == 1) {
					*offset += sprintf(&hlsl[*offset], ", BuiltInTriangleIntersectionAttributes _kong_triangle_intersection_attributes");
				}
				else {
					*offset += sprintf(&hlsl[*offset], ", %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
			}
			*offset += sprintf(&hlsl[*offset], ") {\n");
			*offset += sprintf(&hlsl[*offset], "\t%s _%" PRIu64 " = _kong_triangle_intersection_attributes.barycentrics;\n",
			                   type_string(f->parameter_types[1].type), parameter_ids[1]);
		}
		else {
			*offset += sprintf(&hlsl[*offset], "%s %s(", type_string(f->return_type.type), get_name(f->name));
			for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
				char *payload_prefix = "";
				if (is_payload_type(f->parameter_types[parameter_index].type)) {
					payload_prefix = "inout ";
				}

				if (parameter_index == 0) {
					*offset += sprintf(&hlsl[*offset], "%s%s _%" PRIu64, payload_prefix, type_string(f->parameter_types[parameter_index].type),
					                   parameter_ids[parameter_index]);
				}
				else {
					*offset += sprintf(&hlsl[*offset], ", %s%s _%" PRIu64, payload_prefix, type_string(f->parameter_types[parameter_index].type),
					                   parameter_ids[parameter_index]);
				}
			}
			*offset += sprintf(&hlsl[*offset], ") {\n");
		}

		int indentation = 1;

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];
			switch (o->type) {
			case OPCODE_LOAD_MEMBER: {
				uint64_t global_var_index = 0;
				global *g = NULL;
				for (global_id j = 0; get_global(j) != NULL && get_global(j)->type != NO_TYPE; ++j) {
					g = get_global(j);
					if (o->op_load_member.from.index == g->var_index) {
						global_var_index = g->var_index;
						if (get_type(g->type)->built_in) {
							global_var_index = 0;
						}
						break;
					}
				}

				indent(hlsl, offset, indentation);
				*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = _%" PRIu64, type_string(o->op_load_member.to.type.type), o->op_load_member.to.index,
				                   o->op_load_member.from.index);
				type *s = get_type(o->op_load_member.member_parent_type);
				for (size_t i = 0; i < o->op_load_member.member_indices_size; ++i) {
					if (o->op_load_member.dynamic_member[i]) {
						type *from_type = get_type(o->op_load_member.from.type.type);

						if (global_var_index != 0 && i == 0 && get_type(from_type->base)->built_in) {
							*offset += sprintf(&hlsl[*offset], "[_%" PRIu64 "].data", o->op_load_member.dynamic_member_indices[i].index);
						}
						else if (from_type->array_size == UINT32_MAX && from_type->base == tex2d_type_id) {
							*offset += sprintf(&hlsl[*offset], "[NonUniformResourceIndex(_%" PRIu64 ")]", o->op_load_member.dynamic_member_indices[i].index);
						}
						else {
							*offset += sprintf(&hlsl[*offset], "[_%" PRIu64 "]", o->op_load_member.dynamic_member_indices[i].index);
						}

						s = get_type(o->op_load_member.dynamic_member_indices[i].type.type);
					}
					else {
						if (global_var_index != 0 && i == 0) {
							*offset += sprintf(&hlsl[*offset], "_%s", get_name(s->members.m[o->op_load_member.static_member_indices[i]].name));
						}
						else {
							*offset += sprintf(&hlsl[*offset], ".%s", get_name(s->members.m[o->op_load_member.static_member_indices[i]].name));
						}

						s = get_type(s->members.m[o->op_load_member.static_member_indices[i]].type.type);
					}
				}
				*offset += sprintf(&hlsl[*offset], ";\n");
				break;
			}
			case OPCODE_STORE_MEMBER:
			case OPCODE_SUB_AND_STORE_MEMBER:
			case OPCODE_ADD_AND_STORE_MEMBER:
			case OPCODE_DIVIDE_AND_STORE_MEMBER:
			case OPCODE_MULTIPLY_AND_STORE_MEMBER: {
				uint64_t global_var_index = 0;
				global *g = NULL;
				for (global_id j = 0; get_global(j) != NULL && get_global(j)->type != NO_TYPE; ++j) {
					g = get_global(j);
					if (o->op_store_member.to.index == g->var_index) {
						global_var_index = g->var_index;
						if (get_type(g->type)->built_in) {
							global_var_index = 0;
						}
						break;
					}
				}

				indent(hlsl, offset, indentation);
				*offset += sprintf(&hlsl[*offset], "_%" PRIu64, o->op_store_member.to.index);
				type *s = get_type(o->op_store_member.member_parent_type);
				bool is_array = o->op_store_member.member_parent_array;
				for (size_t i = 0; i < o->op_store_member.member_indices_size; ++i) {
					if (is_array) {
						type *from_type = get_type(o->op_store_member.member_parent_type);

						if (global_var_index != 0 && i == 0 && get_type(from_type->base)->built_in) {
							if (o->op_store_member.dynamic_member[i]) {
								*offset += sprintf(&hlsl[*offset], "[_%" PRIu64 "].data", o->op_store_member.dynamic_member_indices[i].index);
							}
							else {
								*offset += sprintf(&hlsl[*offset], "[%i].data", o->op_store_member.static_member_indices[i]);
							}
						}
						else if (o->op_store_member.dynamic_member[i]) {
							*offset += sprintf(&hlsl[*offset], "[_%" PRIu64 "]", o->op_store_member.dynamic_member_indices[i].index);
						}
						else {
							*offset += sprintf(&hlsl[*offset], "[%i]", o->op_store_member.static_member_indices[i]);
						}
						is_array = false;
					}
					else {
						debug_context context = {0};
						check(!o->op_store_member.dynamic_member[i], context, "Unexpected dynamic member");
						check(o->op_store_member.static_member_indices[i] < s->members.size, context, "Member index out of bounds");
						*offset += sprintf(&hlsl[*offset], ".%s", member_string(s, s->members.m[o->op_store_member.static_member_indices[i]].name));
						is_array = get_type(s->members.m[o->op_store_member.static_member_indices[i]].type.type)->array_size > 0;
						s = get_type(s->members.m[o->op_store_member.static_member_indices[i]].type.type);
					}
				}

				switch (o->type) {
				case OPCODE_STORE_MEMBER:
					*offset += sprintf(&hlsl[*offset], " = _%" PRIu64 ";\n", o->op_store_member.from.index);
					break;
				case OPCODE_SUB_AND_STORE_MEMBER:
					*offset += sprintf(&hlsl[*offset], " -= _%" PRIu64 ";\n", o->op_store_member.from.index);
					break;
				case OPCODE_ADD_AND_STORE_MEMBER:
					*offset += sprintf(&hlsl[*offset], " += _%" PRIu64 ";\n", o->op_store_member.from.index);
					break;
				case OPCODE_DIVIDE_AND_STORE_MEMBER:
					*offset += sprintf(&hlsl[*offset], " /= _%" PRIu64 ";\n", o->op_store_member.from.index);
					break;
				case OPCODE_MULTIPLY_AND_STORE_MEMBER:
					*offset += sprintf(&hlsl[*offset], " *= _%" PRIu64 ";\n", o->op_store_member.from.index);
					break;
				default:
					assert(false);
					break;
				}
				break;
			}
			case OPCODE_RETURN: {
				if (o->size > offsetof(opcode, op_return)) {
					if (f == main && stage == SHADER_STAGE_FRAGMENT && get_type(f->return_type.type)->array_size > 0) {
						indent(hlsl, offset, indentation);
						*offset += sprintf(&hlsl[*offset], "{\n");
						indent(hlsl, offset, indentation + 1);
						*offset += sprintf(&hlsl[*offset], "_kong_colors_out _kong_colors;\n");
						for (uint32_t j = 0; j < get_type(f->return_type.type)->array_size; ++j) {
							*offset += sprintf(&hlsl[*offset], "\t\t_kong_colors._%i = _%" PRIu64 "[%i];\n", j, o->op_return.var.index, j);
						}
						indent(hlsl, offset, indentation + 1);
						*offset += sprintf(&hlsl[*offset], "return _kong_colors;\n");
						indent(hlsl, offset, indentation);
						*offset += sprintf(&hlsl[*offset], "}\n");
					}
					else {
						indent(hlsl, offset, indentation);
						*offset += sprintf(&hlsl[*offset], "return _%" PRIu64 ";\n", o->op_return.var.index);
					}
				}
				else {
					indent(hlsl, offset, indentation);
					*offset += sprintf(&hlsl[*offset], "return;\n");
				}
				break;
			}
			case OPCODE_MULTIPLY: {
				if (o->op_binary.left.type.type == float4x4_id || o->op_binary.left.type.type == float3x3_id) {
					indent(hlsl, offset, indentation);
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = mul(_%" PRIu64 ", _%" PRIu64 ");\n", type_string(o->op_binary.result.type.type),
					                   o->op_binary.result.index, o->op_binary.right.index, o->op_binary.left.index);
				}
				else {
					indent(hlsl, offset, indentation);
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = _%" PRIu64 " * _%" PRIu64 ";\n", type_string(o->op_binary.result.type.type),
					                   o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				}
				break;
			}
			case OPCODE_CALL: {
				indent(hlsl, offset, indentation);
				debug_context context = {0};
				if (o->op_call.func == add_name("sample")) {
					check(o->op_call.parameters_size == 3, context, "sample requires three parameters");
					*offset +=
					    sprintf(&hlsl[*offset], "%s _%" PRIu64 " = _%" PRIu64 ".Sample(_%" PRIu64 ", _%" PRIu64 ");\n", type_string(o->op_call.var.type.type),
					            o->op_call.var.index, o->op_call.parameters[0].index, o->op_call.parameters[1].index, o->op_call.parameters[2].index);
				}
				else if (o->op_call.func == add_name("sample_lod")) {
					check(o->op_call.parameters_size == 4, context, "sample_lod requires four parameters");
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = _%" PRIu64 ".SampleLevel(_%" PRIu64 ", _%" PRIu64 ", _%" PRIu64 ");\n",
					                   type_string(o->op_call.var.type.type), o->op_call.var.index, o->op_call.parameters[0].index,
					                   o->op_call.parameters[1].index, o->op_call.parameters[2].index, o->op_call.parameters[3].index);
				}
				else if (o->op_call.func == add_name("group_id")) {
					check(o->op_call.parameters_size == 0, context, "group_id can not have a parameter");
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = _kong_group_id;\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("group_thread_id")) {
					check(o->op_call.parameters_size == 0, context, "group_thread_id can not have a parameter");
					*offset +=
					    sprintf(&hlsl[*offset], "%s _%" PRIu64 " = _kong_group_thread_id;\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("dispatch_thread_id")) {
					check(o->op_call.parameters_size == 0, context, "dispatch_thread_id can not have a parameter");
					*offset +=
					    sprintf(&hlsl[*offset], "%s _%" PRIu64 " = _kong_dispatch_thread_id;\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("group_index")) {
					check(o->op_call.parameters_size == 0, context, "group_index can not have a parameter");
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = _kong_group_index;\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("instance_id")) {
					check(o->op_call.parameters_size == 0, context, "instance_id can not have a parameter");
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = InstanceID();\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("world_ray_direction")) {
					check(o->op_call.parameters_size == 0, context, "world_ray_direction can not have a parameter");
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = WorldRayDirection();\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("world_ray_origin")) {
					check(o->op_call.parameters_size == 0, context, "world_ray_origin can not have a parameter");
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = WorldRayOrigin();\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("ray_length")) {
					check(o->op_call.parameters_size == 0, context, "ray_length can not have a parameter");
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = RayTCurrent();\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("ray_index")) {
					check(o->op_call.parameters_size == 0, context, "ray_index can not have a parameter");
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = DispatchRaysIndex();\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("ray_dimensions")) {
					check(o->op_call.parameters_size == 0, context, "ray_dimensions can not have a parameter");
					*offset +=
					    sprintf(&hlsl[*offset], "%s _%" PRIu64 " = DispatchRaysDimensions();\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("object_to_world3x3")) {
					check(o->op_call.parameters_size == 0, context, "object_to_world3x3 can not have a parameter");
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = (float3x3)ObjectToWorld4x3();\n", type_string(o->op_call.var.type.type),
					                   o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("primitive_index")) {
					check(o->op_call.parameters_size == 0, context, "primitive_index can not have a parameter");
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = PrimitiveIndex();\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("saturate3")) {
					check(o->op_call.parameters_size == 1, context, "saturate3 requires one parameter");
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = saturate(_%" PRIu64 ");\n", type_string(o->op_call.var.type.type),
					                   o->op_call.var.index, o->op_call.parameters[0].index);
				}
				else if (o->op_call.func == add_name("trace_ray")) {
					check(o->op_call.parameters_size == 3, context, "trace_ray requires three parameters");
					*offset += sprintf(&hlsl[*offset], "TraceRay(_%" PRIu64 ", RAY_FLAG_NONE, 0xFF, 0, 0, 0, _%" PRIu64 ", _%" PRIu64 ");\n",
					                   o->op_call.parameters[0].index, o->op_call.parameters[1].index, o->op_call.parameters[2].index);
				}
				else if (o->op_call.func == add_name("dispatch_mesh")) {
					check(o->op_call.parameters_size == 4, context, "dispatch_mesh requires four parameters");
					*offset +=
					    sprintf(&hlsl[*offset], "DispatchMesh(_%" PRIu64 ", _%" PRIu64 ", _%" PRIu64 ", _%" PRIu64 ");\n", o->op_call.parameters[0].index,
					            o->op_call.parameters[1].index, o->op_call.parameters[2].index, o->op_call.parameters[3].index);
				}
				else if (o->op_call.func == add_name("set_mesh_output_counts")) {
					check(o->op_call.parameters_size == 2, context, "set_mesh_output_counts requires two parameters");
					*offset += sprintf(&hlsl[*offset], "SetMeshOutputCounts(_%" PRIu64 ", _%" PRIu64 ");\n", o->op_call.parameters[0].index,
					                   o->op_call.parameters[1].index);
				}
				else if (o->op_call.func == add_name("set_mesh_triangle")) {
					check(o->op_call.parameters_size == 2, context, "set_mesh_triangle requires two parameters");
					*offset += sprintf(&hlsl[*offset], "_kong_mesh_tris[_%" PRIu64 "] = _%" PRIu64 ";\n", o->op_call.parameters[0].index,
					                   o->op_call.parameters[1].index);
				}
				else if (o->op_call.func == add_name("set_mesh_vertex")) {
					check(o->op_call.parameters_size == 2, context, "set_mesh_vertex requires two parameters");
					*offset += sprintf(&hlsl[*offset], "_kong_mesh_vertices[_%" PRIu64 "] = _%" PRIu64 ";\n", o->op_call.parameters[0].index,
					                   o->op_call.parameters[1].index);
				}
				else {
					if (o->op_call.var.type.type == void_id) {
						*offset += sprintf(&hlsl[*offset], "%s(", function_string(o->op_call.func));
					}
					else {
						*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = %s(", type_string(o->op_call.var.type.type), o->op_call.var.index,
						                   function_string(o->op_call.func));
					}
					if (o->op_call.parameters_size > 0) {
						*offset += sprintf(&hlsl[*offset], "_%" PRIu64, o->op_call.parameters[0].index);
						for (uint8_t i = 1; i < o->op_call.parameters_size; ++i) {
							*offset += sprintf(&hlsl[*offset], ", _%" PRIu64, o->op_call.parameters[i].index);
						}
					}
					*offset += sprintf(&hlsl[*offset], ");\n");
				}
				break;
			}
			default:
				cstyle_write_opcode(hlsl, offset, o, type_string, &indentation);
				break;
			}

			index += o->size;
		}

		*offset += sprintf(&hlsl[*offset], "}\n\n");
	}
}

static void hlsl_export_vertex(char *directory, api_kind d3d, function *main) {
	char *hlsl = (char *)calloc(1024 * 1024, 1);
	size_t offset = 0;

	assert(main->parameters_size > 0);
	type_id vertex_inputs[64];
	for (size_t input_index = 0; input_index < main->parameters_size; ++input_index) {
		vertex_inputs[input_index] = main->parameter_types[input_index].type;
	}
	type_id vertex_output = main->return_type.type;

	debug_context context = {0};
	check(main->parameters_size > 0, context, "vertex input missing");
	check(vertex_output != NO_TYPE, context, "vertex output missing");

	write_types(hlsl, &offset, SHADER_STAGE_VERTEX, vertex_inputs, main->parameters_size, vertex_output, main, NULL, 0);

	write_globals(hlsl, &offset, main, NULL, 0);

	write_functions(hlsl, &offset, SHADER_STAGE_VERTEX, main, NULL, 0);

	uint8_t *output = NULL;
	size_t output_size = 0;
	int result = 1;
	switch (d3d) {
	case API_DIRECT3D9:
		result = compile_hlsl_to_d3d9(hlsl, &output, &output_size, SHADER_STAGE_VERTEX, false);
		break;
	case API_DIRECT3D11:
		result = compile_hlsl_to_d3d11(hlsl, &output, &output_size, SHADER_STAGE_VERTEX, false);
		break;
	case API_DIRECT3D12:
		result = compile_hlsl_to_d3d12(hlsl, &output, &output_size, SHADER_STAGE_VERTEX, false);
		break;
	default:
		error(context, "Unsupported API for HLSL");
	}
	check(result == 0, context, "HLSL compilation failed");

	char *name = get_name(main->name);

	char filename[512];
	sprintf(filename, "kong_%s", name);

	char var_name[256];
	sprintf(var_name, "%s_code", name);

	write_bytecode(hlsl, directory, filename, var_name, output, output_size);
}

static void hlsl_export_amplification(char *directory, function *main) {
	char *hlsl = (char *)calloc(1024 * 1024, 1);
	size_t offset = 0;

	write_types(hlsl, &offset, SHADER_STAGE_AMPLIFICATION, NULL, 0, NO_TYPE, main, NULL, 0);

	write_globals(hlsl, &offset, main, NULL, 0);

	write_functions(hlsl, &offset, SHADER_STAGE_AMPLIFICATION, main, NULL, 0);

	uint8_t *output = NULL;
	size_t output_size = 0;
	int result = compile_hlsl_to_d3d12(hlsl, &output, &output_size, SHADER_STAGE_AMPLIFICATION, false);

	debug_context context = {0};
	check(result == 0, context, "HLSL compilation failed");

	char *name = get_name(main->name);

	char filename[512];
	sprintf(filename, "kong_%s", name);

	char var_name[256];
	sprintf(var_name, "%s_code", name);

	write_bytecode(hlsl, directory, filename, var_name, output, output_size);
}

static void hlsl_export_mesh(char *directory, function *main) {
	char *hlsl = (char *)calloc(1024 * 1024, 1);
	size_t offset = 0;

	attribute *vertices_attribute = find_attribute(&main->attributes, add_name("vertices"));
	if (vertices_attribute == NULL || vertices_attribute->paramters_count != 2) {
		debug_context context = {0};
		error(context, "Mesh function requires a vertices attribute with two parameters");
	}
	assert(vertices_attribute != NULL);
	type_id vertex_output = (type_id)vertices_attribute->parameters[1];

	write_types(hlsl, &offset, SHADER_STAGE_MESH, NULL, 0, vertex_output, main, NULL, 0);

	write_globals(hlsl, &offset, main, NULL, 0);

	write_functions(hlsl, &offset, SHADER_STAGE_MESH, main, NULL, 0);

	uint8_t *output = NULL;
	size_t output_size = 0;
	int result = compile_hlsl_to_d3d12(hlsl, &output, &output_size, SHADER_STAGE_MESH, false);

	debug_context context = {0};
	check(result == 0, context, "HLSL compilation failed");

	char *name = get_name(main->name);

	char filename[512];
	sprintf(filename, "kong_%s", name);

	char var_name[256];
	sprintf(var_name, "%s_code", name);

	write_bytecode(hlsl, directory, filename, var_name, output, output_size);
}

static void hlsl_export_fragment(char *directory, api_kind d3d, function *main) {
	char *hlsl = (char *)calloc(1024 * 1024, 1);
	size_t offset = 0;

	assert(main->parameters_size > 0);
	type_id pixel_input = main->parameter_types[0].type;

	debug_context context = {0};
	check(pixel_input != NO_TYPE, context, "fragment input missing");

	write_types(hlsl, &offset, SHADER_STAGE_FRAGMENT, &pixel_input, 1, NO_TYPE, main, NULL, 0);

	write_globals(hlsl, &offset, main, NULL, 0);

	write_functions(hlsl, &offset, SHADER_STAGE_FRAGMENT, main, NULL, 0);

	uint8_t *output = NULL;
	size_t output_size = 0;
	int result = 1;
	switch (d3d) {
	case API_DIRECT3D9:
		result = compile_hlsl_to_d3d9(hlsl, &output, &output_size, SHADER_STAGE_FRAGMENT, false);
		break;
	case API_DIRECT3D11:
		result = compile_hlsl_to_d3d11(hlsl, &output, &output_size, SHADER_STAGE_FRAGMENT, false);
		break;
	case API_DIRECT3D12:
		result = compile_hlsl_to_d3d12(hlsl, &output, &output_size, SHADER_STAGE_FRAGMENT, false);
		break;
	default:
		error(context, "Unsupported API for HLSL");
	}
	check(result == 0, context, "HLSL compilation failed");

	char *name = get_name(main->name);

	char filename[512];
	sprintf(filename, "kong_%s", name);

	char var_name[256];
	sprintf(var_name, "%s_code", name);

	write_bytecode(hlsl, directory, filename, var_name, output, output_size);
}

static void hlsl_export_compute(char *directory, api_kind d3d, function *main) {
	char *hlsl = (char *)calloc(1024 * 1024, 1);
	size_t offset = 0;

	write_types(hlsl, &offset, SHADER_STAGE_COMPUTE, NULL, 0, NO_TYPE, main, NULL, 0);

	write_globals(hlsl, &offset, main, NULL, 0);

	write_functions(hlsl, &offset, SHADER_STAGE_COMPUTE, main, NULL, 0);

	debug_context context = {0};

	uint8_t *output = NULL;
	size_t output_size = 0;
	int result = 1;
	switch (d3d) {
	case API_DIRECT3D9:
		error(context, "Compute shaders are not supported in Direct3D 9");
		break;
	case API_DIRECT3D11:
		result = compile_hlsl_to_d3d11(hlsl, &output, &output_size, SHADER_STAGE_COMPUTE, false);
		break;
	case API_DIRECT3D12:
		result = compile_hlsl_to_d3d12(hlsl, &output, &output_size, SHADER_STAGE_COMPUTE, false);
		break;
	default:
		error(context, "Unsupported API for HLSL");
	}
	check(result == 0, context, "HLSL compilation failed");

	char *name = get_name(main->name);

	char filename[512];
	sprintf(filename, "kong_%s", name);

	char var_name[256];
	sprintf(var_name, "%s_code", name);

	write_bytecode(hlsl, directory, filename, var_name, output, output_size);
}

static void hlsl_export_all_ray_shaders(char *directory) {
	char *hlsl = (char *)calloc(1024 * 1024, 1);
	debug_context context = {0};
	check(hlsl != NULL, context, "Could not allocate the hlsl string");
	size_t offset = 0;

	function *all_rayshaders[256 * 3];
	size_t all_rayshaders_size = 0;
	for (size_t rayshader_index = 0; rayshader_index < raygen_shaders_size; ++rayshader_index) {
		all_rayshaders[all_rayshaders_size] = raygen_shaders[rayshader_index];
		all_rayshaders_size += 1;
	}
	for (size_t rayshader_index = 0; rayshader_index < raymiss_shaders_size; ++rayshader_index) {
		all_rayshaders[all_rayshaders_size] = raymiss_shaders[rayshader_index];
		all_rayshaders_size += 1;
	}
	for (size_t rayshader_index = 0; rayshader_index < rayclosesthit_shaders_size; ++rayshader_index) {
		all_rayshaders[all_rayshaders_size] = rayclosesthit_shaders[rayshader_index];
		all_rayshaders_size += 1;
	}
	for (size_t rayshader_index = 0; rayshader_index < rayintersection_shaders_size; ++rayshader_index) {
		all_rayshaders[all_rayshaders_size] = rayintersection_shaders[rayshader_index];
		all_rayshaders_size += 1;
	}
	for (size_t rayshader_index = 0; rayshader_index < rayanyhit_shaders_size; ++rayshader_index) {
		all_rayshaders[all_rayshaders_size] = rayanyhit_shaders[rayshader_index];
		all_rayshaders_size += 1;
	}

	if (all_rayshaders_size == 0) {
		char *name = "ray";

		char filename[512];
		sprintf(filename, "kong_%s", name);

		char full_filename[512];

		sprintf(full_filename, "%s/%s.h", directory, filename);
		FILE *file = fopen(full_filename, "wb");

		fprintf(file, "#ifndef KONG_%s_HEADER\n", name);
		fprintf(file, "#define KONG_%s_HEADER\n\n", name);

		fprintf(file, "#define KONG_HAS_NO_RAY_SHADERS\n\n");

		fprintf(file, "#endif\n");

		fclose(file);

		return;
	}

	write_types(hlsl, &offset, SHADER_STAGE_RAY_GENERATION, NULL, 0, NO_TYPE, NULL, all_rayshaders, all_rayshaders_size);

	write_globals(hlsl, &offset, NULL, all_rayshaders, all_rayshaders_size);

	write_functions(hlsl, &offset, SHADER_STAGE_RAY_GENERATION, NULL, all_rayshaders, all_rayshaders_size);

	uint8_t *output = NULL;
	size_t output_size = 0;
	int result = compile_hlsl_to_d3d12(hlsl, &output, &output_size, SHADER_STAGE_RAY_GENERATION, false);
	check(result == 0, context, "HLSL compilation failed");

	char *name = "ray";

	char filename[512];
	sprintf(filename, "kong_%s", name);

	char var_name[256];
	sprintf(var_name, "%s_code", name);

	write_bytecode(hlsl, directory, filename, var_name, output, output_size);
}

void hlsl_export(char *directory, api_kind d3d) {
	int cbv_index = 0;
	int srv_index = 0;
	int uav_index = 0;
	int sampler_index = 0;

	memset(global_register_indices, 0, sizeof(global_register_indices));

	for (global_id i = 0; get_global(i) != NULL && get_global(i)->type != NO_TYPE; ++i) {
		global *g = get_global(i);

		type *t = get_type(g->type);
		type_id base_type = t->array_size > 0 ? t->base : g->type;

		if (base_type == sampler_type_id) {
			global_register_indices[i] = sampler_index;
			sampler_index += 1;
		}
		else if (base_type == tex2d_type_id) {
			if (t->array_size == UINT32_MAX) {
				global_register_indices[i] = 0;
			}
			else if (has_attribute(&g->attributes, add_name("write"))) {
				global_register_indices[i] = uav_index;
				uav_index += 1;
			}
			else {
				global_register_indices[i] = srv_index;
				srv_index += 1;
			}
		}
		else if (base_type == texcube_type_id || base_type == tex2darray_type_id || base_type == bvh_type_id) {
			global_register_indices[i] = srv_index;
			srv_index += 1;
		}
		else if (get_type(g->type)->built_in) {
			if (get_type(g->type)->array_size > 0) {
				global_register_indices[i] = uav_index;
				uav_index += 1;
			}
		}
		else {
			if (get_type(g->type)->array_size > 0) {
				global_register_indices[i] = uav_index;
				uav_index += 1;
			}
			else {
				global_register_indices[i] = cbv_index;
				cbv_index += 1;
			}
		}
	}

	function *vertex_shaders[256];
	size_t vertex_shaders_size = 0;

	function *amplification_shaders[256];
	size_t amplification_shaders_size = 0;

	function *mesh_shaders[256];
	size_t mesh_shaders_size = 0;

	function *fragment_shaders[256];
	size_t fragment_shaders_size = 0;

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
			name_id vertex_shader_name = NO_NAME;
			name_id amplification_shader_name = NO_NAME;
			name_id mesh_shader_name = NO_NAME;
			name_id fragment_shader_name = NO_NAME;

			for (size_t j = 0; j < t->members.size; ++j) {
				if (t->members.m[j].name == add_name("vertex")) {
					vertex_shader_name = t->members.m[j].value.identifier;
				}
				else if (t->members.m[j].name == add_name("amplification")) {
					amplification_shader_name = t->members.m[j].value.identifier;
				}
				else if (t->members.m[j].name == add_name("mesh")) {
					mesh_shader_name = t->members.m[j].value.identifier;
				}
				else if (t->members.m[j].name == add_name("fragment")) {
					fragment_shader_name = t->members.m[j].value.identifier;
				}
			}

			debug_context context = {0};
			check(vertex_shader_name != NO_NAME || mesh_shader_name != NO_NAME, context, "vertex or mesh shader missing");
			check(fragment_shader_name != NO_NAME, context, "fragment shader missing");

			function *vertex_shader = NULL;
			function *amplification_shader = NULL;
			function *mesh_shader = NULL;
			function *fragment_shader = NULL;

			for (function_id i = 0; get_function(i) != NULL; ++i) {
				function *f = get_function(i);
				if (vertex_shader_name != NO_NAME && f->name == vertex_shader_name) {
					vertex_shader = f;
					vertex_shaders[vertex_shaders_size] = f;
					vertex_shaders_size += 1;
				}
				if (amplification_shader_name != NO_NAME && f->name == amplification_shader_name) {
					amplification_shader = f;
					amplification_shaders[amplification_shaders_size] = f;
					amplification_shaders_size += 1;
				}
				if (mesh_shader_name != NO_NAME && f->name == mesh_shader_name) {
					mesh_shader = f;
					mesh_shaders[mesh_shaders_size] = f;
					mesh_shaders_size += 1;
				}
				if (f->name == fragment_shader_name) {
					fragment_shader = f;
					fragment_shaders[fragment_shaders_size] = f;
					fragment_shaders_size += 1;
				}
			}

			global_id all_globals[256];
			size_t all_globals_size = 0;

			if (vertex_shader != NULL) {
				find_referenced_globals(vertex_shader, all_globals, &all_globals_size);
			}
			if (amplification_shader != NULL) {
				find_referenced_globals(amplification_shader, all_globals, &all_globals_size);
			}
			if (mesh_shader != NULL) {
				find_referenced_globals(mesh_shader, all_globals, &all_globals_size);
			}
			if (fragment_shader != NULL) {
				find_referenced_globals(fragment_shader, all_globals, &all_globals_size);
			}

			for (size_t global_index = 0; global_index < all_globals_size; ++global_index) {
				global *g = get_global(all_globals[global_index]);
				if (g->set != NULL) {
					bool found = false;

					for (size_t set_index = 0; set_index < all_descriptor_sets_count; ++set_index) {
						if (all_descriptor_sets[set_index] == g->set) {
							found = true;
							break;
						}
					}

					if (!found) {
						all_descriptor_sets[all_descriptor_sets_count] = g->set;
						all_descriptor_sets_count += 1;
					}
				}
			}
		}
	}

	function *compute_shaders[256];
	size_t compute_shaders_size = 0;

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);
		if (has_attribute(&f->attributes, add_name("compute"))) {
			global_id all_globals[256];
			size_t all_globals_size = 0;

			find_referenced_globals(f, all_globals, &all_globals_size);

			for (size_t global_index = 0; global_index < all_globals_size; ++global_index) {
				global *g = get_global(all_globals[global_index]);
				if (g->set != NULL) {
					bool found = false;

					for (size_t set_index = 0; set_index < all_descriptor_sets_count; ++set_index) {
						if (all_descriptor_sets[set_index] == g->set) {
							found = true;
							break;
						}
					}

					if (!found) {
						all_descriptor_sets[all_descriptor_sets_count] = g->set;
						all_descriptor_sets_count += 1;
					}
				}
			}

			compute_shaders[compute_shaders_size] = f;
			compute_shaders_size += 1;
		}
	}

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in && has_attribute(&t->attributes, add_name("raypipe"))) {
			name_id raygen_shader_name = NO_NAME;
			name_id raymiss_shader_name = NO_NAME;
			name_id rayclosesthit_shader_name = NO_NAME;
			name_id rayintersection_shader_name = NO_NAME;
			name_id rayanyhit_shader_name = NO_NAME;

			for (size_t j = 0; j < t->members.size; ++j) {
				if (t->members.m[j].name == add_name("gen")) {
					raygen_shader_name = t->members.m[j].value.identifier;
				}
				else if (t->members.m[j].name == add_name("miss")) {
					raymiss_shader_name = t->members.m[j].value.identifier;
				}
				else if (t->members.m[j].name == add_name("closest")) {
					rayclosesthit_shader_name = t->members.m[j].value.identifier;
				}
				else if (t->members.m[j].name == add_name("intersection")) {
					rayintersection_shader_name = t->members.m[j].value.identifier;
				}
				else if (t->members.m[j].name == add_name("any")) {
					rayanyhit_shader_name = t->members.m[j].value.identifier;
				}
			}

			debug_context context = {0};
			check(raygen_shader_name != NO_NAME, context, "Ray generation shader missing");
			check(raymiss_shader_name != NO_NAME, context, "Miss shader missing");
			check(rayclosesthit_shader_name != NO_NAME, context, "Closest hit shader missing");

			for (function_id i = 0; get_function(i) != NULL; ++i) {
				function *f = get_function(i);
				if (f->name == raygen_shader_name) {
					raygen_shaders[raygen_shaders_size] = f;
					raygen_shaders_size += 1;
				}
				else if (f->name == raymiss_shader_name) {
					raymiss_shaders[raymiss_shaders_size] = f;
					raymiss_shaders_size += 1;
				}
				else if (f->name == rayclosesthit_shader_name) {
					rayclosesthit_shaders[rayclosesthit_shaders_size] = f;
					rayclosesthit_shaders_size += 1;
				}
				else if (f->name == rayintersection_shader_name) {
					rayintersection_shaders[rayintersection_shaders_size] = f;
					rayintersection_shaders_size += 1;
				}
				else if (f->name == rayanyhit_shader_name) {
					rayanyhit_shaders[rayanyhit_shaders_size] = f;
					rayanyhit_shaders_size += 1;
				}
			}
		}
	}

	for (size_t i = 0; i < vertex_shaders_size; ++i) {
		hlsl_export_vertex(directory, d3d, vertex_shaders[i]);
	}

	if (d3d == API_DIRECT3D12) {
		for (size_t i = 0; i < amplification_shaders_size; ++i) {
			hlsl_export_amplification(directory, amplification_shaders[i]);
		}

		for (size_t i = 0; i < mesh_shaders_size; ++i) {
			hlsl_export_mesh(directory, mesh_shaders[i]);
		}
	}

	for (size_t i = 0; i < fragment_shaders_size; ++i) {
		hlsl_export_fragment(directory, d3d, fragment_shaders[i]);
	}

	for (size_t i = 0; i < compute_shaders_size; ++i) {
		hlsl_export_compute(directory, d3d, compute_shaders[i]);
	}

	if (d3d == API_DIRECT3D12) {
		hlsl_export_all_ray_shaders(directory);
	}
}
