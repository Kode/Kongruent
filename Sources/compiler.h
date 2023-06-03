#pragma once

#include <stdint.h>

typedef struct variable {
	uint64_t index;
} variable;

typedef struct opcode {
	enum { OPCODE_VAR, OPCODE_NOT, OPCODE_STORE_VARIABLE, OPCODE_STORE_MEMBER, OPCODE_LOAD_CONSTANT, OPCODE_LOAD_MEMBER, OPCODE_RETURN } type;
	uint8_t size;

	union {
		struct {
			int a;
		} op_var;
		struct {
			variable from;
			variable to;
		} op_not;
		struct {
			variable from;
			variable to;
		} op_store_var;
		struct {
			variable from;
			variable to;
			size_t member;
		} op_store_member;
		struct {
			float number;
			variable to;
		} op_load_constant;
		struct {
			variable to;
		} op_load_member;
		struct {
			variable var;
		} op_return;
	};
} opcode;

typedef struct opcodes {
	uint8_t o[4096];
	size_t size;
} opcodes;

extern opcodes all_opcodes;

void convert_function_block(struct statement *block);
