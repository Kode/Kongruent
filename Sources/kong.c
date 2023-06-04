#include "compiler.h"
#include "errors.h"
#include "log.h"
#include "parser.h"
#include "tokenizer.h"

#include "backends/hlsl.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *filename = "in/test.kong";

int main(int argc, char **argv) {
	FILE *file = fopen(filename, "rb");

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *data = (char *)malloc(size + 1);
	assert(data != NULL);
	fread(data, 1, size, file);
	fclose(file);
	data[size] = 0;

	tokens tokens = tokenize(data);

	free(data);

	parse(&tokens);

	kong_log(LOG_LEVEL_INFO, "Functions:");
	for (size_t i = 0; i < all_functions.size; ++i) {
		kong_log(LOG_LEVEL_INFO, "%s", all_functions.f[i]->function.name);
	}
	kong_log(LOG_LEVEL_INFO, "");

	kong_log(LOG_LEVEL_INFO, "Structs:");
	for (size_t i = 0; i < all_structs.size; ++i) {
		kong_log(LOG_LEVEL_INFO, "%s", all_structs.s[i]->structy.name);
	}

	for (size_t i = 0; i < all_functions.size; ++i) {
		convert_function_block(all_functions.f[i]->function.block);
	}

	hlsl_export(&all_opcodes.o[0], all_opcodes.size);

	return 0;
}
