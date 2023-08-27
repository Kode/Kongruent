#pragma once

#include "names.h"

#include <stdbool.h>

typedef enum operatorr {
	OPERATOR_EQUALS,
	OPERATOR_NOT_EQUALS,
	OPERATOR_GREATER,
	OPERATOR_GREATER_EQUAL,
	OPERATOR_LESS,
	OPERATOR_LESS_EQUAL,
	OPERATOR_MINUS,
	OPERATOR_PLUS,
	OPERATOR_DIVIDE,
	OPERATOR_MULTIPLY,
	OPERATOR_NOT,
	OPERATOR_OR,
	OPERATOR_AND,
	OPERATOR_MOD,
	OPERATOR_ASSIGN
} operatorr;

typedef struct token {
	int line, column;

	enum {
		TOKEN_EOF,
		TOKEN_BOOLEAN,
		TOKEN_NUMBER,
		// TOKEN_STRING,
		TOKEN_IDENTIFIER,
		TOKEN_ATTRIBUTE,
		TOKEN_LEFT_PAREN,
		TOKEN_RIGHT_PAREN,
		TOKEN_LEFT_CURLY,
		TOKEN_RIGHT_CURLY,
		TOKEN_IF,
		TOKEN_SEMICOLON,
		TOKEN_COLON,
		TOKEN_DOT,
		TOKEN_COMMA,
		TOKEN_OPERATOR,
		TOKEN_IN,
		TOKEN_VOID,
		TOKEN_STRUCT,
		TOKEN_FUNCTION,
		TOKEN_VAR,
		TOKEN_TEX2D,
		TOKEN_SAMPLER,
		TOKEN_RETURN
	} kind;

	union {
		bool boolean;
		double number;
		// char string[MAX_IDENTIFIER_SIZE];
		name_id identifier;
		name_id attribute;
		operatorr op;
	};
} token;

typedef struct tokens {
	token *t;
	size_t current_size;
	size_t max_size;
} tokens;

token tokens_get(tokens *arr, size_t index);

tokens tokenize(const char *source);
