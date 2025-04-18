#include "wgsl.h"

#include "../analyzer.h"
#include "../compiler.h"
#include "../errors.h"
#include "../functions.h"
#include "../parser.h"
#include "../shader_stage.h"
#include "../types.h"
#include "cstyle.h"
#include "d3d11.h"
#include "util.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *type_string(type_id type) {
	if (type == float_id) {
		return "f32";
	}
	if (type == float2_id) {
		return "vec2<f32>";
	}
	if (type == float3_id) {
		return "vec3<f32>";
	}
	if (type == float4_id) {
		return "vec4<f32>";
	}
	if (type == float4x4_id) {
		return "mat4x4<f32>";
	}
	return get_name(get_type(type)->name);
}

// static char *function_string(name_id func) {
//	return get_name(func);
// }

static void write_code(char *wgsl, char *directory, const char *filename) {
	char full_filename[512];

	{
		sprintf(full_filename, "%s/%s.h", directory, filename);
		FILE *file = fopen(full_filename, "wb");
		fprintf(file, "#include <stddef.h>\n\n");
		fprintf(file, "extern const char *wgsl;\n");
		fprintf(file, "extern size_t wgsl_size;\n");
		fclose(file);
	}

	{
		sprintf(full_filename, "%s/%s.c", directory, filename);
		FILE *file = fopen(full_filename, "wb");

		fprintf(file, "#include \"%s.h\"\n\n", filename);

		fprintf(file, "const char *wgsl = \"");

		size_t length = strlen(wgsl);

		for (size_t i = 0; i < length; ++i) {
			if (wgsl[i] == '\n') {
				fprintf(file, "\\n");
			}
			else if (wgsl[i] == '\r') {
				fprintf(file, "\\r");
			}
			else if (wgsl[i] == '\t') {
				fprintf(file, "\\t");
			}
			else if (wgsl[i] == '"') {
				fprintf(file, "\\\"");
			}
			else {
				fprintf(file, "%c", wgsl[i]);
			}
		}

		fprintf(file, "\";\n\n");

		fprintf(file, "size_t wgsl_size = %zu;\n\n", length);

		fprintf(file, "/*\n%s*/\n", wgsl);

		fclose(file);
	}
}

static type_id vertex_inputs[256];
static size_t  vertex_inputs_size = 0;
static type_id fragment_inputs[256];
static size_t  fragment_inputs_size = 0;

static bool is_vertex_input(type_id t) {
	for (size_t i = 0; i < vertex_inputs_size; ++i) {
		if (t == vertex_inputs[i]) {
			return true;
		}
	}
	return false;
}

static bool is_fragment_input(type_id t) {
	for (size_t i = 0; i < fragment_inputs_size; ++i) {
		if (t == fragment_inputs[i]) {
			return true;
		}
	}
	return false;
}

static void write_types(char *wgsl, size_t *offset) {
	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);

		if (!t->built_in && !has_attribute(&t->attributes, add_name("pipe"))) {
			if (t->name == NO_NAME) {
				char name[256];

				bool found = false;
				for (global_id j = 0; get_global(j)->type != NO_TYPE; ++j) {
					global *g = get_global(j);
					if (g->type == i) {
						sprintf(name, "_%" PRIu64, g->var_index);
						found = true;
						break;
					}
				}

				if (!found) {
					strcpy(name, "Unknown");
				}

				*offset += sprintf(&wgsl[*offset], "struct %s_type {\n", name);
			}
			else {
				*offset += sprintf(&wgsl[*offset], "struct %s {\n", get_name(t->name));
			}

			if (is_vertex_input(i)) {
				for (size_t j = 0; j < t->members.size; ++j) {
					*offset += sprintf(&wgsl[*offset], "\t@location(%zu) %s: %s,\n", j, get_name(t->members.m[j].name), type_string(t->members.m[j].type.type));
				}
			}
			else if (is_fragment_input(i)) {
				for (size_t j = 0; j < t->members.size; ++j) {
					if (j == 0) {
						*offset +=
						    sprintf(&wgsl[*offset], "\t@builtin(position) %s: %s,\n", get_name(t->members.m[j].name), type_string(t->members.m[j].type.type));
					}
					else {
						*offset += sprintf(&wgsl[*offset], "\t@location(%zu) %s: %s,\n", j - 1, get_name(t->members.m[j].name),
						                   type_string(t->members.m[j].type.type));
					}
				}
			}
			else {
				for (size_t j = 0; j < t->members.size; ++j) {
					*offset += sprintf(&wgsl[*offset], "\t%s: %s,\n", get_name(t->members.m[j].name), type_string(t->members.m[j].type.type));
				}
			}
			*offset += sprintf(&wgsl[*offset], "};\n\n");
		}
	}
}

