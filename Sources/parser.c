#include "parser.h"
#include "errors.h"
#include "functions.h"
#include "tokenizer.h"
#include "types.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static statement *statement_allocate(void) {
	statement *s = (statement *)malloc(sizeof(statement));
	debug_context context = {0};
	check(s != NULL, context, "Could not allocate statement");
	return s;
}

static void statement_free(statement *statement) {
	free(statement);
}

static void statements_init(statements *statements) {
	statements->size = 0;
}

static void statements_add(statements *statements, statement *statement) {
	statements->s[statements->size] = statement;
	statements->size += 1;
}

static expression *expression_allocate(void) {
	expression *e = (expression *)malloc(sizeof(expression));
	debug_context context = {0};
	check(e != NULL, context, "Could not allocate expression");
	init_type_ref(&e->type, NO_NAME);
	return e;
}

static void expression_free(expression *expression) {
	free(expression);
}

typedef struct state {
	tokens *tokens;
	size_t index;
	debug_context context;
} state_t;

static token current(state_t *state) {
	token token = tokens_get(state->tokens, state->index);
	return token;
}

static void update_debug_context(state_t *state) {
	state->context.column = current(state).column;
	state->context.line = current(state).line;
}

static void advance_state(state_t *state) {
	state->index += 1;
	update_debug_context(state);
}

static void match_token(state_t *state, int token, const char *error_message) {
	if (current(state).kind != token) {
		error(state->context, error_message);
	}
}

static void match_token_identifier(state_t *state) {
	if (current(state).kind != TOKEN_IDENTIFIER) {
		error(state->context, "Expected an identifier");
	}
}

static definition parse_definition(state_t *state);
static statement *parse_statement(state_t *state, block *parent_block);
static expression *parse_expression(state_t *state);

void parse(const char *filename, tokens *tokens) {
	state_t state = {0};
	state.context.filename = filename;
	state.tokens = tokens;
	state.index = 0;

	for (;;) {
		token token = current(&state);
		if (token.kind == TOKEN_NONE) {
			return;
		}
		else {
			parse_definition(&state);
		}
	}
}

static statement *parse_block(state_t *state, block *parent_block) {
	match_token(state, TOKEN_LEFT_CURLY, "Expected an opening curly bracket");
	advance_state(state);

	statements statements;
	statements_init(&statements);

	statement *new_block = statement_allocate();
	new_block->kind = STATEMENT_BLOCK;
	new_block->block.parent = parent_block;
	new_block->block.vars.size = 0;

	for (;;) {
		switch (current(state).kind) {
		case TOKEN_RIGHT_CURLY: {
			advance_state(state);
			new_block->block.statements = statements;
			return new_block;
		}
		case TOKEN_NONE: {
			update_debug_context(state);
			error(state->context, "File ended before a block ended");
			return NULL;
		}
		default:
			statements_add(&statements, parse_statement(state, &new_block->block));
			break;
		}
	}
}

typedef enum modifier {
	MODIFIER_IN,
	// Out,
} modifier_t;

typedef struct modifiers {
	modifier_t m[16];
	size_t size;
} modifiers_t;

static void modifiers_init(modifiers_t *modifiers) {
	modifiers->size = 0;
}

static void modifiers_add(modifiers_t *modifiers, modifier_t modifier) {
	modifiers->m[modifiers->size] = modifier;
	modifiers->size += 1;
}

