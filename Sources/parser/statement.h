#pragma once

#include "expression.h"

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
