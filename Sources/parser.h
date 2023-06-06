#pragma once

#include "functions.h"
#include "names.h"
#include "structs.h"
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

typedef struct definition {
	enum { DEFINITION_FUNCTION, DEFINITION_STRUCT } type;

	union {
		function *function;
		structy *structy;
	};
} definition;

void parse(tokens *tokens);
