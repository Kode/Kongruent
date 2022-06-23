#pragma once

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
	OPERATOR_ASSIGN,
} operator_t;

#define MAX_IDENTIFIER_SIZE 1024

typedef struct token {
	enum {
		TOKEN_EOF,
		TOKEN_BOOLEAN,
		TOKEN_NUMBER,
		TOKEN_STRING,
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
		TOKEN_LET,
		TOKEN_MUT
	} type;

	union {
		bool boolean;
		double number;
		char string[MAX_IDENTIFIER_SIZE];
		char identifier[MAX_IDENTIFIER_SIZE];
		char attribute[MAX_IDENTIFIER_SIZE];
		operator_t op;
	};
} token_t;

typedef struct tokens {
	token_t *t;
	size_t current_size;
	size_t max_size;
} tokens_t;

token_t tokens_get(tokens_t *arr, size_t index);

tokens_t tokenize(const char *source);
