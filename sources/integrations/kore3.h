#ifndef KONG_KORE3_HEADER
#define KONG_KORE3_HEADER

#include "../api.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void kore3_export(char *directory, api_kind api);

#ifdef __cplusplus
}
#endif

#endif
