#include "compiler.h"
#include "errors.h"
#include "functions.h"
#include "log.h"
#include "names.h"
#include "parser.h"
#include "tokenizer.h"
#include "types.h"

#include "backends/hlsl.h"

#include "integrations/c.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *filename = "in/test.kong";

type_id find_local_var_type(block *b, name_id name) {
	if (b == NULL) {
		return NO_TYPE;
	}

	for (size_t i = 0; i < b->vars.size; ++i) {
		if (b->vars.v[i].name == name) {
			assert(b->vars.v[i].type.resolved);
			return b->vars.v[i].type.type;
		}
	}

	return find_local_var_type(b->parent, name);
}

void resolve_types_in_expression(statement *parent, expression *e);

type *resolve_member_var_type(statement *parent_block, type *parent_struct, expression *left) {
	assert(left->kind == EXPRESSION_VARIABLE);

	if (parent_struct != NULL) {
		name_id name = left->variable;

		for (size_t i = 0; i < parent_struct->members.size; ++i) {
			if (parent_struct->members.m[i].name == name) {
				left->type = parent_struct->members.m[i].type;
				return get_type(left->type.type);
			}
		}

		assert(false);
		return NULL;
	}

	if (parent_block != NULL) {
		resolve_types_in_expression(parent_block, left);
		return get_type(left->type.type);
	}

	assert(false);
	return NULL;
}

void resolve_member_type(statement *parent_block, type *parent_struct, expression *e) {
	if (e->kind == EXPRESSION_VARIABLE) {
		resolve_member_var_type(parent_block, parent_struct, e);
		return;
	}

	assert(e->kind == EXPRESSION_MEMBER);

	type *s = resolve_member_var_type(parent_block, parent_struct, e->member.left);

	resolve_member_type(parent_block, s, e->member.right);

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
			e->type.resolved = true;
			break;
		}
		case OPERATOR_MINUS:
		case OPERATOR_PLUS:
		case OPERATOR_DIVIDE:
		case OPERATOR_MULTIPLY:
		case OPERATOR_MOD:
		case OPERATOR_ASSIGN: {
			if (e->binary.left->type.type != e->binary.right->type.type) {
				error("Type mismatch", 0, 0);
			}
			e->type = e->binary.left->type;
			break;
		}
		case OPERATOR_NOT: {
			error("Weird binary operator", 0, 0);
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
			e->type.resolved = true;
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
		default:
			error("Weird unary operator", 0, 0);
			break;
		}
	}
	case EXPRESSION_BOOLEAN: {
		e->type.type = bool_id;
		e->type.resolved = true;
		break;
	}
	case EXPRESSION_NUMBER: {
		e->type.type = float_id;
		e->type.resolved = true;
		break;
	}
	case EXPRESSION_VARIABLE: {
		type_id type = find_local_var_type(&parent->block, e->variable);
		if (type == NO_TYPE) {
			char output[256];
			sprintf(output, "Variable %s not found", get_name(e->variable));
			error(output, 0, 0);
		}
		e->type.type = type;
		e->type.resolved = true;
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
		break;
	}
	case EXPRESSION_MEMBER: {
		resolve_member_type(parent, NULL, e);
		break;
	}
	case EXPRESSION_CONSTRUCTOR: {
		error("not implemented", 0, 0);
		break;
	}
	}

	if (!e->type.resolved) {
		error("Could not resolve type", 0, 0);
	}
}

void resolve_types_in_block(statement *parent, statement *block) {
	assert(block->kind == STATEMENT_BLOCK);

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
			s->local_variable.var.type.type = find_type(var_type_name);
			if (s->local_variable.var.type.type == NO_TYPE) {
				char output[256];
				sprintf(output, "Could not find type %s for %s", get_name(var_type_name), get_name(var_name));
				error(output, 0, 0);
			}
			s->local_variable.var.type.resolved = true;

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
			if (!s->members.m[j].type.resolved) {
				name_id name = s->members.m[j].type.name;
				if (name != NO_NAME) {
					s->members.m[j].type.type = find_type(name);
					if (s->members.m[j].type.type == NO_TYPE) {
						char output[256];
						char *struct_name = get_name(s->name);
						char *member_type_name = get_name(name);
						sprintf(output, "Could not find type %s in %s", member_type_name, struct_name);
						error(output, 0, 0);
					}
					s->members.m[j].type.resolved = true;
				}
			}
		}
	}

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);

		name_id parameter_type_name = f->parameter_type.name;
		f->parameter_type.type = find_type(parameter_type_name);
		if (f->parameter_type.type == NO_TYPE) {
			char output[256];
			char *function_name = get_name(f->name);
			char *parameter_type_name_name = get_name(parameter_type_name);
			sprintf(output, "Could not find type %s for %s", parameter_type_name_name, function_name);
			error(output, 0, 0);
		}
		f->parameter_type.resolved = true;

		name_id return_type_name = f->return_type.name;
		f->return_type.type = find_type(return_type_name);
		if (f->return_type.type == NO_TYPE) {
			char output[256];
			char *function_name = get_name(f->name);
			char *return_type_name_name = get_name(return_type_name);
			sprintf(output, "Could not find type %s for %s", return_type_name_name, function_name);
			error(output, 0, 0);
		}
		f->return_type.resolved = true;
	}

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);

		f->block->block.vars.v[f->block->block.vars.size].name = f->parameter_name;
		f->block->block.vars.v[f->block->block.vars.size].type = f->parameter_type;
		f->block->block.vars.v[f->block->block.vars.size].variable_id = 0;
		++f->block->block.vars.size;

		resolve_types_in_block(NULL, f->block);
	}
}

int main(int argc, char **argv) {
	FILE *file = fopen(filename, "rb");

	if (file == NULL) {
		kong_log(LOG_LEVEL_ERROR, "File %s not found.", filename);
		return 1;
	}

	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *data = (char *)malloc(size + 1);
	assert(data != NULL);

	fread(data, 1, size, file);
	data[size] = 0;

	fclose(file);

	names_init();
	types_init();
	functions_init();

	tokens tokens = tokenize(data);

	free(data);

	parse(&tokens);

	kong_log(LOG_LEVEL_INFO, "Functions:");
	for (function_id i = 0; get_function(i) != NULL; ++i) {
		kong_log(LOG_LEVEL_INFO, "%s", get_name(get_function(i)->name));
	}
	kong_log(LOG_LEVEL_INFO, "");

	kong_log(LOG_LEVEL_INFO, "Types:");
	for (type_id i = 0; get_type(i) != NULL; ++i) {
		kong_log(LOG_LEVEL_INFO, "%s", get_name(get_type(i)->name));
	}

	resolve_types();

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		convert_function_block(&get_function(i)->code, get_function(i)->block);
	}

	hlsl_export();

	c_export();

	return 0;
}
