#pragma once

#include <stdint.h>

#include "names.h"
#include "types.h"

typedef struct variable {
	uint64_t index;
	type_ref type;
} variable;

typedef struct opcode {
	enum {
		OPCODE_VAR,
		OPCODE_NOT,
		OPCODE_STORE_VARIABLE,
		OPCODE_STORE_MEMBER,
		OPCODE_LOAD_CONSTANT,
		OPCODE_LOAD_MEMBER,
		OPCODE_RETURN,
		OPCODE_CALL,
		OPCODE_MULTIPLY,
		OPCODE_ADD
	} type;
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
			bool member_parent_array;
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
		struct {
			variable var;
			name_id func;
			variable parameters[64];
			uint8_t parameters_size;
		} op_call;
		struct {
			variable right;
			variable left;
			variable result;
		} op_multiply;
		struct {
			variable right;
			variable left;
			variable result;
		} op_add;
	};
} opcode;

typedef struct opcodes {
	uint8_t o[4096];
	size_t size;
} opcodes;

void convert_globals(void);

void convert_function_block(opcodes *code, struct statement *block);