static void assign_bindings(uint32_t *bindings) {
	for (size_t set_index = 0; set_index < get_sets_count(); ++set_index) {
		uint32_t binding = 0;

		descriptor_set *set = get_set(set_index);

		if (set->name == add_name("root_constants")) {
			if (set->globals.size != 1) {
				debug_context context = {0};
				error(context, "More than one root constants struct found");
			}

			global_id g_id = set->globals.globals[0];
			global   *g    = get_global(g_id);

			if (get_type(g->type)->built_in) {
				debug_context context = {0};
				error(context, "Unsupported type for a root constant");
			}

			bindings[g_id] = binding;
			binding += 1;

			continue;
		}

		for (size_t g_index = 0; g_index < set->globals.size; ++g_index) {
			global_id global_index = set->globals.globals[g_index];
			bool      writable     = set->globals.writable[g_index];

			global *g = get_global(global_index);

			type   *t         = get_type(g->type);
			type_id base_type = t->array_size > 0 ? t->base : g->type;

			if (base_type == sampler_type_id) {
				bindings[global_index] = binding;
				binding += 1;
			}
			else if (base_type == tex2d_type_id) {
				if (t->array_size == UINT32_MAX) {
					bindings[global_index] = 0;
				}
				else if (writable) {
					bindings[global_index] = binding;
					binding += 1;
				}
				else {
					bindings[global_index] = binding;
					binding += 1;
				}
			}
			else if (base_type == texcube_type_id || base_type == tex2darray_type_id || base_type == bvh_type_id) {
				bindings[global_index] = binding;
				binding += 1;
			}
			else if (get_type(g->type)->built_in) {
				if (get_type(g->type)->array_size > 0) {
					bindings[global_index] = binding;
					binding += 1;
				}
			}
			else {
				if (get_type(g->type)->array_size > 0) {
					bindings[global_index] = binding;
					binding += 1;
				}
				else {
					bindings[global_index] = binding;
					binding += 1;
				}
			}
		}
	}
}

static void write_globals(char *wgsl, size_t *offset) {
	uint32_t bindings[512] = {0};
	assign_bindings(bindings);

	for (global_id i = 0; get_global(i) != NULL && get_global(i)->type != NO_TYPE; ++i) {
		global  *g       = get_global(i);
		uint32_t binding = bindings[i];

		if (g->type == sampler_type_id) {
			*offset += sprintf(&wgsl[*offset], "@group(0) @binding(%u) var _%" PRIu64 ": sampler;\n\n", binding, g->var_index);
		}
		else if (g->type == tex2d_type_id) {
			*offset += sprintf(&wgsl[*offset], "@group(0) @binding(%u) var _%" PRIu64 ": texture_2d<f32>;\n\n", binding, g->var_index);
		}
		else if (g->type == texcube_type_id) {
			*offset += sprintf(&wgsl[*offset], "@group(0) @binding(%u) var _%" PRIu64 ": texture_cube<f32>;\n\n", binding, g->var_index);
		}
		else if (g->type == float_id) {
		}
		else if (g->type == uint_id) {
		}
		else {
			type *t = get_type(g->type);
			char  type_name[256];
			if (t->name != NO_NAME) {
				strcpy(type_name, get_name(t->name));
			}
			else {
				sprintf(type_name, "_%" PRIu64 "_type", g->var_index);
			}
			*offset += sprintf(&wgsl[*offset], "@group(0) @binding(%u) var<uniform> _%" PRIu64 ": %s;\n\n", binding, g->var_index, type_name);
		}
	}
}

