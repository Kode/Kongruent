#include "compiler.h"

#include "errors.h"
#include "parser.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

const char all_names[1024 * 1024];

static uint64_t next_variable_id = 1;

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

#define OP_SIZE(op, opmember) offsetof(opcode, opmember) + sizeof(o.opmember)

variable emit_expression(expression *e) {
	switch (e->type) {
	case EXPRESSION_BINARY: {
		expression *left = e->binary.left;
		expression *right = e->binary.right;

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
				o.size = OP_SIZE(o, op_store_var);
				o.op_store_var.from = v;
				// o.op_store_var.to = left->variable;
				emit_op(&o);
				break;
			}
			case EXPRESSION_MEMBER: {
				variable member_var = emit_expression(left->member.left);

				opcode o;
				o.type = OPCODE_STORE_MEMBER;
				o.size = OP_SIZE(o, op_store_member);
				o.op_store_member.from = v;
				o.op_store_member.to = member_var;
				// o.op_store_member.member = left->member.right;
				emit_op(&o);
				break;
			}
			default:
				error("Expected a variable or a member", 0, 0);
			}

			return v;
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
			o.size = OP_SIZE(o, op_not);
			o.op_not.from = v;
			o.op_not.to = allocate_variable();
			emit_op(&o);
			return o.op_not.to;
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
		o.size = OP_SIZE(o, op_load_constant);
		o.op_load_constant.number = 1.0f;
		o.op_load_constant.to = v;
		emit_op(&o);

		return v;
	}
	case EXPRESSION_STRING:
		error("not implemented", 0, 0);
	case EXPRESSION_VARIABLE: {
		variable v = allocate_variable();
		return v;
	}
	case EXPRESSION_GROUPING:
		error("not implemented", 0, 0);
	case EXPRESSION_CALL:
		error("not implemented", 0, 0);
	case EXPRESSION_MEMBER: {
		variable v = allocate_variable();

		opcode o;
		o.type = OPCODE_LOAD_MEMBER;
		o.size = OP_SIZE(o, op_load_member);
		o.op_load_member.to = v;
		emit_op(&o);

		return v;
	}
	case EXPRESSION_CONSTRUCTOR:
		error("not implemented", 0, 0);
	}

	assert(false);
	variable v;
	v.index = 0;
	return v;
}

void emit_statement(statement *statement) {
	switch (statement->type) {
	case STATEMENT_EXPRESSION:
		emit_expression(statement->expression);
		break;
	case STATEMENT_RETURN_EXPRESSION: {
		opcode o;
		o.type = OPCODE_RETURN;
		variable v = emit_expression(statement->expression);
		if (v.index == 0) {
			o.size = offsetof(opcode, op_return);
		}
		else {
			o.size = OP_SIZE(o, op_return);
			o.op_return.var = v;
		}
		emit_op(&o);
		break;
	}
	case STATEMENT_IF:
		error("not implemented", 0, 0);
		break;
	case STATEMENT_BLOCK:
		error("not implemented", 0, 0);
		break;
	case STATEMENT_LOCAL_VARIABLE: {
		opcode o;
		o.type = OPCODE_VAR;
		o.size = OP_SIZE(o, op_var);
		if (statement->local_variable.init != NULL) {
			emit_expression(statement->local_variable.init);
		}
		strcpy(o.op_var.name, statement->local_variable.name);
		emit_op(&o);
		break;
	}
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
