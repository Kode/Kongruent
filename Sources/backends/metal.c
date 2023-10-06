#include "metal.h"

#include "../compiler.h"
#include "../errors.h"
#include "../functions.h"
#include "../parser.h"
#include "../shader_stage.h"
#include "../types.h"
#include "cstyle.h"
#include "d3d11.h"

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

static void write_code(char *metal, char *directory, const char *filename) {
	char full_filename[512];
	sprintf(full_filename, "%s/%s.metal", directory, filename);

	FILE *file = fopen(full_filename, "wb");
	fprintf(file, "%s", metal);
	fclose(file);
}

static void find_referenced_functions(function *f, function **functions, size_t *functions_size) {
	if (f->block == NULL) {
		// built-in
		return;
	}

	uint8_t *data = f->code.o;
	size_t size = f->code.size;

	size_t index = 0;
	while (index < size) {
		opcode *o = (opcode *)&data[index];
		switch (o->type) {
		case OPCODE_CALL: {
			for (function_id i = 0; get_function(i) != NULL; ++i) {
				function *f = get_function(i);
				if (f->name == o->op_call.func) {
					if (f->block == NULL) {
						// built-in
						break;
					}

					bool found = false;
					for (size_t j = 0; j < *functions_size; ++j) {
						if (functions[j]->name == o->op_call.func) {
							found = true;
							break;
						}
					}
					if (!found) {
						functions[*functions_size] = f;
						*functions_size += 1;
						find_referenced_functions(f, functions, functions_size);
					}
					break;
				}
			}
			break;
		}
		}

		index += o->size;
	}
}

static void add_found_type(type_id t, type_id *types, size_t *types_size) {
	for (size_t i = 0; i < *types_size; ++i) {
		if (types[i] == t) {
			return;
		}
	}

	types[*types_size] = t;
	*types_size += 1;
}

