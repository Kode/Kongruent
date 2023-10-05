#pragma once

#include <stdint.h>

typedef enum Direct3D { DIRECT3D_9, DIRECT3D_11 } Direct3D;

void hlsl_export(char *directory, Direct3D d3d);
