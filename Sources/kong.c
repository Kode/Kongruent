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

typedef struct opcode {
	enum { OPCODE_VAR, OPCODE_NOT, OPCODE_STORE_VARIABLE, OPCODE_STORE_MEMBER, OPCODE_LOAD_CONSTANT } type;
	uint8_t size;

	union {
		struct {
			int a;
		} var;
	};
} opcode;

typedef struct opcodes {
	uint8_t o[4096];
	size_t size;
} opcodes;

opcodes all_opcodes;

typedef struct variable {
	uint64_t index;
} variable;

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
		case OPERATOR_NOT_EQUALS:
		case OPERATOR_GREATER:
		case OPERATOR_GREATER_EQUAL:
		case OPERATOR_LESS:
		case OPERATOR_LESS_EQUAL:
		case OPERATOR_MINUS:
		case OPERATOR_PLUS:
		case OPERATOR_DIVIDE:
		case OPERATOR_MULTIPLY:
		case OPERATOR_NOT:
		case OPERATOR_OR:
		case OPERATOR_AND:
		case OPERATOR_MOD:
		case OPERATOR_ASSIGN: {
			variable v = emit_expression(right);

			switch (left->type) {
			case EXPRESSION_VARIABLE: {
				opcode o;
				o.type = OPCODE_STORE_VARIABLE;
				emit_op(&o);
				break;
			}
			case EXPRESSION_MEMBER: {
				opcode o;
				o.type = OPCODE_STORE_MEMBER;
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
		case OPERATOR_NOT_EQUALS:
		case OPERATOR_GREATER:
		case OPERATOR_GREATER_EQUAL:
		case OPERATOR_LESS:
		case OPERATOR_LESS_EQUAL:
		case OPERATOR_MINUS:
		case OPERATOR_PLUS:
		case OPERATOR_DIVIDE:
		case OPERATOR_MULTIPLY:
		case OPERATOR_NOT: {
			variable v = emit_expression(e->unary.right);
			opcode o;
			o.type = OPCODE_NOT;
			emit_op(&o);
			return v;
		}
		case OPERATOR_OR:
		case OPERATOR_AND:
		case OPERATOR_MOD:
		case OPERATOR_ASSIGN:
			break;
		}
	case EXPRESSION_BOOLEAN:
	case EXPRESSION_NUMBER: {
		opcode o;
		o.type = OPCODE_LOAD_CONSTANT;
		break;
	}
	case EXPRESSION_STRING:
	case EXPRESSION_VARIABLE:
	case EXPRESSION_GROUPING:
	case EXPRESSION_CALL:
	case EXPRESSION_MEMBER:
	case EXPRESSION_CONSTRUCTOR:
		break;
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
