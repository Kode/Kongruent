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
			debug_context context = {0};
			check(b->vars.v[i].type.type != NO_TYPE, context, "Local variable does not have a type");
			variable var;
			var.index = b->vars.v[i].variable_id;
			var.type = b->vars.v[i].type;
			var.kind = VARIABLE_LOCAL;
			return var;
		}
	}

	return find_local_var(b->parent, name);
}

variable find_variable(block *parent, name_id name) {
	variable local_var = find_local_var(parent, name);
	if (local_var.index == 0) {
		allocated_global global = find_allocated_global(name);
		if (global.g.type != NO_TYPE && global.variable_id != 0) {
			variable v;
			init_type_ref(&v.type, NO_NAME);
			v.type.type = global.g.type;
			v.index = global.variable_id;
			return v;
		}
		else {
			debug_context context = {0};
			error(context, "Variable %s not found", get_name(name));

			variable v;
			v.index = 0;
			return v;
		}
	}
	else {
		return local_var;
	}
}

const char all_names[1024 * 1024];

static uint64_t next_variable_id = 1;

variable all_variables[1024 * 1024];

variable allocate_variable(type_ref type, variable_kind kind) {
	variable v;
	v.index = next_variable_id;
	v.type = type;
	v.kind = kind;
	all_variables[v.index] = v;
	++next_variable_id;
	return v;
}

void emit_op(opcodes *code, opcode *o) {
	assert(code->size + o->size < OPCODES_SIZE);
	memcpy(&code->o[code->size], o, o->size);
	code->size += o->size;
}

#define OP_SIZE(op, opmember) offsetof(opcode, opmember) + sizeof(o.opmember)