/*static statement_t *parse_declaration(state_t *state, modifiers_t modifiers) {
    switch (current(state).type) {
    case TOKEN_IN: {
        advance_state(state);
        modifiers_add(&modifiers, MODIFIER_IN);
        return parse_declaration(state, modifiers);
    }
    case TOKEN_IDENTIFIER: {
        // type name
        advance_state(state);
        break;
    }
    case TOKEN_VOID: {
        advance_state(state);
        break;
    }
    default:
        error("Expected a variable declaration");
    }

    token_t identifier;
    if (current(state).type == TOKEN_IDENTIFIER) {
        identifier = current(state);
        advance_state(state);
    }
    else {
        error("Expected an identifier");
    }

    switch (current(state).type) {
    case TOKEN_OPERATOR: {
        operator_t op = current(state).op;
        switch (op) {
        case OPERATOR_ASSIGN: {
            advance_state(state);
            expression_t *expr = parse_expression(state);
            switch (current(state).type) {
            case TOKEN_SEMICOLON: {
                advance_state(state);
                statement_t *statement = statement_allocate();
                statement->type = STATEMENT_DECLARATION;
                strcpy(statement->declaration.name, identifier.identifier);
                statement->declaration.init = expr;
                return statement;
            }
            default:
                error("Expected a semicolon");
                return NULL;
            }
        }
        default:
            error("Expected an assignment operator");
            return NULL;
        };
    }
    case TOKEN_SEMICOLON: {
        advance_state(state);
        statement_t *statement = statement_allocate();
        statement->type = STATEMENT_DECLARATION;
        strcpy(statement->declaration.name, identifier.identifier);
        statement->declaration.init = NULL;
        return statement;
    }
    case TOKEN_LEFT_PAREN: {
        advance_state(state);
        switch (current(state).type) {
        case TOKEN_RIGHT_PAREN: {
            advance_state(state);
            statement_t *statement = statement_allocate();
            statement->type = STATEMENT_FUNCTION;
            statement->function.parameters.size = 0;
            statement->function.block = parse_block(state);
            return statement;
        }
        default:
            error("Expected right paren");
            return NULL;
        }
    }
    default:
        error("Expected an assign or a semicolon");
        return NULL;
    }
}*/

static definition parse_struct(state_t *state);
static definition parse_function(state_t *state);
static definition parse_const(state_t *state);

static definition parse_definition(state_t *state) {
	name_id attribute = NO_NAME;

	if (current(state).kind == TOKEN_ATTRIBUTE) {
		token token = current(state);
		attribute = token.attribute;
		advance_state(state);
	}

	switch (current(state).kind) {
	case TOKEN_STRUCT: {
		definition structy = parse_struct(state);
		get_type(structy.type)->attribute = attribute;
		return structy;
	}
	case TOKEN_FUNCTION: {
		definition d = parse_function(state);
		function *f = get_function(d.function);
		f->attribute = attribute;
		return d;
	}
	case TOKEN_CONST: {
		definition d = parse_const(state);
		return d;
	}
	default: {
		update_debug_context(state);
		error(state->context, "Expected a struct, a function or a const");

		definition d = {0};
		return d;
	}
	}
}

static type_ref parse_type_ref(state_t *state) {
	match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");
	token type_name = current(state);
	advance_state(state);

	uint32_t array_size = 0;
	if (current(state).kind == TOKEN_LEFT_SQUARE) {
		advance_state(state);
		match_token(state, TOKEN_NUMBER, "Expected a number");
		array_size = (uint32_t)current(state).number;
		advance_state(state);
		match_token(state, TOKEN_RIGHT_SQUARE, "Expected a closing square bracket");
		advance_state(state);
	}

	type_ref t;
	init_type_ref(&t, type_name.identifier);
	t.array_size = array_size;
	return t;
}

