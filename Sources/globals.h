#pragma once

#include "names.h"

typedef uint32_t global_id;

typedef enum global_kind {GLOBAL_NONE, GLOBAL_SAMPLER, GLOBAL_TEX2D } global_kind;

typedef struct global {
	name_id name;
	global_kind kind;
} global;

void globals_init(void);

global_id add_global(global_kind kind, name_id name);

global find_global(name_id name);
