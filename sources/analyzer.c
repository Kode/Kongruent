#include "analyzer.h"

#include "array.h"
#include "errors.h"

#include <string.h>

static render_pipelines all_render_pipelines;
// a pipeline group is a collection of pipelines that share shaders
static render_pipeline_groups all_render_pipeline_groups;

static compute_shaders all_compute_shaders;

static raytracing_pipelines all_raytracing_pipelines;
// a pipeline group is a collection of pipelines that share shaders
static raytracing_pipeline_groups all_raytracing_pipeline_groups;

static void find_referenced_global_for_var(variable v, global_array *globals, bool read, bool write) {
	for (global_id j = 0; get_global(j) != NULL && get_global(j)->type != NO_TYPE; ++j) {
		global *g = get_global(j);

		if (v.index == g->var_index) {
			bool found = false;
			for (size_t k = 0; k < globals->size; ++k) {
				if (globals->globals[k] == j) {
					found = true;

					if (read) {
						globals->readable[k] = true;
					}

					if (write) {
						globals->writable[k] = true;
					}

					break;
				}
			}
			if (!found) {
				globals->globals[globals->size]  = j;
				globals->readable[globals->size] = read;
				globals->writable[globals->size] = write;
				globals->size += 1;
			}
			return;
		}
	}
}

