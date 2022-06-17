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

typedef struct token {
	enum {
		Eof,
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
	} type;

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

void add_token(token_array_t *arr, token_t token) {}
