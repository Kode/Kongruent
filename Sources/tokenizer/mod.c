#include "token.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

void add_token(token_array_t *arr, token_t token) {}

token_t get_token(token_array_t *arr, unsigned index) {
	token_t token;
	token.type = TOKEN_EOF;
	return token;
}

bool is_num(char ch) {
	return ch == '0' || ch == '1' || ch == '2' || ch == '3' || ch == '4' || ch == '5' || ch == '6' || ch == '7' || ch == '8' || ch == '9';
}

bool is_op(char ch) {
	return ch == '&' || ch == '|' || ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '=' || ch == '!' || ch == '<' || ch == '>' || ch == '%';
}

bool is_whitespace(char ch) {
	return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
}

void push_identifier(token_array_t *tokens, const char *buffer) {
	if (strcmp(buffer, "true") == 0) {
	}
	else if (strcmp(buffer, "false") == 0) {
	}
	else if (strcmp(buffer, "if") == 0) {
	}
	else if (strcmp(buffer, "float") == 0) {
	}
	else if (strcmp(buffer, "in") == 0) {
	}
	else if (strcmp(buffer, "vec3") == 0) {
	}
	else if (strcmp(buffer, "vec4") == 0) {
	}
	else if (strcmp(buffer, "void") == 0) {
	}
	else if (strcmp(buffer, "struct") == 0) {
	}
	else if (strcmp(buffer, "fn") == 0) {
	}
	else if (strcmp(buffer, "let") == 0) {
	}
	else if (strcmp(buffer, "mut") == 0) {
	}
	else {
	}
	/*match buffer {
	    "true" = > tokens.push(Token::Boolean(true)), "false" = > tokens.push(Token::Boolean(false)), "if" = > tokens.push(Token::If),
	    "float" = > tokens.push(Token::Float), "in" = > tokens.push(Token::In), "vec3" = > tokens.push(Token::Vec3), "vec4" = > tokens.push(Token::Vec4),
	    "void" = > tokens.push(Token::Void), "struct" = > tokens.push(Token::Struct), "fn" = > tokens.push(Token::Function), "let" = > tokens.push(Token::Let),
	    "mut" = > tokens.push(Token::Mut), _ = > tokens.push(Token::Identifier(buffer.to_string())),
	}*/
}

void push_attribute(token_array_t *tokens, const char *buffer) {
	token_t token;
	token.type = Attribute;
	token.data.attribute = buffer;
	add_token(tokens, token);
}

typedef enum mode {
	MODE_SELECT,
	MODE_NUMBER,
	MODE_STRING,
	MODE_OPERATOR,
	MODE_IDENTIFIER,
	MODE_LINE_COMMENT,
	MODE_COMMENT,
	MODE_ATTRIBUTE,
} mode_t;

typedef struct State {
	const char *iterator;
	int next;
	int next_next;
} State_t;

State_t state_create(const char *it) {
	int next = *(++it);
	int next_next = *(++it);
	State_t state;
	state.iterator = it;
	state.next = next;
	state.next_next = next_next;
	return state;
}

void state_advance(State_t *self) {
	self->next = self->next_next;
	self->next_next = *(++self->iterator);
}

double parse_number(const char *buffer) {
	return atoi(buffer);
}

void add_to_buffer(const char *buffer, char ch) {}