void find_referenced_globals(function *f, global_array *globals) {
	if (f->block == NULL) {
		// built-in
		return;
	}

	function *functions[256];
	size_t    functions_size = 0;

	functions[functions_size] = f;
	functions_size += 1;

	find_referenced_functions(f, functions, &functions_size);

	for (size_t l = 0; l < functions_size; ++l) {
		uint8_t *data = functions[l]->code.o;
		size_t   size = functions[l]->code.size;

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
				find_referenced_global_for_var(o->op_binary.left, globals, false, false);
				find_referenced_global_for_var(o->op_binary.right, globals, false, false);
				break;
			}
			case OPCODE_LOAD_ACCESS_LIST: {
				find_referenced_global_for_var(o->op_load_access_list.from, globals, true, false);
				break;
			}
			case OPCODE_STORE_ACCESS_LIST:
			case OPCODE_SUB_AND_STORE_ACCESS_LIST:
			case OPCODE_ADD_AND_STORE_ACCESS_LIST:
			case OPCODE_DIVIDE_AND_STORE_ACCESS_LIST:
			case OPCODE_MULTIPLY_AND_STORE_ACCESS_LIST: {
				find_referenced_global_for_var(o->op_store_access_list.to, globals, false, true);
				break;
			}
			case OPCODE_CALL: {
				for (uint8_t i = 0; i < o->op_call.parameters_size; ++i) {
					find_referenced_global_for_var(o->op_call.parameters[i], globals, false, false);
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

void find_referenced_functions(function *f, function **functions, size_t *functions_size) {
	if (f->block == NULL) {
		// built-in
		return;
	}

	uint8_t *data = f->code.o;
	size_t   size = f->code.size;

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

void find_used_builtins(function *f) {
	if (f->block == NULL) {
		// built-in
		return;
	}

	if (f->used_builtins.builtins_analyzed) {
		return;
	}

	f->used_builtins.builtins_analyzed = true;

	uint8_t *data = f->code.o;
	size_t   size = f->code.size;

	size_t index = 0;
	while (index < size) {
		opcode *o = (opcode *)&data[index];
		switch (o->type) {
		case OPCODE_CALL: {
			name_id func = o->op_call.func;

			if (func == add_name("dispatch_thread_id")) {
				f->used_builtins.dispatch_thread_id = true;
			}

			if (func == add_name("group_thread_id")) {
				f->used_builtins.group_thread_id = true;
			}

			if (func == add_name("group_id")) {
				f->used_builtins.group_id = true;
			}

			if (func == add_name("vertex_id")) {
				f->used_builtins.vertex_id = true;
			}

			for (function_id i = 0; get_function(i) != NULL; ++i) {
				function *called = get_function(i);
				if (called->name == o->op_call.func) {
					find_used_builtins(f);

					f->used_builtins.dispatch_thread_id |= called->used_builtins.dispatch_thread_id;
					f->used_builtins.group_thread_id |= called->used_builtins.group_thread_id;
					f->used_builtins.group_id |= called->used_builtins.group_id;
					f->used_builtins.vertex_id |= called->used_builtins.vertex_id;

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

static global *find_global_by_var(variable var) {
	for (global_id global_index = 0; get_global(global_index) != NULL && get_global(global_index)->type != NO_TYPE; ++global_index) {
		if (var.index == get_global(global_index)->var_index) {
			return get_global(global_index);
		}
	}

	return NULL;
}

void find_used_capabilities(function *f) {
	if (f->block == NULL) {
		// built-in
		return;
	}

	if (f->used_capabilities.capabilities_analyzed) {
		return;
	}

	f->used_capabilities.capabilities_analyzed = true;

	uint8_t *data = f->code.o;
	size_t   size = f->code.size;

	size_t   index                  = 0;
	variable last_base_texture_from = {0};
	variable last_base_texture_to   = {0};

	while (index < size) {
		opcode *o = (opcode *)&data[index];
		switch (o->type) {
		case OPCODE_STORE_ACCESS_LIST: {
			variable to      = o->op_store_access_list.to;
			type_id  to_type = to.type.type;

			if (is_texture(to_type)) {
				assert(get_type(to_type)->array_size == 0);

				f->used_capabilities.image_write = true;

				global *g = find_global_by_var(to);
				assert(g != NULL);
				g->usage |= GLOBAL_USAGE_TEXTURE_WRITE;
			}
			break;
		}
		case OPCODE_LOAD_ACCESS_LIST: {
			variable from      = o->op_load_access_list.from;
			type_id  from_type = from.type.type;

			if (is_texture(from_type)) {
				f->used_capabilities.image_read = true;

				if (get_type(from_type)->array_size > 0) {
					last_base_texture_from = from;
					last_base_texture_to   = o->op_load_access_list.to;
				}
				else {
					global *g = find_global_by_var(from);
					assert(g != NULL);
					g->usage |= GLOBAL_USAGE_TEXTURE_READ;
				}
			}

			break;
		}
		case OPCODE_CALL: {
			name_id func_name = o->op_call.func;

			if (func_name == add_name("sample") || func_name == add_name("sample_lod")) {
				variable tex_parameter = o->op_call.parameters[0];

				global *g = NULL;

				if (tex_parameter.kind == VARIABLE_INTERNAL) {
					assert(last_base_texture_to.index == tex_parameter.index);
					g = find_global_by_var(last_base_texture_from);
				}
				else {
					g = find_global_by_var(tex_parameter);
				}

				assert(g != NULL);

				g->usage |= GLOBAL_USAGE_TEXTURE_SAMPLE;
			}

			for (function_id i = 0; get_function(i) != NULL; ++i) {
				function *called = get_function(i);
				if (called->name == func_name) {
					find_used_capabilities(f);

					f->used_capabilities.image_read |= called->used_capabilities.image_read;
					f->used_capabilities.image_write |= called->used_capabilities.image_write;

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
	size_t    functions_size = 0;

	functions[functions_size] = f;
	functions_size += 1;

	find_referenced_functions(f, functions, &functions_size);

	for (size_t function_index = 0; function_index < functions_size; ++function_index) {
		function     *func    = functions[function_index];
		debug_context context = {0};
		for (uint8_t parameter_index = 0; parameter_index < func->parameters_size; ++parameter_index) {
			check(func->parameter_types[parameter_index].type != NO_TYPE, context, "Function parameter type not found");
			add_found_type(func->parameter_types[parameter_index].type, types, types_size);
		}
		check(func->return_type.type != NO_TYPE, context, "Function return type missing");
		add_found_type(func->return_type.type, types, types_size);

		uint8_t *data = functions[function_index]->code.o;
		size_t   size = functions[function_index]->code.size;

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

static bool has_set(descriptor_sets *sets, descriptor_set *set) {
	for (size_t set_index = 0; set_index < sets->size; ++set_index) {
		if (sets->values[set_index] == set) {
			return true;
		}
	}

	return false;
}

static void add_set(descriptor_sets *sets, descriptor_set *set) {
	if (has_set(sets, set)) {
		return;
	}

	static_array_push_p(sets, set);
}

static void find_referenced_sets(global_array *globals, descriptor_sets *sets) {
	for (size_t global_index = 0; global_index < globals->size; ++global_index) {
		global *g = get_global(globals->globals[global_index]);

		if (g->sets_count == 0) {
			continue;
		}

		if (g->sets_count == 1) {
			add_set(sets, g->sets[0]);
			continue;
		}
	}

	for (size_t global_index = 0; global_index < globals->size; ++global_index) {
		global *g = get_global(globals->globals[global_index]);

		if (g->sets_count < 2) {
			continue;
		}

		bool found = false;

		for (size_t set_index = 0; set_index < g->sets_count; ++set_index) {
			descriptor_set *set = g->sets[set_index];

			if (has_set(sets, set)) {
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

static render_pipeline extract_render_pipeline_from_type(type *t) {
	name_id vertex_shader_name        = NO_NAME;
	name_id amplification_shader_name = NO_NAME;
	name_id mesh_shader_name          = NO_NAME;
	name_id fragment_shader_name      = NO_NAME;

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

	return pipeline;
}

static void find_all_render_pipelines(void) {
	static_array_init(all_render_pipelines);

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
			static_array_push(all_render_pipelines, extract_render_pipeline_from_type(t));
		}
	}
}

static bool same_shader(function *a, function *b) {
	if (a == NULL && b == NULL) {
		return false;
	}

	return a == b;
}

static void find_render_pipeline_groups(void) {
	static_array_init(all_render_pipeline_groups);

	render_pipeline_indices remaining_pipelines;
	static_array_init(remaining_pipelines);

	for (uint32_t index = 0; index < all_render_pipelines.size; ++index) {
		static_array_push(remaining_pipelines, index);
	}

	while (remaining_pipelines.size > 0) {
		render_pipeline_indices next_remaining_pipelines;
		static_array_init(next_remaining_pipelines);

		render_pipeline_group group;
		static_array_init(group);

		static_array_push(group, remaining_pipelines.values[0]);

		for (size_t index = 1; index < remaining_pipelines.size; ++index) {
			uint32_t         pipeline_index = remaining_pipelines.values[index];
			render_pipeline *pipeline       = &all_render_pipelines.values[pipeline_index];

			bool found = false;

			for (size_t index_in_bucket = 0; index_in_bucket < group.size; ++index_in_bucket) {
				render_pipeline *pipeline_in_group = &all_render_pipelines.values[group.values[index_in_bucket]];
				if (same_shader(pipeline->vertex_shader, pipeline_in_group->vertex_shader) ||
				    same_shader(pipeline->amplification_shader, pipeline_in_group->amplification_shader) ||
				    same_shader(pipeline->mesh_shader, pipeline_in_group->mesh_shader) ||
				    same_shader(pipeline->fragment_shader, pipeline_in_group->fragment_shader)) {
					found = true;
					break;
				}
			}

			if (found) {
				static_array_push(group, pipeline_index);
			}
			else {
				static_array_push(next_remaining_pipelines, pipeline_index);
			}
		}

		remaining_pipelines = next_remaining_pipelines;
		static_array_push(all_render_pipeline_groups, group);
	}
}

static void find_all_compute_shaders(void) {
	static_array_init(all_compute_shaders);

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);
		if (has_attribute(&f->attributes, add_name("compute"))) {
			static_array_push(all_compute_shaders, f);
		}
	}
}

static raytracing_pipeline extract_raytracing_pipeline_from_type(type *t) {
	name_id gen_shader_name          = NO_NAME;
	name_id miss_shader_name         = NO_NAME;
	name_id closest_shader_name      = NO_NAME;
	name_id intersection_shader_name = NO_NAME;
	name_id any_shader_name          = NO_NAME;

	for (size_t j = 0; j < t->members.size; ++j) {
		if (t->members.m[j].name == add_name("gen")) {
			gen_shader_name = t->members.m[j].value.identifier;
		}
		else if (t->members.m[j].name == add_name("miss")) {
			miss_shader_name = t->members.m[j].value.identifier;
		}
		else if (t->members.m[j].name == add_name("closest")) {
			closest_shader_name = t->members.m[j].value.identifier;
		}
		else if (t->members.m[j].name == add_name("intersection")) {
			intersection_shader_name = t->members.m[j].value.identifier;
		}
		else if (t->members.m[j].name == add_name("any")) {
			any_shader_name = t->members.m[j].value.identifier;
		}
	}

	raytracing_pipeline pipeline = {0};

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);
		if (gen_shader_name != NO_NAME && f->name == gen_shader_name) {
			pipeline.gen_shader = f;
		}
		if (miss_shader_name != NO_NAME && f->name == miss_shader_name) {
			pipeline.miss_shader = f;
		}
		if (closest_shader_name != NO_NAME && f->name == closest_shader_name) {
			pipeline.closest_shader = f;
		}
		if (intersection_shader_name != NO_NAME && f->name == intersection_shader_name) {
			pipeline.intersection_shader = f;
		}
		if (any_shader_name != NO_NAME && f->name == any_shader_name) {
			pipeline.any_shader = f;
		}
	}

	return pipeline;
}

static void find_all_raytracing_pipelines(void) {
	static_array_init(all_raytracing_pipelines);

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in && has_attribute(&t->attributes, add_name("raypipe"))) {

			static_array_push(all_raytracing_pipelines, extract_raytracing_pipeline_from_type(t));
		}
	}
}

static void find_raytracing_pipeline_groups(void) {
	static_array_init(all_raytracing_pipeline_groups);

	raytracing_pipeline_indices remaining_pipelines;
	static_array_init(remaining_pipelines);

	for (uint32_t index = 0; index < all_raytracing_pipelines.size; ++index) {
		static_array_push(remaining_pipelines, index);
	}

	while (remaining_pipelines.size > 0) {
		raytracing_pipeline_indices next_remaining_pipelines;
		static_array_init(next_remaining_pipelines);

		raytracing_pipeline_group group;
		static_array_init(group);

		static_array_push(group, remaining_pipelines.values[0]);

		for (size_t index = 1; index < remaining_pipelines.size; ++index) {
			uint32_t             pipeline_index = remaining_pipelines.values[index];
			raytracing_pipeline *pipeline       = &all_raytracing_pipelines.values[pipeline_index];

			bool found = false;

			for (size_t index_in_bucket = 0; index_in_bucket < group.size; ++index_in_bucket) {
				raytracing_pipeline *pipeline_in_group = &all_raytracing_pipelines.values[group.values[index_in_bucket]];
				if (pipeline->gen_shader == pipeline_in_group->gen_shader || pipeline->miss_shader == pipeline_in_group->miss_shader ||
				    pipeline->closest_shader == pipeline_in_group->closest_shader || pipeline->intersection_shader == pipeline_in_group->intersection_shader ||
				    pipeline->any_shader == pipeline_in_group->any_shader) {
					found = true;
					break;
				}
			}

			if (found) {
				static_array_push(group, pipeline_index);
			}
			else {
				static_array_push(next_remaining_pipelines, pipeline_index);
			}
		}

		remaining_pipelines = next_remaining_pipelines;
		static_array_push(all_raytracing_pipeline_groups, group);
	}
}

static void check_globals_in_descriptor_set_group(descriptor_set_group *group) {
	static_array(global_id, globals, 256);

	globals set_globals;
	static_array_init(set_globals);

	for (size_t set_index = 0; set_index < group->size; ++set_index) {
		descriptor_set *set = group->values[set_index];
		for (size_t global_index = 0; global_index < set->globals.size; ++global_index) {
			global_id g = set->globals.globals[global_index];

			for (size_t global_index2 = 0; global_index2 < set_globals.size; ++global_index2) {
				if (set_globals.values[global_index2] == g) {
					debug_context context = {0};
					error(context, "Global used from more than one descriptor set in one descriptor set group");
				}
			}

			static_array_push(set_globals, g);
		}
	}
}

static void update_globals_in_descriptor_set_group(descriptor_set_group *group, global_array *globals) {
	for (size_t set_index = 0; set_index < group->size; ++set_index) {
		descriptor_set *set = group->values[set_index];

		for (size_t global_index = 0; global_index < set->globals.size; ++global_index) {
			global_id g = set->globals.globals[global_index];

			for (size_t global_index2 = 0; global_index2 < globals->size; ++global_index2) {
				if (globals->globals[global_index2] == g) {
					if (globals->readable[global_index2]) {
						set->globals.readable[global_index] = true;
					}
					if (globals->writable[global_index2]) {
						set->globals.writable[global_index] = true;
					}
				}
			}
		}
	}
}

static descriptor_set_groups all_descriptor_set_groups;

descriptor_set_group *get_descriptor_set_group(uint32_t descriptor_set_group_index) {
	assert(descriptor_set_group_index < all_descriptor_set_groups.size);
	return &all_descriptor_set_groups.values[descriptor_set_group_index];
}

static void assign_descriptor_set_group_index(function *f, uint32_t descriptor_set_group_index) {
	assert(f->descriptor_set_group_index == UINT32_MAX || f->descriptor_set_group_index == descriptor_set_group_index);
	f->descriptor_set_group_index = descriptor_set_group_index;
}

static void find_descriptor_set_groups(void) {
	static_array_init(all_descriptor_set_groups);

	for (size_t pipeline_group_index = 0; pipeline_group_index < all_render_pipeline_groups.size; ++pipeline_group_index) {
		descriptor_set_group group;
		static_array_init(group);

		global_array function_globals = {0};

		render_pipeline_group *pipeline_group = &all_render_pipeline_groups.values[pipeline_group_index];
		for (size_t pipeline_index = 0; pipeline_index < pipeline_group->size; ++pipeline_index) {
			render_pipeline *pipeline = &all_render_pipelines.values[pipeline_group->values[pipeline_index]];

			if (pipeline->vertex_shader != NULL) {
				find_referenced_globals(pipeline->vertex_shader, &function_globals);
			}
			if (pipeline->amplification_shader != NULL) {
				find_referenced_globals(pipeline->amplification_shader, &function_globals);
			}
			if (pipeline->mesh_shader != NULL) {
				find_referenced_globals(pipeline->mesh_shader, &function_globals);
			}
			if (pipeline->fragment_shader != NULL) {
				find_referenced_globals(pipeline->fragment_shader, &function_globals);
			}
		}

		find_referenced_sets(&function_globals, &group);

		check_globals_in_descriptor_set_group(&group);

		update_globals_in_descriptor_set_group(&group, &function_globals);

		uint32_t descriptor_set_group_index = (uint32_t)all_descriptor_set_groups.size;
		static_array_push(all_descriptor_set_groups, group);

		for (size_t pipeline_index = 0; pipeline_index < pipeline_group->size; ++pipeline_index) {
			render_pipeline *pipeline = &all_render_pipelines.values[pipeline_group->values[pipeline_index]];

			if (pipeline->vertex_shader != NULL) {
				assign_descriptor_set_group_index(pipeline->vertex_shader, descriptor_set_group_index);
			}
			if (pipeline->amplification_shader != NULL) {
				assign_descriptor_set_group_index(pipeline->amplification_shader, descriptor_set_group_index);
			}
			if (pipeline->mesh_shader != NULL) {
				assign_descriptor_set_group_index(pipeline->mesh_shader, descriptor_set_group_index);
			}
			if (pipeline->fragment_shader != NULL) {
				assign_descriptor_set_group_index(pipeline->fragment_shader, descriptor_set_group_index);
			}
		}
	}

	for (size_t compute_shader_index = 0; compute_shader_index < all_compute_shaders.size; ++compute_shader_index) {
		descriptor_set_group group;
		static_array_init(group);

		global_array function_globals = {0};

		find_referenced_globals(all_compute_shaders.values[compute_shader_index], &function_globals);

		find_referenced_sets(&function_globals, &group);

		check_globals_in_descriptor_set_group(&group);

		update_globals_in_descriptor_set_group(&group, &function_globals);

		uint32_t descriptor_set_group_index = (uint32_t)all_descriptor_set_groups.size;
		static_array_push(all_descriptor_set_groups, group);

		for (size_t compute_shader_index = 0; compute_shader_index < all_compute_shaders.size; ++compute_shader_index) {
			assign_descriptor_set_group_index(all_compute_shaders.values[compute_shader_index], descriptor_set_group_index);
		}
	}

	for (size_t pipeline_group_index = 0; pipeline_group_index < all_raytracing_pipeline_groups.size; ++pipeline_group_index) {
		descriptor_set_group group;
		static_array_init(group);

		global_array function_globals = {0};

		raytracing_pipeline_group *pipeline_group = &all_raytracing_pipeline_groups.values[pipeline_group_index];
		for (size_t pipeline_index = 0; pipeline_index < pipeline_group->size; ++pipeline_index) {
			raytracing_pipeline *pipeline = &all_raytracing_pipelines.values[pipeline_group->values[pipeline_index]];

			if (pipeline->gen_shader != NULL) {
				find_referenced_globals(pipeline->gen_shader, &function_globals);
			}
			if (pipeline->miss_shader != NULL) {
				find_referenced_globals(pipeline->miss_shader, &function_globals);
			}
			if (pipeline->closest_shader != NULL) {
				find_referenced_globals(pipeline->closest_shader, &function_globals);
			}
			if (pipeline->intersection_shader != NULL) {
				find_referenced_globals(pipeline->intersection_shader, &function_globals);
			}
			if (pipeline->any_shader != NULL) {
				find_referenced_globals(pipeline->any_shader, &function_globals);
			}
		}

		find_referenced_sets(&function_globals, &group);

		check_globals_in_descriptor_set_group(&group);

		update_globals_in_descriptor_set_group(&group, &function_globals);

		uint32_t descriptor_set_group_index = (uint32_t)all_descriptor_set_groups.size;
		static_array_push(all_descriptor_set_groups, group);

		for (size_t pipeline_index = 0; pipeline_index < pipeline_group->size; ++pipeline_index) {
			raytracing_pipeline *pipeline = &all_raytracing_pipelines.values[pipeline_group->values[pipeline_index]];

			if (pipeline->gen_shader != NULL) {
				assign_descriptor_set_group_index(pipeline->gen_shader, descriptor_set_group_index);
			}
			if (pipeline->miss_shader != NULL) {
				assign_descriptor_set_group_index(pipeline->miss_shader, descriptor_set_group_index);
			}
			if (pipeline->closest_shader != NULL) {
				assign_descriptor_set_group_index(pipeline->closest_shader, descriptor_set_group_index);
			}
			if (pipeline->intersection_shader != NULL) {
				assign_descriptor_set_group_index(pipeline->intersection_shader, descriptor_set_group_index);
			}
			if (pipeline->any_shader != NULL) {
				assign_descriptor_set_group_index(pipeline->any_shader, descriptor_set_group_index);
			}
		}
	}
}

descriptor_set_group *find_descriptor_set_group_for_pipe_type(type *t) {
	if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
		render_pipeline pipeline = extract_render_pipeline_from_type(t);

		if (pipeline.vertex_shader->descriptor_set_group_index != UINT32_MAX) {
			return &all_descriptor_set_groups.values[pipeline.vertex_shader->descriptor_set_group_index];
		}

		if (pipeline.amplification_shader->descriptor_set_group_index != UINT32_MAX) {
			return &all_descriptor_set_groups.values[pipeline.amplification_shader->descriptor_set_group_index];
		}

		if (pipeline.mesh_shader->descriptor_set_group_index != UINT32_MAX) {
			return &all_descriptor_set_groups.values[pipeline.mesh_shader->descriptor_set_group_index];
		}

		if (pipeline.fragment_shader->descriptor_set_group_index != UINT32_MAX) {
			return &all_descriptor_set_groups.values[pipeline.fragment_shader->descriptor_set_group_index];
		}

		return NULL;
	}

	if (!t->built_in && has_attribute(&t->attributes, add_name("raypipe"))) {
		raytracing_pipeline pipeline = extract_raytracing_pipeline_from_type(t);

		if (pipeline.gen_shader->descriptor_set_group_index != UINT32_MAX) {
			return &all_descriptor_set_groups.values[pipeline.gen_shader->descriptor_set_group_index];
		}

		if (pipeline.miss_shader->descriptor_set_group_index != UINT32_MAX) {
			return &all_descriptor_set_groups.values[pipeline.miss_shader->descriptor_set_group_index];
		}

		if (pipeline.closest_shader->descriptor_set_group_index != UINT32_MAX) {
			return &all_descriptor_set_groups.values[pipeline.closest_shader->descriptor_set_group_index];
		}

		if (pipeline.intersection_shader->descriptor_set_group_index != UINT32_MAX) {
			return &all_descriptor_set_groups.values[pipeline.intersection_shader->descriptor_set_group_index];
		}

		if (pipeline.any_shader->descriptor_set_group_index != UINT32_MAX) {
			return &all_descriptor_set_groups.values[pipeline.any_shader->descriptor_set_group_index];
		}

		return NULL;
	}

	return NULL;
}

descriptor_set_group *find_descriptor_set_group_for_function(function *f) {
	if (f->descriptor_set_group_index != UINT32_MAX) {
		return &all_descriptor_set_groups.values[f->descriptor_set_group_index];
	}
	else {
		return NULL;
	}
}

void find_sampler_use() {
	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);

		if (f->block == NULL) {
			continue;
		}

		uint8_t *data = f->code.o;
		size_t   size = f->code.size;

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];

			switch (o->type) {
			case OPCODE_CALL:
				if (o->op_call.func == add_name("sample") || o->op_call.func == add_name("sample_lod")) {
					if (is_depth(get_type(o->op_call.parameters[0].type.type)->tex_format)) {

						type *sampler_type = get_type(o->op_call.parameters[1].type.type);

						global *g = find_global_by_var(o->op_call.parameters[1]);
						assert(g != NULL);
						g->usage |= GLOBAL_USAGE_SAMPLE_DEPTH;
					}
				}
				break;
			default:
				break;
			}

			index += o->size;
		}
	}
}

void analyze(void) {
	find_all_render_pipelines();
	find_render_pipeline_groups();

	find_all_compute_shaders();

	find_all_raytracing_pipelines();
	find_raytracing_pipeline_groups();

	find_descriptor_set_groups();

	find_sampler_use();
}
