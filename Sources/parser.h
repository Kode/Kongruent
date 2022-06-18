#pragma once

#include "tokenizer.h"

#include <stdbool.h>

struct expression;

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
		float number;
		const char *string;
		const char *variable;
		struct expression *grouping;
		struct {
			struct expression *func;
			struct expression *parameters;
		} call;
		struct {
			const char *value1;
			const char *value2;
		} member;
		struct {
			struct expression *parameters;
		} constructor;
	};
} expression_t;

typedef struct Member {
	const char *name;
	const char *member_type;
} Member_t;

typedef struct statement {
	enum {
		STATEMENT_EXPRESSION,
		STATEMENT_IF,
		STATEMENT_BLOCK,
		STATEMENT_DECLARATION,
		STATEMENT_PREPROCESSOR_DIRECTIVE,
		STATEMENT_FUNCTION,
		STATEMENT_STRUCT
	} type;

	union {
		expression_t expression;
		struct {
			void *test;
			void *block;
		} iffy;
		struct {
			void *statements;
		} block;
		struct {
			const char *name;
			expression_t *init;
		} declaration;
		struct {
			const char *name;
			expression_t *parameters;
		} preprocessorDirective;
		struct {
			const char **parameters;
			void *block;
		} function;
		struct {
			const char *attribute;
			const char *name;
			struct Member *members;
		} structy;
	};
} statement_t;

void add_statement(statement_t *statements, void *statement);
