#include "expression.h"
#include "statement.h"
#include "tokenizer.h"

#include <string.h>

void add_statement(statement_t *statements, void *statement) {}

typedef struct state {
	tokens_t *tokens;
	unsigned index;
} state_t;

token_t current(state_t *self) {
	token_t token = tokens_get(self->tokens, self->index);
	return token;
}

void advance_state(state_t *state) {
	state->index += 1;
}

void *parse_statement(state_t *state);
void *parse_expression(state_t *state);

statement_t *parse(tokens_t *tokens) {
	state_t state;
	state.tokens = tokens;
	state.index = 0;

	statement_t *statements = NULL;
	for (;;) {
		token_t token = current(&state);
		switch (token.type) {
		case TOKEN_EOF:
			return statements;
		default: {
			add_statement(statements, parse_statement(&state));
		}
		}
	}
	return statements;
}

void *parse_block(state_t *state) {
	if (current(state).type == TOKEN_LEFT_CURLY) {
		advance_state(state);
	}
	else {
		exit(1); // Expected an opening curly bracket
	}

	statement_t *statements = NULL;
	for (;;) {
		switch (current(state).type) {
		case TOKEN_RIGHT_CURLY: {
			advance_state(&state);
			BlockStatement_t statement;
			statement.statements = statements;
			return &statement;
		}
		default:
			add_statement(statements, parse_statement(state));
			break;
		}
	}

	BlockStatement_t statement;
	statement.statements = statements;
	return &statement;
}

void *parse_preprocessor(const char *name, state_t *_state) {
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

	PreprocessorStatement_t statement;
	statement.name = name;
	statement.parameters = expressions;
	return (void *)&statement;
}

typedef enum Modifier {
	MODIFIER_IN,
	// Out,
} Modifier_t;