static void find_referenced_types(function *f, type_id *types, size_t *types_size) {
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
		function *func = functions[l];
		debug_context context = {0};
		check(func->parameter_type.type != NO_TYPE, context, "Function parameter type missing");
		add_found_type(func->parameter_type.type, types, types_size);
		check(func->return_type.type != NO_TYPE, context, "Function return type missing");
		add_found_type(func->return_type.type, types, types_size);

		uint8_t *data = functions[l]->code.o;
		size_t size = functions[l]->code.size;

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];
			switch (o->type) {
			case OPCODE_VAR:
				add_found_type(o->op_var.var.type.type, types, types_size);
				break;
			}

			index += o->size;
		}
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
			case OPCODE_MULTIPLY: {
				find_referenced_global_for_var(o->op_multiply.left, globals, globals_size);
				find_referenced_global_for_var(o->op_multiply.right, globals, globals_size);
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

static type_id vertex_inputs[256];
static size_t vertex_inputs_size = 0;
static type_id fragment_inputs[256];
static size_t fragment_inputs_size = 0;

static bool is_vertex_input(type_id t) {
	for (size_t i = 0; i < vertex_inputs_size; ++i) {
		if (t == vertex_inputs[i]) {
			return true;
		}
	}
	return false;
}

static bool is_fragment_input(type_id t) {
	for (size_t i = 0; i < fragment_inputs_size; ++i) {
		if (t == fragment_inputs[i]) {
			return true;
		}
	}
	return false;
}

static void write_types(char *metal, size_t *offset) {
	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);

		if (!t->built_in && t->attribute != add_name("pipe")) {
			if (t->name == NO_NAME) {
				char name[256];

				bool found = false;
				for (global_id j = 0; get_global(j).type != NO_TYPE; ++j) {
					global g = get_global(j);
					if (g.type == i) {
						sprintf(name, "_%" PRIu64, g.var_index);
						found = true;
						break;
					}
				}

				if (!found) {
					strcpy(name, "Unknown");
				}

				*offset += sprintf(&metal[*offset], "struct %s_type {\n", name);
			}
			else {
				*offset += sprintf(&metal[*offset], "struct %s {\n", get_name(t->name));
			}

			if (is_vertex_input(i)) {
				for (size_t j = 0; j < t->members.size; ++j) {
					*offset += sprintf(&metal[*offset], "\t%s %s [[attribute(%" PRIu64 ")]];\n", type_string(t->members.m[j].type.type),
					                   get_name(t->members.m[j].name), j);
				}
			}
			else if (is_fragment_input(i)) {
				for (size_t j = 0; j < t->members.size; ++j) {
					if (j == 0) {
						*offset += sprintf(&metal[*offset], "\t%s %s [[position]];\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
					}
					else {
						*offset += sprintf(&metal[*offset], "\t%s %s [[user(locn%" PRIu64 ")]];\n", type_string(t->members.m[j].type.type),
						                   get_name(t->members.m[j].name), j - 1);
					}
				}
			}
			else {
				for (size_t j = 0; j < t->members.size; ++j) {
					*offset += sprintf(&metal[*offset], "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
				}
			}
			*offset += sprintf(&metal[*offset], "};\n\n");
		}
	}
}

static int global_register_indices[512];

static function_id vertex_functions[256];
static size_t vertex_functions_size = 0;
static function_id fragment_functions[256];
static size_t fragment_functions_size = 0;

static bool is_vertex_function(function_id f) {
	for (size_t i = 0; i < vertex_functions_size; ++i) {
		if (f == vertex_functions[i]) {
			return true;
		}
	}
	return false;
}

static bool is_fragment_function(function_id f) {
	for (size_t i = 0; i < fragment_functions_size; ++i) {
		if (f == fragment_functions[i]) {
			return true;
		}
	}
	return false;
}

static void write_functions(char *code, size_t *offset) {
	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);

		if (f->block == NULL) {
			continue;
		}

		uint8_t *data = f->code.o;
		size_t size = f->code.size;

		uint64_t parameter_id = 0;
		for (size_t i = 0; i < f->block->block.vars.size; ++i) {
			if (f->parameter_name == f->block->block.vars.v[i].name) {
				parameter_id = f->block->block.vars.v[i].variable_id;
				break;
			}
		}

		debug_context context = {0};
		check(parameter_id != 0, context, "Parameter not found");

		char buffers[1024];
		strcpy(buffers, "");
		if (is_vertex_function(i) || is_fragment_function(i)) {
			global_id globals[256];
			size_t globals_size = 0;
			find_referenced_globals(f, globals, &globals_size);

			size_t buffers_offset = 0;

			for (size_t i = 0; i < globals_size; ++i) {
				global g = get_global(globals[i]);
				int register_index = global_register_indices[globals[i]];

				if (g.type == sampler_type_id) {
					buffers_offset += sprintf(&buffers[buffers_offset], ", sampler<float> _%" PRIu64 " [[sampler(%i)]]", g.var_index, register_index);
				}
				else if (g.type == tex2d_type_id) {
					buffers_offset += sprintf(&buffers[buffers_offset], ", texture2d<float> _%" PRIu64 " [[texture(%i)]]", g.var_index, register_index);
				}
				else if (g.type == texcube_type_id) {
				}
				else if (g.type == float_id) {
				}
				else {
					buffers_offset += sprintf(&buffers[buffers_offset], ", constant _%" PRIu64 "_type& _%" PRIu64 " [[buffer(%i)]]", g.var_index, g.var_index,
					                          register_index);
				}
			}
		}

		if (is_vertex_function(i)) {
			*offset += sprintf(&code[*offset], "vertex %s %s(%s _%" PRIu64 " [[stage_in]]%s) {\n", type_string(f->return_type.type), get_name(f->name),
			                   type_string(f->parameter_type.type), parameter_id, buffers);
		}
		else if (is_fragment_function(i)) {
			if (f->return_type.array_size > 0) {
				*offset += sprintf(&code[*offset], "struct _render_targets {\n");
				for (uint32_t j = 0; j < f->return_type.array_size; ++j) {
					*offset += sprintf(&code[*offset], "\t%s _%i : SV_Target%i;\n", type_string(f->return_type.type), j, j);
				}
				*offset += sprintf(&code[*offset], "};\n\n");
				*offset += sprintf(&code[*offset], "_render_targets main(%s _%" PRIu64 ") {\n", type_string(f->parameter_type.type), parameter_id);
			}
			else {
				*offset += sprintf(&code[*offset], "fragment ColorOut %s(%s _%" PRIu64 " [[stage_in]]%s) {\n", get_name(f->name),
				                   type_string(f->parameter_type.type), parameter_id, buffers);
			}
		}

		else {
			*offset += sprintf(&code[*offset], "%s %s(%s _%" PRIu64 ") {\n", type_string(f->return_type.type), get_name(f->name),
			                   type_string(f->parameter_type.type), parameter_id);
		}

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

				*offset += sprintf(&code[*offset], "\t%s _%" PRIu64 " = _%" PRIu64, type_string(o->op_load_member.to.type.type), o->op_load_member.to.index,
				                   o->op_load_member.from.index);
				type *s = get_type(o->op_load_member.member_parent_type);
				for (size_t i = 0; i < o->op_load_member.member_indices_size; ++i) {
					*offset += sprintf(&code[*offset], ".%s", get_name(s->members.m[o->op_load_member.member_indices[i]].name));
					s = get_type(s->members.m[o->op_load_member.member_indices[i]].type.type);
				}
				*offset += sprintf(&code[*offset], ";\n");
				break;
			}
			case OPCODE_RETURN: {
				if (o->size > offsetof(opcode, op_return)) {
					if (is_fragment_function(i) && f->return_type.array_size > 0) {
						*offset += sprintf(&code[*offset], "\t{\n");
						*offset += sprintf(&code[*offset], "\t\t_render_targets rts;\n");
						for (uint32_t j = 0; j < f->return_type.array_size; ++j) {
							*offset += sprintf(&code[*offset], "\t\trts._%i = _%" PRIu64 "[%i];\n", j, o->op_return.var.index, j);
						}
						*offset += sprintf(&code[*offset], "\t\treturn rts;\n");
						*offset += sprintf(&code[*offset], "\t}\n");
					}
					else if (is_fragment_function(i)) {
						*offset += sprintf(&code[*offset], "\t{\n");
						*offset += sprintf(&code[*offset], "\t\tColorOut out;\n");
						*offset += sprintf(&code[*offset], "\t\tout.out = _%" PRIu64 ";\n", o->op_return.var.index);
						*offset += sprintf(&code[*offset], "\t\treturn out;\n");
						*offset += sprintf(&code[*offset], "\t}\n");
					}
					else {
						*offset += sprintf(&code[*offset], "\treturn _%" PRIu64 ";\n", o->op_return.var.index);
					}
				}
				else {
					*offset += sprintf(&code[*offset], "\treturn;\n");
				}
				break;
			}
			case OPCODE_CALL: {
				debug_context context = {0};
				if (o->op_call.func == add_name("sample")) {
					check(o->op_call.parameters_size == 3, context, "sample requires three parameters");
					*offset +=
					    sprintf(&code[*offset], "\t%s _%" PRIu64 " = _%" PRIu64 ".sample(_%" PRIu64 ", _%" PRIu64 ");\n", type_string(o->op_call.var.type.type),
					            o->op_call.var.index, o->op_call.parameters[0].index, o->op_call.parameters[1].index, o->op_call.parameters[2].index);
				}
				else if (o->op_call.func == add_name("sample_lod")) {
					check(o->op_call.parameters_size == 4, context, "sample_lod requires four parameters");
					*offset += sprintf(&code[*offset], "\t%s _%" PRIu64 " = _%" PRIu64 ".sampleLevel(_%" PRIu64 ", _%" PRIu64 ", _%" PRIu64 ");\n",
					                   type_string(o->op_call.var.type.type), o->op_call.var.index, o->op_call.parameters[0].index,
					                   o->op_call.parameters[1].index, o->op_call.parameters[2].index, o->op_call.parameters[3].index);
				}
				else {
					*offset += sprintf(&code[*offset], "\t%s _%" PRIu64 " = %s(", type_string(o->op_call.var.type.type), o->op_call.var.index,
					                   function_string(o->op_call.func));
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
			case OPCODE_MULTIPLY: {
				*offset += sprintf(&code[*offset], "\t%s _%" PRIu64 " = _%" PRIu64 " * _%" PRIu64 ";\n", type_string(o->op_multiply.result.type.type),
				                   o->op_multiply.result.index, o->op_multiply.left.index, o->op_multiply.right.index);
				break;
			}
			default:
				cstyle_write_opcode(code, offset, o, type_string);
				break;
			}

			index += o->size;
		}

		*offset += sprintf(&code[*offset], "}\n\n");
	}
}

