#pragma once

#include <stdbool.h>

typedef enum Operator {
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
} Operator_t;

struct Token {
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

	union data {
		bool boolean;
		double number;
		const char *string;
		const char *identifier;
		const char *attribute;
		Operator_t operator;
	};
};
