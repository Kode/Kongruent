#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum EShLanguage { EShLangVertex, EShLangFragment, EShLangGeometry, EShLangTessControl, EShLangTessEvaluation, EShLangCompute } EShLanguage;

int compile_hlsl_to_d3d11(const char *source, uint8_t **output, size_t *outputlength, EShLanguage stage, bool debug);