variable emit_expression(opcodes *code, block *parent, expression *e) {
	switch (e->kind) {
	case EXPRESSION_BINARY: {
		expression *left = e->binary.left;
		expression *right = e->binary.right;

		debug_context context = {0};

		switch (e->binary.op) {
		case OPERATOR_EQUALS:
		case OPERATOR_NOT_EQUALS:
		case OPERATOR_GREATER:
		case OPERATOR_GREATER_EQUAL:
		case OPERATOR_LESS:
		case OPERATOR_LESS_EQUAL:
		case OPERATOR_AND: 
		case OPERATOR_OR: {
			variable right_var = emit_expression(code, parent, right);
			variable left_var = emit_expression(code, parent, left);
			type_ref t;
			init_type_ref(&t, NO_NAME);
			t.type = bool_id;
			variable result_var = allocate_variable(t, VARIABLE_LOCAL);

			opcode o;
			switch (e->binary.op) {
			case OPERATOR_EQUALS:
				o.type = OPCODE_EQUALS;
				break;
			case OPERATOR_NOT_EQUALS:
				o.type = OPCODE_NOT_EQUALS;
				break;
			case OPERATOR_GREATER:
				o.type = OPCODE_GREATER;
				break;
			case OPERATOR_GREATER_EQUAL:
				o.type = OPCODE_GREATER_EQUAL;
				break;
			case OPERATOR_LESS:
				o.type = OPCODE_LESS;
				break;
			case OPERATOR_LESS_EQUAL:
				o.type = OPCODE_LESS_EQUAL;
				break;
			case OPERATOR_AND:
				o.type = OPCODE_AND;
				break;
			case OPERATOR_OR:
				o.type = OPCODE_OR;
				break;
			default: {
				debug_context context = {0};
				error(context, "Unexpected operator");
			}
			}
			o.size = OP_SIZE(o, op_binary);
			o.op_binary.right = right_var;
			o.op_binary.left = left_var;
			o.op_binary.result = result_var;
			emit_op(code, &o);

			return result_var;
		}
		case OPERATOR_MINUS:
		case OPERATOR_PLUS:
		case OPERATOR_DIVIDE:
		case OPERATOR_MULTIPLY: {
			variable right_var = emit_expression(code, parent, right);
			variable left_var = emit_expression(code, parent, left);
			variable result_var = allocate_variable(right_var.type, VARIABLE_LOCAL);

			opcode o;
			switch (e->binary.op) {
			case OPERATOR_MINUS:
				o.type = OPCODE_SUB;
				break;
			case OPERATOR_PLUS:
				o.type = OPCODE_ADD;
				break;
			case OPERATOR_DIVIDE:
				o.type = OPCODE_DIVIDE;
				break;
			case OPERATOR_MULTIPLY:
				o.type = OPCODE_MULTIPLY;
				break;
			default: {
				debug_context context = {0};
				error(context, "Unexpected operator");
			}
			}
			o.size = OP_SIZE(o, op_binary);
			o.op_binary.right = right_var;
			o.op_binary.left = left_var;
			o.op_binary.result = result_var;
			emit_op(code, &o);

			return result_var;
		}
		case OPERATOR_NOT: {
			debug_context context = {0};
			error(context, "! is not a binary operator");
		}
		case OPERATOR_MOD: {
			debug_context context = {0};
			error(context, "not implemented");
		}
		case OPERATOR_ASSIGN:
		case OPERATOR_MINUS_ASSIGN:
		case OPERATOR_PLUS_ASSIGN:
		case OPERATOR_DIVIDE_ASSIGN:
		case OPERATOR_MULTIPLY_ASSIGN: {
			variable v = emit_expression(code, parent, right);

			switch (left->kind) {
			case EXPRESSION_VARIABLE: {
				opcode o;
				switch (e->binary.op) {
				case OPERATOR_ASSIGN:
					o.type = OPCODE_STORE_VARIABLE;
					break;
				case OPERATOR_MINUS_ASSIGN:
					o.type = OPCODE_SUB_AND_STORE_VARIABLE;
					break;
				case OPERATOR_PLUS_ASSIGN:
					o.type = OPCODE_ADD_AND_STORE_VARIABLE;
					break;
				case OPERATOR_DIVIDE_ASSIGN:
					o.type = OPCODE_DIVIDE_AND_STORE_VARIABLE;
					break;
				case OPERATOR_MULTIPLY_ASSIGN:
					o.type = OPCODE_MULTIPLY_AND_STORE_VARIABLE;
					break;
				default: {
					debug_context context = {0};
					error(context, "Unexpected operator");
				}
				}
				o.size = OP_SIZE(o, op_store_var);
				o.op_store_var.from = v;
				o.op_store_var.to = find_variable(parent, left->variable);
				emit_op(code, &o);
				break;
			}
			case EXPRESSION_MEMBER: {
				variable member_var = emit_expression(code, parent, left->member.left);

				opcode o;
				switch (e->binary.op) {
				case OPERATOR_ASSIGN:
					o.type = OPCODE_STORE_MEMBER;
					break;
				case OPERATOR_MINUS_ASSIGN:
					o.type = OPCODE_SUB_AND_STORE_MEMBER;
					break;
				case OPERATOR_PLUS_ASSIGN:
					o.type = OPCODE_ADD_AND_STORE_MEMBER;
					break;
				case OPERATOR_DIVIDE_ASSIGN:
					o.type = OPCODE_DIVIDE_AND_STORE_MEMBER;
					break;
				case OPERATOR_MULTIPLY_ASSIGN:
					o.type = OPCODE_MULTIPLY_AND_STORE_MEMBER;
					break;
				default: {
					debug_context context = {0};
					error(context, "Unexpected operator");
				}
				}
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
					debug_context context = {0};
					check(right->type.type != NO_TYPE, context, "Part of the member does not have a type");

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
						check(found, context, "Variable for a member not found");
					}
					else if (right->member.left->kind == EXPRESSION_INDEX) {
						o.op_store_member.member_indices[o.op_store_member.member_indices_size] = (uint16_t)right->member.left->index;
						++o.op_store_member.member_indices_size;
					}
					else {
						debug_context context = {0};
						error(context, "Malformed member construct");
					}

					prev_struct = right->member.left->type.type;
					prev_s = get_type(prev_struct);
					right = right->member.right;
				}

				{
					debug_context context = {0};
					check(right->type.type != NO_TYPE, context, "Part of the member does not have a type");
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
						check(found, context, "Member not found");
					}
					else if (right->kind == EXPRESSION_INDEX) {
						o.op_store_member.member_indices[o.op_store_member.member_indices_size] = (uint16_t)right->index;
						++o.op_store_member.member_indices_size;
					}
					else {
						error(context, "Malformed member construct");
					}
				}

				emit_op(code, &o);
				break;
			}
			default: {
				debug_context context = {0};
				error(context, "Expected a variable or a member");
			}
			}

			return v;
		}
		}
		break;
	}
	case EXPRESSION_UNARY: {
		debug_context context = {0};
		switch (e->unary.op) {
		case OPERATOR_EQUALS:
			error(context, "not implemented");
		case OPERATOR_NOT_EQUALS:
			error(context, "not implemented");
		case OPERATOR_GREATER:
			error(context, "not implemented");
		case OPERATOR_GREATER_EQUAL:
			error(context, "not implemented");
		case OPERATOR_LESS:
			error(context, "not implemented");
		case OPERATOR_LESS_EQUAL:
			error(context, "not implemented");
		case OPERATOR_MINUS:
			error(context, "not implemented");
		case OPERATOR_PLUS:
			error(context, "not implemented");
		case OPERATOR_DIVIDE:
			error(context, "not implemented");
		case OPERATOR_MULTIPLY:
			error(context, "not implemented");
		case OPERATOR_NOT: {
			variable v = emit_expression(code, parent, e->unary.right);
			opcode o;
			o.type = OPCODE_NOT;
			o.size = OP_SIZE(o, op_not);
			o.op_not.from = v;
			o.op_not.to = allocate_variable(v.type, VARIABLE_LOCAL);
			emit_op(code, &o);
			return o.op_not.to;
		}
		case OPERATOR_OR:
			error(context, "not implemented");
		case OPERATOR_AND:
			error(context, "not implemented");
		case OPERATOR_MOD:
			error(context, "not implemented");
		case OPERATOR_ASSIGN:
			error(context, "not implemented");
		}
	}
	case EXPRESSION_BOOLEAN: {
		debug_context context = {0};
		error(context, "not implemented");
	}
	case EXPRESSION_NUMBER: {
		type_ref t;
		init_type_ref(&t, NO_NAME);
		t.type = float_id;
		variable v = allocate_variable(t, VARIABLE_LOCAL);

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
		return find_variable(parent, e->variable);
	}
	case EXPRESSION_GROUPING: {
		debug_context context = {0};
		error(context, "not implemented");
	}
	case EXPRESSION_CALL: {
		type_ref t;
		init_type_ref(&t, NO_NAME);
		t.type = float4_id;
		variable v = allocate_variable(t, VARIABLE_LOCAL);

		opcode o;
		o.type = OPCODE_CALL;
		o.size = OP_SIZE(o, op_call);
		o.op_call.func = e->call.func_name;
		o.op_call.var = v;

		debug_context context = {0};
		check(e->call.parameters.size <= sizeof(o.op_call.parameters) / sizeof(variable), context, "Call parameters missized");
		for (size_t i = 0; i < e->call.parameters.size; ++i) {
			o.op_call.parameters[i] = emit_expression(code, parent, e->call.parameters.e[i]);
		}
		o.op_call.parameters_size = (uint8_t)e->call.parameters.size;

		emit_op(code, &o);

		return v;
	}
	case EXPRESSION_MEMBER: {
		variable v = allocate_variable(e->type, VARIABLE_LOCAL);

		opcode o;
		o.type = OPCODE_LOAD_MEMBER;
		o.size = OP_SIZE(o, op_load_member);

		debug_context context = {0};
		check(e->member.left->kind == EXPRESSION_VARIABLE, context, "Misformed member construct");

		o.op_load_member.from = find_variable(parent, e->member.left->variable);

		check(o.op_load_member.from.index != 0, context, "Load var is broken");
		o.op_load_member.to = v;

		o.op_load_member.member_indices_size = 0;
		expression *right = e->member.right;
		type_id prev_struct = e->member.left->type.type;
		type *prev_s = get_type(prev_struct);
		o.op_load_member.member_parent_type = prev_struct;

		while (right->kind == EXPRESSION_MEMBER) {
			check(right->type.type != NO_TYPE, context, "Malformed member construct");
			check(right->member.left->kind == EXPRESSION_VARIABLE, context, "Malformed member construct");

			bool found = false;
			for (size_t i = 0; i < prev_s->members.size; ++i) {
				if (prev_s->members.m[i].name == right->member.left->variable) {
					o.op_load_member.member_indices[o.op_load_member.member_indices_size] = (uint16_t)i;
					++o.op_load_member.member_indices_size;
					found = true;
					break;
				}
			}
			check(found, context, "Member not found");

			prev_struct = right->member.left->type.type;
			prev_s = get_type(prev_struct);
			right = right->member.right;
		}

		{
			check(right->type.type != NO_TYPE, context, "Malformed member construct");
			check(right->kind == EXPRESSION_VARIABLE, context, "Malformed member construct");

			bool found = false;
			for (size_t i = 0; i < prev_s->members.size; ++i) {
				if (prev_s->members.m[i].name == right->variable) {
					o.op_load_member.member_indices[o.op_load_member.member_indices_size] = (uint16_t)i;
					++o.op_load_member.member_indices_size;
					found = true;
					break;
				}
			}
			check(found, context, "Member not found");
		}

		emit_op(code, &o);

		return v;
	}
	case EXPRESSION_CONSTRUCTOR: {
		debug_context context = {0};
		error(context, "not implemented");
	}
	}

	{
		debug_context context = {0};
		error(context, "Supposedly unreachable code reached");
		variable v;
		v.index = 0;
		return v;
	}
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
	case STATEMENT_IF: {
		variable previous_conditions[64];
		uint8_t previous_conditions_size = 0;

		{
			opcode o;
			o.type = OPCODE_IF;
			o.size = OP_SIZE(o, op_if);

			variable initial_condition = emit_expression(code, parent, statement->iffy.test);

			o.op_if.condition = initial_condition;
			o.op_if.exclusions_size = 0;

			emit_op(code, &o);

			previous_conditions[previous_conditions_size] = initial_condition;
			previous_conditions_size += 1;
		}

		emit_statement(code, parent, statement->iffy.if_block);

		for (uint16_t i = 0; i < statement->iffy.else_size; ++i) {
			opcode o;
			o.type = OPCODE_IF;
			o.size = OP_SIZE(o, op_if) + sizeof(variable) * i;

			for (uint8_t previous_condition_index = 0; previous_condition_index < previous_conditions_size; ++previous_condition_index) {
				o.op_if.exclusions[previous_condition_index] = previous_conditions[previous_condition_index];
			}
			o.op_if.exclusions_size = previous_conditions_size;

			if (statement->iffy.else_tests[i] != NULL) {
				variable v = emit_expression(code, parent, statement->iffy.else_tests[i]);
				o.op_if.condition = v;

				previous_conditions[previous_conditions_size] = v;
				previous_conditions_size += 1;
			}
			else {
				o.op_if.condition.index = 0;
				o.op_if.condition.kind = VARIABLE_GLOBAL;
				o.op_if.condition.type.name = NO_NAME;
				o.op_if.condition.type.type = NO_TYPE;
			}

			emit_op(code, &o);

			emit_statement(code, parent, statement->iffy.else_blocks[i]);
		}
		
		break;
	}
	case STATEMENT_WHILE: {
		{
			opcode o;
			o.type = OPCODE_WHILE_START;
			o.size = OP_SIZE(o, op_nothing);
			emit_op(code, &o);
		}

		{
			opcode o;
			o.type = OPCODE_WHILE_CONDITION;
			o.size = OP_SIZE(o, op_while);

			variable v = emit_expression(code, parent, statement->whiley.test);

			o.op_while.condition = v;

			emit_op(code, &o);
		}

		emit_statement(code, parent, statement->whiley.while_block);

		{
			opcode o;
			o.type = OPCODE_WHILE_END;
			o.size = OP_SIZE(o, op_nothing);
			emit_op(code, &o);
		}

		break;
	}
	case STATEMENT_DO_WHILE: {
		{
			opcode o;
			o.type = OPCODE_WHILE_START;
			o.size = OP_SIZE(o, op_nothing);
			emit_op(code, &o);
		}

		emit_statement(code, parent, statement->whiley.while_block);

		{
			opcode o;
			o.type = OPCODE_WHILE_CONDITION;
			o.size = OP_SIZE(o, op_while);

			variable v = emit_expression(code, parent, statement->whiley.test);

			o.op_while.condition = v;

			emit_op(code, &o);
		}

		{
			opcode o;
			o.type = OPCODE_WHILE_END;
			o.size = OP_SIZE(o, op_nothing);
			emit_op(code, &o);
		}

		break;
	}
	case STATEMENT_BLOCK: {
		for (size_t i = 0; i < statement->block.vars.size; ++i) {
			variable var = allocate_variable(statement->block.vars.v[i].type, VARIABLE_LOCAL);
			statement->block.vars.v[i].variable_id = var.index;
		}

		{
			opcode o;
			o.type = OPCODE_BLOCK_START;
			o.size = OP_SIZE(o, op_nothing);
			emit_op(code, &o);
		}

		for (size_t i = 0; i < statement->block.statements.size; ++i) {
			emit_statement(code, &statement->block, statement->block.statements.s[i]);
		}

		{
			opcode o;
			o.type = OPCODE_BLOCK_END;
			o.size = OP_SIZE(o, op_nothing);
			emit_op(code, &o);
		}

		break;
	}
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
		debug_context context = {0};
		check(statement->local_variable.var.type.type != NO_TYPE, context, "Local var has no type");
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
		variable v = allocate_variable(t, VARIABLE_GLOBAL);
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
		debug_context context = {0};
		error(context, "Expected a block");
	}
	for (size_t i = 0; i < block->block.vars.size; ++i) {
		variable var = allocate_variable(block->block.vars.v[i].type, VARIABLE_LOCAL);
		block->block.vars.v[i].variable_id = var.index;
	}
	for (size_t i = 0; i < block->block.statements.size; ++i) {
		emit_statement(code, &block->block, block->block.statements.s[i]);
	}
}
