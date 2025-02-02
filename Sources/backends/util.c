#include "util.h"

#include "../errors.h"

#include <string.h>

void indent(char *code, size_t *offset, int indentation) {
	indentation = indentation < 15 ? indentation : 15;
	char str[16];
	memset(str, '\t', sizeof(str));
	str[indentation] = 0;
	*offset += sprintf(&code[*offset], "%s", str);
}

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
				assert(false);
				break;
			}

			index += o->size;
		}
	}
}

uint32_t base_type_size(type_id type) {
	if (type == float_id) {
		return 4;
	}
	if (type == float2_id) {
		return 4 * 2;
	}
	if (type == float3_id) {
		return 4 * 3;
	}
	if (type == float4_id) {
		return 4 * 4;
	}
	if (type == float4x4_id) {
		return 4 * 4 * 4;
	}
	if (type == float3x3_id) {
		return 3 * 4 * 4;
	}

	if (type == uint_id) {
		return 4;
	}
	if (type == uint2_id) {
		return 4 * 2;
	}
	if (type == uint3_id) {
		return 4 * 3;
	}
	if (type == uint4_id) {
		return 4 * 4;
	}

	if (type == int_id) {
		return 4;
	}
	if (type == int2_id) {
		return 4 * 2;
	}
	if (type == int3_id) {
		return 4 * 3;
	}
	if (type == int4_id) {
		return 4 * 4;
	}

	debug_context context = {0};
	error(context, "Unknown type %s for structure", get_name(get_type(type)->name));
	return 1;
}

uint32_t struct_size(type_id id) {
	uint32_t size = 0;
	type *t = get_type(id);
	for (size_t member_index = 0; member_index < t->members.size; ++member_index) {
		size += base_type_size(t->members.m[member_index].type.type);
	}
	return size;
}
