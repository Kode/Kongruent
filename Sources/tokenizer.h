#pragma once

#include <stdbool.h>

typedef enum operatorr {
	Equals,
	NotEquals,
	Greater,
	GreaterEqual,
	Less,
	LessEqual,
	Minus,
	Plus,
	Div,
	Multiply,
	Not,
	Or,
	And,
	Mod,
	Assign,
} operator_t;

typedef enum token_type {
	TOKEN_EOF,
	Boolean,
	Number,
	String,
	Identifier,
	Attribute,
	LeftParen,
	RightParen,
	LeftCurly,
	RightCurly,
	If,
	Semicolon,
	Colon,
	Dot,
	Comma,
	Operator,
	Float,
	Vec3,
	Vec4,
	In,
	Void,
	Struct,
	Function,
	Let,
	Mut,
	FuncRet
} token_type_t;

typedef struct token {
	token_type_t type;

	union {
		bool boolean;
		double number;
		const char *string;
		const char *identifier;
		const char *attribute;
		operator_t op;
	} data;
} token_t;

typedef struct token_array {
	int a;
} token_array_t;

void add_token(token_array_t *arr, token_t token);

token_t get_token(token_array_t *arr, unsigned index);

token_array_t tokenize(const char *source);
