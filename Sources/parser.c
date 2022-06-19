#include "parser.h"
#include "tokenizer.h"

#include <stdlib.h>
#include <string.h>

static expression_t *expression_allocate(void) {
	return (expression_t *)malloc(sizeof(expression_t));
}

static void expression_free(expression_t *expression) {
	free(expression);
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

static void add_statement(statements_t *statements, statement_t *statement) {
	statements->s[statements->size] = statement;
	statements->size += 1;
}

void expressions_init(expressions_t *expressions) {
	expressions->size = 0;
}

static void expressions_add(expressions_t *expressions, expression_t *expression) {
	expressions->e[expressions->size] = expression;
	expressions->size += 1;
}

typedef struct state {
	tokens_t *tokens;
	size_t index;
} state_t;

static token_t current(state_t *self) {
	token_t token = tokens_get(self->tokens, self->index);
	return token;
}

static void advance_state(state_t *state) {
	state->index += 1;
}

static statement_t *parse_statement(state_t *state);
static expression_t *parse_expression(state_t *state);

statements_t parse(tokens_t *tokens) {
	state_t state;
	state.tokens = tokens;
	state.index = 0;

	statements_t statements;
	statements_init(&statements);

	for (;;) {
		token_t token = current(&state);
		switch (token.type) {
		case TOKEN_EOF:
			return statements;
		default: {
			add_statement(&statements, parse_statement(&state));
		}
		}
	}

	return statements;
}

static statement_t *parse_block(state_t *state) {
	if (current(state).type == TOKEN_LEFT_CURLY) {
		advance_state(state);
	}
	else {
		exit(1); // Expected an opening curly bracket
	}

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
			add_statement(&statements, parse_statement(state));
			break;
		}
	}

	statement_t *statement = statement_allocate();
	statement->type = STATEMENT_BLOCK;
	statement->block.statements = statements;
	return statement;
}

static statement_t *parse_preprocessor(token_t token, state_t *_state) {
	expression_t *expressions = NULL;
	/*loop {
	    match state.current() {
	        Token::NewLine => {
	            state.advance();
	            break
	        }
	        _ => {
	            expressions.push(expression(state));
	        }
	    }
	}*/

	statement_t *statement = statement_allocate();
	statement->type = STATEMENT_PREPROCESSOR_DIRECTIVE;
	strcpy(statement->preprocessorDirective.name, token.attribute);
	statement->preprocessorDirective.parameters = expressions;
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

static statement_t *parse_declaration(state_t *state, modifiers_t modifiers) {
	switch (current(state).type) {
	case TOKEN_IN: {
		advance_state(state);
		modifiers_add(&modifiers, MODIFIER_IN);
		return parse_declaration(state, modifiers);
	}
	case TOKEN_VEC3: {
		advance_state(state);
		break;
	}
	case TOKEN_VEC4: {
		advance_state(state);
		break;
	}
	case TOKEN_FLOAT: {
		advance_state(state);
		break;
	}
	case TOKEN_VOID: {
		advance_state(state);
		break;
	}
	default:
		exit(1); // Expected a variable declaration
	}

	token_t identifier;
	if (current(state).type == TOKEN_IDENTIFIER) {
		identifier = current(state);
		advance_state(state);
	}
	else {
		exit(1); // Expected an identifier
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
				exit(1); // Expected a semicolon
			}
			break;
		}
		default:
			exit(1); // Expected an assignment operator
		}
		break;
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
			exit(1); // Expected right paren
		}
	}
	default:
		exit(1); // Expected an assign or a semicolon
	}
}

static statement_t *parse_struct(state_t *state);

