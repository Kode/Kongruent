#include "errors.h"
#include "log.h"
#include "parser.h"
#include "tokenizer.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

const char *filename = "in/test.kong";

typedef struct opcode {
	enum { OPCODE_VAR, OPCODE_NOT } type;

	union {
		struct {
			int a;
		} var;
	};
} opcode;

typedef struct opcodes {
	opcode o[256];
	size_t size;
} opcodes;

opcodes all_opcodes;

typedef struct variable {
	uint64_t index;
} variable;

void emit_op(opcode opcode) {
	all_opcodes.o[all_opcodes.size] = opcode;
	all_opcodes.size += 1;
}

variable emit_expression(expression *expression) {
	switch (expression->type) {
	case EXPRESSION_BINARY: {
		switch (expression->binary.op) {
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
		case OPERATOR_ASSIGN:
			break;
		}
		break;
	}
	case EXPRESSION_UNARY:
		switch (expression->unary.op) {
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
			variable v = emit_expression(expression->unary.right);
			opcode opcode;
			opcode.type = OPCODE_NOT;
			emit_op(opcode);
			return v;
		}
		case OPERATOR_OR:
		case OPERATOR_AND:
		case OPERATOR_MOD:
		case OPERATOR_ASSIGN:
			break;
		}
	case EXPRESSION_BOOLEAN:
	case EXPRESSION_NUMBER:
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
