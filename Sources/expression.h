#pragma once

#include "tokenizer.h"

#include <stdbool.h>

struct expression;

typedef struct MemberExpression {
	const char *value1;
	const char *value2;
} MemberExpression_t;

typedef struct ExprBinary {
	struct expression *left;
	operator_t op;
	struct expression *right;
} ExprBinary_t;

typedef struct ExprUnary {
	operator_t op;
	struct expression *right;
} ExprUnary_t;

typedef struct ConstructorExpression {
	struct expression *parameters;
} ConstructorExpression_t;

struct CallStatement;

typedef struct expression {
	enum { Binary, Unary, EXPRESSION_BOOLEAN, EXPRESSION_NUMBER, EXPRESSION_STRING, Variable, Grouping, Call, Member, Constructor } type;

	union {
		ExprBinary_t binary;
		ExprUnary_t unary;
		bool boolean;
		float number;
		const char *string;
		const char *variable;
		struct expression *grouping;
		struct CallStatement *call;
		MemberExpression_t member;
		ConstructorExpression_t constructor;
	} data;
} expression_t;
