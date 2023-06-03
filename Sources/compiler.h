#pragma once

#include <stdint.h>

typedef struct opcodes {
	uint8_t o[4096];
	size_t size;
} opcodes;

extern opcodes all_opcodes;

void convert_function_block(struct statement *block);
