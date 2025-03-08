#include "analyzer.h"

#include "array.h"
#include "errors.h"

#include <string.h>

static void find_referenced_global_for_var(variable v, global_id *globals, size_t *globals_size) {
	for (global_id j = 0; get_global(j) != NULL && get_global(j)->type != NO_TYPE; ++j) {
		global *g = get_global(j);
		if (v.index == g->var_index) {
			bool found = false;
			for (size_t k = 0; k < *globals_size; ++k) {
				if (globals[k] == j) {
					found = true;
					break;
				}
			}
			if (!found) {
				globals[*globals_size] = j;
				*globals_size += 1;
			}
			return;
		}
	}
}

void find_referenced_globals(function *f, global_id *globals, size_t *globals_size) {
	if (f->block == NULL) {
		// built-in
		return;
	}

	function *functions[256];
	size_t functions_size = 0;

	functions[functions_size] = f;
	functions_size += 1;

	find_referenced_functions(f, functions, &functions_size);

	for (size_t l = 0; l < functions_size; ++l) {
		uint8_t *data = functions[l]->code.o;
		size_t size = functions[l]->code.size;

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];
			switch (o->type) {
			case OPCODE_MULTIPLY:
			case OPCODE_DIVIDE:
			case OPCODE_ADD:
			case OPCODE_SUB:
			case OPCODE_EQUALS:
			case OPCODE_NOT_EQUALS:
			case OPCODE_GREATER:
			case OPCODE_GREATER_EQUAL:
			case OPCODE_LESS:
			case OPCODE_LESS_EQUAL: {
				find_referenced_global_for_var(o->op_binary.left, globals, globals_size);
				find_referenced_global_for_var(o->op_binary.right, globals, globals_size);
				break;
			}
			case OPCODE_LOAD_MEMBER: {
				find_referenced_global_for_var(o->op_load_member.from, globals, globals_size);
				break;
			}
			case OPCODE_STORE_MEMBER:
			case OPCODE_SUB_AND_STORE_MEMBER:
			case OPCODE_ADD_AND_STORE_MEMBER:
			case OPCODE_DIVIDE_AND_STORE_MEMBER:
			case OPCODE_MULTIPLY_AND_STORE_MEMBER: {
				find_referenced_global_for_var(o->op_store_member.to, globals, globals_size);
				break;
			}
			case OPCODE_CALL: {
				for (uint8_t i = 0; i < o->op_call.parameters_size; ++i) {
					find_referenced_global_for_var(o->op_call.parameters[i], globals, globals_size);
				}
				break;
			}
			default:
				break;
			}

			index += o->size;
		}
	}
}

static bool has_set(descriptor_set **sets, size_t *sets_size, descriptor_set *set) {
	for (size_t set_index = 0; set_index < *sets_size; ++set_index) {
		if (sets[set_index] == set) {
			return true;
		}
	}

	return false;
}

static void add_set(descriptor_set **sets, size_t *sets_size, descriptor_set *set) {
	if (has_set(sets, sets_size, set)) {
		return;
	}

	sets[*sets_size] = set;
	*sets_size += 1;
}

void find_referenced_sets(function *f, descriptor_set **sets, size_t *sets_size) {
	if (f->block == NULL) {
		// built-in
		return;
	}

	global_id globals[256];
	size_t globals_size = 0;
	find_referenced_globals(f, globals, &globals_size);

	for (size_t global_index = 0; global_index < globals_size; ++global_index) {
		global *g = get_global(globals[global_index]);

		if (g->sets_count == 0) {
			continue;
		}

		if (g->sets_count == 1) {
			add_set(sets, sets_size, g->sets[0]);
			continue;
		}
	}

	for (size_t global_index = 0; global_index < globals_size; ++global_index) {
		global *g = get_global(globals[global_index]);

		if (g->sets_count < 2) {
			continue;
		}

		bool found = false;

		for (size_t set_index = 0; set_index < g->sets_count; ++set_index) {
			descriptor_set *set = g->sets[set_index];

			if (has_set(sets, sets_size, set)) {
				found = true;
				break;
			}
		}

		if (!found) {
			debug_context context = {0};
			error(context, "Global %s could be used from multiple descriptor sets.", get_name(g->name));
		}
	}
}

