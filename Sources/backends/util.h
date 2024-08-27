#pragma once

#include "../functions.h"
#include "../globals.h"

#include <stdint.h>

void find_referenced_functions(function *f, function **functions, size_t *functions_size);
void find_referenced_types(function *f, type_id *types, size_t *types_size);
void find_referenced_globals(function *f, global_id *globals, size_t *globals_size);

void indent(char *code, size_t *offset, int indentation);