static statement_t *parse_statement(state_t *state) {
	switch (current(state).type) {
	case TOKEN_ATTRIBUTE: {
		token_t token = current(state);
		advance_state(state);
		return parse_preprocessor(token, state);
	}
	case TOKEN_IF: {
		advance_state(state);
		switch (current(state).type) {
		case TOKEN_LEFT_PAREN:
			advance_state(state);
			break;
		default:
			exit(1); // Expected an opening bracket
		}
		expression_t *test = parse_expression(state);
		switch (current(state).type) {
		case TOKEN_RIGHT_PAREN:
			advance_state(state);
			break;
		default:
			exit(1); // Expected a closing bracket
		}
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
	case TOKEN_IN: {
		modifiers_t modifiers;
		modifiers_init(&modifiers);
		return parse_declaration(state, modifiers);
	}
	case TOKEN_FLOAT: {
		modifiers_t modifiers;
		modifiers_init(&modifiers);
		return parse_declaration(state, modifiers);
	}
	case TOKEN_VEC3: {
		modifiers_t modifiers;
		modifiers_init(&modifiers);
		return parse_declaration(state, modifiers);
	}
	case TOKEN_VEC4: {
		modifiers_t modifiers;
		modifiers_init(&modifiers);
		return parse_declaration(state, modifiers);
	}
	case TOKEN_VOID: {
		modifiers_t modifiers;
		modifiers_init(&modifiers);
		return parse_declaration(state, modifiers);
	}
	case TOKEN_STRUCT: {
		return parse_struct(state);
	}
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
			exit(1); // Expected a semicolon
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
			switch (current(state).type) {
			case TOKEN_IDENTIFIER: {
				token_t token2 = current(state);
				advance_state(state);

				expression_t *member = expression_allocate();
				member->type = EXPRESSION_MEMBER;
				strcpy(member->member.value1, token.identifier);
				strcpy(member->member.value2, token2.identifier);

				switch (current(state).type) {
				case TOKEN_LEFT_PAREN:
					return parse_call(state, member);
				default:
					return member;
				}
			}
			default:
				exit(1); // Expected an identifier
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
		switch (current(state).type) {
		case TOKEN_RIGHT_PAREN:
			break;
		default:
			exit(1); // Expected a closing bracket
		}
		expression_t *grouping = expression_allocate();
		grouping->type = EXPRESSION_GROUPING;
		grouping->grouping = expr;
		return grouping;
	}
	case TOKEN_VEC4: {
		advance_state(state);
		switch (current(state).type) {
		case TOKEN_LEFT_PAREN:
			break;
		default:
			exit(1); // Expected an opening bracket
		}
		advance_state(state);

		expressions_t expressions;
		expressions_init(&expressions);

		bool done = false;
		while (!done) {
			switch (current(state).type) {
			case TOKEN_RIGHT_PAREN:
				done = true;
				break;
			default:
				expressions_add(&expressions, parse_expression(state));
				break;
			}
			advance_state(state);
			switch (current(state).type) {
			case TOKEN_COMMA:
				break;
			case TOKEN_RIGHT_PAREN:
				done = true;
				break;
			default:
				expressions_add(&expressions, parse_expression(state));
				break;
			}
			advance_state(state);
		}
		advance_state(state);

		expression_t *constructor = expression_allocate();
		constructor->type = EXPRESSION_CONSTRUCTOR;
		constructor->constructor.parameters = expressions;
		return constructor;
	}
	default:
		exit(1); // Unexpected token: {:?}, state.current()
	}
}

static expression_t *parse_call(state_t *state, expression_t *func) {
	switch (current(state).type) {
	case TOKEN_LEFT_PAREN: {
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
			switch (current(state).type) {
			case TOKEN_RIGHT_PAREN: {
				advance_state(state);

				expression_t *call = expression_allocate();
				call->type = EXPRESSION_CALL;
				call->call.func = func;
				call->call.parameters = expr;
				return call;
			}
			default:
				exit(1); // Expected a closing bracket
			}
		}
		}
	}
	default:
		exit(1); // Fascinating
	}
}

static statement_t *parse_struct(state_t *state) {
	token_t name, member_name, type_name;

	advance_state(state);
	switch (current(state).type) {
	case TOKEN_IDENTIFIER:
		name = current(state);
		advance_state(state);
		switch (current(state).type) {
		case TOKEN_LEFT_CURLY: {
			advance_state(state);
			switch (current(state).type) {
			case TOKEN_IDENTIFIER:
				member_name = current(state);
				advance_state(state);
				switch (current(state).type) {
				case TOKEN_COLON: {
					advance_state(state);
					switch (current(state).type) {
					case TOKEN_IDENTIFIER: {
						type_name = current(state);
						advance_state(state);
						switch (current(state).type) {
						case TOKEN_SEMICOLON: {
							advance_state(state);
							switch (current(state).type) {
							case TOKEN_RIGHT_CURLY: {
								advance_state(state);
							}
							default:
								exit(1); // Expected a closing curly bracket
							}
							break;
						}
						default:
							exit(1); // Expected a semicolon
						}
						break;
					}
					default:
						exit(1); // Expected an identifier
					}
					break;
				}
				default:
					exit(1); // Expected a colon
				}
				break;
			default:
				exit(1); // Expected an identifier
			}
			break;
		}
		default:
			exit(1); // Expected an opening curly bracket
		}
	default:
		exit(1); // Expected an identifier
	}

	member_t member;
	strcpy(member.name, member_name.identifier);
	strcpy(member.member_type, type_name.identifier);

	members_t members;
	members.m[0] = member;
	members.size = 1;

	statement_t *statement = statement_allocate();
	statement->type = STATEMENT_STRUCT;
	statement->structy.attribute[0] = 0;
	strcpy(statement->structy.name, name.identifier);
	statement->structy.members = members;
	return statement;
}