static void metal_export_everything(char *directory) {
	char *metal = (char *)calloc(1024 * 1024, 1);
	debug_context context = {0};
	check(metal != NULL, context, "Could not allocate Metal string");
	size_t offset = 0;

	offset += sprintf(&metal[offset], "#include <metal_stdlib>\n");
	offset += sprintf(&metal[offset], "#include <simd/simd.h>\n\n");
	offset += sprintf(&metal[offset], "using namespace metal;\n\n");

	offset += sprintf(&metal[offset], "struct ColorOut {\n");
	offset += sprintf(&metal[offset], "\tfloat4 out [[color(0)]];\n");
	offset += sprintf(&metal[offset], "};\n\n");

	write_types(metal, &offset);

	write_functions(metal, &offset);

	write_code(metal, directory, "kong");
}

void metal_export(char *directory) {
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

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in && t->attribute == add_name("pipe")) {
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
					vertex_functions[vertex_functions_size] = i;
					vertex_functions_size += 1;

					vertex_inputs[vertex_inputs_size] = f->parameter_type.type;
					vertex_inputs_size += 1;
				}
				else if (f->name == fragment_shader_name) {
					fragment_functions[fragment_functions_size] = i;
					fragment_functions_size += 1;

					fragment_inputs[fragment_inputs_size] = f->parameter_type.type;
					fragment_inputs_size += 1;
				}
			}
		}
	}

	metal_export_everything(directory);
}
