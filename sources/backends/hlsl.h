#ifndef KONG_HLSL_HEADER
#define KONG_HLSL_HEADER

#include "../api.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void hlsl_export(char *directory, api_kind d3d, bool debug);

#ifdef __cplusplus
}
#endif

#endif
