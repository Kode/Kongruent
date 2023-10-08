#include "compiler.h"
#include "errors.h"
#include "functions.h"
#include "globals.h"
#include "log.h"
#include "names.h"
#include "parser.h"
#include "tokenizer.h"
#include "types.h"

#include "backends/glsl.h"
#include "backends/hlsl.h"
#include "backends/metal.h"
#include "backends/spirv.h"
#include "backends/wgsl.h"

#include "integrations/kinc.h"

#include "dir.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

type_ref find_local_var_type(block *b, name_id name) {
	if (b == NULL) {
		type_ref t;
		init_type_ref(&t, NO_NAME);
	}

	for (size_t i = 0; i < b->vars.size; ++i) {
		if (b->vars.v[i].name == name) {
			debug_context context = {0};
			check(b->vars.v[i].type.type != NO_TYPE, context, "Local var has not type");
			return b->vars.v[i].type;
		}
	}

	return find_local_var_type(b->parent, name);
}

void resolve_types_in_expression(statement *parent, expression *e);

type_ref resolve_member_var_type(statement *parent_block, type_ref parent_type, expression *left) {
	if (left->kind == EXPRESSION_VARIABLE) {
		if (parent_type.type != NO_TYPE) {
			name_id name = left->variable;

			type *parent_struct = get_type(parent_type.type);
			for (size_t i = 0; i < parent_struct->members.size; ++i) {
				if (parent_struct->members.m[i].name == name) {
					left->type = parent_struct->members.m[i].type;
					return left->type;
				}
			}

			debug_context context = {0};
			error(context, "Member not found");
			type_ref t;
			init_type_ref(&t, NO_NAME);
			return t;
		}

		if (parent_block != NULL) {
			resolve_types_in_expression(parent_block, left);
			return left->type;
		}
	}
	else if (left->kind == EXPRESSION_INDEX) {
		if (parent_type.type != NO_TYPE) {
			init_type_ref(&left->type, NO_NAME);
			left->type = parent_type;
			return parent_type;
		}
	}

	{
		debug_context context = {0};
		error(context, "Member not found");
		type_ref t;
		init_type_ref(&t, NO_NAME);
		return t;
	}
}

void resolve_member_type(statement *parent_block, type_ref parent_type, expression *e) {
	if (e->kind == EXPRESSION_VARIABLE) {
		resolve_member_var_type(parent_block, parent_type, e);
		return;
	}

	debug_context context = {0};
	check(e->kind == EXPRESSION_MEMBER, context, "Malformed member");

	type_ref t = resolve_member_var_type(parent_block, parent_type, e->member.left);

	resolve_member_type(parent_block, t, e->member.right);

	e->type = e->member.right->type;
}

