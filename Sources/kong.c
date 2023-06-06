#include "compiler.h"
#include "errors.h"
#include "functions.h"
#include "log.h"
#include "names.h"
#include "parser.h"
#include "structs.h"
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
	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *data = (char *)malloc(size + 1);
	assert(data != NULL);

	fread(data, 1, size, file);
	data[size] = 0;

	fclose(file);

	names_init();
	init_structs();
	init_functions();

	tokens tokens = tokenize(data);

	free(data);

	parse(&tokens);

	kong_log(LOG_LEVEL_INFO, "Functions:");
	for (size_t i = 0; i < all_functions.size; ++i) {
		kong_log(LOG_LEVEL_INFO, "%s", get_name(all_functions.f[i].name));
	}
	kong_log(LOG_LEVEL_INFO, "");

	kong_log(LOG_LEVEL_INFO, "Structs:");
	for (size_t i = 0; i < all_structs.size; ++i) {
		kong_log(LOG_LEVEL_INFO, "%s", get_name(all_structs.s[i].name));
	}

	for (size_t i = 0; i < all_functions.size; ++i) {
		convert_function_block(all_functions.f[i].block);
	}

	hlsl_export(&all_opcodes.o[0], all_opcodes.size);

	return 0;
}