token_array_t tokenize(const char *source) {
	token_array_t tokens;
	mode_t mode = MODE_SELECT;
	const char *buffer;

	const char *it = source;
	State_t state = state_create(it);

	for (;;) {
		if (state.next < 0) {
			switch (mode) {
			case MODE_IDENTIFIER:
				push_identifier(&tokens, buffer);
				break;
			case MODE_ATTRIBUTE:
				push_attribute(&tokens, buffer);
				break;
			case MODE_NUMBER: {
				token_t token;
				token.type = Number;
				token.data.number = parse_number(buffer);
				add_token(&tokens, token);
				break;
			}
			case MODE_SELECT:
			case MODE_LINE_COMMENT:
				break;
			case MODE_STRING:
				exit(1); // Unclosed string
			case MODE_OPERATOR:
				exit(1); // File ends with an operator
			case MODE_COMMENT:
				exit(1); // Unclosed comment
			}
			return tokens;
		}
		else {
			char ch = (char)state.next;
			switch (mode) {
			case MODE_SELECT: {
				if (ch == '/') {
					if (state.next_next >= 0) {
						char chch = (char)state.next_next;
						switch (chch) {
						case '/':
							mode = MODE_LINE_COMMENT;
							break;
						case '*':
							mode = MODE_COMMENT;
							break;
						default:
							buffer = ch;
							mode = MODE_OPERATOR;
						}
					}
				}
				else if (ch == '#') {
					state_advance(&state);
					if (state.next >= 0) {
						char ch = (char)state.next;
						if (ch != '[') {
							exit(1); // Expected [
						}
					}
					else {
						exit(1); // Expected [
					}

					mode = MODE_ATTRIBUTE;
					buffer = "";
				}
				else if (is_num(ch)) {
					mode = MODE_NUMBER;
					buffer = ch;
				}
				else if (is_op(ch)) {
					mode = MODE_OPERATOR;
					buffer = ch;
				}
				else if (is_whitespace(ch)) {
				}
				else if (ch == '(') {
					token_t token;
					token.type = LeftParen;
					add_token(&tokens, token);
				}
				else if (ch == ')') {
					token_t token;
					token.type = RightParen;
					add_token(&tokens, token);
				}
				else if (ch == '{') {
					token_t token;
					token.type = LeftCurly;
					add_token(&tokens, token);
				}
				else if (ch == '}') {
					token_t token;
					token.type = RightCurly;
					add_token(&tokens, token);
				}
				else if (ch == ';') {
					token_t token;
					token.type = Semicolon;
					add_token(&tokens, token);
				}
				else if (ch == '.') {
					token_t token;
					token.type = Dot;
					add_token(&tokens, token);
				}
				else if (ch == ':') {
					token_t token;
					token.type = Colon;
					add_token(&tokens, token);
				}
				else if (ch == ',') {
					token_t token;
					token.type = Comma;
					add_token(&tokens, token);
				}
				else if (ch == '"' || ch == '\'') {
					mode = MODE_STRING;
					buffer = "";
				}
				else {
					mode = MODE_IDENTIFIER;
					buffer = ch;
				}
				state_advance(&state);
				break;
			}
			case MODE_LINE_COMMENT: {
				if (ch == '\n') {
					mode = MODE_SELECT;
				}
				state_advance(&state);
				break;
			}
			case MODE_COMMENT: {
				if (ch == '*') {
					if (state.next_next >= 0) {
						char chch = (char)state.next_next;
						if (chch == '/') {
							mode = MODE_SELECT;
							state_advance(&state);
						}
					}
				}
				state_advance(&state);
				break;
			}
			case MODE_NUMBER: {
				if (is_num(ch) || ch == '.') {
					add_to_buffer(buffer, ch);
					state_advance(&state);
				}
				else {
					token_t token;
					token.type = Number;
					token.data.number = parse_number(buffer);
					add_token(&tokens, token);
					mode = MODE_SELECT;
				}
				break;
			}
			case MODE_OPERATOR: {
				const char *long_op = buffer;
				add_to_buffer(long_op, ch);
				if (long_op == "==" || long_op == "!=" || long_op == "<=" || long_op == ">=" || long_op == "||" || long_op == "&&" || long_op == "->") {
					add_to_buffer(buffer, ch);
					state_advance(&state);
				}
				else {
					if (strcmp(buffer, "==") == 0) {
						token_t token;
						token.type = Operator;
						token.data.op = Equals;
						add_token(&tokens, token);
					}
					else if (strcmp(buffer, "!=")) {
						token_t token;
						token.type = Operator;
						token.data.op = NotEquals;
						add_token(&tokens, token);
					}
					else if (strcmp(buffer, ">")) {
						token_t token;
						token.type = Operator;
						token.data.op = Greater;
						add_token(&tokens, token);
					}
					else if (strcmp(buffer, ">=")) {
						token_t token;
						token.type = Operator;
						token.data.op = GreaterEqual;
						add_token(&tokens, token);
					}
					else if (strcmp(buffer, "<")) {
						token_t token;
						token.type = Operator;
						token.data.op = Less;
						add_token(&tokens, token);
					}
					else if (strcmp(buffer, "<=")) {
						token_t token;
						token.type = Operator;
						token.data.op = LessEqual;
						add_token(&tokens, token);
					}
					else if (strcmp(buffer, "-")) {
						token_t token;
						token.type = Operator;
						token.data.op = Minus;
						add_token(&tokens, token);
					}
					else if (strcmp(buffer, "+")) {
						token_t token;
						token.type = Operator;
						token.data.op = Plus;
						add_token(&tokens, token);
					}
					else if (strcmp(buffer, "/")) {
						token_t token;
						token.type = Operator;
						token.data.op = Div;
						add_token(&tokens, token);
					}
					else if (strcmp(buffer, "*")) {
						token_t token;
						token.type = Operator;
						token.data.op = Multiply;
						add_token(&tokens, token);
					}
					else if (strcmp(buffer, "!")) {
						token_t token;
						token.type = Operator;
						token.data.op = Not;
						add_token(&tokens, token);
					}
					else if (strcmp(buffer, "||")) {
						token_t token;
						token.type = Operator;
						token.data.op = Or;
						add_token(&tokens, token);
					}
					else if (strcmp(buffer, "&&")) {
						token_t token;
						token.type = Operator;
						token.data.op = And;
						add_token(&tokens, token);
					}
					else if (strcmp(buffer, "%")) {
						token_t token;
						token.type = Operator;
						token.data.op = Mod;
						add_token(&tokens, token);
					}
					else if (strcmp(buffer, "=")) {
						token_t token;
						token.type = Operator;
						token.data.op = Assign;
						add_token(&tokens, token);
					}
					else if (strcmp(buffer, "->")) {
						token_t token;
						token.type = Operator;
						token.data.op = Equals;
						add_token(&tokens, token);
					}
					else {
						exit(1); // Weird operator
					}
				}
				mode = MODE_SELECT;
				break;
			}
			case MODE_STRING: {
				if (ch == '"' || ch == '\'') {
					token_t token;
					token.type = String;
					token.data.string = buffer;
					add_token(&tokens, token);

					state_advance(&state);
					mode = MODE_SELECT;
				}
				else {
					add_to_buffer(buffer, ch);
					state_advance(&state);
				}
				break;
			}
			case MODE_IDENTIFIER: {
				if (is_op(ch) || ch == ' ' || ch == '(' || ch == ')' || ch == '{' || ch == '}' || ch == '"' || ch == '\'' || ch == ';' || ch == '.' ||
				    ch == ',' || ch == ':' || ch == '\n' || ch == '\r') {
					push_identifier(&tokens, buffer);
					mode = MODE_SELECT;
				}
				else {
					add_to_buffer(buffer, ch);
					state_advance(&state);
				}
				break;
			}
			case MODE_ATTRIBUTE: {
				if (ch == ']') {
					push_attribute(&tokens, buffer);
					state_advance(&state);
					mode = MODE_SELECT;
				}
				else {
					add_to_buffer(buffer, ch);
					state_advance(&state);
				}
				break;
			}
			}
		}
	}
}