static statement *parse_statement(state_t *state, block *parent_block) {
	switch (current(state).kind) {
	case TOKEN_IF: {
		advance_state(state);
		match_token(state, TOKEN_LEFT_PAREN, "Expected an opening bracket");
		advance_state(state);

		expression *test = parse_expression(state);
		match_token(state, TOKEN_RIGHT_PAREN, "Expected a closing bracket");
		advance_state(state);

		statement *if_block = parse_statement(state, parent_block);
		statement *s = statement_allocate();
		s->kind = STATEMENT_IF;
		s->iffy.test = test;
		s->iffy.if_block = if_block;

		s->iffy.else_size = 0;

		while (current(state).kind == TOKEN_ELSE) {
			advance_state(state);

			if (current(state).kind == TOKEN_IF) {
				advance_state(state);
				match_token(state, TOKEN_LEFT_PAREN, "Expected an opening bracket");
				advance_state(state);

				expression *test = parse_expression(state);
				match_token(state, TOKEN_RIGHT_PAREN, "Expected a closing bracket");
				advance_state(state);

				statement *if_block = parse_statement(state, parent_block);

				s->iffy.else_tests[s->iffy.else_size] = test;
				s->iffy.else_blocks[s->iffy.else_size] = if_block;
			}
			else {
				statement *else_block = parse_statement(state, parent_block);
				s->iffy.else_tests[s->iffy.else_size] = NULL;
				s->iffy.else_blocks[s->iffy.else_size] = else_block;
			}

			s->iffy.else_size += 1;
			assert(s->iffy.else_size < 64);
		}

		return s;
	}
	case TOKEN_WHILE: {
		advance_state(state);
		match_token(state, TOKEN_LEFT_PAREN, "Expected an opening bracket");
		advance_state(state);

		expression *test = parse_expression(state);
		match_token(state, TOKEN_RIGHT_PAREN, "Expected a closing bracket");
		advance_state(state);

		statement *while_block = parse_statement(state, parent_block);
		
		statement *s = statement_allocate();
		s->kind = STATEMENT_WHILE;
		s->whiley.test = test;
		s->whiley.while_block = while_block;

		return s;
	}
	case TOKEN_DO: {
		advance_state(state);
		
		statement *do_block = parse_statement(state, parent_block);
		
		statement *s = statement_allocate();
		s->kind = STATEMENT_DO_WHILE;
		s->whiley.while_block = do_block;

		match_token(state, TOKEN_WHILE, "Expected \"while\"");
		advance_state(state);
		match_token(state, TOKEN_LEFT_PAREN, "Expected an opening bracket");
		advance_state(state);

		expression *test = parse_expression(state);
		match_token(state, TOKEN_RIGHT_PAREN, "Expected a closing bracket");
		advance_state(state);

		s->whiley.test = test;

		match_token(state, TOKEN_SEMICOLON, "Expected a semicolon");
		advance_state(state);

		return s;
	}
	case TOKEN_FOR: {
		statements outer_block_statements;
		statements_init(&outer_block_statements);

		statement *outer_block = statement_allocate();
		outer_block->kind = STATEMENT_BLOCK;
		outer_block->block.parent = parent_block;
		outer_block->block.vars.size = 0;
		outer_block->block.statements = outer_block_statements;

		advance_state(state);

		match_token(state, TOKEN_LEFT_PAREN, "Expected an opening bracket");
		advance_state(state);

		statement *pre = parse_statement(state, &outer_block->block);
		statements_add(&outer_block->block.statements, pre);

		expression *test = parse_expression(state);
		match_token(state, TOKEN_SEMICOLON, "Expected a semicolon");
		advance_state(state);

		expression *post_expression = parse_expression(state);

		match_token(state, TOKEN_RIGHT_PAREN, "Expected a closing bracket");
		advance_state(state);

		statement *inner_block = parse_statement(state, &outer_block->block);

		statement *post_statement = statement_allocate();
		post_statement->kind = STATEMENT_EXPRESSION;
		post_statement->expression = post_expression;

		statements_add(&inner_block->block.statements, post_statement);

		statement *s = statement_allocate();
		s->kind = STATEMENT_WHILE;
		s->whiley.test = test;
		s->whiley.while_block = inner_block;

		statements_add(&outer_block->block.statements, s);

		return outer_block;
	}
	case TOKEN_LEFT_CURLY: {
		return parse_block(state, parent_block);
	}
	case TOKEN_VAR: {
		advance_state(state);

		match_token_identifier(state);
		token name = current(state);
		advance_state(state);

		match_token(state, TOKEN_COLON, "Expected a colon");
		advance_state(state);

		type_ref type = parse_type_ref(state);

		expression *init = NULL;

		if (current(state).kind == TOKEN_OPERATOR) {
			check(current(state).op == OPERATOR_ASSIGN, state->context, "Expected an assign");
			advance_state(state);
			init = parse_expression(state);
		}

		match_token(state, TOKEN_SEMICOLON, "Expected a semicolon");
		advance_state(state);

		statement *statement = statement_allocate();
		statement->kind = STATEMENT_LOCAL_VARIABLE;
		statement->local_variable.var.name = name.identifier;
		statement->local_variable.var.type = type;
		statement->local_variable.var.variable_id = 0;
		statement->local_variable.init = init;
		return statement;
	}
	case TOKEN_RETURN: {
		advance_state(state);

		expression *expr = parse_expression(state);
		match_token(state, TOKEN_SEMICOLON, "Expected a semicolon");
		advance_state(state);

		statement *statement = statement_allocate();
		statement->kind = STATEMENT_RETURN_EXPRESSION;
		statement->expression = expr;
		return statement;
	}
	default: {
		expression *expr = parse_expression(state);
		match_token(state, TOKEN_SEMICOLON, "Expected a semicolon");
		advance_state(state);

		statement *statement = statement_allocate();
		statement->kind = STATEMENT_EXPRESSION;
		statement->expression = expr;
		return statement;
	}
	}
}

