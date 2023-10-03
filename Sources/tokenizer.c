#include "tokenizer.h"
#include "errors.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

token tokens_get(tokens *tokens, size_t index) {
	assert(tokens->current_size > index);
	return tokens->t[index];
}

static bool is_num(char ch) {
	return ch == '0' || ch == '1' || ch == '2' || ch == '3' || ch == '4' || ch == '5' || ch == '6' || ch == '7' || ch == '8' || ch == '9';
}

static bool is_op(char ch) {
	return ch == '&' || ch == '|' || ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '=' || ch == '!' || ch == '<' || ch == '>' || ch == '%';
}

static bool is_whitespace(char ch) {
	return ch == ' ' || (ch >= 9 && ch <= 13);
}

typedef enum mode {
	MODE_SELECT,
	MODE_NUMBER,
	// MODE_STRING,
	MODE_OPERATOR,
	MODE_IDENTIFIER,
	MODE_LINE_COMMENT,
	MODE_COMMENT,
	MODE_ATTRIBUTE,
} mode;

typedef struct tokenizer_state {
	const char *iterator;
	char next;
	char next_next;
	int line, column;
	bool line_end;
} tokenizer_state;

static void tokenizer_state_init(tokenizer_state *state, const char *source) {
	state->line = state->column = 0;
	state->iterator = source;
	state->next = *state->iterator;
	if (*state->iterator != 0) {
		state->iterator += 1;
	}
	state->next_next = *state->iterator;
	state->line_end = false;
}

static void tokenizer_state_advance(tokenizer_state *state) {
	state->next = state->next_next;
	if (*state->iterator != 0) {
		state->iterator += 1;
	}
	state->next_next = *state->iterator;

	if (state->line_end) {
		state->line_end = false;
		state->line += 1;
		state->column = 0;
	}
	else {
		state->column += 1;
	}

	if (state->next == '\n') {
		state->line_end = true;
	}
}

typedef struct tokenizer_buffer {
	char *buf;
	size_t current_size;
	size_t max_size;
	int column, line;
} tokenizer_buffer;

static void tokenizer_buffer_init(tokenizer_buffer *buffer) {
	buffer->max_size = 1024 * 1024;
	buffer->buf = (char *)malloc(buffer->max_size);
	buffer->current_size = 0;
	buffer->column = buffer->line = 0;
}

static void tokenizer_buffer_reset(tokenizer_buffer *buffer, tokenizer_state *state) {
	buffer->current_size = 0;
	buffer->column = state->column;
	buffer->line = state->line;
}

static void tokenizer_buffer_add(tokenizer_buffer *buffer, char ch) {
	assert(buffer->current_size < buffer->max_size);
	buffer->buf[buffer->current_size] = ch;
	buffer->current_size += 1;
}

static bool tokenizer_buffer_equals(tokenizer_buffer *buffer, const char *str) {
	buffer->buf[buffer->current_size] = 0;
	return strcmp(buffer->buf, str) == 0;
}

static name_id tokenizer_buffer_to_name(tokenizer_buffer *buffer) {
	assert(buffer->current_size < buffer->max_size);
	buffer->buf[buffer->current_size] = 0;
	buffer->current_size += 1;
	return add_name(buffer->buf);
}

static double tokenizer_buffer_parse_number(tokenizer_buffer *buffer) {
	char *end = &buffer->buf[buffer->current_size];
	return strtod(buffer->buf, &end);
}

token token_create(int kind, tokenizer_state *state) {
	token token;
	token.kind = kind;
	token.column = state->column;
	token.line = state->line;
	return token;
}

static void tokens_init(tokens *tokens) {
	tokens->max_size = 1024 * 1024;
	tokens->t = malloc(tokens->max_size * sizeof(token));
	tokens->current_size = 0;
}

static void tokens_add(tokens *tokens, token token) {
	tokens->t[tokens->current_size] = token;
	tokens->current_size += 1;
	assert(tokens->current_size <= tokens->max_size);
}

