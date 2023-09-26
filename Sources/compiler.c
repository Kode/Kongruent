#include "compiler.h"

#include "errors.h"
#include "parser.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

typedef struct allocated_global {
	global g;
	uint64_t variable_id;
} allocated_global;

static allocated_global allocated_globals[1024];
static size_t allocated_globals_size = 0;

allocated_global find_allocated_global(name_id name) {
	for (size_t i = 0; i < allocated_globals_size; ++i) {
		if (name == allocated_globals[i].g.name) {
			return allocated_globals[i];
		}
	}

	allocated_global a;
	a.g.type = NO_TYPE;
	a.g.name = NO_NAME;
	a.variable_id = 0;
	return a;
}

variable find_local_var(block *b, name_id name) {
	if (b == NULL) {
		variable var;
		var.index = 0;
		init_type_ref(&var.type, NO_NAME);
		return var;
	}

	for (size_t i = 0; i < b->vars.size; ++i) {
		if (b->vars.v[i].name == name) {
			assert(b->vars.v[i].type.type != NO_TYPE);
			variable var;
			var.index = b->vars.v[i].variable_id;
			var.type = b->vars.v[i].type;
			return var;
		}
	}

	return find_local_var(b->parent, name);
}

const char all_names[1024 * 1024];

static uint64_t next_variable_id = 1;

variable all_variables[1024 * 1024];

variable allocate_variable(type_ref type) {
	variable v;
	v.index = next_variable_id;
	v.type = type;
	all_variables[v.index] = v;
	++next_variable_id;
	return v;
}

void emit_op(opcodes *code, opcode *o) {
	memcpy(&code->o[code->size], o, o->size);
	code->size += o->size;
}

#define OP_SIZE(op, opmember) offsetof(opcode, opmember) + sizeof(o.opmember)