static function_id vertex_functions[256];
static size_t      vertex_functions_size = 0;
static function_id fragment_functions[256];
static size_t      fragment_functions_size = 0;

static bool is_vertex_function(function_id f) {
	for (size_t i = 0; i < vertex_functions_size; ++i) {
		if (f == vertex_functions[i]) {
			return true;
		}
	}
	return false;
}

static bool is_fragment_function(function_id f) {
	for (size_t i = 0; i < fragment_functions_size; ++i) {
		if (f == fragment_functions[i]) {
			return true;
		}
	}
	return false;
}

static void write_functions(char *code, size_t *offset) {
	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);

		if (f->block == NULL) {
			continue;
		}

		uint8_t *data = f->code.o;
		size_t   size = f->code.size;

		uint64_t parameter_ids[256] = {0};
		for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
			for (size_t i = 0; i < f->block->block.vars.size; ++i) {
				if (f->parameter_names[parameter_index] == f->block->block.vars.v[i].name) {
					parameter_ids[parameter_index] = f->block->block.vars.v[i].variable_id;
					break;
				}
			}
		}

		debug_context context = {0};
		for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
			check(parameter_ids[parameter_index] != 0, context, "Parameter not found");
		}

		if (is_vertex_function(i)) {
			*offset += sprintf(&code[*offset], "@vertex fn %s(", get_name(f->name));
			for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
				if (parameter_index == 0) {
					*offset +=
					    sprintf(&code[*offset], "_%" PRIu64 ": %s", parameter_ids[parameter_index], type_string(f->parameter_types[parameter_index].type));
				}
				else {
					*offset +=
					    sprintf(&code[*offset], ", _%" PRIu64 ": %s", parameter_ids[parameter_index], type_string(f->parameter_types[parameter_index].type));
				}
			}
			*offset += sprintf(&code[*offset], ") -> %s {\n", type_string(f->return_type.type));
		}
		else if (is_fragment_function(i)) {
			if (get_type(f->return_type.type)->array_size > 0) {
				*offset += sprintf(&code[*offset], "struct _kong_colors_out {\n");
				for (uint32_t j = 0; j < get_type(f->return_type.type)->array_size; ++j) {
					*offset += sprintf(&code[*offset], "\t%s _%i : SV_Target%i;\n", type_string(f->return_type.type), j, j);
				}
				*offset += sprintf(&code[*offset], "};\n\n");

				*offset += sprintf(&code[*offset], "_kong_colors_out main(");
				for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
					if (parameter_index == 0) {
						*offset +=
						    sprintf(&code[*offset], "%s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
					}
					else {
						*offset +=
						    sprintf(&code[*offset], ", %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
					}
				}
				*offset += sprintf(&code[*offset], ") {\n");
			}
			else {
				*offset += sprintf(&code[*offset], "@fragment fn %s(", get_name(f->name));
				for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
					if (parameter_index == 0) {
						*offset +=
						    sprintf(&code[*offset], "_%" PRIu64 ": %s", parameter_ids[parameter_index], type_string(f->parameter_types[parameter_index].type));
					}
					else {
						*offset += sprintf(&code[*offset], ", _%" PRIu64 ": %s", parameter_ids[parameter_index],
						                   type_string(f->parameter_types[parameter_index].type));
					}
				}
				*offset += sprintf(&code[*offset], ") -> @location(0) %s {\n", type_string(f->return_type.type));
			}
		}

		else {
			*offset += sprintf(&code[*offset], "%s %s(", type_string(f->return_type.type), get_name(f->name));
			for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
				if (parameter_index == 0) {
					*offset += sprintf(&code[*offset], "%s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
				else {
					*offset += sprintf(&code[*offset], "%s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
			}
			*offset += sprintf(&code[*offset], ") {\n");
		}

		int indentation = 1;

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];
			switch (o->type) {
			case OPCODE_VAR:
				indent(code, offset, indentation);
				if (get_type(o->op_var.var.type.type)->array_size > 0) {
					*offset += sprintf(&code[*offset], "%s _%" PRIu64 "[%i];\n", type_string(o->op_var.var.type.type), o->op_var.var.index,
					                   get_type(o->op_var.var.type.type)->array_size);
				}
				else {
					*offset += sprintf(&code[*offset], "var _%" PRIu64 ": %s;\n", o->op_var.var.index, type_string(o->op_var.var.type.type));
				}
				break;
			case OPCODE_LOAD_ACCESS_LIST: {
				indent(code, offset, indentation);
				*offset += sprintf(&code[*offset], "var _%" PRIu64 ": %s = _%" PRIu64, o->op_load_access_list.to.index,
				                   type_string(o->op_load_access_list.to.type.type), o->op_load_access_list.from.index);

				type *s = get_type(o->op_load_access_list.from.type.type);

				for (size_t i = 0; i < o->op_load_access_list.access_list_size; ++i) {
					switch (o->op_load_access_list.access_list[i].kind) {
					case ACCESS_ELEMENT:
						*offset += sprintf(&code[*offset], "[_%" PRIu64 "]", o->op_load_access_list.access_list[i].access_element.index.index);
						break;
					case ACCESS_MEMBER:
						*offset += sprintf(&code[*offset], ".%s", get_name(o->op_load_access_list.access_list[i].access_member.name));
						break;
					case ACCESS_SWIZZLE: {
						char swizzle[4];

						for (uint32_t swizzle_index = 0; swizzle_index < o->op_load_access_list.access_list[i].access_swizzle.swizzle.size; ++swizzle_index) {
							swizzle[swizzle_index] = "xyzw"[o->op_load_access_list.access_list[i].access_swizzle.swizzle.indices[swizzle_index]];
						}
						swizzle[o->op_load_access_list.access_list[i].access_swizzle.swizzle.size] = 0;

						*offset += sprintf(&code[*offset], ".%s", swizzle);
						break;
					}
					}

					s = get_type(o->op_load_access_list.access_list[i].type);
				}

				*offset += sprintf(&code[*offset], ";\n");

				break;
			}
			case OPCODE_STORE_ACCESS_LIST:
			case OPCODE_SUB_AND_STORE_ACCESS_LIST:
			case OPCODE_ADD_AND_STORE_ACCESS_LIST:
			case OPCODE_DIVIDE_AND_STORE_ACCESS_LIST:
			case OPCODE_MULTIPLY_AND_STORE_ACCESS_LIST: {
				indent(code, offset, indentation);
				*offset += sprintf(&code[*offset], "_%" PRIu64, o->op_store_access_list.to.index);

				type *s = get_type(o->op_store_access_list.to.type.type);

				for (size_t i = 0; i < o->op_store_access_list.access_list_size; ++i) {
					switch (o->op_store_access_list.access_list[i].kind) {
					case ACCESS_ELEMENT:
						*offset += sprintf(&code[*offset], "[_%" PRIu64 "]", o->op_store_access_list.access_list[i].access_element.index.index);
						break;
					case ACCESS_MEMBER:
						*offset += sprintf(&code[*offset], ".%s", get_name(o->op_store_access_list.access_list[i].access_member.name));
						break;
					case ACCESS_SWIZZLE: {
						char swizzle[4];

						for (uint32_t swizzle_index = 0; swizzle_index < o->op_store_access_list.access_list[i].access_swizzle.swizzle.size; ++swizzle_index) {
							swizzle[swizzle_index] = "xyzw"[o->op_store_access_list.access_list[i].access_swizzle.swizzle.indices[swizzle_index]];
						}
						swizzle[o->op_store_access_list.access_list[i].access_swizzle.swizzle.size] = 0;

						*offset += sprintf(&code[*offset], ".%s", swizzle);

						break;
					}
					}

					s = get_type(o->op_store_access_list.access_list[i].type);
				}

				switch (o->type) {
				case OPCODE_STORE_ACCESS_LIST:
					*offset += sprintf(&code[*offset], " = _%" PRIu64 ";\n", o->op_store_access_list.from.index);
					break;
				case OPCODE_SUB_AND_STORE_ACCESS_LIST:
					*offset += sprintf(&code[*offset], " -= _%" PRIu64 ";\n", o->op_store_access_list.from.index);
					break;
				case OPCODE_ADD_AND_STORE_ACCESS_LIST:
					*offset += sprintf(&code[*offset], " += _%" PRIu64 ";\n", o->op_store_access_list.from.index);
					break;
				case OPCODE_DIVIDE_AND_STORE_ACCESS_LIST:
					*offset += sprintf(&code[*offset], " /= _%" PRIu64 ";\n", o->op_store_access_list.from.index);
					break;
				case OPCODE_MULTIPLY_AND_STORE_ACCESS_LIST:
					*offset += sprintf(&code[*offset], " *= _%" PRIu64 ";\n", o->op_store_access_list.from.index);
					break;
				default:
					assert(false);
					break;
				}
				break;
			}
			case OPCODE_RETURN: {
				if (o->size > offsetof(opcode, op_return)) {
					if (is_fragment_function(i) && get_type(f->return_type.type)->array_size > 0) {
						indent(code, offset, indentation);
						*offset += sprintf(&code[*offset], "{\n");
						indent(code, offset, indentation + 1);
						*offset += sprintf(&code[*offset], "_kong_colors_out _kong_colors;\n");
						for (uint32_t j = 0; j < get_type(f->return_type.type)->array_size; ++j) {
							indent(code, offset, indentation + 1);
							*offset += sprintf(&code[*offset], "_kong_colors._%i = _%" PRIu64 "[%i];\n", j, o->op_return.var.index, j);
						}
						indent(code, offset, indentation + 1);
						*offset += sprintf(&code[*offset], "return _kong_colors;\n");
						indent(code, offset, indentation);
						*offset += sprintf(&code[*offset], "}\n");
					}
					else {
						indent(code, offset, indentation);
						*offset += sprintf(&code[*offset], "return _%" PRIu64 ";\n", o->op_return.var.index);
					}
				}
				else {
					indent(code, offset, indentation);
					*offset += sprintf(&code[*offset], "return;\n");
				}
				break;
			}
			case OPCODE_MULTIPLY: {
				indent(code, offset, indentation);
				*offset += sprintf(&code[*offset], "var _%" PRIu64 ": %s = _%" PRIu64 " * _%" PRIu64 ";\n", o->op_binary.result.index,
				                   type_string(o->op_binary.result.type.type), o->op_binary.left.index, o->op_binary.right.index);
				break;
			}
			case OPCODE_DIVIDE: {
				indent(code, offset, indentation);
				*offset += sprintf(&code[*offset], "var _%" PRIu64 ": %s = _%" PRIu64 " / _%" PRIu64 ";\n", o->op_binary.result.index,
				                   type_string(o->op_binary.result.type.type), o->op_binary.left.index, o->op_binary.right.index);
				break;
			}
			case OPCODE_ADD: {
				indent(code, offset, indentation);
				*offset += sprintf(&code[*offset], "var _%" PRIu64 ": %s = _%" PRIu64 " + _%" PRIu64 ";\n", o->op_binary.result.index,
				                   type_string(o->op_binary.result.type.type), o->op_binary.left.index, o->op_binary.right.index);
				break;
			}
			case OPCODE_SUB: {
				indent(code, offset, indentation);
				*offset += sprintf(&code[*offset], "var _%" PRIu64 ": %s = _%" PRIu64 " - _%" PRIu64 ";\n", o->op_binary.result.index,
				                   type_string(o->op_binary.result.type.type), o->op_binary.left.index, o->op_binary.right.index);
				break;
			}
			case OPCODE_LOAD_FLOAT_CONSTANT:
				indent(code, offset, indentation);
				*offset += sprintf(&code[*offset], "var _%" PRIu64 ": %s = %f;\n", o->op_load_float_constant.to.index,
				                   type_string(o->op_load_float_constant.to.type.type), o->op_load_float_constant.number);
				break;
			case OPCODE_LOAD_BOOL_CONSTANT:
				indent(code, offset, indentation);
				*offset += sprintf(&code[*offset], "var _%" PRIu64 ": %s = %s;\n", o->op_load_bool_constant.to.index,
				                   type_string(o->op_load_bool_constant.to.type.type), o->op_load_bool_constant.boolean ? "true" : "false");
				break;
			case OPCODE_CALL: {
				debug_context context = {0};
				if (o->op_call.func == add_name("sample")) {
					check(o->op_call.parameters_size == 3, context, "sample requires three arguments");
					indent(code, offset, indentation);
					*offset += sprintf(&code[*offset], "var _%" PRIu64 ": %s = textureSample(_%" PRIu64 ", _%" PRIu64 ", _%" PRIu64 ");\n",
					                   o->op_call.var.index, type_string(o->op_call.var.type.type), o->op_call.parameters[0].index,
					                   o->op_call.parameters[1].index, o->op_call.parameters[2].index);
				}
				else if (o->op_call.func == add_name("sample_lod")) {
					check(o->op_call.parameters_size == 4, context, "sample_lod requires four arguments");
					indent(code, offset, indentation);
					*offset += sprintf(&code[*offset], "var _%" PRIu64 ": %s = textureSample(_%" PRIu64 ",_%" PRIu64 ", _%" PRIu64 ", _%" PRIu64 ");\n",
					                   o->op_call.var.index, type_string(o->op_call.var.type.type), o->op_call.parameters[0].index,
					                   o->op_call.parameters[1].index, o->op_call.parameters[2].index, o->op_call.parameters[3].index);
				}
				else {
					const char *function_name = get_name(o->op_call.func);
					if (o->op_call.func == add_name("float2")) {
						function_name = "vec2<f32>";
					}
					else if (o->op_call.func == add_name("float3")) {
						function_name = "vec3<f32>";
					}
					else if (o->op_call.func == add_name("float4")) {
						function_name = "vec4<f32>";
					}

					indent(code, offset, indentation);
					*offset +=
					    sprintf(&code[*offset], "var _%" PRIu64 ": %s = %s(", o->op_call.var.index, type_string(o->op_call.var.type.type), function_name);
					if (o->op_call.parameters_size > 0) {
						*offset += sprintf(&code[*offset], "_%" PRIu64, o->op_call.parameters[0].index);
						for (uint8_t i = 1; i < o->op_call.parameters_size; ++i) {
							*offset += sprintf(&code[*offset], ", _%" PRIu64, o->op_call.parameters[i].index);
						}
					}
					*offset += sprintf(&code[*offset], ");\n");
				}
				break;
			}
			default:
				cstyle_write_opcode(code, offset, o, type_string, &indentation);
				break;
			}

			index += o->size;
		}

		*offset += sprintf(&code[*offset], "}\n\n");
	}
}

