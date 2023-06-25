#pragma once

#include <stdint.h>

#include "names.h"
#include "types.h"

typedef struct variable {
	uint64_t index;
	type_id type;
} variable;

typedef struct opcode {
	enum { OPCODE_VAR, OPCODE_NOT, OPCODE_STORE_VARIABLE, OPCODE_STORE_MEMBER, OPCODE_LOAD_CONSTANT, OPCODE_LOAD_MEMBER, OPCODE_RETURN } type;
	uint32_t size;

	union {
		struct {
			variable var;
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
			uint16_t member_indices[64];
			type_id member_parent_type;
			uint8_t member_indices_size;
		} op_store_member;
		struct {
			float number;
			variable to;
		} op_load_constant;
		struct {
			variable from;
			variable to;
			uint16_t member_indices[64];
			type_id member_parent_type;
			uint8_t member_indices_size;
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

void convert_function_block(opcodes *code, struct statement *block);