static expression *parse_assign(state_t *state);

static expression *parse_expression(state_t *state) {
	return parse_assign(state);
}

static expression *parse_logical(state_t *state);

static expression *parse_assign(state_t *state) {
	expression *expr = parse_logical(state);
	bool done = false;
	while (!done) {
		if (current(state).kind == TOKEN_OPERATOR) {
			operatorr op = current(state).op;
			if (op == OPERATOR_ASSIGN || op == OPERATOR_MINUS_ASSIGN || op == OPERATOR_PLUS_ASSIGN || op == OPERATOR_DIVIDE_ASSIGN || op == OPERATOR_MULTIPLY_ASSIGN) {
				advance_state(state);
				expression *right = parse_logical(state);
				expression *expression = expression_allocate();
				expression->kind = EXPRESSION_BINARY;
				expression->binary.left = expr;
				expression->binary.op = op;
				expression->binary.right = right;
				expr = expression;
			}
			else {
				done = true;
			}
		}
		else {
			done = true;
		}
	}
	return expr;
}

static expression *parse_equality(state_t *state);

static expression *parse_logical(state_t *state) {
	expression *expr = parse_equality(state);
	bool done = false;
	while (!done) {
		if (current(state).kind == TOKEN_OPERATOR) {
			operatorr op = current(state).op;
			if (op == OPERATOR_OR || op == OPERATOR_AND) {
				advance_state(state);
				expression *right = parse_equality(state);
				expression *expression = expression_allocate();
				expression->kind = EXPRESSION_BINARY;
				expression->binary.left = expr;
				expression->binary.op = op;
				expression->binary.right = right;
				expr = expression;
			}
			else {
				done = true;
			}
		}
		else {
			done = true;
		}
	}
	return expr;
}

static expression *parse_comparison(state_t *state);

static expression *parse_equality(state_t *state) {
	expression *expr = parse_comparison(state);
	bool done = false;
	while (!done) {
		if (current(state).kind == TOKEN_OPERATOR) {
			operatorr op = current(state).op;
			if (op == OPERATOR_EQUALS || op == OPERATOR_NOT_EQUALS) {
				advance_state(state);
				expression *right = parse_comparison(state);
				expression *expression = expression_allocate();
				expression->kind = EXPRESSION_BINARY;
				expression->binary.left = expr;
				expression->binary.op = op;
				expression->binary.right = right;
				expr = expression;
			}
			else {
				done = true;
			}
		}
		else {
			done = true;
		}
	}
	return expr;
}

static expression *parse_addition(state_t *state);

static expression *parse_comparison(state_t *state) {
	expression *expr = parse_addition(state);
	bool done = false;
	while (!done) {
		if (current(state).kind == TOKEN_OPERATOR) {
			operatorr op = current(state).op;
			if (op == OPERATOR_GREATER || op == OPERATOR_GREATER_EQUAL || op == OPERATOR_LESS || op == OPERATOR_LESS_EQUAL) {
				advance_state(state);
				expression *right = parse_addition(state);
				expression *expression = expression_allocate();
				expression->kind = EXPRESSION_BINARY;
				expression->binary.left = expr;
				expression->binary.op = op;
				expression->binary.right = right;
				expr = expression;
			}
			else {
				done = true;
			}
		}
		else {
			done = true;
		}
	}
	return expr;
}

static expression *parse_multiplication(state_t *state);

static expression *parse_addition(state_t *state) {
	expression *expr = parse_multiplication(state);
	bool done = false;
	while (!done) {
		if (current(state).kind == TOKEN_OPERATOR) {
			operatorr op = current(state).op;
			if (op == OPERATOR_MINUS || op == OPERATOR_PLUS) {
				advance_state(state);
				expression *right = parse_multiplication(state);
				expression *expression = expression_allocate();
				expression->kind = EXPRESSION_BINARY;
				expression->binary.left = expr;
				expression->binary.op = op;
				expression->binary.right = right;
				expr = expression;
			}
			else {
				done = true;
			}
		}
		else {
			done = true;
		}
	}
	return expr;
}