static void tokens_add_identifier(tokenizer_state *state, tokens *tokens, tokenizer_buffer *buffer) {
	token token;

	if (tokenizer_buffer_equals(buffer, "true")) {
		token = token_create(TOKEN_BOOLEAN, state);
		token.boolean = true;
	}
	else if (tokenizer_buffer_equals(buffer, "false")) {
		token = token_create(TOKEN_BOOLEAN, state);
		token.boolean = false;
	}
	else if (tokenizer_buffer_equals(buffer, "if")) {
		token = token_create(TOKEN_IF, state);
	}
	else if (tokenizer_buffer_equals(buffer, "in")) {
		token = token_create(TOKEN_IN, state);
	}
	else if (tokenizer_buffer_equals(buffer, "void")) {
		token = token_create(TOKEN_VOID, state);
	}
	else if (tokenizer_buffer_equals(buffer, "struct")) {
		token = token_create(TOKEN_STRUCT, state);
	}
	else if (tokenizer_buffer_equals(buffer, "fun")) {
		token = token_create(TOKEN_FUNCTION, state);
	}
	else if (tokenizer_buffer_equals(buffer, "var")) {
		token = token_create(TOKEN_VAR, state);
	}
	else if (tokenizer_buffer_equals(buffer, "const")) {
		token = token_create(TOKEN_CONST, state);
	}
	else if (tokenizer_buffer_equals(buffer, "return")) {
		token = token_create(TOKEN_RETURN, state);
	}
	else {
		token = token_create(TOKEN_IDENTIFIER, state);
		token.identifier = tokenizer_buffer_to_name(buffer);
	}

	token.column = buffer->column;
	token.line = buffer->line;
	tokens_add(tokens, token);
}

static void tokens_add_attribute(tokenizer_state *state, tokens *tokens, tokenizer_buffer *buffer) {
	token token = token_create(TOKEN_ATTRIBUTE, state);
	token.attribute = tokenizer_buffer_to_name(buffer);
	token.column = buffer->column;
	token.line = buffer->line;
	tokens_add(tokens, token);
}

