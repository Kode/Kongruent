#pragma once

#include "names.h"
#include "tokenizer.h"

#include <stdbool.h>

struct expression;

typedef struct expressions {
	struct expression *e[256];
	size_t size;
} expressions;

typedef struct expression {
	enum {
		EXPRESSION_BINARY,
		EXPRESSION_UNARY,
		EXPRESSION_BOOLEAN,
		EXPRESSION_NUMBER,
		// EXPRESSION_STRING,
		EXPRESSION_VARIABLE,
		EXPRESSION_GROUPING,
		EXPRESSION_CALL,
		EXPRESSION_MEMBER,
		EXPRESSION_CONSTRUCTOR
	} type;

	union {
		struct {
			struct expression *left;
			operatorr op;
			struct expression *right;
		} binary;
		struct {
			operatorr op;
			struct expression *right;
		} unary;
		bool boolean;
		double number;
		// char string[MAX_IDENTIFIER_SIZE];
		name_id variable;
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
			expressions parameters;
		} constructor;
	};
} expression;

typedef struct member {
	name_id name;
	name_id member_type;
} member;

typedef struct members {
	member m[16];
	size_t size;
} members;

struct statement;

typedef struct statements {
	struct statement *s[256];
	size_t size;
} statements;

typedef struct statement {
	enum { STATEMENT_EXPRESSION, STATEMENT_RETURN_EXPRESSION, STATEMENT_IF, STATEMENT_BLOCK, STATEMENT_LOCAL_VARIABLE } type;

	union {
		expression *expression;
		struct {
			expression *test;
			struct statement *block;
		} iffy;
		struct {
			statements statements;
		} block;
		struct {
			name_id name;
			name_id type_name;
			expression *init;
		} local_variable;
	};
} statement;

struct definition;

typedef struct definitions {
	struct definition *d[256];
	size_t size;
} definitions;

typedef struct definition {
	enum { DEFINITION_FUNCTION, DEFINITION_STRUCT } type;

	union {
		struct {
			name_id attribute;
			name_id name;
			name_id return_type_name;
			name_id parameter_name;
			name_id parameter_type_name;
			struct statement *block;
		} function;
		struct {
			name_id attribute;
			name_id name;
			members members;
		} structy;
	};
} definition;

void parse(tokens *tokens);

typedef struct functions {
	definition *f[256];
	size_t size;
} functions;

typedef struct structs {
	definition *s[256];
	size_t size;
} structs;

extern struct functions all_functions;
extern struct structs all_structs;
