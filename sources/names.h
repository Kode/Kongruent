#ifndef KONG_NAMES_HEADER
#define KONG_NAMES_HEADER

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NO_NAME 0

typedef size_t name_id;

void names_init(void);

name_id add_name(const char *name);

char *get_name(name_id index);

#ifdef __cplusplus
}
#endif

#endif