tokens tokenize(const char *source) {
	mode mode = MODE_SELECT;

	tokens tokens;
	tokens_init(&tokens);

	tokenizer_state state;
	tokenizer_state_init(&state, source);

	tokenizer_buffer buffer;
	tokenizer_buffer_init(&buffer);

	for (;;) {
		if (state.next == 0) {
			switch (mode) {
			case MODE_IDENTIFIER:
				tokens_add_identifier(&state, &tokens, &buffer);
				break;
			case MODE_ATTRIBUTE:
				tokens_add_attribute(&state, &tokens, &buffer);
				break;
			case MODE_NUMBER: {
				token token = token_create(TOKEN_NUMBER, &state);
				token.number = tokenizer_buffer_parse_number(&buffer);
				tokens_add(&tokens, token);
				break;
			}
			case MODE_SELECT:
			case MODE_LINE_COMMENT:
				break;
			// case MODE_STRING:
			//	error("Unclosed string", state.column, state.line);
			case MODE_OPERATOR:
				error(state.column, state.line, "File ends with an operator");
			case MODE_COMMENT:
				error(state.column, state.line, "Unclosed comment");
			}

			tokens_add(&tokens, token_create(TOKEN_NONE, &state));
			return tokens;
		}
		else {
			char ch = (char)state.next;
			switch (mode) {
			case MODE_SELECT: {
				if (ch == '/') {
					if (state.next_next >= 0) {
						char chch = state.next_next;
						switch (chch) {
						case '/':
							mode = MODE_LINE_COMMENT;
							break;
						case '*':
							mode = MODE_COMMENT;
							break;
						default:
							tokenizer_buffer_reset(&buffer, &state);
							tokenizer_buffer_add(&buffer, ch);
							mode = MODE_OPERATOR;
						}
					}
				}
				else if (ch == '#') {
					tokenizer_state_advance(&state);
					if (state.next >= 0) {
						char ch = state.next;
						if (ch != '[') {
							error(state.column, state.line, "Expected [");
						}
					}
					else {
						error(state.column, state.line, "Expected [");
					}

					mode = MODE_ATTRIBUTE;
					tokenizer_buffer_reset(&buffer, &state);
				}
				else if (is_num(ch)) {
					mode = MODE_NUMBER;
					tokenizer_buffer_reset(&buffer, &state);
					tokenizer_buffer_add(&buffer, ch);
				}
				else if (is_op(ch)) {
					mode = MODE_OPERATOR;
					tokenizer_buffer_reset(&buffer, &state);
					tokenizer_buffer_add(&buffer, ch);
				}
				else if (is_whitespace(ch)) {
				}
				else if (ch == '(') {
					tokens_add(&tokens, token_create(TOKEN_LEFT_PAREN, &state));
				}
				else if (ch == ')') {
					tokens_add(&tokens, token_create(TOKEN_RIGHT_PAREN, &state));
				}
				else if (ch == '{') {
					tokens_add(&tokens, token_create(TOKEN_LEFT_CURLY, &state));
				}
				else if (ch == '}') {
					tokens_add(&tokens, token_create(TOKEN_RIGHT_CURLY, &state));
				}
				else if (ch == '[') {
					tokens_add(&tokens, token_create(TOKEN_LEFT_SQUARE, &state));
				}
				else if (ch == ']') {
					tokens_add(&tokens, token_create(TOKEN_RIGHT_SQUARE, &state));
				}
				else if (ch == ';') {
					tokens_add(&tokens, token_create(TOKEN_SEMICOLON, &state));
				}
				else if (ch == '.') {
					tokens_add(&tokens, token_create(TOKEN_DOT, &state));
				}
				else if (ch == ':') {
					tokens_add(&tokens, token_create(TOKEN_COLON, &state));
				}
				else if (ch == ',') {
					tokens_add(&tokens, token_create(TOKEN_COMMA, &state));
				}
				else if (ch == '"' || ch == '\'') {
					// mode = MODE_STRING;
					// tokenizer_buffer_reset(&buffer, &state);
					error(state.column, state.line, "Strings are not supported");
				}
				else {
					mode = MODE_IDENTIFIER;
					tokenizer_buffer_reset(&buffer, &state);
					tokenizer_buffer_add(&buffer, ch);
				}
				tokenizer_state_advance(&state);
				break;
			}
			case MODE_LINE_COMMENT: {
				if (ch == '\n') {
					mode = MODE_SELECT;
				}
				tokenizer_state_advance(&state);
				break;
			}
			case MODE_COMMENT: {
				if (ch == '*') {
					if (state.next_next >= 0) {
						char chch = (char)state.next_next;
						if (chch == '/') {
							mode = MODE_SELECT;
							tokenizer_state_advance(&state);
						}
					}
				}
				tokenizer_state_advance(&state);
				break;
			}
			case MODE_NUMBER: {
				if (is_num(ch) || ch == '.') {
					tokenizer_buffer_add(&buffer, ch);
					tokenizer_state_advance(&state);
				}
				else {
					token token = token_create(TOKEN_NUMBER, &state);
					token.number = tokenizer_buffer_parse_number(&buffer);
					tokens_add(&tokens, token);
					mode = MODE_SELECT;
				}
				break;
			}
			case MODE_OPERATOR: {
				char long_op[3];
				long_op[0] = 0;
				if (buffer.current_size == 1) {
					long_op[0] = buffer.buf[0];
					long_op[1] = ch;
					long_op[2] = 0;
				}

				if (strcmp(long_op, "==") == 0 || strcmp(long_op, "!=") == 0 || strcmp(long_op, "<=") == 0 || strcmp(long_op, ">=") == 0 ||
				    strcmp(long_op, "||") == 0 || strcmp(long_op, "&&") == 0 || strcmp(long_op, "->") == 0) {
					tokenizer_buffer_add(&buffer, ch);
					tokenizer_state_advance(&state);
				}

				if (tokenizer_buffer_equals(&buffer, "==")) {
					token token = token_create(TOKEN_OPERATOR, &state);
					token.op = OPERATOR_EQUALS;
					tokens_add(&tokens, token);
				}
				else if (tokenizer_buffer_equals(&buffer, "!=")) {
					token token = token_create(TOKEN_OPERATOR, &state);
					token.op = OPERATOR_NOT_EQUALS;
					tokens_add(&tokens, token);
				}
				else if (tokenizer_buffer_equals(&buffer, ">")) {
					token token = token_create(TOKEN_OPERATOR, &state);
					token.op = OPERATOR_GREATER;
					tokens_add(&tokens, token);
				}
				else if (tokenizer_buffer_equals(&buffer, ">=")) {
					token token = token_create(TOKEN_OPERATOR, &state);
					token.op = OPERATOR_GREATER_EQUAL;
					tokens_add(&tokens, token);
				}
				else if (tokenizer_buffer_equals(&buffer, "<")) {
					token token = token_create(TOKEN_OPERATOR, &state);
					token.op = OPERATOR_LESS;
					tokens_add(&tokens, token);
				}
				else if (tokenizer_buffer_equals(&buffer, "<=")) {
					token token = token_create(TOKEN_OPERATOR, &state);
					token.op = OPERATOR_LESS_EQUAL;
					tokens_add(&tokens, token);
				}
				else if (tokenizer_buffer_equals(&buffer, "-")) {
					token token = token_create(TOKEN_OPERATOR, &state);
					token.op = OPERATOR_MINUS;
					tokens_add(&tokens, token);
				}
				else if (tokenizer_buffer_equals(&buffer, "+")) {
					token token = token_create(TOKEN_OPERATOR, &state);
					token.op = OPERATOR_PLUS;
					tokens_add(&tokens, token);
				}
				else if (tokenizer_buffer_equals(&buffer, "/")) {
					token token = token_create(TOKEN_OPERATOR, &state);
					token.op = OPERATOR_DIVIDE;
					tokens_add(&tokens, token);
				}
				else if (tokenizer_buffer_equals(&buffer, "*")) {
					token token = token_create(TOKEN_OPERATOR, &state);
					token.op = OPERATOR_MULTIPLY;
					tokens_add(&tokens, token);
				}
				else if (tokenizer_buffer_equals(&buffer, "!")) {
					token token = token_create(TOKEN_OPERATOR, &state);
					token.op = OPERATOR_NOT;
					tokens_add(&tokens, token);
				}
				else if (tokenizer_buffer_equals(&buffer, "||")) {
					token token = token_create(TOKEN_OPERATOR, &state);
					token.op = OPERATOR_OR;
					tokens_add(&tokens, token);
				}
				else if (tokenizer_buffer_equals(&buffer, "&&")) {
					token token = token_create(TOKEN_OPERATOR, &state);
					token.op = OPERATOR_AND;
					tokens_add(&tokens, token);
				}
				else if (tokenizer_buffer_equals(&buffer, "%")) {
					token token = token_create(TOKEN_OPERATOR, &state);
					token.op = OPERATOR_MOD;
					tokens_add(&tokens, token);
				}
				else if (tokenizer_buffer_equals(&buffer, "=")) {
					token token = token_create(TOKEN_OPERATOR, &state);
					token.op = OPERATOR_ASSIGN;
					tokens_add(&tokens, token);
				}
				else {
					error(state.column, state.line, "Weird operator");
				}

				mode = MODE_SELECT;
				break;
			}
			/*case MODE_STRING: {
				if (ch == '"' || ch == '\'') {
				    token token = token_create(TOKEN_STRING, &state);
				    tokenizer_buffer_copy_to_string(&buffer, token.string);
				    token.column = buffer.column;
				    token.line = buffer.line;
				    tokens_add(&tokens, token);

				    tokenizer_state_advance(&state);
				    mode = MODE_SELECT;
				}
				else {
				    tokenizer_buffer_add(&buffer, ch);
				    tokenizer_state_advance(&state);
				}
				break;
			}*/
			case MODE_IDENTIFIER: {
				if (is_whitespace(ch) || is_op(ch) || ch == '(' || ch == ')' || ch == '{' || ch == '}' || ch == '[' || ch == ']' || ch == '"' || ch == '\'' ||
				    ch == ';' || ch == '.' || ch == ',' || ch == ':') {
					tokens_add_identifier(&state, &tokens, &buffer);
					mode = MODE_SELECT;
				}
				else {
					tokenizer_buffer_add(&buffer, ch);
					tokenizer_state_advance(&state);
				}
				break;
			}
			case MODE_ATTRIBUTE: {
				if (ch == ']') {
					tokens_add_attribute(&state, &tokens, &buffer);
					tokenizer_state_advance(&state);
					mode = MODE_SELECT;
				}
				else {
					tokenizer_buffer_add(&buffer, ch);
					tokenizer_state_advance(&state);
				}
				break;
			}
			}
		}
	}
}
