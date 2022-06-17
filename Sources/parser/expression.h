#pragma once

#include <stdbool.h>

struct expression;
typedef struct expression expression_t;

struct MemberExpression {
	const char *value1;
	const char *value2;
};

typedef struct ExprBinary {
	expression_t *left;
	Operator op;
	expression_t *right;
} ExprBinary_t;

typedef struct ExprUnary {
	Operator op;
	expression_t *right;
} ExprUnary_t;

struct ConstructorExpression {
	expression_t *parameters;
};

struct expression_t {
	enum { Binary, Unary, Boolean, Number, String, Variable, Grouping, Call, Member, Constructor } type;

	union {
		ExprBinary_t binary;
		ExprUnary_t unary;
		bool boolean;
		float number;
		const char *string;
		const char *variable;
		expression_t *grouping;
		CallStatement call;
		MemberExpression member;
		ConstructorExpression constructor;
	} data;
};