static void wgsl_export_everything(char *directory) {
	char         *wgsl    = (char *)calloc(1024 * 1024, 1);
	debug_context context = {0};
	check(wgsl != NULL, context, "Could not allocate the wgsl string");
	size_t offset = 0;

	write_types(wgsl, &offset);

	write_globals(wgsl, &offset);

	write_functions(wgsl, &offset);

	write_code(wgsl, directory, "wgsl");
}

void wgsl_export(char *directory) {
	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
			name_id vertex_shader_name   = NO_NAME;
			name_id fragment_shader_name = NO_NAME;

			for (size_t j = 0; j < t->members.size; ++j) {
				if (t->members.m[j].name == add_name("vertex")) {
					vertex_shader_name = t->members.m[j].value.identifier;
				}
				else if (t->members.m[j].name == add_name("fragment")) {
					fragment_shader_name = t->members.m[j].value.identifier;
				}
			}

			debug_context context = {0};
			check(vertex_shader_name != NO_NAME, context, "vertex shader not found");
			check(fragment_shader_name != NO_NAME, context, "fragment shader not found");

			for (function_id i = 0; get_function(i) != NULL; ++i) {
				function *f = get_function(i);
				if (f->name == vertex_shader_name) {
					vertex_functions[vertex_functions_size] = i;
					vertex_functions_size += 1;

					assert(f->parameters_size > 0);
					vertex_inputs[vertex_inputs_size] = f->parameter_types[0].type;
					vertex_inputs_size += 1;
				}
				else if (f->name == fragment_shader_name) {
					fragment_functions[fragment_functions_size] = i;
					fragment_functions_size += 1;

					assert(f->parameters_size > 0);
					fragment_inputs[fragment_inputs_size] = f->parameter_types[0].type;
					fragment_inputs_size += 1;
				}
			}
		}
	}

	wgsl_export_everything(directory);
}
