#pragma once

typedef enum shader_stage {
	SHADER_STAGE_VERTEX,
	SHADER_STAGE_AMPLIFICATION,
	SHADER_STAGE_MESH,
	SHADER_STAGE_FRAGMENT,
	SHADER_STAGE_COMPUTE,
	SHADER_STAGE_RAY_GENERATION,
	SHADER_STAGE_RAY_MISS,
	SHADER_STAGE_RAY_CLOSEST_HIT,
	SHADER_STAGE_RAY_INTERSECTION,
	SHADER_STAGE_RAY_ANY_HIT
} shader_stage;
