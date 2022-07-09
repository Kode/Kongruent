#include "parser.h"
#include "errors.h"
#include "tokenizer.h"

#include <stdlib.h>
#include <string.h>

static definition_t *definition_allocate(void) {
	return (definition_t *)malloc(sizeof(definition_t));
}

static void definition_free(definition_t *definition) {
	free(definition);
}

static void definitions_init(definitions_t *definitions) {
	definitions->size = 0;
}

static void definitions_add(definitions_t *definitions, definition_t *definition) {
	definitions->d[definitions->size] = definition;
	definitions->size += 1;
}

static statement_t *statement_allocate(void) {
	return (statement_t *)malloc(sizeof(statement_t));
}

static void statement_free(statement_t *statement) {
	free(statement);
}

static void statements_init(statements_t *statements) {
	statements->size = 0;
}

static void statements_add(statements_t *statements, statement_t *statement) {
	statements->s[statements->size] = statement;
	statements->size += 1;
}

void expressions_init(expressions_t *expressions) {
	expressions->size = 0;
}

static expression_t *expression_allocate(void) {
	return (expression_t *)malloc(sizeof(expression_t));
}

static void expression_free(expression_t *expression) {
	free(expression);
}

static void expressions_add(expressions_t *expressions, expression_t *expression) {
	expressions->e[expressions->size] = expression;
	expressions->size += 1;
}

typedef struct state {
	tokens_t *tokens;
	size_t index;
} state_t;

static token_t current(state_t *state) {
	token_t token = tokens_get(state->tokens, state->index);
	return token;
}

static void advance_state(state_t *state) {
	state->index += 1;
}

static void match_token(state_t *state, int token, const char *error_message) {
	if (current(state).type != token) {
		error(error_message, current(state).column, current(state).line);
	}
}

static definition_t *parse_definition(state_t *state);
static statement_t *parse_statement(state_t *state);
static expression_t *parse_expression(state_t *state);

definitions_t parse(tokens_t *tokens) {
	state_t state;
	state.tokens = tokens;
	state.index = 0;

	definitions_t definitions;
	definitions_init(&definitions);

	for (;;) {
		token_t token = current(&state);
		switch (token.type) {
		case TOKEN_EOF:
			return definitions;
		default: {
			definitions_add(&definitions, parse_definition(&state));
		}
		}
	}
}

