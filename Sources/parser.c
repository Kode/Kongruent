#include "expression.h"
#include "statement.h"
#include "tokenizer.h"

#include <string.h>

void add_statement(statement_t *statements, void *statement) {}

typedef struct state {
	token_array_t *tokens;
	unsigned index;
} state_t;

token_t current(state_t *self) {
	token_t token = get_token(self, self->index);
	return token;
}

void advance_state(state_t *state) {
	state->index += 1;
}

void *parse_statement(state_t *state);
void *parse_expression(state_t *state);

statement_t *parse(token_array_t *tokens) {
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
	if (current(state).type == LeftCurly) {
		advance_state(state);
	}
	else {
		exit(1); // Expected an opening curly bracket
	}

	statement_t *statements = NULL;
	for (;;) {
		switch (current(state).type) {
		case RightCurly: {
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
	case In: {
		advance_state(&state);
		*modifiers = MODIFIER_IN;
		++modifiers;
		return parse_declaration(state, modifiers);
	}
	case Vec3: {
		advance_state(&state);
		break;
	}
	case Vec4: {
		advance_state(&state);
		break;
	}
	case Float: {
		advance_state(&state);
		break;
	}
	case Void: {
		advance_state(&state);
		break;
	}
	default:
		exit(1); // Expected a variable declaration
	}

	const char *name;
	if (current(state).type == Identifier) {
		name = current(state).data.identifier;
		advance_state(&state);
	}
	else {
		exit(1); // Expected an identifier
	}

	switch (current(state).type) {
	case Operator: {
		operator_t op = current(state).data.op;
		switch (op) {
		case Assign: {
			advance_state(&state);
			void *expr = parse_expression(state);
			switch (current(state).type) {
			case Semicolon: {
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
	case Semicolon: {
		advance_state(&state);
		DeclarationStatement_t statement;
		statement.name = name;
		statement.init = NULL;
		return (void *)&statement;
	}
	case LeftParen: {
		advance_state(&state);
		switch (current(state).type) {
		case RightParen: {
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
	case Attribute: {
		const char *value = current(state).data.attribute;
		advance_state(&state);
		return parse_preprocessor(value, state);
	}
	case If: {
		advance_state(&state);
		switch (current(state).type) {
		case LeftParen:
			advance_state(&state);
			break;
		default:
			exit(1); // Expected an opening bracket
		}
		void *test = parse_expression(state);
		switch (current(state).type) {
		case RightParen:
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
	case LeftCurly: {
		BlockStatement_t statement;
		statement.statements = parse_block(state);
		return (void *)&statement;
	}
	case In: {
		return parse_declaration(state, NULL);
	}
	case Float: {
		return parse_declaration(state, NULL);
	}
	case Vec3: {
		return parse_declaration(state, NULL);
	}
	case Vec4: {
		return parse_declaration(state, NULL);
	}
	case Void: {
		return parse_declaration(state, NULL);
	}
	case Struct: {
		return parse_struct(state);
	}
	default: {
		void *expr = parse_expression(state);
		switch (current(state).type) {
		case Semicolon: {
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
		case Operator: {
			operator_t op = current(state).data.op;
			switch (op) {
			case Assign: {
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
		case Operator: {
			operator_t op = current(state).data.op;
			switch (op) {
			case Or:
			case And: {
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
		case Operator: {
			operator_t op = current(state).data.op;
			switch (op) {
			case Equals:
			case NotEquals: {
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
		case Operator: {
			operator_t op = current(state).data.op;
			switch (op) {
			case Greater:
			case GreaterEqual:
			case Less:
			case LessEqual: {
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
		case Operator: {
			operator_t op = current(state).data.op;
			switch (op) {
			case Minus:
			case Plus: {
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
		case Operator: {
			operator_t op = current(state).data.op;
			switch (op) {
			case Div:
			case Multiply:
			case Mod: {
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
		case Operator: {
			operator_t op = current(state).data.op;
			switch (op) {
			case Not:
			case Minus: {
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
	case Boolean: {
		bool value = current(state).data.boolean;
		advance_state(&state);
		expression_t expression;
		expression.type = Boolean;
		expression.data.boolean = value;
		return &expression;
	}
	case Number: {
		double value = current(state).data.number;
		advance_state(&state);
		expression_t expression;
		expression.type = Number;
		expression.data.number = value;
		return &expression;
	}
	case String: {
		const char *value = current(state).data.string;
		advance_state(&state);
		expression_t expression;
		expression.type = String;
		expression.data.string = value;
		return &expression;
	}
	case Identifier: {
		const char *value = current(state).data.identifier;
		advance_state(&state);
		switch (current(state).type) {
		case LeftParen: {
			expression_t var;
			var.type = Variable;
			var.data.variable = value;
			return parse_call(state, var);
		}
		case Colon: {
			advance_state(&state);
			switch (current(state).type) {
			case Identifier: {
				const char *value2 = current(state).data.identifier;
				advance_state(&state);

				expression_t member;
				member.type = Member;
				member.data.member.value1 = value;
				member.data.member.value2 = value2;

				switch (current(state).type) {
				case LeftParen:
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
	case LeftParen: {
		advance_state(&state);
		void *expr = parse_expression(state);
		switch (current(state).type) {
		case RightParen:
			break;
		default:
			exit(1); // Expected a closing bracket
		}
		expression_t grouping;
		grouping.type = Grouping;
		grouping.data.grouping = expr;
		return &grouping;
	}
	case Vec4: {
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
			case RightParen:
				done = true;
				break;
			default:
				expressions = parse_expression(state);
				++expressions;
				break;
			}
			advance_state(&state);
			switch (current(state).type) {
			case Comma:
				break;
			case RightParen:
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
	case LeftParen: {
		advance_state(&state);
		switch (current(state).type) {
		case RightParen: {
			advance_state(&state);

			CallStatement_t call;
			call.func = func;
			call.parameters = NULL;
			return &call;
		}
		default: {
			void *expr = parse_expression(state);
			switch (current(state).type) {
			case RightParen: {
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
	case Identifier:
		name = current(state).data.identifier;
		advance_state(&state);
		switch (current(state).type) {
		case LeftCurly: {
			advance_state(&state);
			switch (current(state).type) {
			case Identifier:
				member_name = current(state).data.identifier;
				advance_state(&state);
				switch (current(state).type) {
				case Colon: {
					advance_state(&state);
					switch (current(state).type) {
					case Identifier: {
						type_name = current(state).data.identifier;
						advance_state(&state);
						switch (current(state).type) {
						case Semicolon: {
							advance_state(&state);
							switch (current(state).type) {
							case RightCurly: {
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
	statement.type = Struct;
	StructStatement_t structy;
	structy.attribute = NULL;
	structy.name = name;
	structy.members = &member;
	statement.data.structy = structy;
	return &statement;
}
