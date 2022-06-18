#include "tokenizer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

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

	tokens_t tokens = tokenize(data);

	free(data);

	// tree_t _parse_tree = parser_parse(tokens);

	return 0;
}
