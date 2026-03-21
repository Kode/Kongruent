#ifndef KONG_CSTYLE_HEADER
#define KONG_CSTYLE_HEADER

#include "../compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef const char *(*type_string_func)(type_id type);

void cstyle_write_opcode(char *code, size_t *offset, opcode *o, type_string_func type_string, int *indentation);

#ifdef __cplusplus
}
#endif

#endif
