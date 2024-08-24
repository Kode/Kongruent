#include "hlsl.h"

#include "../compiler.h"
#include "../errors.h"
#include "../functions.h"
#include "../parser.h"
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
	return get_name(get_type(type)->name);
}

static char *function_string(name_id func) {
	return get_name(func);
}

static void write_bytecode(char *hlsl, char *directory, const char *filename, const char *name, uint8_t *output, size_t output_size) {
	char full_filename[512];

	{
		sprintf(full_filename, "%s/%s.h", directory, filename);
		FILE *file = fopen(full_filename, "wb");
		fprintf(file, "#include <stddef.h>\n");
		fprintf(file, "#include <stdint.h>\n\n");
		fprintf(file, "extern uint8_t *%s;\n", name);
		fprintf(file, "extern size_t %s_size;\n", name);
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

static void find_referenced_global_for_var(variable v, global_id *globals, size_t *globals_size) {
	for (global_id j = 0; get_global(j).type != NO_TYPE; ++j) {
		global g = get_global(j);
		if (v.index == g.var_index) {
			bool found = false;
			for (size_t k = 0; k < *globals_size; ++k) {
				if (globals[k] == j) {
					found = true;
					break;
				}
			}
			if (!found) {
				globals[*globals_size] = j;
				*globals_size += 1;
			}
			return;
		}
	}
}

static void find_referenced_globals(function *f, global_id *globals, size_t *globals_size) {
	if (f->block == NULL) {
		// built-in
		return;
	}

	function *functions[256];
	size_t functions_size = 0;

	functions[functions_size] = f;
	functions_size += 1;

	find_referenced_functions(f, functions, &functions_size);

	for (size_t l = 0; l < functions_size; ++l) {
		uint8_t *data = functions[l]->code.o;
		size_t size = functions[l]->code.size;

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];
			switch (o->type) {
			case OPCODE_MULTIPLY:
			case OPCODE_DIVIDE:
			case OPCODE_ADD:
			case OPCODE_SUB:
			case OPCODE_EQUALS:
			case OPCODE_NOT_EQUALS:
			case OPCODE_GREATER:
			case OPCODE_GREATER_EQUAL:
			case OPCODE_LESS:
			case OPCODE_LESS_EQUAL: {
				find_referenced_global_for_var(o->op_binary.left, globals, globals_size);
				find_referenced_global_for_var(o->op_binary.right, globals, globals_size);
				break;
			}
			case OPCODE_LOAD_MEMBER: {
				find_referenced_global_for_var(o->op_load_member.from, globals, globals_size);
				break;
			}
			case OPCODE_CALL: {
				for (uint8_t i = 0; i < o->op_call.parameters_size; ++i) {
					find_referenced_global_for_var(o->op_call.parameters[i], globals, globals_size);
				}
				break;
			}
			}

			index += o->size;
		}
	}
}