static expression *parse_unary(state_t *state);

static expression *parse_multiplication(state_t *state) {
	expression *expr = parse_unary(state);
	bool done = false;
	while (!done) {
		if (current(state).kind == TOKEN_OPERATOR) {
			operatorr op = current(state).op;
			if (op == OPERATOR_DIVIDE || op == OPERATOR_MULTIPLY || op == OPERATOR_MOD) {
				advance_state(state);
				expression *right = parse_unary(state);
				expression *expression = expression_allocate();
				expression->kind = EXPRESSION_BINARY;
				expression->binary.left = expr;
				expression->binary.op = op;
				expression->binary.right = right;
				expr = expression;
			}
			else {
				done = true;
			}
		}
		else {
			done = true;
		}
	}
	return expr;
}

static expression *parse_primary(state_t *state);

static expression *parse_unary(state_t *state) {
	bool done = false;
	while (!done) {
		if (current(state).kind == TOKEN_OPERATOR) {
			operatorr op = current(state).op;
			if (op == OPERATOR_NOT || op == OPERATOR_MINUS) {
				advance_state(state);
				expression *right = parse_unary(state);
				expression *expression = expression_allocate();
				expression->kind = EXPRESSION_UNARY;
				expression->unary.op = op;
				expression->unary.right = right;
				return expression;
			}
			else {
				done = true;
			}
		}
		else {
			done = true;
		}
	}
	return parse_primary(state);
}

static expression *parse_call(state_t *state, name_id func_name);

static expression *parse_member(state_t *state, bool number) {
	if (current(state).kind == TOKEN_IDENTIFIER && !number) {
		token token = current(state);
		advance_state(state);

		if (current(state).kind == TOKEN_LEFT_PAREN) {
			return parse_call(state, token.identifier);
		}
		else if (current(state).kind == TOKEN_DOT || current(state).kind == TOKEN_LEFT_SQUARE) {
			bool number = current(state).kind == TOKEN_LEFT_SQUARE;

			advance_state(state);

			expression *var = expression_allocate();
			var->kind = EXPRESSION_VARIABLE;
			var->variable = token.identifier;

			expression *member = expression_allocate();
			member->kind = EXPRESSION_MEMBER;
			member->member.left = var;
			member->member.right = parse_member(state, number);

			return member;
		}
		else {
			expression *var = expression_allocate();
			var->kind = EXPRESSION_VARIABLE;
			var->variable = token.identifier;
			return var;
		}
	}
	else if (current(state).kind == TOKEN_NUMBER && number) {
		uint32_t index = (uint32_t)current(state).number;
		advance_state(state);
		match_token(state, TOKEN_RIGHT_SQUARE, "Expected a closing square bracket");
		advance_state(state);

		if (current(state).kind == TOKEN_DOT || current(state).kind == TOKEN_LEFT_SQUARE) {
			bool number = current(state).kind == TOKEN_LEFT_SQUARE;

			advance_state(state);

			expression *var = expression_allocate();
			var->kind = EXPRESSION_INDEX;
			var->index = index;

			expression *member = expression_allocate();
			member->kind = EXPRESSION_MEMBER;
			member->member.left = var;
			member->member.right = parse_member(state, number);

			return member;
		}
		else {
			expression *var = expression_allocate();
			var->kind = EXPRESSION_INDEX;
			var->index = index;
			return var;
		}
	}
	else {
		error(state->context, "Unexpected token");
		return NULL;
	}
}

