#pragma once

#include "../functions.h"

#include <stdint.h>

void find_referenced_functions(function *f, function **functions, size_t *functions_size);
void find_referenced_types(function *f, type_id *types, size_t *types_size);

void indent(char *code, size_t *offset, int indentation);
