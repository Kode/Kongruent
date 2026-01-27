#include "kompjuta.h"

#include "../analyzer.h"
#include "../compiler.h"
#include "../errors.h"
#include "../functions.h"
#include "../parser.h"
#include "../shader_stage.h"
#include "../types.h"
#include "cstyle.h"
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
		return "kore_float2";
	}
	if (type == float3_id) {
		return "kore_float3";
	}
	if (type == float4_id) {
		return "kore_float4";
	}
	if (type == float4x4_id) {
		return "kore_matrix4x4";
	}
	if (type == int2_id) {
		return "kore_int2";
	}
	if (type == int3_id) {
		return "kore_int3";
	}
	if (type == int4_id) {
		return "kore_int4";
	}
	if (type == uint_id) {
		return "uint32_t";
	}
	if (type == uint2_id) {
		return "kore_uint2";
	}
	if (type == uint3_id) {
		return "kore_uint3";
	}
	if (type == uint4_id) {
		return "kore_uint4";
	}
	return get_name(get_type(type)->name);
}

static char *type_string_simd(type_id type) {
	if (type == float_id) {
		return "vfloat32m1_t";
	}
	if (type == float2_id) {
		return "kore_float2_x32";
	}
	if (type == float3_id) {
		return "kore_float3_x32";
	}
	if (type == float4_id) {
		return "kore_float4_x32";
	}
	if (type == float4x4_id) {
		return "kore_matrix4_x32";
	}
	if (type == int_id) {
		return "kore_int32x4";
	}
	if (type == int2_id) {
		return "kore_int2x4";
	}
	if (type == int3_id) {
		return "kore_int3x4";
	}
	if (type == int4_id) {
		return "kore_int4x4";
	}
	if (type == uint_id) {
		return "kore_uint32x4";
	}
	if (type == uint2_id) {
		return "kore_uint2x4";
	}
	if (type == uint3_id) {
		return "kore_uint3x4";
	}
	if (type == uint4_id) {
		return "kore_uint4x4";
	}
	return get_name(get_type(type)->name);
}