static expression *parse_primary(state_t *state) {
	expression *left = NULL;

	switch (current(state).kind) {
	case TOKEN_BOOLEAN: {
		bool value = current(state).boolean;
		advance_state(state);
		left = expression_allocate();
		left->kind = EXPRESSION_BOOLEAN;
		left->boolean = value;
		break;
	}
	case TOKEN_NUMBER: {
		double value = current(state).number;
		advance_state(state);
		left = expression_allocate();
		left->kind = EXPRESSION_NUMBER;
		left->number = value;
		break;
	}
	/*case TOKEN_STRING: {
		token token = current(state);
		advance_state(state);
		left = expression_allocate();
		left->kind = EXPRESSION_STRING;
		left->string = add_name(token.string);
		break;
	}*/
	case TOKEN_IDENTIFIER: {
		token token = current(state);
		advance_state(state);
		if (current(state).kind == TOKEN_LEFT_PAREN) {
			left = parse_call(state, token.identifier);
		}
		else {
			expression *var = expression_allocate();
			var->kind = EXPRESSION_VARIABLE;
			var->variable = token.identifier;
			left = var;
		}
		break;
	}
	case TOKEN_LEFT_PAREN: {
		advance_state(state);
		expression *expr = parse_expression(state);
		match_token(state, TOKEN_RIGHT_PAREN, "Expected a closing bracket");
		advance_state(state);
		left = expression_allocate();
		left->kind = EXPRESSION_GROUPING;
		left->grouping = expr;
		break;
	}
	default:
		error(state->context, "Unexpected token");
		return NULL;
	}

	if (current(state).kind == TOKEN_DOT || current(state).kind == TOKEN_LEFT_SQUARE) {
		bool number = current(state).kind == TOKEN_LEFT_SQUARE;

		advance_state(state);
		expression *right = parse_member(state, number);

		expression *member = expression_allocate();
		member->kind = EXPRESSION_MEMBER;
		member->member.left = left;
		member->member.right = right;

		if (current(state).kind == TOKEN_LEFT_PAREN) {
			// return parse_call(state, member);
			error(state->context, "Function members not currently supported");
			return NULL;
		}
		else {
			return member;
		}
	}

	return left;
}

static expressions parse_parameters(state_t *state) {
	expressions e;
	e.size = 0;

	if (current(state).kind == TOKEN_RIGHT_PAREN) {
		return e;
	}

	for (;;) {
		e.e[e.size] = parse_expression(state);
		e.size += 1;

		if (current(state).kind == TOKEN_COMMA) {
			advance_state(state);
		}
		else {
			match_token(state, TOKEN_RIGHT_PAREN, "Expected a closing bracket");
			advance_state(state);
			return e;
		}
	}
}

static expression *parse_call(state_t *state, name_id func_name) {
	match_token(state, TOKEN_LEFT_PAREN, "Expected an opening bracket");
	advance_state(state);

	expression *call = NULL;

	call = expression_allocate();
	call->kind = EXPRESSION_CALL;
	call->call.func_name = func_name;
	call->call.parameters = parse_parameters(state);

	if (current(state).kind == TOKEN_DOT) {
		advance_state(state);
		expression *right = parse_expression(state);

		expression *member = expression_allocate();
		member->kind = EXPRESSION_MEMBER;
		member->member.left = call;
		member->member.right = right;

		if (current(state).kind == TOKEN_LEFT_PAREN) {
			// return parse_call(state, member);
			error(state->context, "Function members not currently supported");
			return NULL;
		}
		else {
			return member;
		}
	}

	return call;
}

