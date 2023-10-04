#include "glsl.h"

#include "../compiler.h"
#include "../errors.h"
#include "../functions.h"
#include "../parser.h"
#include "../shader_stage.h"
#include "../types.h"
#include "cstyle.h"

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
		return "vec2";
	}
	if (type == float3_id) {
		return "vec3";
	}
	if (type == float4_id) {
		return "vec4";
	}
	if (type == float4x4_id) {
		return "mat4";
	}
	return get_name(get_type(type)->name);
}

static void write_code(char *glsl, char *directory, const char *filename, const char *name) {
	char full_filename[512];

	{
		sprintf(full_filename, "%s/%s.h", directory, filename);
		FILE *file = fopen(full_filename, "wb");
		fprintf(file, "#include <stddef.h>\n\n");
		fprintf(file, "extern const char *%s;\n", name);
		fprintf(file, "extern size_t %s_size;\n", name);
		fclose(file);
	}

	{
		sprintf(full_filename, "%s/%s.c", directory, filename);

		FILE *file = fopen(full_filename, "wb");
		fprintf(file, "#include \"%s.h\"\n\n", filename);

		fprintf(file, "const char *%s = \"", name);

		size_t length = strlen(glsl);

		for (size_t i = 0; i < length; ++i) {
			if (glsl[i] == '\n') {
				fprintf(file, "\\n");
			}
			else if (glsl[i] == '\r') {
				fprintf(file, "\\r");
			}
			else if (glsl[i] == '\t') {
				fprintf(file, "\\t");
			}
			else if (glsl[i] == '"') {
				fprintf(file, "\\\"");
			}
			else {
				fprintf(file, "%c", glsl[i]);
			}
		}

		fprintf(file, "\";\n\n");

		fprintf(file, "size_t %s_size = %" PRIu64 ";\n\n", name, length);

		fprintf(file, "/*\n%s*/\n", glsl);

		fclose(file);
	}
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
		check(func->parameter_type.type != NO_TYPE, context, "Function parameter type not found");
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

static void write_types(char *glsl, size_t *offset, shader_stage stage, type_id input, type_id output, function *main) {
	type_id types[256];
	size_t types_size = 0;
	find_referenced_types(main, types, &types_size);

	for (size_t i = 0; i < types_size; ++i) {
		type *t = get_type(types[i]);

		if (!t->built_in && t->attribute != add_name("pipe")) {
			if (stage == SHADER_STAGE_VERTEX && types[i] == input) {
				for (size_t j = 0; j < t->members.size; ++j) {
					*offset += sprintf(&glsl[*offset], "layout(location = %" PRIu64 ") in %s %s_%s;\n", j, type_string(t->members.m[j].type.type),
					                   get_name(t->name), get_name(t->members.m[j].name));
				}
			}
			else if (stage == SHADER_STAGE_VERTEX && types[i] == output) {
				for (size_t j = 0; j < t->members.size; ++j) {
					if (j != 0) {
						*offset += sprintf(&glsl[*offset], "layout(location = %" PRIu64 ") out %s %s_%s;\n", j - 1, type_string(t->members.m[j].type.type),
						                   get_name(t->name), get_name(t->members.m[j].name));
					}
				}
			}
			else if (stage == SHADER_STAGE_FRAGMENT && types[i] == input) {
				for (size_t j = 0; j < t->members.size; ++j) {
					if (j != 0) {
						*offset += sprintf(&glsl[*offset], "layout(location = %" PRIu64 ") in %s %s;\n", j - 1, type_string(t->members.m[j].type.type),
						                   get_name(t->members.m[j].name));
					}
				}
			}
		}
	}

	if (stage == SHADER_STAGE_FRAGMENT) {
		*offset += sprintf(&glsl[*offset], "out vec4 FragColor;\n");
	}

	*offset += sprintf(&glsl[*offset], "\n");

	for (size_t i = 0; i < types_size; ++i) {
		type *t = get_type(types[i]);

		if (!t->built_in && t->attribute != add_name("pipe")) {
			*offset += sprintf(&glsl[*offset], "struct %s {\n", get_name(t->name));

			for (size_t j = 0; j < t->members.size; ++j) {
				*offset += sprintf(&glsl[*offset], "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
			}

			*offset += sprintf(&glsl[*offset], "};\n\n");
		}
	}
}

static int global_register_indices[512];

static void write_globals(char *glsl, size_t *offset, function *main) {
	global_id globals[256];
	size_t globals_size = 0;
	find_referenced_globals(main, globals, &globals_size);

	for (size_t i = 0; i < globals_size; ++i) {
		global g = get_global(globals[i]);
		int register_index = global_register_indices[globals[i]];

		if (g.type == sampler_type_id) {
			*offset += sprintf(&glsl[*offset], "SamplerState _%" PRIu64 " : register(s%i);\n\n", g.var_index, register_index);
		}
		else if (g.type == tex2d_type_id) {
			*offset += sprintf(&glsl[*offset], "Texture2D<float4> _%" PRIu64 " : register(t%i);\n\n", g.var_index, register_index);
		}
		else if (g.type == texcube_type_id) {
			*offset += sprintf(&glsl[*offset], "TextureCube<float4> _%" PRIu64 " : register(t%i);\n\n", g.var_index, register_index);
		}
		else if (g.type == float_id) {
		}
		else {
			*offset += sprintf(&glsl[*offset], "layout(binding = %i) uniform _%" PRIu64 " {\n", register_index, g.var_index);
			type *t = get_type(g.type);
			for (size_t i = 0; i < t->members.size; ++i) {
				*offset +=
				    sprintf(&glsl[*offset], "\t%s _%" PRIu64 "_%s;\n", type_string(t->members.m[i].type.type), g.var_index, get_name(t->members.m[i].name));
			}
			*offset += sprintf(&glsl[*offset], "}\n\n");
		}
	}
}

static void write_functions(char *code, size_t *offset, shader_stage stage, type_id input, type_id output, function *main) {
	function *functions[256];
	size_t functions_size = 0;

	functions[functions_size] = main;
	functions_size += 1;

	find_referenced_functions(main, functions, &functions_size);

	for (size_t i = 0; i < functions_size; ++i) {
		function *f = functions[i];

		debug_context context = {0};
		check(f->block != NULL, context, "Function has no block");

		uint8_t *data = f->code.o;
		size_t size = f->code.size;

		uint64_t parameter_id = 0;
		for (size_t i = 0; i < f->block->block.vars.size; ++i) {
			if (f->parameter_name == f->block->block.vars.v[i].name) {
				parameter_id = f->block->block.vars.v[i].variable_id;
				break;
			}
		}

		check(parameter_id != 0, context, "Parameter not found");
		if (f == main) {
			if (stage == SHADER_STAGE_VERTEX) {
				*offset += sprintf(&code[*offset], "void main() {\n");
			}
			else if (stage == SHADER_STAGE_FRAGMENT) {
				if (f->return_type.array_size > 0) {
					*offset += sprintf(&code[*offset], "struct _render_targets {\n");
					for (uint32_t j = 0; j < f->return_type.array_size; ++j) {
						*offset += sprintf(&code[*offset], "\t%s _%i : SV_Target%i;\n", type_string(f->return_type.type), j, j);
					}
					*offset += sprintf(&code[*offset], "};\n\n");
					*offset += sprintf(&code[*offset], "_render_targets main(%s _%" PRIu64 ") {\n", type_string(f->parameter_type.type), parameter_id);
				}
				else {
					*offset += sprintf(&code[*offset], "void main() {\n");
				}
			}
			else {
				debug_context context = {0};
				error(context, "Unsupported shader stage");
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
			case OPCODE_CALL: {
				if (o->op_call.func == add_name("sample")) {
					debug_context context = {0};
					check(o->op_call.parameters_size == 3, context, "sample requires three parameters");
					*offset +=
					    sprintf(&code[*offset], "\t%s _%" PRIu64 " = _%" PRIu64 ".Sample(_%" PRIu64 ", _%" PRIu64 ");\n", type_string(o->op_call.var.type.type),
					            o->op_call.var.index, o->op_call.parameters[0].index, o->op_call.parameters[1].index, o->op_call.parameters[2].index);
				}
				else if (o->op_call.func == add_name("sample_lod")) {
					debug_context context = {0};
					check(o->op_call.parameters_size == 4, context, "sample_lod requires four parameters");
					*offset += sprintf(&code[*offset], "\t%s _%" PRIu64 " = _%" PRIu64 ".SampleLevel(_%" PRIu64 ", _%" PRIu64 ", _%" PRIu64 ");\n",
					                   type_string(o->op_call.var.type.type), o->op_call.var.index, o->op_call.parameters[0].index,
					                   o->op_call.parameters[1].index, o->op_call.parameters[2].index, o->op_call.parameters[3].index);
				}
				else {
					const char *function_name = get_name(o->op_call.func);
					if (o->op_call.func == add_name("float2")) {
						function_name = "vec2";
					}
					else if (o->op_call.func == add_name("float3")) {
						function_name = "vec3";
					}
					else if (o->op_call.func == add_name("float4")) {
						function_name = "vec4";
					}

					*offset += sprintf(&code[*offset], "\t%s _%" PRIu64 " = %s(", type_string(o->op_call.var.type.type), o->op_call.var.index, function_name);
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
			case OPCODE_LOAD_MEMBER: {
				uint64_t global_var_index = 0;
				for (global_id j = 0; get_global(j).type != NO_TYPE; ++j) {
					global g = get_global(j);
					if (o->op_load_member.from.index == g.var_index) {
						global_var_index = g.var_index;
						break;
					}
				}

				if (f == main && o->op_load_member.member_parent_type == input) {
					*offset += sprintf(&code[*offset], "\t%s _%" PRIu64 " = %s", type_string(o->op_load_member.to.type.type), o->op_load_member.to.index,
					                   type_string(o->op_load_member.member_parent_type));
				}
				else {
					*offset += sprintf(&code[*offset], "\t%s _%" PRIu64 " = _%" PRIu64, type_string(o->op_load_member.to.type.type), o->op_load_member.to.index,
					                   o->op_load_member.from.index);
				}

				type *s = get_type(o->op_load_member.member_parent_type);
				for (size_t i = 0; i < o->op_load_member.member_indices_size; ++i) {
					if (f == main && o->op_load_member.member_parent_type == input && i == 0) {
						*offset += sprintf(&code[*offset], "_%s", get_name(s->members.m[o->op_load_member.member_indices[i]].name));
					}
					else if (global_var_index != 0) {
						*offset += sprintf(&code[*offset], "_%s", get_name(s->members.m[o->op_load_member.member_indices[i]].name));
					}
					else {
						*offset += sprintf(&code[*offset], ".%s", get_name(s->members.m[o->op_load_member.member_indices[i]].name));
					}
					s = get_type(s->members.m[o->op_load_member.member_indices[i]].type.type);
				}
				*offset += sprintf(&code[*offset], ";\n");
				break;
			}
			case OPCODE_RETURN: {
				if (o->size > offsetof(opcode, op_return)) {
					if (f == main && stage == SHADER_STAGE_VERTEX) {
						*offset += sprintf(&code[*offset], "\t{\n");

						type *t = get_type(f->return_type.type);

						*offset += sprintf(&code[*offset], "\t\tgl_Position.x = _%" PRIu64 ".%s.x;\n", o->op_return.var.index, get_name(t->members.m[0].name));
						*offset += sprintf(&code[*offset], "\t\tgl_Position.y = _%" PRIu64 ".%s.y;\n", o->op_return.var.index, get_name(t->members.m[0].name));
						*offset += sprintf(&code[*offset], "\t\tgl_Position.z = (_%" PRIu64 ".%s.z * 2.0) - _%" PRIu64 ".%s.w;\n", o->op_return.var.index,
						                   get_name(t->members.m[0].name), o->op_return.var.index, get_name(t->members.m[0].name));
						*offset += sprintf(&code[*offset], "\t\tgl_Position.w = _%" PRIu64 ".%s.w;\n", o->op_return.var.index, get_name(t->members.m[0].name));

						for (size_t j = 1; j < t->members.size; ++j) {
							*offset += sprintf(&code[*offset], "\t\t%s_%s = _%" PRIu64 ".%s;\n", get_name(t->name), get_name(t->members.m[j].name),
							                   o->op_return.var.index, get_name(t->members.m[j].name));
						}

						*offset += sprintf(&code[*offset], "\t\treturn;\n");
						*offset += sprintf(&code[*offset], "\t}\n");
					}
					else if (f == main && stage == SHADER_STAGE_FRAGMENT && f->return_type.array_size > 0) {
						*offset += sprintf(&code[*offset], "\t{\n");
						*offset += sprintf(&code[*offset], "\t\t_render_targets rts;\n");
						for (uint32_t j = 0; j < f->return_type.array_size; ++j) {
							*offset += sprintf(&code[*offset], "\t\trts._%i = _%" PRIu64 "[%i];\n", j, o->op_return.var.index, j);
						}
						*offset += sprintf(&code[*offset], "\t\treturn rts;\n");
						*offset += sprintf(&code[*offset], "\t}\n");
					}
					else if (f == main && stage == SHADER_STAGE_FRAGMENT) {
						*offset += sprintf(&code[*offset], "\t{\n");
						*offset += sprintf(&code[*offset], "\t\tFragColor = _%" PRIu64 ";\n", o->op_return.var.index);
						*offset += sprintf(&code[*offset], "\t\treturn;\n");
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

static void glsl_export_vertex(char *directory, function *main) {
	char *glsl = (char *)calloc(1024 * 1024, 1);
	debug_context context = {0};
	check(glsl != NULL, context, "Could not allocate glsl string");

	size_t offset = 0;

	type_id vertex_input = main->parameter_type.type;
	type_id vertex_output = main->return_type.type;

	check(vertex_input != NO_TYPE, context, "vertex input missing");
	check(vertex_output != NO_TYPE, context, "vertex output missing");

	offset += sprintf(&glsl[offset], "#version 330\n\n");

	write_types(glsl, &offset, SHADER_STAGE_VERTEX, vertex_input, vertex_output, main);

	write_globals(glsl, &offset, main);

	write_functions(glsl, &offset, SHADER_STAGE_VERTEX, vertex_input, vertex_output, main);

	char *name = get_name(main->name);

	char filename[512];
	sprintf(filename, "kong_%s", name);

	char var_name[256];
	sprintf(var_name, "%s_code", name);

	write_code(glsl, directory, filename, var_name);
}

static void glsl_export_fragment(char *directory, function *main) {
	char *glsl = (char *)calloc(1024 * 1024, 1);
	debug_context context = {0};
	check(glsl != NULL, context, "Could not allocate glsl string");

	size_t offset = 0;

	type_id pixel_input = main->parameter_type.type;

	check(pixel_input != NO_TYPE, context, "fragment input missing");

	offset += sprintf(&glsl[offset], "#version 330\n\n");

	write_types(glsl, &offset, SHADER_STAGE_FRAGMENT, pixel_input, NO_TYPE, main);

	write_globals(glsl, &offset, main);

	write_functions(glsl, &offset, SHADER_STAGE_FRAGMENT, pixel_input, NO_TYPE, main);

	char *name = get_name(main->name);

	char filename[512];
	sprintf(filename, "kong_%s", name);

	char var_name[256];
	sprintf(var_name, "%s_code", name);

	write_code(glsl, directory, filename, var_name);
}

void glsl_export(char *directory) {
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

	for (size_t i = 0; i < vertex_shaders_size; ++i) {
		glsl_export_vertex(directory, vertex_shaders[i]);
	}

	for (size_t i = 0; i < fragment_shaders_size; ++i) {
		glsl_export_fragment(directory, fragment_shaders[i]);
	}
}
