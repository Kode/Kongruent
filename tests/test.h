#include <kinc/graphics4/pipeline.h>
#include <kinc/graphics4/vertexbuffer.h>
#include <kinc/math/vector.h>

typedef struct VertexIn {
	kinc_vector3_t position;
} VertexIn;

extern kinc_g4_vertex_structure_t VertexIn_structure;

typedef struct FragmentIn {
	kinc_vector4_t position;
} FragmentIn;

extern kinc_g4_vertex_structure_t FragmentIn_structure;

void kong_init(void);

extern kinc_g4_pipeline_t Pipe;