void resolve_types_in_expression(statement *parent, expression *e) {
	switch (e->kind) {
	case EXPRESSION_BINARY: {
		resolve_types_in_expression(parent, e->binary.left);
		resolve_types_in_expression(parent, e->binary.right);
		switch (e->binary.op) {
		case OPERATOR_EQUALS:
		case OPERATOR_NOT_EQUALS:
		case OPERATOR_GREATER:
		case OPERATOR_GREATER_EQUAL:
		case OPERATOR_LESS:
		case OPERATOR_LESS_EQUAL:
		case OPERATOR_OR:
		case OPERATOR_AND: {
			e->type.type = bool_id;
			break;
		}
		case OPERATOR_MULTIPLY: {
			type_id left_type = e->binary.left->type.type;
			type_id right_type = e->binary.right->type.type;
			if (left_type == right_type || (left_type == float4x4_id && right_type == float4_id)) {
				e->type = e->binary.right->type;
			}
			else {
				debug_context context = {0};
				error(context, "Type mismatch %s vs %s", get_name(get_type(left_type)->name), get_name(get_type(right_type)->name));
			}
			break;
		}
		case OPERATOR_MINUS:
		case OPERATOR_PLUS:
		case OPERATOR_DIVIDE:
		case OPERATOR_MOD:
		case OPERATOR_ASSIGN: {
			type_id left_type = e->binary.left->type.type;
			type_id right_type = e->binary.right->type.type;
			if (left_type != right_type) {
				debug_context context = {0};
				error(context, "Type mismatch %s vs %s", get_name(get_type(left_type)->name), get_name(get_type(right_type)->name));
			}
			e->type = e->binary.left->type;
			break;
		}
		case OPERATOR_NOT: {
			debug_context context = {0};
			error(context, "Weird binary operator");
			break;
		}
		}
		break;
	}
	case EXPRESSION_UNARY: {
		resolve_types_in_expression(parent, e->unary.right);
		switch (e->unary.op) {
		case OPERATOR_MINUS:
		case OPERATOR_PLUS: {
			e->type = e->unary.right->type;
			break;
		}
		case OPERATOR_NOT: {
			e->type.type = bool_id;
			break;
		}
		case OPERATOR_EQUALS:
		case OPERATOR_NOT_EQUALS:
		case OPERATOR_GREATER:
		case OPERATOR_GREATER_EQUAL:
		case OPERATOR_LESS:
		case OPERATOR_LESS_EQUAL:
		case OPERATOR_DIVIDE:
		case OPERATOR_MULTIPLY:
		case OPERATOR_OR:
		case OPERATOR_AND:
		case OPERATOR_MOD:
		case OPERATOR_ASSIGN:
		default: {
			debug_context context = {0};
			error(context, "Weird unary operator");
			break;
		}
		}
	}
	case EXPRESSION_BOOLEAN: {
		e->type.type = bool_id;
		break;
	}
	case EXPRESSION_NUMBER: {
		e->type.type = float_id;
		break;
	}
	case EXPRESSION_VARIABLE: {
		global g = find_global(e->variable);
		if (g.type != NO_TYPE) {
			e->type.type = g.type;
		}
		else {
			type_ref type = find_local_var_type(&parent->block, e->variable);
			if (type.type == NO_TYPE) {
				debug_context context = {0};
				error(context, "Variable %s not found", get_name(e->variable));
			}
			e->type = type;
		}
		break;
	}
	case EXPRESSION_GROUPING: {
		resolve_types_in_expression(parent, e->grouping);
		e->type = e->grouping->type;
		break;
	}
	case EXPRESSION_CALL: {
		for (function_id i = 0; get_function(i) != NULL; ++i) {
			function *f = get_function(i);
			if (f->name == e->call.func_name) {
				e->type = f->return_type;
				break;
			}
		}
		for (size_t i = 0; i < e->call.parameters.size; ++i) {
			resolve_types_in_expression(parent, e->call.parameters.e[i]);
		}
		break;
	}
	case EXPRESSION_MEMBER: {
		type_ref t;
		init_type_ref(&t, NO_NAME);
		resolve_member_type(parent, t, e);
		break;
	}
	case EXPRESSION_CONSTRUCTOR: {
		debug_context context = {0};
		error(context, "not implemented");
		break;
	}
	}

	if (e->type.type == NO_TYPE) {
		debug_context context = {0};
		error(context, "Could not resolve type");
	}
}

void resolve_types_in_block(statement *parent, statement *block) {
	debug_context context = {0};
	check(block->kind == STATEMENT_BLOCK, context, "Malformed block");

	for (size_t i = 0; i < block->block.statements.size; ++i) {
		statement *s = block->block.statements.s[i];
		switch (s->kind) {
		case STATEMENT_EXPRESSION: {
			resolve_types_in_expression(block, s->expression);
			break;
		}
		case STATEMENT_RETURN_EXPRESSION: {
			resolve_types_in_expression(block, s->expression);
			break;
		}
		case STATEMENT_IF: {
			resolve_types_in_expression(block, s->iffy.test);
			resolve_types_in_block(block, s->iffy.block);
			break;
		}
		case STATEMENT_BLOCK: {
			resolve_types_in_block(block, s);
			break;
		}
		case STATEMENT_LOCAL_VARIABLE: {
			name_id var_name = s->local_variable.var.name;
			name_id var_type_name = s->local_variable.var.type.name;
			if (var_type_name != NO_NAME && s->local_variable.var.type.type == NO_TYPE) {
				s->local_variable.var.type.type = find_type_by_name(var_type_name);
			}
			if (s->local_variable.var.type.type == NO_TYPE) {
				debug_context context = {0};
				error(context, "Could not find type %s for %s", get_name(var_type_name), get_name(var_name));
			}

			if (s->local_variable.init != NULL) {
				resolve_types_in_expression(block, s->local_variable.init);
			}

			block->block.vars.v[block->block.vars.size].name = var_name;
			block->block.vars.v[block->block.vars.size].type = s->local_variable.var.type;
			++block->block.vars.size;
			break;
		}
		}
	}
}