static void write_types(char *hlsl, size_t *offset, shader_stage stage, type_id input, type_id output, function *main, function **rayshaders,
                        size_t rayshaders_count) {
	type_id types[256];
	size_t types_size = 0;
	if (main != NULL) {
		find_referenced_types(main, types, &types_size);
	}
	for (size_t rayshader_index = 0; rayshader_index < rayshaders_count; ++rayshader_index) {
		find_referenced_types(rayshaders[rayshader_index], types, &types_size);
	}

	for (size_t i = 0; i < types_size; ++i) {
		type *t = get_type(types[i]);

		if (!t->built_in && !has_attribute(&t->attributes, add_name("pipe"))) {
			*offset += sprintf(&hlsl[*offset], "struct %s {\n", get_name(t->name));

			if (stage == SHADER_STAGE_VERTEX && types[i] == input) {
				for (size_t j = 0; j < t->members.size; ++j) {
					*offset += sprintf(&hlsl[*offset], "\t%s %s : TEXCOORD%zu;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name), j);
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
			else if (stage == SHADER_STAGE_FRAGMENT && types[i] == input) {
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
		global g = get_global(globals[i]);
		int register_index = global_register_indices[globals[i]];

		if (g.type == sampler_type_id) {
			*offset += sprintf(&hlsl[*offset], "SamplerState _%" PRIu64 " : register(s%i);\n\n", g.var_index, register_index);
		}
		else if (g.type == tex2d_type_id) {
			*offset += sprintf(&hlsl[*offset], "Texture2D<float4> _%" PRIu64 " : register(t%i);\n\n", g.var_index, register_index);
		}
		else if (g.type == texcube_type_id) {
			*offset += sprintf(&hlsl[*offset], "TextureCube<float4> _%" PRIu64 " : register(t%i);\n\n", g.var_index, register_index);
		}
		else if (g.type == float_id) {
			*offset += sprintf(&hlsl[*offset], "static const float _%" PRIu64 " = %f;\n\n", g.var_index, g.value.value.floats[0]);
		}
		else if (g.type == float2_id) {
			*offset += sprintf(&hlsl[*offset], "static const float2 _%" PRIu64 " = float2(%f, %f);\n\n", g.var_index, g.value.value.floats[0], g.value.value.floats[1]);
		}
		else if (g.type == float3_id) {
			*offset += sprintf(&hlsl[*offset], "static const float3 _%" PRIu64 " = float3(%f, %f, %f);\n\n", g.var_index, g.value.value.floats[0],
			                   g.value.value.floats[1], g.value.value.floats[2]);
		}
		else if (g.type == float4_id) {
			*offset += sprintf(&hlsl[*offset], "static const float4 _%" PRIu64 " = float4(%f, %f, %f, %f);\n\n", g.var_index, g.value.value.floats[0],
			                   g.value.value.floats[1], g.value.value.floats[2], g.value.value.floats[3]);
		}
		else {
			*offset += sprintf(&hlsl[*offset], "cbuffer _%" PRIu64 " : register(b%i) {\n", g.var_index, register_index);
			type *t = get_type(g.type);
			for (size_t i = 0; i < t->members.size; ++i) {
				*offset +=
				    sprintf(&hlsl[*offset], "\t%s _%" PRIu64 "_%s;\n", type_string(t->members.m[i].type.type), g.var_index, get_name(t->members.m[i].name));
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

	for (size_t i = 0; i < functions_size; ++i) {
		function *f = functions[i];

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
				*offset += sprintf(&hlsl[*offset], "%s main(", type_string(f->return_type.type));
				for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
					if (parameter_index == 0) {
						*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64, type_string(f->parameter_types[parameter_index].type),
						                   parameter_ids[parameter_index]);
					}
					else {
						*offset += sprintf(&hlsl[*offset], ", %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type),
						                   parameter_ids[parameter_index]);
					}
				}
				*offset += sprintf(&hlsl[*offset], ") {\n");
			}
			else if (stage == SHADER_STAGE_FRAGMENT) {
				if (f->return_type.array_size > 0) {
					*offset += sprintf(&hlsl[*offset], "struct _kong_colors_out {\n");
					for (uint32_t j = 0; j < f->return_type.array_size; ++j) {
						*offset += sprintf(&hlsl[*offset], "\t%s _%i : SV_Target%i;\n", type_string(f->return_type.type), j, j);
					}
					*offset += sprintf(&hlsl[*offset], "};\n\n");
					
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
					*offset += sprintf(&hlsl[*offset], ") : SV_Target0 {\n");
				}
			}
			else if (stage == SHADER_STAGE_COMPUTE) {
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
				*offset += sprintf(&hlsl[*offset], ") {\n");
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
					*offset += sprintf(&hlsl[*offset], "inout %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
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
					*offset += sprintf(&hlsl[*offset], "inout %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
				else if (parameter_index == 1) {
					*offset += sprintf(&hlsl[*offset], ", BuiltInTriangleIntersectionAttributes _kong_triangle_intersection_attributes"); 
				}
				else {
					*offset += sprintf(&hlsl[*offset], ", %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
			}
			*offset += sprintf(&hlsl[*offset], ") {\n");
			*offset += sprintf(&hlsl[*offset], "\t%s _%" PRIu64 " = _kong_triangle_intersection_attributes.barycentrics;\n", type_string(f->parameter_types[1].type), parameter_ids[1]);
		}
		else {
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

		int indentation = 1;

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];
			switch (o->type) {
			case OPCODE_LOAD_MEMBER: {
				uint64_t global_var_index = 0;
				for (global_id j = 0; get_global(j).type != NO_TYPE; ++j) {
					global g = get_global(j);
					if (o->op_load_member.from.index == g.var_index) {
						global_var_index = g.var_index;
						break;
					}
				}

				indent(hlsl, offset, indentation);
				*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = _%" PRIu64, type_string(o->op_load_member.to.type.type), o->op_load_member.to.index,
				                   o->op_load_member.from.index);
				type *s = get_type(o->op_load_member.member_parent_type);
				for (size_t i = 0; i < o->op_load_member.member_indices_size; ++i) {
					if (global_var_index != 0) {
						*offset += sprintf(&hlsl[*offset], "_%s", get_name(s->members.m[o->op_load_member.member_indices[i]].name));
					}
					else {
						*offset += sprintf(&hlsl[*offset], ".%s", get_name(s->members.m[o->op_load_member.member_indices[i]].name));
					}
					s = get_type(s->members.m[o->op_load_member.member_indices[i]].type.type);
				}
				*offset += sprintf(&hlsl[*offset], ";\n");
				break;
			}
			case OPCODE_RETURN: {
				if (o->size > offsetof(opcode, op_return)) {
					if (f == main && stage == SHADER_STAGE_FRAGMENT && f->return_type.array_size > 0) {
						indent(hlsl, offset, indentation);
						*offset += sprintf(&hlsl[*offset], "{\n");
						indent(hlsl, offset, indentation + 1);
						*offset += sprintf(&hlsl[*offset], "_kong_colors_out _kong_colors;\n");
						for (uint32_t j = 0; j < f->return_type.array_size; ++j) {
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
				if (o->op_binary.left.type.type == float4x4_id) {
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
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = SV_GroupID;\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("group_thread_id")) {
					check(o->op_call.parameters_size == 0, context, "group_thread_id can not have a parameter");
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = SV_GroupThreadID;\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("dispatch_thread_id")) {
					check(o->op_call.parameters_size == 0, context, "dispatch_thread_id can not have a parameter");
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = SV_DispatchThreadID;\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("group_index")) {
					check(o->op_call.parameters_size == 0, context, "group_index can not have a parameter");
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = SV_GroupIndex;\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else {
					*offset += sprintf(&hlsl[*offset], "%s _%" PRIu64 " = %s(", type_string(o->op_call.var.type.type), o->op_call.var.index,
					                   function_string(o->op_call.func));
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
	type_id vertex_input = main->parameter_types[0].type;
	type_id vertex_output = main->return_type.type;

	debug_context context = {0};
	check(vertex_input != NO_TYPE, context, "vertex input missing");
	check(vertex_output != NO_TYPE, context, "vertex output missing");

	write_types(hlsl, &offset, SHADER_STAGE_VERTEX, vertex_input, vertex_output, main, NULL, 0);

	write_globals(hlsl, &offset, main, NULL, 0);

	write_functions(hlsl, &offset, SHADER_STAGE_VERTEX, main, NULL, 0);

	char *output = NULL;
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

static void hlsl_export_fragment(char *directory, api_kind d3d, function *main) {
	char *hlsl = (char *)calloc(1024 * 1024, 1);
	size_t offset = 0;

	assert(main->parameters_size > 0);
	type_id pixel_input = main->parameter_types[0].type;

	debug_context context = {0};
	check(pixel_input != NO_TYPE, context, "fragment input missing");

	write_types(hlsl, &offset, SHADER_STAGE_FRAGMENT, pixel_input, NO_TYPE, main, NULL, 0);

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

	write_types(hlsl, &offset, SHADER_STAGE_COMPUTE, NO_TYPE, NO_TYPE, main, NULL, 0);

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
	for (size_t rayshader_index = 0; rayshader_index < raygen_shaders_size; ++rayshader_index){
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

	write_types(hlsl, &offset, SHADER_STAGE_RAY_GENERATION, NO_TYPE, NO_TYPE, NULL, all_rayshaders, all_rayshaders_size);

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
		else if (g.type == float_id) {
		}
		else {
			global_register_indices[i] = cbuffer_index;
			cbuffer_index += 1;
		}
	}

	function *vertex_shaders[256];
	size_t vertex_shaders_size = 0;

	function *fragment_shaders[256];
	size_t fragment_shaders_size = 0;

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
			name_id vertex_shader_name = NO_NAME;
			name_id fragment_shader_name = NO_NAME;

			for (size_t j = 0; j < t->members.size; ++j) {
				if (t->members.m[j].name == add_name("vertex")) {
					vertex_shader_name = t->members.m[j].value.identifier;
				}
				else if (t->members.m[j].name == add_name("fragment")) {
					fragment_shader_name = t->members.m[j].value.identifier;
				}
			}

			debug_context context = {0};
			check(vertex_shader_name != NO_NAME, context, "vertex shader missing");
			check(fragment_shader_name != NO_NAME, context, "fragment shader missing");

			for (function_id i = 0; get_function(i) != NULL; ++i) {
				function *f = get_function(i);
				if (f->name == vertex_shader_name) {
					vertex_shaders[vertex_shaders_size] = f;
					vertex_shaders_size += 1;
				}
				else if (f->name == fragment_shader_name) {
					fragment_shaders[fragment_shaders_size] = f;
					fragment_shaders_size += 1;
				}
			}
		}
	}

	function *compute_shaders[256];
	size_t compute_shaders_size = 0;

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);
		if (has_attribute(&f->attributes, add_name("compute"))) {
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
			}
		}
	}

	for (size_t i = 0; i < vertex_shaders_size; ++i) {
		hlsl_export_vertex(directory, d3d, vertex_shaders[i]);
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
