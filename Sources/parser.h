#pragma once

#include "tokenizer.h"

#include <stdbool.h>

struct expression;

typedef struct expressions {
	struct expression *e[256];
	size_t size;
} expressions_t;

typedef struct expression {
	enum {
		EXPRESSION_BINARY,
		EXPRESSION_UNARY,
		EXPRESSION_BOOLEAN,
		EXPRESSION_NUMBER,
		EXPRESSION_STRING,
		EXPRESSION_VARIABLE,
		EXPRESSION_GROUPING,
		EXPRESSION_CALL,
		EXPRESSION_MEMBER,
		EXPRESSION_CONSTRUCTOR
	} type;

	union {
		struct {
			struct expression *left;
			operator_t op;
			struct expression *right;
		} binary;
		struct {
			operator_t op;
			struct expression *right;
		} unary;
		bool boolean;
		double number;
		char string[MAX_IDENTIFIER_SIZE];
		char variable[MAX_IDENTIFIER_SIZE];
		struct expression *grouping;
		struct {
			struct expression *func;
			struct expression *parameters;
		} call;
		struct {
			struct expression *left;
			struct expression *right;
		} member;
		struct {
			expressions_t parameters;
		} constructor;
	};
} expression_t;

typedef struct member {
	char name[MAX_IDENTIFIER_SIZE];
	char member_type[MAX_IDENTIFIER_SIZE];
} member_t;

typedef struct members {
	member_t m[16];
	size_t size;
} members_t;

struct statement;

typedef struct statements {
	struct statement *s[256];
	size_t size;
} statements_t;

typedef struct statement {
	enum { STATEMENT_EXPRESSION, STATEMENT_RETURN_EXPRESSION, STATEMENT_IF, STATEMENT_BLOCK, STATEMENT_LOCAL_VARIABLE } type;

	union {
		expression_t *expression;
		struct {
			expression_t *test;
			struct statement *block;
		} iffy;
		struct {
			statements_t statements;
		} block;
		struct {
			char name[MAX_IDENTIFIER_SIZE];
			char type_name[MAX_IDENTIFIER_SIZE];
			expression_t *init;
		} local_variable;
	};
} statement_t;

struct definition;

typedef struct definitions {
	struct definition *d[256];
	size_t size;
} definitions_t;

typedef struct definition {
	enum { DEFINITION_FUNCTION, DEFINITION_STRUCT } type;

	union {
		struct {
			char attribute[MAX_IDENTIFIER_SIZE];
			char name[MAX_IDENTIFIER_SIZE];
			char return_type_name[MAX_IDENTIFIER_SIZE];
			char parameter_name[MAX_IDENTIFIER_SIZE];
			char parameter_type_name[MAX_IDENTIFIER_SIZE];
			struct statement *block;
		} function;
		struct {
			char attribute[MAX_IDENTIFIER_SIZE];
			char name[MAX_IDENTIFIER_SIZE];
			members_t members;
		} structy;
	};
} definition_t;

definitions_t parse(tokens_t *tokens);