static definition parse_struct_inner(state_t *state, name_id name) {
	match_token(state, TOKEN_LEFT_CURLY, "Expected an opening curly bracket");
	advance_state(state);

	token member_names[MAX_MEMBERS];
	token type_names[MAX_MEMBERS];
	token member_values[MAX_MEMBERS];
	size_t count = 0;

	while (current(state).kind != TOKEN_RIGHT_CURLY) {
		debug_context context = {0};
		check(count < MAX_MEMBERS, context, "Out of members");

		match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");
		member_names[count] = current(state);

		advance_state(state);

		if (current(state).kind == TOKEN_COLON) {
			advance_state(state);
			match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");
			type_names[count] = current(state);
			advance_state(state);
		}
		else {
			token t;
			t.kind = TOKEN_IDENTIFIER;
			t.identifier = NO_NAME;
			type_names[count] = t;
		}

		if (current(state).kind == TOKEN_OPERATOR && current(state).op == OPERATOR_ASSIGN) {
			advance_state(state);
			if (current(state).kind == TOKEN_BOOLEAN || current(state).kind == TOKEN_NUMBER || current(state).kind == TOKEN_IDENTIFIER) {
				member_values[count] = current(state);
				advance_state(state);
			}
			else {
				debug_context context = {0};
				error(context, "Unsupported assign in struct");
			}
		}
		else {
			member_values[count].kind = TOKEN_NONE;
			member_values[count].identifier = NO_NAME;
		}

		match_token(state, TOKEN_SEMICOLON, "Expected a semicolon");

		advance_state(state);

		++count;
	}

	advance_state(state);

	definition definition;
	definition.kind = DEFINITION_STRUCT;

	definition.type = add_type(name);

	type *s = get_type(definition.type);

	for (size_t i = 0; i < count; ++i) {
		member member;
		member.name = member_names[i].identifier;
		member.value = member_values[i];
		if (member.value.kind != TOKEN_NONE) {
			if (member.value.kind == TOKEN_BOOLEAN) {
				init_type_ref(&member.type, add_name("bool"));
			}
			else if (member.value.kind == TOKEN_NUMBER) {
				init_type_ref(&member.type, add_name("float"));
			}
			else if (member.value.kind == TOKEN_IDENTIFIER) {
				global g = find_global(member.value.identifier);
				if (g.name != NO_NAME) {
					init_type_ref(&member.type, get_type(g.type)->name);
				}
				else {
					init_type_ref(&member.type, add_name("fun"));
				}
			}
			else {
				debug_context context = {0};
				error(context, "Unsupported value in struct");
			}
		}
		else {
			init_type_ref(&member.type, type_names[i].identifier);
		}

		s->members.m[i] = member;
	}
	s->members.size = count;

	return definition;
}

static definition parse_struct(state_t *state) {
	advance_state(state);

	match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");
	token name = current(state);
	advance_state(state);

	return parse_struct_inner(state, name.identifier);
}

static definition parse_function(state_t *state) {
	advance_state(state);
	match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");

	token name = current(state);
	advance_state(state);
	match_token(state, TOKEN_LEFT_PAREN, "Expected an opening bracket");
	advance_state(state);
	match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");

	token param_name = current(state);
	advance_state(state);
	match_token(state, TOKEN_COLON, "Expected a colon");
	advance_state(state);
	type_ref param_type = parse_type_ref(state);
	match_token(state, TOKEN_RIGHT_PAREN, "Expected a closing bracket");
	advance_state(state);
	match_token(state, TOKEN_COLON, "Expected a colon");
	advance_state(state);
	type_ref return_type = parse_type_ref(state);

	statement *block = parse_block(state, NULL);
	definition d;

	d.kind = DEFINITION_FUNCTION;
	d.function = add_function(name.identifier);
	function *f = get_function(d.function);
	f->return_type = return_type;
	f->parameter_name = param_name.identifier;
	f->parameter_type = param_type;
	f->block = block;

	return d;
}

static definition parse_const(state_t *state) {
	advance_state(state);
	match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");

	token name = current(state);
	advance_state(state);
	match_token(state, TOKEN_COLON, "Expected a colon");
	advance_state(state);

	name_id type_name = NO_NAME;
	type_id type = NO_TYPE;

	if (current(state).kind == TOKEN_LEFT_CURLY) {
		type = parse_struct_inner(state, NO_NAME).type;
	}
	else {
		match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");
		type_name = current(state).identifier;
		advance_state(state);
	}

	match_token(state, TOKEN_SEMICOLON, "Expected a semicolon");
	advance_state(state);

	definition d;

	if (type_name == NO_NAME) {
		debug_context context = {0};
		check(type != NO_TYPE, context, "Const has not type");
		d.kind = DEFINITION_CONST_CUSTOM;
		d.global = add_global(type, name.identifier);
	}
	else if (type_name == add_name("tex2d")) {
		d.kind = DEFINITION_TEX2D;
		d.global = add_global(tex2d_type_id, name.identifier);
	}
	else if (type_name == add_name("texcube")) {
		d.kind = DEFINITION_TEXCUBE;
		d.global = add_global(texcube_type_id, name.identifier);
	}
	else if (type_name == add_name("sampler")) {
		d.kind = DEFINITION_SAMPLER;
		d.global = add_global(sampler_type_id, name.identifier);
	}
	else {
		debug_context context = {0};
		error(context, "Unsupported global");
	}

	return d;
}