static statement_t *parse_block(state_t *state) {
	match_token(state, TOKEN_LEFT_CURLY, "Expected an opening curly bracket");
	advance_state(state);

	statements_t statements;
	statements_init(&statements);

	for (;;) {
		switch (current(state).type) {
		case TOKEN_RIGHT_CURLY: {
			advance_state(state);
			statement_t *statement = statement_allocate();
			statement->type = STATEMENT_BLOCK;
			statement->block.statements = statements;
			return statement;
		}
		default:
			statements_add(&statements, parse_statement(state));
			break;
		}
	}

	statement_t *statement = statement_allocate();
	statement->type = STATEMENT_BLOCK;
	statement->block.statements = statements;
	return statement;
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

static definition_t *parse_struct(state_t *state);
static definition_t *parse_function(state_t *state);

static definition_t *parse_definition(state_t *state) {
	char attribute[MAX_IDENTIFIER_SIZE];
	attribute[0] = 0;

	if (current(state).type == TOKEN_ATTRIBUTE) {
		token_t token = current(state);
		strcpy(attribute, token.attribute);
		advance_state(state);
	}

	switch (current(state).type) {
	case TOKEN_STRUCT: {
		definition_t *structy = parse_struct(state);
		strcpy(structy->structy.attribute, attribute);
		return structy;
	}
	case TOKEN_FUNCTION: {
		definition_t *function = parse_function(state);
		strcpy(function->function.attribute, attribute);
		return function;
	}
	default: {
		error("Expected a struct or function", current(state).column, current(state).line);
		return NULL;
	}
	}
}

static statement_t *parse_statement(state_t *state) {
	switch (current(state).type) {
	case TOKEN_IF: {
		advance_state(state);
		match_token(state, TOKEN_LEFT_PAREN, "Expected an opening bracket");
		advance_state(state);

		expression_t *test = parse_expression(state);
		match_token(state, TOKEN_RIGHT_PAREN, "Expected a closing bracket");
		advance_state(state);

		statement_t *block = parse_statement(state);
		statement_t *statement = statement_allocate();
		statement->type = STATEMENT_IF;
		statement->iffy.test = test;
		statement->iffy.block = block;
		return statement;
	}
	case TOKEN_LEFT_CURLY: {
		return parse_block(state);
	}
	/*case TOKEN_IN : {
		modifiers_t modifiers;
		modifiers_init(&modifiers);
		return parse_declaration(state, modifiers);
	}
	case TOKEN_VOID: {
		modifiers_t modifiers;
		modifiers_init(&modifiers);
		return parse_declaration(state, modifiers);
	}*/
	default: {
		expression_t *expr = parse_expression(state);
		switch (current(state).type) {
		case TOKEN_SEMICOLON: {
			advance_state(state);

			statement_t *statement = statement_allocate();
			statement->type = STATEMENT_EXPRESSION;
			statement->expression = expr;
			return statement;
		}
		default:
			error("Expected a semicolon", current(state).column, current(state).line);
			return NULL;
		}
	}
	}
}

static expression_t *parse_assign(state_t *state);

static expression_t *parse_expression(state_t *state) {
	return parse_assign(state);
}

static expression_t *parse_logical(state_t *state);

static expression_t *parse_assign(state_t *state) {
	expression_t *expr = parse_logical(state);
	bool done = false;
	while (!done) {
		switch (current(state).type) {
		case TOKEN_OPERATOR: {
			operator_t op = current(state).op;
			switch (op) {
			case OPERATOR_ASSIGN: {
				advance_state(state);
				void *right = parse_logical(state);
				expression_t *expression = expression_allocate();
				expression->type = EXPRESSION_BINARY;
				expression->binary.left = expr;
				expression->binary.op = op;
				expression->binary.right = right;
				expr = expression;
				break;
			}
			default:
				done = true;
				break;
			}
		}
		default:
			done = true;
			break;
		}
	}
	return expr;
}

static expression_t *parse_equality(state_t *state);

static expression_t *parse_logical(state_t *state) {
	expression_t *expr = parse_equality(state);
	bool done = false;
	while (!done) {
		switch (current(state).type) {
		case TOKEN_OPERATOR: {
			operator_t op = current(state).op;
			switch (op) {
			case OPERATOR_OR:
			case OPERATOR_AND: {
				advance_state(state);
				expression_t *right = parse_equality(state);
				expression_t *expression = expression_allocate();
				expression->type = EXPRESSION_BINARY;
				expression->binary.left = expr;
				expression->binary.op = op;
				expression->binary.right = right;
				expr = expression;
			}
			default:
				done = true;
				break;
			}
		}
		default:
			done = true;
			break;
		}
	}
	return expr;
}

static expression_t *parse_comparison(state_t *state);

static expression_t *parse_equality(state_t *state) {
	expression_t *expr = parse_comparison(state);
	bool done = false;
	while (!done) {
		switch (current(state).type) {
		case TOKEN_OPERATOR: {
			operator_t op = current(state).op;
			switch (op) {
			case OPERATOR_EQUALS:
			case OPERATOR_NOT_EQUALS: {
				advance_state(state);
				expression_t *right = parse_comparison(state);
				expression_t *expression = expression_allocate();
				expression->type = EXPRESSION_BINARY;
				expression->binary.left = expr;
				expression->binary.op = op;
				expression->binary.right = right;
				expr = expression;
			}
			default:
				done = true;
				break;
			}
		}
		default:
			done = true;
			break;
		}
	}
	return expr;
}

static expression_t *parse_addition(state_t *state);

static expression_t *parse_comparison(state_t *state) {
	expression_t *expr = parse_addition(state);
	bool done = false;
	while (!done) {
		switch (current(state).type) {
		case TOKEN_OPERATOR: {
			operator_t op = current(state).op;
			switch (op) {
			case OPERATOR_GREATER:
			case OPERATOR_GREATER_EQUAL:
			case OPERATOR_LESS:
			case OPERATOR_LESS_EQUAL: {
				advance_state(state);
				expression_t *right = parse_addition(state);
				expression_t *expression = expression_allocate();
				expression->type = EXPRESSION_BINARY;
				expression->binary.left = expr;
				expression->binary.op = op;
				expression->binary.right = right;
				expr = expression;
			}
				done = true;
				break;
			}
		}
			done = true;
			break;
		}
	}
	return expr;
}

static expression_t *parse_multiplication(state_t *state);

static expression_t *parse_addition(state_t *state) {
	expression_t *expr = parse_multiplication(state);
	bool done = false;
	while (!done) {
		switch (current(state).type) {
		case TOKEN_OPERATOR: {
			operator_t op = current(state).op;
			switch (op) {
			case OPERATOR_MINUS:
			case OPERATOR_PLUS: {
				advance_state(state);
				expression_t *right = parse_multiplication(state);
				expression_t *expression = expression_allocate();
				expression->type = EXPRESSION_BINARY;
				expression->binary.left = expr;
				expression->binary.op = op;
				expression->binary.right = right;
				expr = expression;
			}
			default:
				done = true;
				break;
			}
		}
		default:
			done = true;
			break;
		}
	}
	return expr;
}

static expression_t *parse_unary(state_t *state);

static expression_t *parse_multiplication(state_t *state) {
	expression_t *expr = parse_unary(state);
	bool done = false;
	while (!done) {
		switch (current(state).type) {
		case TOKEN_OPERATOR: {
			operator_t op = current(state).op;
			switch (op) {
			case OPERATOR_DIVIDE:
			case OPERATOR_MULTIPLY:
			case OPERATOR_MOD: {
				advance_state(state);
				expression_t *right = parse_unary(state);
				expression_t *expression = expression_allocate();
				expression->type = EXPRESSION_BINARY;
				expression->binary.left = expr;
				expression->binary.op = op;
				expression->binary.right = right;
				expr = expression;
			}
			default:
				done = true;
				break;
			}
		}
		default:
			done = true;
			break;
		}
	}
	return expr;
}

static expression_t *parse_primary(state_t *state);

static expression_t *parse_unary(state_t *state) {
	bool done = false;
	while (!done) {
		switch (current(state).type) {
		case TOKEN_OPERATOR: {
			operator_t op = current(state).op;
			switch (op) {
			case OPERATOR_NOT:
			case OPERATOR_MINUS: {
				advance_state(state);
				expression_t *right = parse_unary(state);
				expression_t *expression = expression_allocate();
				expression->type = EXPRESSION_UNARY;
				expression->unary.op = op;
				expression->unary.right = right;
				return expression;
			}
			default:
				done = true;
				break;
			}
		}
		default:
			done = true;
			break;
		}
	}
	return parse_primary(state);
}

static expression_t *parse_call(state_t *state, expression_t *func);

static expression_t *parse_primary(state_t *state) {
	switch (current(state).type) {
	case TOKEN_BOOLEAN: {
		bool value = current(state).boolean;
		advance_state(state);
		expression_t *expression = expression_allocate();
		expression->type = EXPRESSION_BOOLEAN;
		expression->boolean = value;
		return expression;
	}
	case TOKEN_NUMBER: {
		double value = current(state).number;
		advance_state(state);
		expression_t *expression = expression_allocate();
		expression->type = EXPRESSION_NUMBER;
		expression->number = value;
		return expression;
	}
	case TOKEN_STRING: {
		token_t token = current(state);
		advance_state(state);
		expression_t *expression = expression_allocate();
		expression->type = EXPRESSION_STRING;
		strcpy(expression->string, token.string);
		return expression;
	}
	case TOKEN_IDENTIFIER: {
		token_t token = current(state);
		advance_state(state);
		switch (current(state).type) {
		case TOKEN_LEFT_PAREN: {
			expression_t *var = expression_allocate();
			var->type = EXPRESSION_VARIABLE;
			strcpy(var->variable, token.identifier);
			return parse_call(state, var);
		}
		case TOKEN_COLON: {
			advance_state(state);
			match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");
			token_t token2 = current(state);
			advance_state(state);

			expression_t *member = expression_allocate();
			member->type = EXPRESSION_MEMBER;
			strcpy(member->member.value1, token.identifier);
			strcpy(member->member.value2, token2.identifier);

			if (current(state).type == TOKEN_LEFT_PAREN) {
				return parse_call(state, member);
			}
			else {
				return member;
			}
		}
		default: {
			expression_t *var = expression_allocate();
			var->type = EXPRESSION_VARIABLE;
			strcpy(var->variable, token.identifier);
			return var;
		}
		}
	}
	case TOKEN_LEFT_PAREN: {
		advance_state(state);
		expression_t *expr = parse_expression(state);
		match_token(state, TOKEN_RIGHT_PAREN, "Expected a closing bracket");
		expression_t *grouping = expression_allocate();
		grouping->type = EXPRESSION_GROUPING;
		grouping->grouping = expr;
		return grouping;
	}
	default:
		error("Unexpected token", current(state).column, current(state).line);
		return NULL;
	}
}

static expression_t *parse_call(state_t *state, expression_t *func) {
	match_token(state, TOKEN_LEFT_PAREN, "Expected an opening bracket");
	advance_state(state);
	switch (current(state).type) {
	case TOKEN_RIGHT_PAREN: {
		advance_state(state);

		expression_t *call = expression_allocate();
		call->type = EXPRESSION_CALL;
		call->call.func = func;
		call->call.parameters = NULL;
		return call;
	}
	default: {
		expression_t *expr = parse_expression(state);
		match_token(state, TOKEN_RIGHT_PAREN, "Expected a closing bracket");
		advance_state(state);

		expression_t *call = expression_allocate();
		call->type = EXPRESSION_CALL;
		call->call.func = func;
		call->call.parameters = expr;
		return call;
	}
	}
}

static definition_t *parse_struct(state_t *state) {
	advance_state(state);
	match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");
	token_t name = current(state);

	advance_state(state);
	match_token(state, TOKEN_LEFT_CURLY, "Expected an opening curly bracket");

	advance_state(state);
	match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");
	token_t member_name = current(state);

	advance_state(state);
	match_token(state, TOKEN_COLON, "Expected a colon");

	advance_state(state);
	match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");
	token_t type_name = current(state);

	advance_state(state);
	match_token(state, TOKEN_SEMICOLON, "Expected a semicolon");

	advance_state(state);
	match_token(state, TOKEN_RIGHT_CURLY, "Expected a closing curly bracket");

	advance_state(state);

	member_t member;
	strcpy(member.name, member_name.identifier);
	strcpy(member.member_type, type_name.identifier);

	members_t members;
	members.m[0] = member;
	members.size = 1;

	definition_t *definition = definition_allocate();
	definition->type = DEFINITION_STRUCT;
	definition->structy.attribute[0] = 0;
	strcpy(definition->structy.name, name.identifier);
	definition->structy.members = members;
	return definition;
}

static definition_t *parse_function(state_t *state) {
	advance_state(state);
	match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");

	token_t name = current(state);
	advance_state(state);
	match_token(state, TOKEN_LEFT_PAREN, "Expected an opening bracket");
	advance_state(state);
	match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");

	token_t param_name = current(state);
	advance_state(state);
	match_token(state, TOKEN_COLON, "Expected a colon");
	advance_state(state);
	token_t param_type_name = current(state);
	advance_state(state);
	match_token(state, TOKEN_RIGHT_PAREN, "Expected a closing bracket");
	advance_state(state);
	match_token(state, TOKEN_FUNCTION_THINGY, "Expected a function-thingy");
	advance_state(state);
	match_token(state, TOKEN_IDENTIFIER, "Expected an identifier");

	token_t return_type_name = current(state);
	advance_state(state);
	statement_t *block = parse_block(state);
	definition_t *function = definition_allocate();
	function->type = DEFINITION_FUNCTION;
	strcpy(function->function.name, name.identifier);
	strcpy(function->function.return_type_name, return_type_name.identifier);
	strcpy(function->function.parameter_name, param_name.identifier);
	strcpy(function->function.parameter_type_name, param_type_name.identifier);
	function->function.block = block;
	return function;
}
