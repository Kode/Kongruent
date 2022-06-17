#pragma once

struct Statement {
	enum {
		Expression,
		If,
		Block,
		Declaration,
		PreprocessorDirective,
		Function,
		Struct,
	} type;

	union {
		Expression expression;
		IfStatement iffy;
		BlockStatement block;
		DeclarationStatement declaration;
		PreprocessorStatement preprocessorDirective;
		FunctionStatement function;
		StructStatement structy;
	} data;
};

struct DeclarationStatement {
	const char *name;
	expression_t *init;
};

struct BlockStatement {
	Statement *statements;
};

struct FunctionStatement {
	const char **parameters;
	BlockStatement block;
};

struct IfStatement {
	expression_t test;
	Statement block;
};

struct CallStatement {
	Expression func;
	Expression *parameters;
};

struct PreprocessorStatement {
	const char *name;
	Expression *parameters;
};

struct Member {
	const char *name;
	const char *member_type;
};

struct StructStatement {
	const char *attribute;
	const char *name;
	Member *members;
};