void find_pipeline_buckets(void) {
	typedef struct render_pipeline {
		function *vertex_shader;
		function *amplification_shader;
		function *mesh_shader;
		function *fragment_shader;
	} render_pipeline;

	static_array(render_pipeline, render_pipelines, 256);

	render_pipelines all_render_pipelines;
	static_array_init(all_render_pipelines);

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
			name_id vertex_shader_name = NO_NAME;
			name_id amplification_shader_name = NO_NAME;
			name_id mesh_shader_name = NO_NAME;
			name_id fragment_shader_name = NO_NAME;

			for (size_t j = 0; j < t->members.size; ++j) {
				if (t->members.m[j].name == add_name("vertex")) {
					vertex_shader_name = t->members.m[j].value.identifier;
				}
				else if (t->members.m[j].name == add_name("amplification")) {
					amplification_shader_name = t->members.m[j].value.identifier;
				}
				else if (t->members.m[j].name == add_name("mesh")) {
					mesh_shader_name = t->members.m[j].value.identifier;
				}
				else if (t->members.m[j].name == add_name("fragment")) {
					fragment_shader_name = t->members.m[j].value.identifier;
				}
			}

			debug_context context = {0};
			check(vertex_shader_name != NO_NAME || mesh_shader_name != NO_NAME, context, "vertex or mesh shader missing");
			check(fragment_shader_name != NO_NAME, context, "fragment shader missing");

			render_pipeline pipeline = {0};

			for (function_id i = 0; get_function(i) != NULL; ++i) {
				function *f = get_function(i);
				if (vertex_shader_name != NO_NAME && f->name == vertex_shader_name) {
					pipeline.vertex_shader = f;
				}
				if (amplification_shader_name != NO_NAME && f->name == amplification_shader_name) {
					pipeline.amplification_shader = f;
				}
				if (mesh_shader_name != NO_NAME && f->name == mesh_shader_name) {
					pipeline.mesh_shader = f;
				}
				if (f->name == fragment_shader_name) {
					pipeline.fragment_shader = f;
				}
			}

			static_array_push(all_render_pipelines, pipeline);
		}
	}

	static_array(uint32_t, pipeline_indices, 256);

	static_array(pipeline_indices, pipeline_buckets, 64);

	pipeline_buckets buckets;
	static_array_init(buckets);

	pipeline_indices remaining_pipelines;
	static_array_init(remaining_pipelines);

	for (uint32_t index = 0; index < all_render_pipelines.size; ++index) {
		static_array_push(remaining_pipelines, index);
	}

	while (remaining_pipelines.size > 0) {
		pipeline_indices next_remaining_pipelines;
		static_array_init(next_remaining_pipelines);

		pipeline_indices bucket;
		static_array_init(bucket);

		static_array_push(bucket, remaining_pipelines.values[0]);

		for (size_t index = 1; index < remaining_pipelines.size; ++index) {
			uint32_t pipeline_index = remaining_pipelines.values[index];
			render_pipeline *pipeline = &all_render_pipelines.values[pipeline_index];

			bool found = false;

			for (size_t index_in_bucket = 0; index_in_bucket < bucket.size; ++index_in_bucket) {
				render_pipeline *pipeline_in_bucket = &all_render_pipelines.values[bucket.values[index_in_bucket]];
				if (pipeline->vertex_shader == pipeline_in_bucket->vertex_shader ||
				    pipeline->amplification_shader == pipeline_in_bucket->amplification_shader || pipeline->mesh_shader == pipeline_in_bucket->mesh_shader ||
				    pipeline->fragment_shader == pipeline_in_bucket->fragment_shader) {
					found = true;
					break;
				}
			}

			if (found) {
				static_array_push(bucket, pipeline_index);
			}
			else {
				static_array_push(next_remaining_pipelines, pipeline_index);
			}
		}

		remaining_pipelines = next_remaining_pipelines;
		static_array_push(buckets, bucket);
	}
}

void find_referenced_functions(function *f, function **functions, size_t *functions_size) {
	if (f->block == NULL) {
		// built-in
		return;
	}

	uint8_t *data = f->code.o;
	size_t size = f->code.size;

	size_t index = 0;
	while (index < size) {
		opcode *o = (opcode *)&data[index];
		switch (o->type) {
		case OPCODE_CALL: {
			for (function_id i = 0; get_function(i) != NULL; ++i) {
				function *f = get_function(i);
				if (f->name == o->op_call.func) {
					if (f->block == NULL) {
						// built-in
						break;
					}

					bool found = false;
					for (size_t j = 0; j < *functions_size; ++j) {
						if (functions[j]->name == o->op_call.func) {
							found = true;
							break;
						}
					}
					if (!found) {
						functions[*functions_size] = f;
						*functions_size += 1;
						find_referenced_functions(f, functions, functions_size);
					}
					break;
				}
			}
			break;
		}
		default:
			break;
		}

		index += o->size;
	}
}

static void add_found_type(type_id t, type_id *types, size_t *types_size) {
	for (size_t i = 0; i < *types_size; ++i) {
		if (types[i] == t) {
			return;
		}
	}

	types[*types_size] = t;
	*types_size += 1;
}

void find_referenced_types(function *f, type_id *types, size_t *types_size) {
	if (f->block == NULL) {
		// built-in
		return;
	}

	function *functions[256];
	size_t functions_size = 0;

	functions[functions_size] = f;
	functions_size += 1;

	find_referenced_functions(f, functions, &functions_size);

	for (size_t function_index = 0; function_index < functions_size; ++function_index) {
		function *func = functions[function_index];
		debug_context context = {0};
		for (uint8_t parameter_index = 0; parameter_index < func->parameters_size; ++parameter_index) {
			check(func->parameter_types[parameter_index].type != NO_TYPE, context, "Function parameter type not found");
			add_found_type(func->parameter_types[parameter_index].type, types, types_size);
		}
		check(func->return_type.type != NO_TYPE, context, "Function return type missing");
		add_found_type(func->return_type.type, types, types_size);

		uint8_t *data = functions[function_index]->code.o;
		size_t size = functions[function_index]->code.size;

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];
			switch (o->type) {
			case OPCODE_VAR:
				add_found_type(o->op_var.var.type.type, types, types_size);
				break;
			default:
				break;
			}

			index += o->size;
		}
	}
}
