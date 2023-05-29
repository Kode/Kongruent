#include "errors.h"
#include "log.h"
#include "parser.h"
#include "tokenizer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

const char *filename = "in/test.kong";

void convert_function_block(struct statement *block) {
	if (block->type != STATEMENT_BLOCK) {
		error("Expected a block", 0, 0);
	}
	for (size_t i = 0; i < block->block.statements.size; ++i) {
		statement *s = block->block.statements.s[i];
		switch (s->type) {
		case STATEMENT_EXPRESSION:
			break;
		case STATEMENT_RETURN_EXPRESSION:
			break;
		case STATEMENT_IF:
			break;
		case STATEMENT_BLOCK:
			break;
		case STATEMENT_LOCAL_VARIABLE:
			break;
		}
	}
}

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

	log(LOG_LEVEL_INFO, "Functions:");
	for (size_t i = 0; i < all_functions.size; ++i) {
		log(LOG_LEVEL_INFO, "%s", all_functions.f[i]->function.name);
	}
	log(LOG_LEVEL_INFO, "");

	log(LOG_LEVEL_INFO, "Structs:");
	for (size_t i = 0; i < all_structs.size; ++i) {
		log(LOG_LEVEL_INFO, "%s", all_structs.s[i]->structy.name);
	}

	for (size_t i = 0; i < all_functions.size; ++i) {
		convert_function_block(all_functions.f[i]->function.block);
	}

	return 0;
}