static void write_code(char *code, char *header_code, char *directory, const char *filename, const char *name, shader_stage stage, function *main) {
	char full_filename[512];

	{
		sprintf(full_filename, "%s/%s.h", directory, filename);
		FILE *file = fopen(full_filename, "wb");
		fprintf(file, "#include <kong.h>\n\n");
		fprintf(file, "#include <stddef.h>\n");
		fprintf(file, "#include <stdint.h>\n\n");

		fprintf(file, "%s", header_code);

		if (stage == SHADER_STAGE_VERTEX) {
			uint64_t parameter_ids[256] = {0};
			for (uint8_t parameter_index = 0; parameter_index < main->parameters_size; ++parameter_index) {
				for (size_t i = 0; i < main->block->block.vars.size; ++i) {
					if (main->parameter_names[parameter_index] == main->block->block.vars.v[i].name) {
						parameter_ids[parameter_index] = main->block->block.vars.v[i].variable_id;
						break;
					}
				}
			}

			fprintf(file, "void vs_%s(size_t _lane_count, void *__output, ", name);
			for (uint8_t parameter_index = 0; parameter_index < main->parameters_size; ++parameter_index) {
				if (parameter_index == 0) {
					fprintf(file, "void *__%" PRIu64, parameter_ids[parameter_index]);
				}
				else {
					fprintf(file, ", %s *_%" PRIu64, type_string_simd(main->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
			}
			fprintf(file, ");\n");
		}
		else if (stage == SHADER_STAGE_FRAGMENT) {
			fprintf(file, "void fs_%s();\n\n", name);
		}
		else {
			fprintf(file, "void %s(uint32_t workgroup_count_x, uint32_t workgroup_count_y, uint32_t workgroup_count_z);\n\n", name);
		}

		fclose(file);
	}

	{
		sprintf(full_filename, "%s/%s.c", directory, filename);

		FILE *file = fopen(full_filename, "wb");
		fprintf(file, "#include \"%s.h\"\n\n", filename);

		fprintf(file, "#include <kore3/kompjuta/riscv_vector_util.h>\n\n");
		fprintf(file, "#include <string.h>\n\n");

		if (stage == SHADER_STAGE_FRAGMENT) {
			fprintf(file, "/*\n");
		}

		fprintf(file, "%s", code);

		if (stage == SHADER_STAGE_FRAGMENT) {
			fprintf(file, "*/\n");
			fprintf(file, "void fs_%s() {}\n", get_name(main->name));
		}

		fclose(file);
	}
}

static void write_types(char *code, size_t *offset, function *main) {
	type_id types[256];
	size_t  types_size = 0;
	find_referenced_types(main, types, &types_size);

	for (size_t i = 0; i < types_size; ++i) {
		type *t = get_type(types[i]);

		if (main->parameters_size > 0 && types[i] == main->parameter_types[0].type) {
			continue;
		}

		if (!t->built_in && !has_attribute(&t->attributes, add_name("pipe"))) {
			*offset += sprintf(&code[*offset], "typedef struct %s {\n", get_name(t->name));

			for (size_t j = 0; j < t->members.size; ++j) {
				*offset += sprintf(&code[*offset], "\t%s %s;\n", type_string_simd(t->members.m[j].type.type), get_name(t->members.m[j].name));
			}

			*offset += sprintf(&code[*offset], "} %s;\n\n", get_name(t->name));
		}
	}
}

static void write_globals(char *code, size_t *offset, char *header_code, size_t *header_offset, function *main) {
	global_array globals = {0};

	find_referenced_globals(main, &globals);

	for (size_t i = 0; i < globals.size; ++i) {
		global *g = get_global(globals.globals[i]);

		type   *t         = get_type(g->type);
		type_id base_type = t->array_size > 0 ? t->base : g->type;

		if (base_type == sampler_type_id) {
			*offset += sprintf(&code[*offset], "SamplerState _%" PRIu64 ";\n\n", g->var_index);
		}
		else if (get_type(base_type)->tex_kind != TEXTURE_KIND_NONE) {
			if (get_type(base_type)->tex_kind == TEXTURE_KIND_2D) {
				if (has_attribute(&g->attributes, add_name("write"))) {
					*offset += sprintf(&code[*offset], "RWTexture2D<float4> _%" PRIu64 ";\n\n", g->var_index);
				}
				else {
					if (t->array_size > 0 && t->array_size == UINT32_MAX) {
						*offset += sprintf(&code[*offset], "Texture2D<float4> _%" PRIu64 "[];\n\n", g->var_index);
					}
					else {
						*offset += sprintf(&code[*offset], "Texture2D<float4> _%" PRIu64 ";\n\n", g->var_index);
					}
				}
			}
			else if (get_type(base_type)->tex_kind == TEXTURE_KIND_2D_ARRAY) {
				*offset += sprintf(&code[*offset], "Texture2DArray<float4> _%" PRIu64 ";\n\n", g->var_index);
			}
			else if (get_type(base_type)->tex_kind == TEXTURE_KIND_CUBE) {
				*offset += sprintf(&code[*offset], "TextureCube<float4> _%" PRIu64 ";\n\n", g->var_index);
			}
			else {
				// TODO
				assert(false);
			}
		}
		else if (base_type == bvh_type_id) {
			*offset += sprintf(&code[*offset], "RaytracingAccelerationStructure  _%" PRIu64 ";\n\n", g->var_index);
		}
		else if (base_type == float_id) {
			*offset += sprintf(&code[*offset], "static const float _%" PRIu64 " = %f;\n\n", g->var_index, g->value.value.floats[0]);
		}
		else if (base_type == float2_id) {
			*offset += sprintf(&code[*offset], "static const kore_float2 _%" PRIu64 " = float2(%f, %f);\n\n", g->var_index, g->value.value.floats[0],
			                   g->value.value.floats[1]);
		}
		else if (base_type == float3_id) {
			*offset += sprintf(&code[*offset], "static const kore_float3 _%" PRIu64 " = float3(%f, %f, %f);\n\n", g->var_index, g->value.value.floats[0],
			                   g->value.value.floats[1], g->value.value.floats[2]);
		}
		else if (base_type == float4_id) {
			if (t->array_size > 0) {
				*header_offset += sprintf(&header_code[*header_offset], "void set_%s(kore_float4 *value);\n\n", get_name(g->name));

				*offset += sprintf(&code[*offset], "static kore_float4 *_%llu;\n\n", g->var_index);
				*offset += sprintf(&code[*offset], "void set_%s(kore_float4 *value) {\n", get_name(g->name));
				*offset += sprintf(&code[*offset], "\t_%" PRIu64 " = value;\n", g->var_index);
				*offset += sprintf(&code[*offset], "}\n\n");
			}
			else {
				*offset += sprintf(&code[*offset], "static const float4 _%" PRIu64 " = float4(%f, %f, %f, %f);\n\n", g->var_index, g->value.value.floats[0],
				                   g->value.value.floats[1], g->value.value.floats[2], g->value.value.floats[3]);
			}
		}
		else {
			*header_offset += sprintf(&header_code[*header_offset], "void set_%s(%s_type *value);\n\n", get_name(g->name), get_name(g->name));

			*offset += sprintf(&code[*offset], "static %s_type *_%" PRIu64 ";\n\n", get_name(g->name), g->var_index);
			*offset += sprintf(&code[*offset], "void set_%s(%s_type *value) {\n", get_name(g->name), get_name(g->name));
			*offset += sprintf(&code[*offset], "\t_%" PRIu64 " = value;\n", g->var_index);
			*offset += sprintf(&code[*offset], "}\n\n");
		}
	}
}

static const char *type_to_mini(type_ref t) {
	if (t.type == int_id) {
		return "_i1";
	}
	else if (t.type == int2_id) {
		return "_i2";
	}
	else if (t.type == int3_id) {
		return "_i3";
	}
	else if (t.type == int4_id) {
		return "_i4";
	}
	else if (t.type == uint_id) {
		return "_u1";
	}
	else if (t.type == uint2_id) {
		return "_u2";
	}
	else if (t.type == uint3_id) {
		return "_u3";
	}
	else if (t.type == uint4_id) {
		return "_u4";
	}
	else if (t.type == float_id) {
		return "_f1";
	}
	else if (t.type == float2_id) {
		return "_f2";
	}
	else if (t.type == float3_id) {
		return "_f3";
	}
	else if (t.type == float4_id) {
		return "_f4";
	}
	else {
		debug_context context = {0};
		error(context, "Unknown parameter type");
		return "error";
	}
}

static void write_functions(char *code, const char *main_name, size_t *offset, shader_stage stage, function *main) {
	function *functions[256];
	size_t    functions_size = 0;

	functions[functions_size] = main;
	functions_size += 1;

	find_referenced_functions(main, functions, &functions_size);

	for (size_t i = 0; i < functions_size; ++i) {
		function *f = functions[i];

		debug_context context = {0};
		check(f->block != NULL, context, "Function has no block");

		uint8_t *data = f->code.o;
		size_t   size = f->code.size;

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

		int indentation = 1;

		if (f == main && stage == SHADER_STAGE_COMPUTE) {
			attribute *threads_attribute = find_attribute(&f->attributes, add_name("threads"));
			if (threads_attribute == NULL || threads_attribute->paramters_count != 3) {
				debug_context context = {0};
				error(context, "Compute function requires a threads attribute with three parameters");
			}

			*offset += sprintf(&code[*offset], "void %s(uint32_t workgroup_count_x, uint32_t workgroup_count_y, uint32_t workgroup_count_z) {\n", main_name);

			*offset += sprintf(&code[*offset], "\tuint32_t local_size_x = %i;\n\tuint32_t local_size_y = %i;\n\tuint32_t local_size_z = %i;\n",
			                   (int)threads_attribute->parameters[0], (int)threads_attribute->parameters[1], (int)threads_attribute->parameters[2]);

			indent(code, offset, indentation);
			*offset += sprintf(&code[*offset], "for (uint32_t workgroup_index_z = 0; workgroup_index_z < workgroup_count_z; ++workgroup_index_z) {\n");
			++indentation;

			indent(code, offset, indentation);
			*offset += sprintf(&code[*offset], "for (uint32_t workgroup_index_y = 0; workgroup_index_y < workgroup_count_y; ++workgroup_index_y) {\n");
			++indentation;

			indent(code, offset, indentation);
			*offset += sprintf(&code[*offset], "for (uint32_t workgroup_index_x = 0; workgroup_index_x < workgroup_count_x; ++workgroup_index_x) {\n");
			++indentation;

			indent(code, offset, indentation);
			*offset += sprintf(&code[*offset], "for (uint32_t local_index_z = 0; local_index_z < local_size_z; ++local_index_z) {\n");
			++indentation;

			indent(code, offset, indentation);
			*offset += sprintf(&code[*offset], "for (uint32_t local_index_y = 0; local_index_y < local_size_y; ++local_index_y) {\n");
			++indentation;

			indent(code, offset, indentation);
			*offset += sprintf(&code[*offset], "for (uint32_t local_index_x = 0; local_index_x < local_size_x; local_index_x += 4) {\n");
			++indentation;

			indent(code, offset, indentation);
			*offset += sprintf(&code[*offset], "kore_uint3x4 group_id;\n");

			indent(code, offset, indentation);
			*offset += sprintf(&code[*offset], "group_id.x = kore_uint32x4_load_all(workgroup_index_x);\n");

			indent(code, offset, indentation);
			*offset += sprintf(&code[*offset], "group_id.y = kore_uint32x4_load_all(workgroup_index_y);\n");

			indent(code, offset, indentation);
			*offset += sprintf(&code[*offset], "group_id.z = kore_uint32x4_load_all(workgroup_index_z);\n\n");

			indent(code, offset, indentation);
			*offset += sprintf(&code[*offset], "kore_uint3x4 group_thread_id;\n");

			indent(code, offset, indentation);
			*offset +=
			    sprintf(&code[*offset], "group_thread_id.x = kore_uint32x4_load(local_index_x, local_index_x + 1, local_index_x + 2, local_index_x + 3);\n");

			indent(code, offset, indentation);
			*offset += sprintf(&code[*offset], "group_thread_id.y = kore_uint32x4_load_all(local_index_y);\n");

			indent(code, offset, indentation);
			*offset += sprintf(&code[*offset], "group_thread_id.z = kore_uint32x4_load_all(local_index_z);\n\n");

			indent(code, offset, indentation);
			*offset += sprintf(&code[*offset], "kore_uint3x4 dispatch_thread_id;\n");

			indent(code, offset, indentation);
			*offset += sprintf(
			    &code[*offset],
			    "dispatch_thread_id.x = kore_uint32x4_add(kore_uint32x4_mul(group_id.x, kore_uint32x4_load_all(workgroup_count_x)), group_thread_id.x);\n");

			indent(code, offset, indentation);
			*offset += sprintf(
			    &code[*offset],
			    "dispatch_thread_id.y = kore_uint32x4_add(kore_uint32x4_mul(group_id.y, kore_uint32x4_load_all(workgroup_count_y)), group_thread_id.y);\n");

			indent(code, offset, indentation);
			*offset += sprintf(&code[*offset], "dispatch_thread_id.z = kore_uint32x4_add(kore_uint32x4_mul(group_id.z, "
			                                   "kore_uint32x4_load_all(workgroup_count_z)), group_thread_id.z);\n\n");

			indent(code, offset, indentation);
			*offset += sprintf(&code[*offset],
			                   "kore_uint32x4 group_index = kore_uint32x4_add(kore_uint32x4_mul(group_thread_id.z, "
			                   "kore_uint32x4_mul(kore_uint32x4_load_all(workgroup_count_x), kore_uint32x4_load_all(workgroup_count_y))), "
			                   "kore_uint32x4_add(kore_uint32x4_mul(group_thread_id.y, kore_uint32x4_load_all(workgroup_count_x)), group_thread_id.x));\n\n");
		}
		else if (f == main && stage == SHADER_STAGE_VERTEX) {
			*offset += sprintf(&code[*offset], "__attribute__((naked)) void vs_%s(size_t _lane_count, void *__output, ", get_name(f->name));
			for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
				if (parameter_index == 0) {
					*offset += sprintf(&code[*offset], "void *__%" PRIu64, parameter_ids[parameter_index]);
				}
				else {
					*offset +=
					    sprintf(&code[*offset], ", %s *_%" PRIu64, type_string_simd(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
			}
			*offset += sprintf(&code[*offset], ") {\n");
			*offset += sprintf(&code[*offset], "\tsize_t _vector_length = __riscv_vsetvl_e32m1(_lane_count);\n");
			*offset += sprintf(&code[*offset], "\t%s *_output = __output;\n", type_string_simd(f->return_type.type));
			*offset += sprintf(&code[*offset], "\t%s *_%" PRIu64 " = __%" PRIu64 ";\n", type_string_simd(f->parameter_types[0].type), parameter_ids[0],
			                   parameter_ids[0]);
		}
		else {
			*offset += sprintf(&code[*offset], "%s %s(", type_string_simd(f->return_type.type), get_name(f->name));
			for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
				if (parameter_index == 0) {
					*offset +=
					    sprintf(&code[*offset], "%s _%" PRIu64, type_string_simd(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
				else {
					*offset +=
					    sprintf(&code[*offset], ", %s _%" PRIu64, type_string_simd(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
			}
			*offset += sprintf(&code[*offset], ") {\n");
		}

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];
			switch (o->type) {
			case OPCODE_ADD: {
				indent(code, offset, indentation);
				*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = kore_cpu_compute_add", type_string_simd(o->op_binary.result.type.type),
				                   o->op_binary.result.index);
				*offset += sprintf(&code[*offset], "%s", type_to_mini(o->op_binary.left.type));
				*offset += sprintf(&code[*offset], "%s", type_to_mini(o->op_binary.right.type));
				*offset += sprintf(&code[*offset], "_x32(_%" PRIu64 ", _%" PRIu64 ");\n", o->op_binary.left.index, o->op_binary.right.index);
				break;
			}
			case OPCODE_SUB: {
				indent(code, offset, indentation);
				*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = kore_cpu_compute_sub", type_string_simd(o->op_binary.result.type.type),
				                   o->op_binary.result.index);
				*offset += sprintf(&code[*offset], "%s", type_to_mini(o->op_binary.left.type));
				*offset += sprintf(&code[*offset], "%s", type_to_mini(o->op_binary.right.type));
				*offset += sprintf(&code[*offset], "_x32(_%" PRIu64 ", _%" PRIu64 ");\n", o->op_binary.left.index, o->op_binary.right.index);
				break;
			}
			case OPCODE_MULTIPLY: {
				indent(code, offset, indentation);
				*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = kore_cpu_compute_mult", type_string_simd(o->op_binary.result.type.type),
				                   o->op_binary.result.index);
				*offset += sprintf(&code[*offset], "%s", type_to_mini(o->op_binary.left.type));
				*offset += sprintf(&code[*offset], "%s", type_to_mini(o->op_binary.right.type));
				*offset += sprintf(&code[*offset], "_x32(_%" PRIu64 ", _%" PRIu64 ");\n", o->op_binary.left.index, o->op_binary.right.index);
				break;
			}
			case OPCODE_DIVIDE: {
				indent(code, offset, indentation);
				*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = kore_cpu_compute_div", type_string_simd(o->op_binary.result.type.type),
				                   o->op_binary.result.index);
				*offset += sprintf(&code[*offset], "%s", type_to_mini(o->op_binary.left.type));
				*offset += sprintf(&code[*offset], "%s", type_to_mini(o->op_binary.right.type));
				*offset += sprintf(&code[*offset], "_x32(_%" PRIu64 ", _%" PRIu64 ");\n", o->op_binary.left.index, o->op_binary.right.index);
				break;
			}
			case OPCODE_LOAD_FLOAT_CONSTANT:
				indent(code, offset, indentation);
				*offset +=
				    sprintf(&code[*offset], "%s _%" PRIu64 " = __riscv_vfmv_v_f_f32m1(%ff, _vector_length);\n",
				            type_string_simd(o->op_load_float_constant.to.type.type), o->op_load_float_constant.to.index, o->op_load_float_constant.number);
				break;
			case OPCODE_LOAD_INT_CONSTANT:
				indent(code, offset, indentation);
				*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = kore_int32x4_load_all(%i);\n", type_string_simd(o->op_load_int_constant.to.type.type),
				                   o->op_load_int_constant.to.index, o->op_load_int_constant.number);
				break;
			case OPCODE_CALL: {
				if (o->op_call.func == add_name("group_id")) {
					check(o->op_call.parameters_size == 0, context, "group_id can not have a parameter");
					indent(code, offset, indentation);
					*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = group_id;\n", type_string_simd(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("group_thread_id")) {
					check(o->op_call.parameters_size == 0, context, "group_thread_id can not have a parameter");
					indent(code, offset, indentation);
					*offset +=
					    sprintf(&code[*offset], "%s _%" PRIu64 " = group_thread_id;\n", type_string_simd(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("dispatch_thread_id")) {
					check(o->op_call.parameters_size == 0, context, "dispatch_thread_id can not have a parameter");
					indent(code, offset, indentation);
					*offset +=
					    sprintf(&code[*offset], "%s _%" PRIu64 " = dispatch_thread_id;\n", type_string_simd(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("group_index")) {
					check(o->op_call.parameters_size == 0, context, "group_index can not have a parameter");
					indent(code, offset, indentation);
					*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = group_index;\n", type_string_simd(o->op_call.var.type.type), o->op_call.var.index);
				}
				else {
					const char *function_name = get_name(o->op_call.func);
					if (o->op_call.func == add_name("float")) {
						function_name = "create_float";
					}
					else if (o->op_call.func == add_name("float2")) {
						function_name = "create_float2";
					}
					else if (o->op_call.func == add_name("float3")) {
						function_name = "create_float3";
					}
					else if (o->op_call.func == add_name("float4")) {
						function_name = "create_float4";
					}

					indent(code, offset, indentation);

					*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = kore_riscv_%s", type_string_simd(o->op_call.var.type.type), o->op_call.var.index,
					                   function_name);

					for (uint8_t parameter_index = 0; parameter_index < o->op_call.parameters_size; ++parameter_index) {
						variable v = o->op_call.parameters[parameter_index];
						*offset += sprintf(&code[*offset], "%s", type_to_mini(v.type));
					}

					*offset += sprintf(&code[*offset], "_x32(");

					if (o->op_call.parameters_size > 0) {
						*offset += sprintf(&code[*offset], "_%" PRIu64, o->op_call.parameters[0].index);
						for (uint8_t i = 1; i < o->op_call.parameters_size; ++i) {
							*offset += sprintf(&code[*offset], ", _%" PRIu64, o->op_call.parameters[i].index);
						}
					}
					*offset += sprintf(&code[*offset], ");\n");
				}
				break;
			}
			case OPCODE_LOAD_ACCESS_LIST: {
				uint64_t global_var_index = 0;
				global  *g                = NULL;
				for (global_id j = 0; get_global(j) != NULL && get_global(j)->type != NO_TYPE; ++j) {
					g = get_global(j);
					if (o->op_load_access_list.from.index == g->var_index) {
						global_var_index = g->var_index;
						if (get_type(g->type)->built_in) {
							global_var_index = 0;
						}
						break;
					}
				}

				indent(code, offset, indentation);
				*offset +=
				    sprintf(&code[*offset], "%s _temp_%" PRIu64 "[] = {", type_string(o->op_load_access_list.to.type.type), o->op_load_access_list.to.index);

				for (uint32_t simd_index = 0; simd_index < 32u; ++simd_index) {
					*offset += sprintf(&code[*offset], "_%" PRIu64 "[%i]", o->op_load_access_list.from.index, simd_index);

					type *s = get_type(o->op_load_access_list.from.type.type);

					for (size_t i = 0; i < o->op_load_access_list.access_list_size; ++i) {
						switch (o->op_load_access_list.access_list[i].kind) {
						case ACCESS_ELEMENT: {
							type *from_type = get_type(o->op_load_access_list.from.type.type);

							if (from_type->array_size == UINT32_MAX && get_type(from_type->base)->tex_kind != TEXTURE_KIND_NONE) {
								*offset += sprintf(&code[*offset], "[NonUniformResourceIndex(_%" PRIu64 ")]",
								                   o->op_load_access_list.access_list[i].access_element.index.index);
							}
							else if (global_var_index != 0 && i == 0 && get_type(from_type->base)->built_in) {
								*offset += sprintf(&code[*offset], "[_%" PRIu64 "].data", o->op_load_access_list.access_list[i].access_element.index.index);
							}
							else {
								*offset += sprintf(&code[*offset], "[_%" PRIu64 "]", o->op_load_access_list.access_list[i].access_element.index.index);
							}
							break;
						}
						case ACCESS_MEMBER:
							if (global_var_index != 0 && i == 0) {
								*offset += sprintf(&code[*offset], "_%s", member_string(s, o->op_load_access_list.access_list[i].access_member.name));
							}
							else {
								*offset += sprintf(&code[*offset], ".%s", member_string(s, o->op_load_access_list.access_list[i].access_member.name));
							}
							break;
						case ACCESS_SWIZZLE: {
							char swizzle[4];

							for (uint32_t swizzle_index = 0; swizzle_index < o->op_load_access_list.access_list[i].access_swizzle.swizzle.size;
							     ++swizzle_index) {
								swizzle[swizzle_index] = "xyzw"[o->op_load_access_list.access_list[i].access_swizzle.swizzle.indices[swizzle_index]];
							}
							swizzle[o->op_load_access_list.access_list[i].access_swizzle.swizzle.size] = 0;

							*offset += sprintf(&code[*offset], ".%s", swizzle);
							break;
						}
						}

						s = get_type(o->op_load_access_list.access_list[i].type);
					}

					*offset += sprintf(&code[*offset], ", ");
				}

				*offset += sprintf(&code[*offset], "};\n");

				indent(code, offset, indentation);
				*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = __riscv_vle32_v_f32m1(_temp_%" PRIu64 ", _vector_length);\n",
				                   type_string_simd(o->op_load_access_list.to.type.type), o->op_load_access_list.to.index, o->op_load_access_list.to.index);

				break;
			}
			case OPCODE_STORE_ACCESS_LIST:
			case OPCODE_SUB_AND_STORE_ACCESS_LIST:
			case OPCODE_ADD_AND_STORE_ACCESS_LIST:
			case OPCODE_DIVIDE_AND_STORE_ACCESS_LIST:
			case OPCODE_MULTIPLY_AND_STORE_ACCESS_LIST: {
				indent(code, offset, indentation);
				*offset += sprintf(&code[*offset], "_%" PRIu64, o->op_store_access_list.to.index);

				type *s = get_type(o->op_store_access_list.to.type.type);

				for (size_t i = 0; i < o->op_store_access_list.access_list_size; ++i) {
					switch (o->op_store_access_list.access_list[i].kind) {
					case ACCESS_ELEMENT:
						*offset += sprintf(&code[*offset], "[kore_int32x4_get(_%" PRIu64 ", %i)]",
						                   o->op_store_access_list.access_list[i].access_element.index.index, 0);
						break;
					case ACCESS_MEMBER:
						*offset += sprintf(&code[*offset], ".%s", get_name(o->op_store_access_list.access_list[i].access_member.name));
						break;
					case ACCESS_SWIZZLE: {
						char swizzle[4];

						for (uint32_t swizzle_index = 0; swizzle_index < o->op_store_access_list.access_list[i].access_swizzle.swizzle.size; ++swizzle_index) {
							swizzle[swizzle_index] = "xyzw"[o->op_store_access_list.access_list[i].access_swizzle.swizzle.indices[swizzle_index]];
						}
						swizzle[o->op_store_access_list.access_list[i].access_swizzle.swizzle.size] = 0;

						*offset += sprintf(&code[*offset], ".%s", swizzle);

						break;
					}
					}

					s = get_type(o->op_store_access_list.access_list[i].type);
				}

				switch (o->type) {
				case OPCODE_STORE_ACCESS_LIST:
					*offset += sprintf(&code[*offset], " = _%" PRIu64 ";\n", o->op_store_access_list.from.index);
					break;
				case OPCODE_SUB_AND_STORE_ACCESS_LIST:
					*offset += sprintf(&code[*offset], " -= _%" PRIu64 ";\n", o->op_store_access_list.from.index);
					break;
				case OPCODE_ADD_AND_STORE_ACCESS_LIST:
					*offset += sprintf(&code[*offset], " += _%" PRIu64 ";\n", o->op_store_access_list.from.index);
					break;
				case OPCODE_DIVIDE_AND_STORE_ACCESS_LIST:
					*offset += sprintf(&code[*offset], " /= _%" PRIu64 ";\n", o->op_store_access_list.from.index);
					break;
				case OPCODE_MULTIPLY_AND_STORE_ACCESS_LIST:
					*offset += sprintf(&code[*offset], " *= _%" PRIu64 ";\n", o->op_store_access_list.from.index);
					break;
				default:
					assert(false);
					break;
				}

				break;
			}
			case OPCODE_RETURN: {
				if (o->size > offsetof(opcode, op_return)) {
					indent(code, offset, indentation);
					*offset += sprintf(&code[*offset], "memcpy(_output, &_%" PRIu64 ", sizeof(_%" PRIu64 ") / 32 * _lane_count);\n", o->op_return.var.index,
					                   o->op_return.var.index);
					indent(code, offset, indentation);
					*offset += sprintf(&code[*offset], "return;\n");
				}
				else {
					indent(code, offset, indentation);
					*offset += sprintf(&code[*offset], "return;\n");
				}
				break;
			}
			default:
				cstyle_write_opcode(code, offset, o, type_string_simd, &indentation);
				break;
			}

			index += o->size;
		}

		if (f == main && stage == SHADER_STAGE_COMPUTE) {
			for (int i = 0; i < 6; ++i) {
				--indentation;
				indent(code, offset, indentation);
				*offset += sprintf(&code[*offset], "}\n");
			}

			*offset += sprintf(&code[*offset], "}\n\n");
		}
		else {
			*offset += sprintf(&code[*offset], "}\n\n");
		}
	}
}

static void kompjuta_export_vertex(char *directory, function *main) {
	debug_context context = {0};

	char *code = (char *)calloc(1024 * 1024, 1);
	check(code != NULL, context, "Could not allocate code string");

	size_t offset = 0;

	char *header_code = (char *)calloc(1024 * 1024, 1);
	check(header_code != NULL, context, "Could not allocate code string");

	size_t header_offset = 0;

	write_types(code, &offset, main);

	write_globals(code, &offset, header_code, &header_offset, main);

	char *main_name = get_name(main->name);

	write_functions(code, main_name, &offset, SHADER_STAGE_VERTEX, main);

	char filename[512];
	sprintf(filename, "kong_%s", main_name);

	write_code(code, header_code, directory, filename, main_name, SHADER_STAGE_VERTEX, main);
}

static void kompjuta_export_fragment(char *directory, function *main) {
	debug_context context = {0};

	char *code = (char *)calloc(1024 * 1024, 1);
	check(code != NULL, context, "Could not allocate code string");

	size_t offset = 0;

	char *header_code = (char *)calloc(1024 * 1024, 1);
	check(header_code != NULL, context, "Could not allocate code string");

	size_t header_offset = 0;

	write_types(code, &offset, main);

	write_globals(code, &offset, header_code, &header_offset, main);

	char *main_name = get_name(main->name);

	write_functions(code, main_name, &offset, SHADER_STAGE_FRAGMENT, main);

	char filename[512];
	sprintf(filename, "kong_%s", main_name);

	write_code(code, header_code, directory, filename, main_name, SHADER_STAGE_FRAGMENT, main);
}

static void kompjuta_export_compute(char *directory, function *main) {
	debug_context context = {0};

	char *code = (char *)calloc(1024 * 1024, 1);
	check(code != NULL, context, "Could not allocate code string");

	size_t offset = 0;

	char *header_code = (char *)calloc(1024 * 1024, 1);
	check(header_code != NULL, context, "Could not allocate code string");

	size_t header_offset = 0;

	assert(main->parameters_size == 0);

	write_types(code, &offset, main);

	write_globals(code, &offset, header_code, &header_offset, main);

	char *main_name = get_name(main->name);

	write_functions(code, main_name, &offset, SHADER_STAGE_COMPUTE, main);

	char filename[512];
	sprintf(filename, "kong_%s", main_name);

	write_code(code, header_code, directory, filename, main_name, SHADER_STAGE_COMPUTE, main);
}

void kompjuta_export(char *directory) {
	function *vertex_shaders[256];
	size_t    vertex_shaders_size = 0;

	function *fragment_shaders[256];
	size_t    fragment_shaders_size = 0;

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
			name_id vertex_shader_name   = NO_NAME;
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
	size_t    compute_shaders_size = 0;

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);
		if (has_attribute(&f->attributes, add_name("compute"))) {
			compute_shaders[compute_shaders_size] = f;
			compute_shaders_size += 1;
		}
	}

	for (size_t i = 0; i < vertex_shaders_size; ++i) {
		kompjuta_export_vertex(directory, vertex_shaders[i]);
	}

	for (size_t i = 0; i < fragment_shaders_size; ++i) {
		kompjuta_export_fragment(directory, fragment_shaders[i]);
	}

	for (size_t i = 0; i < compute_shaders_size; ++i) {
		kompjuta_export_compute(directory, compute_shaders[i]);
	}
}
