#include "functions.h"

#include "errors.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static function *functions = NULL;
static function_id functions_size = 1024;
static function_id next_function_index = 0;

function_id sample_id;
function_id float2_constructor_id;
function_id float3_constructor_id;
function_id float4_constructor_id;

static void add_func_int(char *name) {
	function_id func = add_function(add_name(name));
	function *f = get_function(func);
	init_type_ref(&f->return_type, add_name("int"));
	f->return_type.type = find_type_by_ref(&f->return_type);
	f->parameters_size = 0;
	f->block = NULL;
}

static void add_func_float3_float_float_float(char *name) {
	function_id func = add_function(add_name(name));
	function *f = get_function(func);
	init_type_ref(&f->return_type, add_name("float3"));
	f->return_type.type = find_type_by_ref(&f->return_type);
	f->parameter_names[0] = add_name("a");
	f->parameter_names[1] = add_name("b");
	f->parameter_names[2] = add_name("c");
	for (int i = 0; i < 3; ++i) {
		init_type_ref(&f->parameter_types[0], add_name("float"));
		f->parameter_types[0].type = find_type_by_ref(&f->parameter_types[0]);
	}
	f->parameters_size = 3;
	f->block = NULL;
}

static void add_func_float3(char *name) {
	function_id func = add_function(add_name(name));
	function *f = get_function(func);
	init_type_ref(&f->return_type, add_name("float3"));
	f->return_type.type = find_type_by_ref(&f->return_type);
	f->parameters_size = 0;
	f->block = NULL;
}

static void add_func_uint3(char *name) {
	function_id func = add_function(add_name(name));
	function *f = get_function(func);
	init_type_ref(&f->return_type, add_name("uint3"));
	f->return_type.type = find_type_by_ref(&f->return_type);
	f->parameters_size = 0;
	f->block = NULL;
}

static void add_func_float_float(char *name) {
	function_id func = add_function(add_name(name));
	function *f = get_function(func);
	init_type_ref(&f->return_type, add_name("float"));
	f->return_type.type = find_type_by_ref(&f->return_type);
	f->parameter_names[0] = add_name("a");
	init_type_ref(&f->parameter_types[0], add_name("float"));
	f->parameter_types[0].type = find_type_by_ref(&f->parameter_types[0]);
	f->parameters_size = 1;
	f->block = NULL;
}

static void add_func_float3_float3(char *name) {
	function_id func = add_function(add_name(name));
	function *f = get_function(func);
	init_type_ref(&f->return_type, add_name("float3"));
	f->return_type.type = find_type_by_ref(&f->return_type);
	f->parameter_names[0] = add_name("a");
	init_type_ref(&f->parameter_types[0], add_name("float3"));
	f->parameter_types[0].type = find_type_by_ref(&f->parameter_types[0]);
	f->parameters_size = 1;
	f->block = NULL;
}

void functions_init(void) {
	function *new_functions = realloc(functions, functions_size * sizeof(function));
	debug_context context = {0};
	check(new_functions != NULL, context, "Could not allocate functions");
	functions = new_functions;
	next_function_index = 0;

	{
		sample_id = add_function(add_name("sample"));
		function *f = get_function(sample_id);
		init_type_ref(&f->return_type, add_name("float4"));
		f->return_type.type = find_type_by_ref(&f->return_type);
		f->parameter_names[0] = add_name("tex_coord");
		init_type_ref(&f->parameter_types[0], add_name("float2"));
		f->parameter_types[0].type = find_type_by_ref(&f->parameter_types[0]);
		f->parameters_size = 1;
		f->block = NULL;
	}

	{
		sample_id = add_function(add_name("sample_lod"));
		function *f = get_function(sample_id);
		init_type_ref(&f->return_type, add_name("float4"));
		f->return_type.type = find_type_by_ref(&f->return_type);
		f->parameter_names[0] = add_name("tex_coord");
		init_type_ref(&f->parameter_types[0], add_name("float2"));
		f->parameter_types[0].type = find_type_by_ref(&f->parameter_types[0]);
		f->parameters_size = 1;
		f->block = NULL;
	}

	{
		float2_constructor_id = add_function(add_name("float2"));
		function *f = get_function(float2_constructor_id);
		init_type_ref(&f->return_type, add_name("float2"));
		f->return_type.type = find_type_by_ref(&f->return_type);
		f->parameter_names[0] = add_name("x");
		init_type_ref(&f->parameter_types[0], add_name("float"));
		f->parameter_types[0].type = find_type_by_ref(&f->parameter_types[0]);
		f->parameters_size = 1;
		f->block = NULL;
	}

	{
		float3_constructor_id = add_function(add_name("float3"));
		function *f = get_function(float3_constructor_id);
		init_type_ref(&f->return_type, add_name("float3"));
		f->return_type.type = find_type_by_ref(&f->return_type);
		f->parameter_names[0] = add_name("x");
		init_type_ref(&f->parameter_types[0], add_name("float"));
		f->parameter_types[0].type = find_type_by_ref(&f->parameter_types[0]);
		f->parameters_size = 1;
		f->block = NULL;
	}

	{
		float4_constructor_id = add_function(add_name("float4"));
		function *f = get_function(float4_constructor_id);
		init_type_ref(&f->return_type, add_name("float4"));
		f->return_type.type = find_type_by_ref(&f->return_type);
		f->parameter_names[0] = add_name("x");
		init_type_ref(&f->parameter_types[0], add_name("float"));
		f->parameter_types[0].type = find_type_by_ref(&f->parameter_types[0]);
		f->parameters_size = 1;
		f->block = NULL;
	}

	add_func_int("group_id");
	add_func_int("group_thread_id");
	add_func_int("dispatch_thread_id");
	add_func_int("group_index");

	add_func_float3_float_float_float("lerp");
	add_func_float3("world_ray_direction");
	add_func_float3_float3("normalize");
	add_func_float_float("saturate");
	add_func_uint3("ray_index");
	add_func_float3("ray_dimensions");
}

static void grow_if_needed(uint64_t size) {
	while (size >= functions_size) {
		functions_size *= 2;
		function *new_functions = realloc(functions, functions_size * sizeof(function));
		debug_context context = {0};
		check(new_functions != NULL, context, "Could not allocate functions");
		functions = new_functions;
	}
}

function_id add_function(name_id name) {
	grow_if_needed(next_function_index + 1);

	function_id f = next_function_index;
	++next_function_index;

	functions[f].name = name;
	functions[f].attributes.attributes_count = 0;
	init_type_ref(&functions[f].return_type, NO_NAME);
	functions[f].parameters_size = 0;
	functions[f].block = NULL;
	memset(functions[f].code.o, 0, sizeof(functions[f].code.o));
	functions[f].code.size = 0;

	return f;
}

function_id find_function(name_id name) {
	for (function_id i = 0; i < next_function_index; ++i) {
		if (functions[i].name == name) {
			return i;
		}
	}

	return NO_FUNCTION;
}

function *get_function(function_id function) {
	if (function >= next_function_index) {
		return NULL;
	}
	return &functions[function];
}