variable emit_expression(opcodes *code, block *parent, expression *e) {
	switch (e->kind) {
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
		case OPERATOR_PLUS: {
			variable right_var = emit_expression(code, parent, right);
			variable left_var = emit_expression(code, parent, left);
			variable result_var = allocate_variable(right_var.type);

			opcode o;
			o.type = OPCODE_ADD;
			o.size = OP_SIZE(o, op_add);
			o.op_add.right = right_var;
			o.op_add.left = left_var;
			o.op_add.result = result_var;
			emit_op(code, &o);

			return result_var;
		}
		case OPERATOR_DIVIDE:
			error("not implemented", 0, 0);
		case OPERATOR_MULTIPLY: {
			variable right_var = emit_expression(code, parent, right);
			variable left_var = emit_expression(code, parent, left);
			variable result_var = allocate_variable(right_var.type);

			opcode o;
			o.type = OPCODE_MULTIPLY;
			o.size = OP_SIZE(o, op_multiply);
			o.op_multiply.right = right_var;
			o.op_multiply.left = left_var;
			o.op_multiply.result = result_var;
			emit_op(code, &o);

			return result_var;
		}
		case OPERATOR_NOT:
			error("not implemented", 0, 0);
		case OPERATOR_OR:
			error("not implemented", 0, 0);
		case OPERATOR_AND:
			error("not implemented", 0, 0);
		case OPERATOR_MOD:
			error("not implemented", 0, 0);
		case OPERATOR_ASSIGN: {
			variable v = emit_expression(code, parent, right);

			switch (left->kind) {
			case EXPRESSION_VARIABLE: {
				opcode o;
				o.type = OPCODE_STORE_VARIABLE;
				o.size = OP_SIZE(o, op_store_var);
				o.op_store_var.from = v;
				// o.op_store_var.to = left->variable;
				emit_op(code, &o);
				break;
			}
			case EXPRESSION_MEMBER: {
				variable member_var = emit_expression(code, parent, left->member.left);

				opcode o;
				o.type = OPCODE_STORE_MEMBER;
				o.size = OP_SIZE(o, op_store_member);
				o.op_store_member.from = v;
				o.op_store_member.to = member_var;
				// o.op_store_member.member = left->member.right;

				o.op_store_member.member_indices_size = 0;
				expression *right = left->member.right;
				type_id prev_struct = left->member.left->type.type;
				type *prev_s = get_type(prev_struct);
				o.op_store_member.member_parent_type = prev_struct;
				o.op_store_member.member_parent_array = left->member.left->type.array_size > 0;

				while (right->kind == EXPRESSION_MEMBER) {
					assert(right->type.type != NO_TYPE);

					if (right->member.left->kind == EXPRESSION_VARIABLE) {
						bool found = false;
						for (size_t i = 0; i < prev_s->members.size; ++i) {
							if (prev_s->members.m[i].name == right->member.left->variable) {
								o.op_store_member.member_indices[o.op_store_member.member_indices_size] = (uint16_t)i;
								++o.op_store_member.member_indices_size;
								found = true;
								break;
							}
						}
						assert(found);
					}
					else if (right->member.left->kind == EXPRESSION_INDEX) {
						o.op_store_member.member_indices[o.op_store_member.member_indices_size] = (uint16_t)right->member.left->index;
						++o.op_store_member.member_indices_size;
					}
					else {
						assert(false);
					}

					prev_struct = right->member.left->type.type;
					prev_s = get_type(prev_struct);
					right = right->member.right;
				}

				{
					assert(right->type.type != NO_TYPE);
					if (right->kind == EXPRESSION_VARIABLE) {
						bool found = false;
						for (size_t i = 0; i < prev_s->members.size; ++i) {
							if (prev_s->members.m[i].name == right->variable) {
								o.op_store_member.member_indices[o.op_store_member.member_indices_size] = (uint16_t)i;
								++o.op_store_member.member_indices_size;
								found = true;
								break;
							}
						}
						assert(found);
					}
					else if (right->kind == EXPRESSION_INDEX) {
						o.op_store_member.member_indices[o.op_store_member.member_indices_size] = (uint16_t)right->index;
						++o.op_store_member.member_indices_size;
					}
					else {
						assert(false);
					}
				}

				emit_op(code, &o);
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
			variable v = emit_expression(code, parent, e->unary.right);
			opcode o;
			o.type = OPCODE_NOT;
			o.size = OP_SIZE(o, op_not);
			o.op_not.from = v;
			o.op_not.to = allocate_variable(v.type);
			emit_op(code, &o);
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
		type_ref t;
		init_type_ref(&t, NO_NAME);
		t.type = float_id;
		variable v = allocate_variable(t);

		opcode o;
		o.type = OPCODE_LOAD_CONSTANT;
		o.size = OP_SIZE(o, op_load_constant);
		o.op_load_constant.number = (float)e->number;
		o.op_load_constant.to = v;
		emit_op(code, &o);

		return v;
	}
	// case EXPRESSION_STRING:
	//	error("not implemented", 0, 0);
	case EXPRESSION_VARIABLE: {
		allocated_global g = find_allocated_global(e->variable);
		if (g.g.type != NO_TYPE) {
			assert(g.variable_id != 0);

			variable var;
			init_type_ref(&var.type, NO_NAME);
			var.type.type = g.g.type;
			var.index = g.variable_id;
			return var;
		}
		else {
			variable var = find_local_var(parent, e->variable);
			assert(var.index != 0);
			return var;
		}
	}
	case EXPRESSION_GROUPING:
		error("not implemented", 0, 0);
	case EXPRESSION_CALL: {
		type_ref t;
		init_type_ref(&t, NO_NAME);
		t.type = float4_id;
		variable v = allocate_variable(t);

		opcode o;
		o.type = OPCODE_CALL;
		o.size = OP_SIZE(o, op_call);
		o.op_call.func = e->call.func_name;
		o.op_call.var = v;

		assert(e->call.parameters.size <= sizeof(o.op_call.parameters) / sizeof(variable));
		for (size_t i = 0; i < e->call.parameters.size; ++i) {
			o.op_call.parameters[i] = emit_expression(code, parent, e->call.parameters.e[i]);
		}
		o.op_call.parameters_size = (uint8_t)e->call.parameters.size;

		emit_op(code, &o);

		return v;
	}
	case EXPRESSION_MEMBER: {
		variable v = allocate_variable(e->type);

		opcode o;
		o.type = OPCODE_LOAD_MEMBER;
		o.size = OP_SIZE(o, op_load_member);
		assert(e->member.left->kind == EXPRESSION_VARIABLE);
		o.op_load_member.from = find_local_var(parent, e->member.left->variable);
		if (o.op_load_member.from.index == 0) {
			allocated_global global = find_allocated_global(e->member.left->variable);
			if (global.g.type != NO_TYPE) {
				variable v;
				v.index = global.variable_id;
				init_type_ref(&v.type, NO_NAME);
				v.type.type = global.g.type;
				o.op_load_member.from = v;
			}
		}
		assert(o.op_load_member.from.index != 0);
		o.op_load_member.to = v;

		o.op_load_member.member_indices_size = 0;
		expression *right = e->member.right;
		type_id prev_struct = e->member.left->type.type;
		type *prev_s = get_type(prev_struct);
		o.op_load_member.member_parent_type = prev_struct;

		while (right->kind == EXPRESSION_MEMBER) {
			assert(right->type.type != NO_TYPE);
			assert(right->member.left->kind == EXPRESSION_VARIABLE);

			bool found = false;
			for (size_t i = 0; i < prev_s->members.size; ++i) {
				if (prev_s->members.m[i].name == right->member.left->variable) {
					o.op_load_member.member_indices[o.op_load_member.member_indices_size] = (uint16_t)i;
					++o.op_load_member.member_indices_size;
					found = true;
					break;
				}
			}
			assert(found);

			prev_struct = right->member.left->type.type;
			prev_s = get_type(prev_struct);
			right = right->member.right;
		}

		{
			assert(right->type.type != NO_TYPE);
			assert(right->kind == EXPRESSION_VARIABLE);

			bool found = false;
			for (size_t i = 0; i < prev_s->members.size; ++i) {
				if (prev_s->members.m[i].name == right->variable) {
					o.op_load_member.member_indices[o.op_load_member.member_indices_size] = (uint16_t)i;
					++o.op_load_member.member_indices_size;
					found = true;
					break;
				}
			}
			assert(found);
		}

		emit_op(code, &o);

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

void emit_statement(opcodes *code, block *parent, statement *statement) {
	switch (statement->kind) {
	case STATEMENT_EXPRESSION:
		emit_expression(code, parent, statement->expression);
		break;
	case STATEMENT_RETURN_EXPRESSION: {
		opcode o;
		o.type = OPCODE_RETURN;
		variable v = emit_expression(code, parent, statement->expression);
		if (v.index == 0) {
			o.size = offsetof(opcode, op_return);
		}
		else {
			o.size = OP_SIZE(o, op_return);
			o.op_return.var = v;
		}
		emit_op(code, &o);
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

		variable init_var;
		if (statement->local_variable.init != NULL) {
			init_var = emit_expression(code, parent, statement->local_variable.init);
		}

		variable local_var = find_local_var(parent, statement->local_variable.var.name);
		statement->local_variable.var.variable_id = local_var.index;
		o.op_var.var.index = statement->local_variable.var.variable_id;
		assert(statement->local_variable.var.type.type != NO_TYPE);
		o.op_var.var.type = statement->local_variable.var.type;
		emit_op(code, &o);

		if (statement->local_variable.init != NULL) {
			opcode o;
			o.type = OPCODE_STORE_VARIABLE;
			o.size = OP_SIZE(o, op_store_var);

			o.op_store_var.from = init_var;
			o.op_store_var.to = local_var;

			emit_op(code, &o);
		}

		break;
	}
	}
}

void convert_globals(void) {
	for (global_id i = 0; get_global(i).type != NO_TYPE; ++i) {
		global g = get_global(i);

		type_ref t;
		init_type_ref(&t, NO_NAME);
		t.type = g.type;
		variable v = allocate_variable(t);
		allocated_globals[allocated_globals_size].g = g;
		allocated_globals[allocated_globals_size].variable_id = v.index;
		allocated_globals_size += 1;

		assign_global_var(i, v.index);
	}
}

void convert_function_block(opcodes *code, struct statement *block) {
	if (block == NULL) {
		// built-in
		return;
	}

	if (block->kind != STATEMENT_BLOCK) {
		error("Expected a block", 0, 0);
	}
	for (size_t i = 0; i < block->block.vars.size; ++i) {
		variable var = allocate_variable(block->block.vars.v[i].type);
		block->block.vars.v[i].variable_id = var.index;
	}
	for (size_t i = 0; i < block->block.statements.size; ++i) {
		emit_statement(code, &block->block, block->block.statements.s[i]);
	}
}
