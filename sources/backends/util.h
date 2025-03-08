#pragma once

#include "../functions.h"
#include "../globals.h"
#include "../sets.h"

#include <stdint.h>

void find_referenced_functions(function *f, function **functions, size_t *functions_size);
void find_referenced_types(function *f, type_id *types, size_t *types_size);
void find_referenced_globals(function *f, global_id *globals, size_t *globals_size);
void find_referenced_sets(function *f, descriptor_set **sets, size_t *sets_size);
void find_pipeline_buckets(void);

void indent(char *code, size_t *offset, int indentation);

uint32_t base_type_size(type_id type);

uint32_t struct_size(type_id id);
