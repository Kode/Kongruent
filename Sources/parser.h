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

typedef struct DeclarationStatement {
	const char *name;
	expression_t *init;
} DeclarationStatement_t;

typedef struct BlockStatement {
	void *statements;
} BlockStatement_t;

typedef struct FunctionStatement {
	const char **parameters;
	void *block;
} FunctionStatement_t;

typedef struct IfStatement {
	void *test;
	void *block;
} IfStatement_t;

typedef struct CallStatement {
	expression_t func;
	expression_t *parameters;
} CallStatement_t;

typedef struct PreprocessorStatement {
	const char *name;
	expression_t *parameters;
} PreprocessorStatement_t;

typedef struct Member {
	const char *name;
	const char *member_type;
} Member_t;

typedef struct StructStatement {
	const char *attribute;
	const char *name;
	struct Member *members;
} StructStatement_t;

typedef struct statement {
	enum {
		Expression,
		STATEMENT_IF,
		Block,
		Declaration,
		PreprocessorDirective,
		STATEMENT_FUNCTION,
		STATEMENT_STRUCT,
	} type;

	union {
		expression_t expression;
		IfStatement_t iffy;
		BlockStatement_t block;
		DeclarationStatement_t declaration;
		PreprocessorStatement_t preprocessorDirective;
		FunctionStatement_t function;
		StructStatement_t structy;
	} data;
} statement_t;

void add_statement(statement_t *statements, void *statement);
