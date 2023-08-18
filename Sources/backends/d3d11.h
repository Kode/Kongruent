#pragma once

#include <stdbool.h>

typedef enum EShLanguage { EShLangVertex, EShLangFragment, EShLangGeometry, EShLangTessControl, EShLangTessEvaluation, EShLangCompute } EShLanguage;

int compile_hlsl_to_d3d11(const char *source, char **output, size_t *outputlength, EShLanguage stage, bool debug);
