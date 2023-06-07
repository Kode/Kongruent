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

void resolve_types(void) {
	for (struct_id i = 0; get_struct(i) != NULL; ++i) {
		structy *s = get_struct(i);
		for (size_t j = 0; j < s->members.size; ++j) {
			if (!s->members.m[j].type.resolved) {
				name_id name = s->members.m[j].type.name;
				s->members.m[j].type.type = find_struct(name);
				if (s->members.m[j].type.type == NO_STRUCT) {
					char output[256];
					char *struct_name = get_name(s->name);
					char *member_type_name = get_name(name);
					sprintf(output, "Could not find type %s in %s", member_type_name, struct_name);
					error(output, 0, 0);
				}
				s->members.m[j].type.resolved = true;
			}
		}
	}

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);

		name_id parameter_type_name = f->parameter_type.name;
		f->parameter_type.type = find_struct(parameter_type_name);
		if (f->parameter_type.type == NO_STRUCT) {
			char output[256];
			char *function_name = get_name(f->name);
			char *parameter_type_name_name = get_name(parameter_type_name);
			sprintf(output, "Could not find type %s for %s", parameter_type_name_name, function_name);
			error(output, 0, 0);
		}
		f->parameter_type.resolved = true;

		name_id return_type_name = f->return_type.name;
		f->return_type.type = find_struct(return_type_name);
		if (f->return_type.type == NO_STRUCT) {
			char output[256];
			char *function_name = get_name(f->name);
			char *return_type_name_name = get_name(return_type_name);
			sprintf(output, "Could not find type %s for %s", return_type_name_name, function_name);
			error(output, 0, 0);
		}
		f->return_type.resolved = true;
	}

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);
		assert(f->block->kind == STATEMENT_BLOCK);
		for (size_t j = 0; j < f->block->block.statements.size; ++j) {
			statement *s = f->block->block.statements.s[j];
			if (s->kind == STATEMENT_LOCAL_VARIABLE) {
				name_id var_type_name = s->local_variable.type.name;
				s->local_variable.type.type = find_struct(var_type_name);
				if (s->local_variable.type.type == NO_STRUCT) {
					char output[256];
					char *type_name = get_name(var_type_name);
					sprintf(output, "Could not find type %s", type_name);
					error(output, 0, 0);
				}
				s->local_variable.type.resolved = true;
			}
		}
	}
}

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
	structs_init();
	functions_init();

	tokens tokens = tokenize(data);

	free(data);

	parse(&tokens);

	kong_log(LOG_LEVEL_INFO, "Functions:");
	for (function_id i = 0; get_function(i) != NULL; ++i) {
		kong_log(LOG_LEVEL_INFO, "%s", get_name(get_function(i)->name));
	}
	kong_log(LOG_LEVEL_INFO, "");

	kong_log(LOG_LEVEL_INFO, "Structs:");
	for (struct_id i = 0; get_struct(i) != NULL; ++i) {
		kong_log(LOG_LEVEL_INFO, "%s", get_name(get_struct(i)->name));
	}

	resolve_types();

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		convert_function_block(get_function(i)->block);
	}

	hlsl_export(&all_opcodes.o[0], all_opcodes.size);

	return 0;
}
