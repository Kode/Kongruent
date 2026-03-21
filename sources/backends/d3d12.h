#ifndef KONG_D3D12_HEADER
#define KONG_D3D12_HEADER

#include "../shader_stage.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int compile_hlsl_to_d3d12(const char *source, uint8_t **output, size_t *outputlength, shader_stage stage, bool debug);

#ifdef __cplusplus
}
#endif

#endif