void *parse_declaration(state_t *state, Modifier_t *modifiers) {
	switch (current(state).type) {
	case TOKEN_IN: {
		advance_state(&state);
		*modifiers = MODIFIER_IN;
		++modifiers;
		return parse_declaration(state, modifiers);
	}
	case TOKEN_VEC3: {
		advance_state(&state);
		break;
	}
	case TOKEN_VEC4: {
		advance_state(&state);
		break;
	}
	case TOKEN_FLOAT: {
		advance_state(&state);
		break;
	}
	case TOKEN_VOID: {
		advance_state(&state);
		break;
	}
	default:
		exit(1); // Expected a variable declaration
	}

	const char *name;
	if (current(state).type == TOKEN_IDENTIFIER) {
		name = current(state).data.identifier;
		advance_state(&state);
	}
	else {
		exit(1); // Expected an identifier
	}

	switch (current(state).type) {
	case TOKEN_OPERATOR: {
		operator_t op = current(state).data.op;
		switch (op) {
		case OPERATOR_ASSIGN: {
			advance_state(&state);
			void *expr = parse_expression(state);
			switch (current(state).type) {
			case TOKEN_SEMICOLON: {
				advance_state(&state);
				DeclarationStatement_t statement;
				statement.name = name;
				statement.init = expr;
				return (void *)&statement;
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
		advance_state(&state);
		DeclarationStatement_t statement;
		statement.name = name;
		statement.init = NULL;
		return (void *)&statement;
	}
	case TOKEN_LEFT_PAREN: {
		advance_state(&state);
		switch (current(state).type) {
		case TOKEN_RIGHT_PAREN: {
			advance_state(&state);
			FunctionStatement_t statement;
			const char **parameters = {""};
			statement.parameters = parameters;
			statement.block = parse_block(state);
			return (void *)&statement;
		}
		default:
			exit(1); // Expected right paren
		}
	}
	default:
		exit(1); // Expected an assign or a semicolon
	}
}

void *parse_struct(state_t *state);

void *parse_statement(state_t *state) {
	switch (current(state).type) {
	case TOKEN_ATTRIBUTE: {
		const char *value = current(state).data.attribute;
		advance_state(&state);
		return parse_preprocessor(value, state);
	}
	case TOKEN_IF: {
		advance_state(&state);
		switch (current(state).type) {
		case TOKEN_LEFT_PAREN:
			advance_state(&state);
			break;
		default:
			exit(1); // Expected an opening bracket
		}
		void *test = parse_expression(state);
		switch (current(state).type) {
		case TOKEN_RIGHT_PAREN:
			advance_state(&state);
			break;
		default:
			exit(1); // Expected a closing bracket
		}
		void *block = parse_statement(state);
		IfStatement_t statement;
		statement.test = test;
		statement.block = block;
		return (void *)&statement;
	}
	case TOKEN_LEFT_CURLY: {
		BlockStatement_t statement;
		statement.statements = parse_block(state);
		return (void *)&statement;
	}
	case TOKEN_IN: {
		return parse_declaration(state, NULL);
	}
	case TOKEN_FLOAT: {
		return parse_declaration(state, NULL);
	}
	case TOKEN_VEC3: {
		return parse_declaration(state, NULL);
	}
	case TOKEN_VEC4: {
		return parse_declaration(state, NULL);
	}
	case TOKEN_VOID: {
		return parse_declaration(state, NULL);
	}
	case TOKEN_STRUCT: {
		return parse_struct(state);
	}
	default: {
		void *expr = parse_expression(state);
		switch (current(state).type) {
		case TOKEN_SEMICOLON: {
			advance_state(&state);
			return expr;
		}
		default:
			exit(1); // Expected a semicolon
		}
	}
	}
}

void *parse_assign(state_t *state);

void *parse_expression(state_t *state) {
	return parse_assign(state);
}

void *parse_logical(state_t *state);

void *parse_assign(state_t *state) {
	void *expr = parse_logical(state);
	bool done = false;
	while (!done) {
		switch (current(state).type) {
		case TOKEN_OPERATOR: {
			operator_t op = current(state).data.op;
			switch (op) {
			case OPERATOR_ASSIGN: {
				advance_state(&state);
				void *right = parse_logical(state);
				ExprBinary_t expression;
				expression.left = expr;
				expression.op = op;
				expression.right = right;
				expr = &expression;
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

void *parse_equality(state_t *state);

void *parse_logical(state_t *state) {
	void *expr = parse_equality(state);
	bool done = false;
	while (!done) {
		switch (current(state).type) {
		case TOKEN_OPERATOR: {
			operator_t op = current(state).data.op;
			switch (op) {
			case OPERATOR_OR:
			case OPERATOR_AND: {
				advance_state(&state);
				void *right = parse_equality(state);
				ExprBinary_t expression;
				expression.left = expr;
				expression.op = op;
				expression.right = right;
				expr = &expression;
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

void *parse_comparison(state_t *state);

void *parse_equality(state_t *state) {
	void *expr = parse_comparison(state);
	bool done = false;
	while (!done) {
		switch (current(state).type) {
		case TOKEN_OPERATOR: {
			operator_t op = current(state).data.op;
			switch (op) {
			case OPERATOR_EQUALS:
			case OPERATOR_NOT_EQUALS: {
				advance_state(&state);
				void *right = parse_comparison(state);
				ExprBinary_t expression;
				expression.left = expr;
				expression.op = op;
				expression.right = right;
				expr = &expression;
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

void *parse_addition(state_t *state);

void *parse_comparison(state_t *state) {
	void *expr = parse_addition(state);
	bool done = false;
	while (!done) {
		switch (current(state).type) {
		case TOKEN_OPERATOR: {
			operator_t op = current(state).data.op;
			switch (op) {
			case OPERATOR_GREATER:
			case OPERATOR_GREATER_EQUAL:
			case OPERATOR_LESS:
			case OPERATOR_LESS_EQUAL: {
				advance_state(&state);
				void *right = parse_addition(state);
				ExprBinary_t expression;
				expression.left = expr;
				expression.op = op;
				expression.right = right;
				expr = &expression;
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

void *parse_multiplication(state_t *state);

void *parse_addition(state_t *state) {
	void *expr = parse_multiplication(state);
	bool done = false;
	while (!done) {
		switch (current(state).type) {
		case TOKEN_OPERATOR: {
			operator_t op = current(state).data.op;
			switch (op) {
			case OPERATOR_MINUS:
			case OPERATOR_PLUS: {
				advance_state(&state);
				void *right = parse_multiplication(state);
				ExprBinary_t expression;
				expression.left = expr;
				expression.op = op;
				expression.right = right;
				expr = &expression;
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

void *parse_unary(state_t *state);

void *parse_multiplication(state_t *state) {
	void *expr = parse_unary(state);
	bool done = false;
	while (!done) {
		switch (current(state).type) {
		case TOKEN_OPERATOR: {
			operator_t op = current(state).data.op;
			switch (op) {
			case OPERATOR_DIVIDE:
			case OPERATOR_MULTIPLY:
			case OPERATOR_MOD: {
				advance_state(&state);
				void *right = parse_unary(state);
				ExprBinary_t expression;
				expression.left = expr;
				expression.op = op;
				expression.right = right;
				expr = &expression;
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

void *parse_primary(state_t *state);

void *parse_unary(state_t *state) {
	bool done = false;
	while (!done) {
		switch (current(state).type) {
		case TOKEN_OPERATOR: {
			operator_t op = current(state).data.op;
			switch (op) {
			case OPERATOR_NOT:
			case OPERATOR_MINUS: {
				advance_state(&state);
				void *right = parse_unary(state);
				ExprUnary_t expression;
				expression.op = op;
				expression.right = right;
				return &expression;
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

void *parse_call(state_t *state, expression_t func);

void *parse_primary(state_t *state) {
	switch (current(state).type) {
	case TOKEN_BOOLEAN: {
		bool value = current(state).data.boolean;
		advance_state(&state);
		expression_t expression;
		expression.type = EXPRESSION_BOOLEAN;
		expression.data.boolean = value;
		return &expression;
	}
	case TOKEN_NUMBER: {
		double value = current(state).data.number;
		advance_state(&state);
		expression_t expression;
		expression.type = EXPRESSION_NUMBER;
		expression.data.number = value;
		return &expression;
	}
	case TOKEN_STRING: {
		const char *value = current(state).data.string;
		advance_state(&state);
		expression_t expression;
		expression.type = EXPRESSION_STRING;
		expression.data.string = value;
		return &expression;
	}
	case TOKEN_IDENTIFIER: {
		const char *value = current(state).data.identifier;
		advance_state(&state);
		switch (current(state).type) {
		case TOKEN_LEFT_PAREN: {
			expression_t var;
			var.type = Variable;
			var.data.variable = value;
			return parse_call(state, var);
		}
		case TOKEN_COLON: {
			advance_state(&state);
			switch (current(state).type) {
			case TOKEN_IDENTIFIER: {
				const char *value2 = current(state).data.identifier;
				advance_state(&state);

				expression_t member;
				member.type = Member;
				member.data.member.value1 = value;
				member.data.member.value2 = value2;

				switch (current(state).type) {
				case TOKEN_LEFT_PAREN:
					return parse_call(state, member);
				default:
					member;
				}
			}
			default:
				exit(1); // Expected an identifier
			}
		}
		default: {
			expression_t var;
			var.type = Variable;
			var.data.variable = value;
			return &var;
		}
		}
	}
	case TOKEN_LEFT_PAREN: {
		advance_state(&state);
		void *expr = parse_expression(state);
		switch (current(state).type) {
		case TOKEN_RIGHT_PAREN:
			break;
		default:
			exit(1); // Expected a closing bracket
		}
		expression_t grouping;
		grouping.type = Grouping;
		grouping.data.grouping = expr;
		return &grouping;
	}
	case TOKEN_VEC4: {
		advance_state(&state);
		switch (current(state).type) {
		LeftParen:
			break;
		default:
			exit(1); // Expected an opening bracket
		}
		advance_state(&state);

		expression_t *expressions = NULL;
		bool done = false;
		while (!done) {
			switch (current(state).type) {
			case TOKEN_RIGHT_PAREN:
				done = true;
				break;
			default:
				expressions = parse_expression(state);
				++expressions;
				break;
			}
			advance_state(&state);
			switch (current(state).type) {
			case TOKEN_COMMA:
				break;
			case TOKEN_RIGHT_PAREN:
				done = true;
				break;
			default:
				expressions = parse_expression(state);
				++expressions;
				break;
			}
			advance_state(&state);
		}
		advance_state(&state);

		expression_t constructor;
		constructor.type = Constructor;
		ConstructorExpression_t expr;
		expr.parameters = expressions;
		constructor.data.constructor = expr;
		return &constructor;
	}
	default:
		exit(1); // Unexpected token: {:?}, state.current()
	}
}

void *parse_call(state_t *state, expression_t func) {
	switch (current(state).type) {
	case TOKEN_LEFT_PAREN: {
		advance_state(&state);
		switch (current(state).type) {
		case TOKEN_RIGHT_PAREN: {
			advance_state(&state);

			CallStatement_t call;
			call.func = func;
			call.parameters = NULL;
			return &call;
		}
		default: {
			void *expr = parse_expression(state);
			switch (current(state).type) {
			case TOKEN_RIGHT_PAREN: {
				advance_state(&state);

				CallStatement_t call;
				call.func = func;
				call.parameters = expr;
				return &call;
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

void *parse_struct(state_t *state) {
	const char *name;
	const char *member_name;
	const char *type_name;

	advance_state(&state);
	switch (current(state).type) {
	case TOKEN_IDENTIFIER:
		name = current(state).data.identifier;
		advance_state(&state);
		switch (current(state).type) {
		case TOKEN_LEFT_CURLY: {
			advance_state(&state);
			switch (current(state).type) {
			case TOKEN_IDENTIFIER:
				member_name = current(state).data.identifier;
				advance_state(&state);
				switch (current(state).type) {
				case TOKEN_COLON: {
					advance_state(&state);
					switch (current(state).type) {
					case TOKEN_IDENTIFIER: {
						type_name = current(state).data.identifier;
						advance_state(&state);
						switch (current(state).type) {
						case TOKEN_SEMICOLON: {
							advance_state(&state);
							switch (current(state).type) {
							case TOKEN_RIGHT_CURLY: {
								advance_state(&state);
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

	Member_t member;
	member.name = member_name;
	member.member_type = type_name;

	statement_t statement;
	statement.type = TOKEN_STRUCT;
	StructStatement_t structy;
	structy.attribute = NULL;
	structy.name = name;
	structy.members = &member;
	statement.data.structy = structy;
	return &statement;
}