void resolve_types(void) {
	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *s = get_type(i);
		for (size_t j = 0; j < s->members.size; ++j) {
			if (s->members.m[j].type.type == NO_TYPE) {
				name_id name = s->members.m[j].type.name;
				s->members.m[j].type.type = find_type_by_name(name);
				if (s->members.m[j].type.type == NO_TYPE) {
					debug_context context = {0};
					error(context, "Could not find type %s in %s", get_name(name), get_name(s->name));
				}
			}
		}
	}

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);

		if (f->parameter_type.type == NO_TYPE) {
			name_id parameter_type_name = f->parameter_type.name;
			f->parameter_type.type = find_type_by_name(parameter_type_name);
			if (f->parameter_type.type == NO_TYPE) {
				debug_context context = {0};
				error(context, "Could not find type %s for %s", get_name(parameter_type_name), get_name(f->name));
			}
		}

		if (f->return_type.type == NO_TYPE) {
			name_id return_type_name = f->return_type.name;
			f->return_type.type = find_type_by_name(return_type_name);
			if (f->return_type.type == NO_TYPE) {
				debug_context context = {0};
				error(context, "Could not find type %s for %s", get_name(return_type_name), get_name(f->name));
			}
		}
	}

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);

		if (f->block == NULL) {
			// built in
			continue;
		}

		f->block->block.vars.v[f->block->block.vars.size].name = f->parameter_name;
		f->block->block.vars.v[f->block->block.vars.size].type = f->parameter_type;
		f->block->block.vars.v[f->block->block.vars.size].variable_id = 0;
		++f->block->block.vars.size;

		resolve_types_in_block(NULL, f->block);
	}
}

typedef enum arg_mode { MODE_MODECHECK, MODE_INPUT, MODE_OUTPUT, MODE_PLATFORM, MODE_API } arg_mode;

static void help(void) {
	printf("No help is coming.");
}

static void read_file(char *filename) {
	FILE *file = fopen(filename, "rb");

	if (file == NULL) {
		kong_log(LOG_LEVEL_ERROR, "File %s not found.", filename);
		exit(1);
	}

	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *data = (char *)malloc(size + 1);

	debug_context context = {0};
	context.filename = filename;
	check(data != NULL, context, "Could not allocate memory to read file %s", filename);

	fread(data, 1, size, file);
	data[size] = 0;

	fclose(file);

	tokens tokens = tokenize(filename, data);

	free(data);

	parse(filename, &tokens);
}

