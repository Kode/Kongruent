#include "errors.h"
#include "log.h"
#include "parser.h"
#include "tokenizer.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *filename = "in/test.kong";

const char all_names[1024 * 1024];

typedef struct variable {
	uint64_t index;
} variable;

static uint64_t next_variable_id = 0;

typedef struct opcode {
	enum { OPCODE_VAR, OPCODE_NOT, OPCODE_STORE_VARIABLE, OPCODE_STORE_MEMBER, OPCODE_LOAD_CONSTANT } type;
	uint8_t size;

	union {
		struct {
			int a;
		} var;
		struct {
			variable var;
		} op_not;
		struct {
			variable from;
			variable to;
		} op_store_var;
		struct {
			variable from;
			variable to;
			size_t member;
		} op_store_member;
		struct {
			float number;
			variable to;
		} op_load_constant;
	};
} opcode;

typedef struct opcodes {
	uint8_t o[4096];
	size_t size;
} opcodes;

opcodes all_opcodes;

variable allocate_variable() {
	variable v;
	v.index = next_variable_id;
	++next_variable_id;
	return v;
}

void emit_op(opcode *o) {
	memcpy(&all_opcodes.o[all_opcodes.size], o, o->size);
	all_opcodes.size += o->size;
}

variable emit_expression(expression *e) {
	switch (e->type) {
	case EXPRESSION_BINARY: {
		expression *left = e->binary.left;
		expression *right = e->binary.left;

		switch (e->binary.op) {
		case OPERATOR_EQUALS:
			error("not implemented", 0, 0);
		case OPERATOR_NOT_EQUALS:
			error("not implemented", 0, 0);
		case OPERATOR_GREATER:
			error("not implemented", 0, 0);
		case OPERATOR_GREATER_EQUAL:
			error("not implemented", 0, 0);
		case OPERATOR_LESS:
			error("not implemented", 0, 0);
		case OPERATOR_LESS_EQUAL:
			error("not implemented", 0, 0);
		case OPERATOR_MINUS:
			error("not implemented", 0, 0);
		case OPERATOR_PLUS:
			error("not implemented", 0, 0);
		case OPERATOR_DIVIDE:
			error("not implemented", 0, 0);
		case OPERATOR_MULTIPLY:
			error("not implemented", 0, 0);
		case OPERATOR_NOT:
			error("not implemented", 0, 0);
		case OPERATOR_OR:
			error("not implemented", 0, 0);
		case OPERATOR_AND:
			error("not implemented", 0, 0);
		case OPERATOR_MOD:
			error("not implemented", 0, 0);
		case OPERATOR_ASSIGN: {
			variable v = emit_expression(right);

			switch (left->type) {
			case EXPRESSION_VARIABLE: {
				opcode o;
				o.type = OPCODE_STORE_VARIABLE;
				o.size = 2 + sizeof(o.op_store_var);
				o.op_store_var.from = v;
				// o.op_store_var.to = left->variable;
				emit_op(&o);
				break;
			}
			case EXPRESSION_MEMBER: {
				variable member_var = emit_expression(left->member.left);

				opcode o;
				o.type = OPCODE_STORE_MEMBER;
				o.size = 2 + sizeof(o.op_store_member);
				o.op_store_member.from = v;
				o.op_store_member.to = member_var;
				// o.op_store_member.member = left->member.right;
				emit_op(&o);
				break;
			}
			default:
				error("Expected a variable or a member", 0, 0);
			}
			break;
		}
		}
		break;
	}
	case EXPRESSION_UNARY:
		switch (e->unary.op) {
		case OPERATOR_EQUALS:
			error("not implemented", 0, 0);
		case OPERATOR_NOT_EQUALS:
			error("not implemented", 0, 0);
		case OPERATOR_GREATER:
			error("not implemented", 0, 0);
		case OPERATOR_GREATER_EQUAL:
			error("not implemented", 0, 0);
		case OPERATOR_LESS:
			error("not implemented", 0, 0);
		case OPERATOR_LESS_EQUAL:
			error("not implemented", 0, 0);
		case OPERATOR_MINUS:
			error("not implemented", 0, 0);
		case OPERATOR_PLUS:
			error("not implemented", 0, 0);
		case OPERATOR_DIVIDE:
			error("not implemented", 0, 0);
		case OPERATOR_MULTIPLY:
			error("not implemented", 0, 0);
		case OPERATOR_NOT: {
			variable v = emit_expression(e->unary.right);
			opcode o;
			o.type = OPCODE_NOT;
			o.size = 2 + sizeof(o.op_not);
			o.op_not.var = v;
			emit_op(&o);
			return v;
		}
		case OPERATOR_OR:
			error("not implemented", 0, 0);
		case OPERATOR_AND:
			error("not implemented", 0, 0);
		case OPERATOR_MOD:
			error("not implemented", 0, 0);
		case OPERATOR_ASSIGN:
			error("not implemented", 0, 0);
		}
	case EXPRESSION_BOOLEAN:
		error("not implemented", 0, 0);
	case EXPRESSION_NUMBER: {
		variable v = allocate_variable();

		opcode o;
		o.type = OPCODE_LOAD_CONSTANT;
		o.size = 2 + sizeof(o.op_load_constant);
		o.op_load_constant.number = 1.0f;
		o.op_load_constant.to = v;

		return v;
	}
	case EXPRESSION_STRING:
		error("not implemented", 0, 0);
	case EXPRESSION_VARIABLE:
		error("not implemented", 0, 0);
	case EXPRESSION_GROUPING:
		error("not implemented", 0, 0);
	case EXPRESSION_CALL:
		error("not implemented", 0, 0);
	case EXPRESSION_MEMBER:
		error("not implemented", 0, 0);
	case EXPRESSION_CONSTRUCTOR:
		error("not implemented", 0, 0);
	}
	assert(false);
}

void emit_statement(statement *statement) {
	switch (statement->type) {
	case STATEMENT_EXPRESSION:
		emit_expression(statement->expression);
		break;
	case STATEMENT_RETURN_EXPRESSION:
		break;
	case STATEMENT_IF:
		break;
	case STATEMENT_BLOCK:
		break;
	case STATEMENT_LOCAL_VARIABLE:
		break;
	}
}

void convert_function_block(struct statement *block) {
	if (block->type != STATEMENT_BLOCK) {
		error("Expected a block", 0, 0);
	}
	for (size_t i = 0; i < block->block.statements.size; ++i) {
		emit_statement(block->block.statements.s[i]);
	}
}

int main(int argc, char **argv) {
	FILE *file = fopen(filename, "rb");

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *data = (char *)malloc(size + 1);
	assert(data != NULL);
	fread(data, 1, size, file);
	fclose(file);
	data[size] = 0;

	tokens tokens = tokenize(data);

	free(data);

	parse(&tokens);

	log(LOG_LEVEL_INFO, "Functions:");
	for (size_t i = 0; i < all_functions.size; ++i) {
		log(LOG_LEVEL_INFO, "%s", all_functions.f[i]->function.name);
	}
	log(LOG_LEVEL_INFO, "");

	log(LOG_LEVEL_INFO, "Structs:");
	for (size_t i = 0; i < all_structs.size; ++i) {
		log(LOG_LEVEL_INFO, "%s", all_structs.s[i]->structy.name);
	}

	for (size_t i = 0; i < all_functions.size; ++i) {
		convert_function_block(all_functions.f[i]->function.block);
	}

	return 0;
}
