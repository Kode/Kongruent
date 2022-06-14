#include "token.h"

#include <stdbool.h>
#include <string.h>

bool is_num(char ch) {
	return ch == '0' || ch == '1' || ch == '2' || ch == '3' || ch == '4' || ch == '5' || ch == '6' || ch == '7' || ch == '8' || ch == '9';
}

bool is_op(char ch) {
	return ch == '&' || ch == '|' || ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '=' || ch == '!' || ch == '<' || ch == '>' || ch == '%';
}

bool is_whitespace(char ch) {
	return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
}

void push_identifier(Token *tokens, const char *buffer) {
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

void push_attribute(Token *tokens, const char *buffer) {
	tokens.push(Token::Attribute(buffer.to_string()));
}

enum Mode {
	Select,
	Number,
	Stringy,
	Operator,
	Identifier,
	LineComment,
	Comment,
	Attribute,
};

typedef struct State {
	char *iterator;
	int next;
	int next_next;
} State_t;

State_t *state_create(char *it) {
	int next = it.next();
	int next_next = it.next();
	State_t *state = NULL;
	state->iterator = it;
	state->next = next;
	state->next_next = next_next;
	return state;
}

void state_advance(State *self) {
	self.next = self.next_next;
	self.next_next = self.iterator.next();
}

double parse_number(char *buffer) {
	return buffer.parse::<f64>().expect(&format !("Failed to parse \"{}\" as a number.", buffer));
}