int main(int argc, char **argv) {
	arg_mode mode = MODE_MODECHECK;

	char *inputs[256] = {0};
	size_t inputs_size = 0;
	char *platform = NULL;
	api_kind api = API_UNKNOWN;
	char *output = NULL;

	for (int i = 1; i < argc; ++i) {
		switch (mode) {
		case MODE_MODECHECK: {
			if (argv[i][0] == '-') {
				if (argv[i][1] == '-') {
					if (strcmp(&argv[i][2], "in") == 0) {
						mode = MODE_INPUT;
					}
					else if (strcmp(&argv[i][2], "out") == 0) {
						mode = MODE_OUTPUT;
					}
					else if (strcmp(&argv[i][2], "platform") == 0) {
						mode = MODE_PLATFORM;
					}
					else if (strcmp(&argv[i][2], "api") == 0) {
						mode = MODE_API;
					}
					else if (strcmp(&argv[i][2], "help") == 0) {
						help();
						return 0;
					}
					else {
						debug_context context = {0};
						error(context, "Unknown parameter %s", &argv[i][2]);
					}
				}
				else {
					debug_context context = {0};
					check(argv[i][1] != 0, context, "Borked parameter");
					check(argv[i][2] == 0, context, "Borked parameter");
					switch (argv[i][1]) {
					case 'i':
						mode = MODE_INPUT;
						break;
					case 'o':
						mode = MODE_OUTPUT;
						break;
					case 'p':
						mode = MODE_PLATFORM;
						break;
					case 'a':
						mode = MODE_API;
						break;
					case 'h':
						help();
						return 0;
					default: {
						debug_context context = {0};
						error(context, "Unknown parameter %s", argv[i][1]);
					}
					}
				}
			}
			else {
				debug_context context = {0};
				error(context, "Wrong parameter syntax");
			}
			break;
		}
		case MODE_INPUT: {
			inputs[inputs_size] = argv[i];
			inputs_size += 1;
			mode = MODE_MODECHECK;
			break;
		}
		case MODE_OUTPUT: {
			output = argv[i];
			mode = MODE_MODECHECK;
			break;
		}
		case MODE_PLATFORM: {
			platform = argv[i];
			mode = MODE_MODECHECK;
			break;
		}
		case MODE_API: {
			if (strcmp(argv[i], "direct3d9") == 0) {
				api = API_DIRECT3D9;
			}
			else if (strcmp(argv[i], "direct3d11") == 0) {
				api = API_DIRECT3D11;
			}
			else if (strcmp(argv[i], "direct3d12") == 0) {
				api = API_DIRECT3D12;
			}
			else if (strcmp(argv[i], "opengl") == 0) {
				api = API_OPENGL;
			}
			else if (strcmp(argv[i], "metal") == 0) {
				api = API_METAL;
			}
			else if (strcmp(argv[i], "webgpu") == 0) {
				api = API_WEBGPU;
			}
			else if (strcmp(argv[i], "vulkan") == 0) {
				api = API_VULKAN;
			}
			else {
				debug_context context = {0};
				error(context, "Unknown API %s", argv[i]);
			}
			mode = MODE_MODECHECK;
			break;
		}
		}
	}

	debug_context context = {0};
	check(mode == MODE_MODECHECK, context, "Wrong parameter syntax");
	check(inputs_size > 0, context, "no input parameters found");
	check(output != NULL, context, "output parameter not found");
	check(platform != NULL, context, "platform parameter not found");
	check(api != API_UNKNOWN, context, "api parameter not found");

	names_init();
	types_init();
	functions_init();
	globals_init();

	for (size_t i = 0; i < inputs_size; ++i) {
		directory dir = open_dir(inputs[i]);

		file f = read_next_file(&dir);
		while (f.valid) {
			char path[1024];
			strcpy(path, inputs[i]);
			strcat(path, "/");
			strcat(path, f.name);

			read_file(path);

			f = read_next_file(&dir);
		}

		close_dir(&dir);
	}

#ifndef NDEBUG
	kong_log(LOG_LEVEL_INFO, "Functions:");
	for (function_id i = 0; get_function(i) != NULL; ++i) {
		kong_log(LOG_LEVEL_INFO, "%s", get_name(get_function(i)->name));
	}
	kong_log(LOG_LEVEL_INFO, "");

	kong_log(LOG_LEVEL_INFO, "Types:");
	for (type_id i = 0; get_type(i) != NULL; ++i) {
		kong_log(LOG_LEVEL_INFO, "%s", get_name(get_type(i)->name));
	}
#endif

	resolve_types();

	convert_globals();
	for (function_id i = 0; get_function(i) != NULL; ++i) {
		convert_function_block(&get_function(i)->code, get_function(i)->block);
	}

	switch (api) {
	case API_DIRECT3D9:
	case API_DIRECT3D11:
	case API_DIRECT3D12:
		hlsl_export(output, api);
		break;
	case API_OPENGL:
		glsl_export(output);
		break;
	case API_METAL:
		metal_export(output);
		break;
	case API_WEBGPU:
		wgsl_export(output);
		break;
	case API_VULKAN: {
		kong_log(LOG_LEVEL_WARNING, "Beware, SPIR-V export is not yet working");
		spirv_export(output);
		break;
	}
	default: {
		debug_context context = {0};
		error(context, "Unknown API");
	}
	}

	kinc_export(output, api);

	return 0;
}
