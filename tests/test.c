#include "test.h"

#include "vert.h"
#include "frag.h"

kinc_g4_pipeline_t Pipe;

kinc_g4_vertex_structure_t VertexIn_structure;
kinc_g4_vertex_structure_t FragmentIn_structure;

void kong_init(void) {
	kinc_g4_pipeline_init(&Pipe);

	kinc_g4_shader_t pos;
	kinc_g4_shader_init(&pos, kong_vert_code, kong_vert_code_size, KINC_G4_SHADER_TYPE_VERTEX);
	Pipe.vertex_shader = &pos;

	kinc_g4_shader_t pixel;
	kinc_g4_shader_init(&pixel, kong_frag_code, kong_frag_code_size, KINC_G4_SHADER_TYPE_FRAGMENT);
	Pipe.fragment_shader = &pixel;

	kinc_g4_vertex_structure_init(&VertexIn_structure);
	kinc_g4_vertex_structure_add(&VertexIn_structure, "position", KINC_G4_VERTEX_DATA_F32_3X);

	kinc_g4_vertex_structure_init(&FragmentIn_structure);
	kinc_g4_vertex_structure_add(&FragmentIn_structure, "position", KINC_G4_VERTEX_DATA_F32_4X);

	Pipe.input_layout[0] = &VertexIn_structure;
	Pipe.input_layout[1] = NULL;

	kinc_g4_pipeline_compile(&Pipe);
}
