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
	assert(s != NULL);
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
	assert(e != NULL);
	e->type.resolved = false;
	e->type.name = NO_NAME;
	return e;
}

static void expression_free(expression *expression) {
	free(expression);
}

typedef struct state {
	tokens *tokens;
	size_t index;
} state_t;

static token current(state_t *state) {
	token token = tokens_get(state->tokens, state->index);
	return token;
}

static void advance_state(state_t *state) {
	state->index += 1;
}

static void match_token(state_t *state, int token, const char *error_message) {
	if (current(state).kind != token) {
		error(error_message, current(state).column, current(state).line);
	}
}

static void match_token_identifier(state_t *state) {
	if (current(state).kind != TOKEN_IDENTIFIER) {
		error("Expected an identifier", current(state).column, current(state).line);
	}
}

static definition parse_definition(state_t *state);
static statement *parse_statement(state_t *state);
static expression *parse_expression(state_t *state);

void parse(tokens *tokens) {
	state_t state;
	state.tokens = tokens;
	state.index = 0;

	for (;;) {
		token token = current(&state);
		if (token.kind == TOKEN_EOF) {
			return;
		}
		else {
			parse_definition(&state);
		}
	}
}

static statement *parse_block(state_t *state) {
	match_token(state, TOKEN_LEFT_CURLY, "Expected an opening curly bracket");
	advance_state(state);

	statements statements;
	statements_init(&statements);

	for (;;) {
		switch (current(state).kind) {
		case TOKEN_RIGHT_CURLY: {
			advance_state(state);
			statement *statement = statement_allocate();
			statement->kind = STATEMENT_BLOCK;
			statement->block.parent = NULL;
			statement->block.vars.size = 0;
			statement->block.statements = statements;
			return statement;
		}
		case TOKEN_EOF:
			error("File ended before a block ended", current(state).column, current(state).line);
			return NULL;
		default:
			statements_add(&statements, parse_statement(state));
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
	default: {
		error("Expected a struct or function", current(state).column, current(state).line);

		definition d = {0};
		return d;
	}
	}
}

static statement *parse_statement(state_t *state) {
	switch (current(state).kind) {
	case TOKEN_IF: {
		advance_state(state);
		match_token(state, TOKEN_LEFT_PAREN, "Expected an opening bracket");
		advance_state(state);

		expression *test = parse_expression(state);
		match_token(state, TOKEN_RIGHT_PAREN, "Expected a closing bracket");
		advance_state(state);

		statement *block = parse_statement(state);
		statement *statement = statement_allocate();
		statement->kind = STATEMENT_IF;
		statement->iffy.test = test;
		statement->iffy.block = block;
		return statement;
	}
	case TOKEN_LEFT_CURLY: {
		return parse_block(state);
	}
	case TOKEN_LET: {
		advance_state(state);
		bool mutable = false;
		if (current(state).kind == TOKEN_MUT) {
			mutable = true;
			advance_state(state);
		}

		match_token_identifier(state);
		token name = current(state);
		advance_state(state);

		match_token(state, TOKEN_COLON, "Expected a colon");
		advance_state(state);

		match_token_identifier(state);
		token type_name = current(state);
		advance_state(state);

		match_token(state, TOKEN_SEMICOLON, "Expected a semicolon");
		advance_state(state);

		statement *statement = statement_allocate();
		statement->kind = STATEMENT_LOCAL_VARIABLE;
		statement->local_variable.var.name = name.identifier;
		statement->local_variable.var.type.resolved = false;
		statement->local_variable.var.type.name = type_name.identifier;
		statement->local_variable.var.variable_id = 0;
		statement->local_variable.init = NULL;
		return statement;
	}
	default: {
		expression *expr = parse_expression(state);
		if (current(state).kind == TOKEN_SEMICOLON) {
			advance_state(state);

			statement *statement = statement_allocate();
			statement->kind = STATEMENT_EXPRESSION;
			statement->expression = expr;
			return statement;
		}
		else {
			statement *statement = statement_allocate();
			statement->kind = STATEMENT_RETURN_EXPRESSION;
			statement->expression = expr;
			return statement;
		}
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
			if (op == OPERATOR_ASSIGN) {
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

static expression *parse_member(state_t *state) {
	if (current(state).kind == TOKEN_IDENTIFIER) {
		token token = current(state);
		advance_state(state);

		if (current(state).kind == TOKEN_LEFT_PAREN) {
			return parse_call(state, token.identifier);
		}
		else if (current(state).kind == TOKEN_DOT) {
			advance_state(state);

			expression *var = expression_allocate();
			var->kind = EXPRESSION_VARIABLE;
			var->variable = token.identifier;

			expression *member = expression_allocate();
			member->kind = EXPRESSION_MEMBER;
			member->member.left = var;
			member->member.right = parse_member(state);

			return member;
		}
		else {
			expression *var = expression_allocate();
			var->kind = EXPRESSION_VARIABLE;
			var->variable = token.identifier;
			return var;
		}
	}
	else {
		error("Unexpected token", current(state).column, current(state).line);
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
		error("Unexpected token", current(state).column, current(state).line);
		return NULL;
	}

	if (current(state).kind == TOKEN_DOT) {
		advance_state(state);
		expression *right = parse_member(state);

		expression *member = expression_allocate();
		member->kind = EXPRESSION_MEMBER;
		member->member.left = left;
		member->member.right = right;

		if (current(state).kind == TOKEN_LEFT_PAREN) {
			// return parse_call(state, member);
			error("Function members not currently supported", current(state).column, current(state).line);
			return NULL;
		}
		else {
			return member;
		}
	}

	return left;
}

static expression *parse_call(state_t *state, name_id func_name) {
	match_token(state, TOKEN_LEFT_PAREN, "Expected an opening bracket");
	advance_state(state);

	expression *call = NULL;

	if (current(state).kind == TOKEN_RIGHT_PAREN) {
		advance_state(state);

		call = expression_allocate();
		call->kind = EXPRESSION_CALL;
		call->call.func_name = func_name;
		call->call.parameters = NULL;
	}
	else {
		expression *expr = parse_expression(state);
		match_token(state, TOKEN_RIGHT_PAREN, "Expected a closing bracket");
		advance_state(state);

		call = expression_allocate();
		call->kind = EXPRESSION_CALL;
		call->call.func_name = func_name;
		call->call.parameters = expr;
	}

	if (current(state).kind == TOKEN_DOT) {
		advance_state(state);
		expression *right = parse_expression(state);

		expression *member = expression_allocate();
		member->kind = EXPRESSION_MEMBER;
		member->member.left = call;
		member->member.right = right;

		if (current(state).kind == TOKEN_LEFT_PAREN) {
			// return parse_call(state, member);
			error("Function members not currently supported", current(state).column, current(state).line);
			return NULL;
		}
		else {
			return member;
		}
	}

	return call;
}

static definition parse_struct(state_t *state) {
	advance_state(state);

	match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");
	token name = current(state);
	advance_state(state);

	match_token(state, TOKEN_LEFT_CURLY, "Expected an opening curly bracket");
	advance_state(state);

	token member_names[MAX_MEMBERS];
	token type_names[MAX_MEMBERS];
	size_t count = 0;

	while (current(state).kind != TOKEN_RIGHT_CURLY) {
		assert(count < MAX_MEMBERS);

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
			match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");
			advance_state(state);
		}

		match_token(state, TOKEN_SEMICOLON, "Expected a semicolon");

		advance_state(state);

		++count;
	}

	advance_state(state);

	definition definition;
	definition.kind = DEFINITION_STRUCT;

	definition.type = add_type(name.identifier);

	type *s = get_type(definition.type);

	for (size_t i = 0; i < count; ++i) {
		member member;
		member.name = member_names[i].identifier;
		member.type.resolved = false;
		member.type.name = type_names[i].identifier;

		s->members.m[i] = member;
	}
	s->members.size = count;

	return definition;
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
	token param_type_name = current(state);
	advance_state(state);
	match_token(state, TOKEN_RIGHT_PAREN, "Expected a closing bracket");
	advance_state(state);
	match_token(state, TOKEN_FUNCTION_THINGY, "Expected a function-thingy");
	advance_state(state);
	match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");

	token return_type_name = current(state);
	advance_state(state);
	statement *block = parse_block(state);
	definition d;

	d.kind = DEFINITION_FUNCTION;
	d.function = add_function(name.identifier);
	function *f = get_function(d.function);
	f->return_type.resolved = false;
	f->return_type.name = return_type_name.identifier;
	f->parameter_name = param_name.identifier;
	f->parameter_type.resolved = false;
	f->parameter_type.name = param_type_name.identifier;
	f->block = block;

	return d;
}
