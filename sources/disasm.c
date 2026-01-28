#include "disasm.h"

#include "compiler.h"
#include "errors.h"
#include "functions.h"
#include "log.h"
#include "parser.h"
#include "sets.h"
#include "shader_stage.h"
#include "types.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*static char *type_string(type_id type) {
    if (type == float_id) {
        return "float";
    }
    if (type == float2_id) {
        return "float2";
    }
    if (type == float3_id) {
        return "float3";
    }
    if (type == float4_id) {
        return "float4";
    }
    if (type == float4x4_id) {
        return "float4x4";
    }
    if (type == ray_type_id) {
        return "RayDesc";
    }
    if (type == bvh_type_id) {
        return "RaytracingAccelerationStructure";
    }
    if (type == tex2d_type_id) {
        return "Texture2D<float4>";
    }
    return get_name(get_type(type)->name);
}*/

static void write_functions(void) {
	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);

		if (f->block == NULL) {
			continue;
		}

		kong_log(LOG_LEVEL_INFO, "Function: %s", get_name(f->name));

		uint8_t *data = f->code.o;
		size_t   size = f->code.size;

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];
			switch (o->type) {
			case OPCODE_RETURN:
				kong_log(LOG_LEVEL_INFO, "RETURN $%zu", o->op_return.var.index);
				break;
			case OPCODE_LOAD_ACCESS_LIST: {
				char accesses[1024];
				int  offset = 0;

				for (int i = 0; i < o->op_load_access_list.access_list_size; ++i) {
					switch (o->op_load_access_list.access_list[i].kind) {
					case ACCESS_ELEMENT:
						offset += sprintf(&accesses[offset], "$%" PRIu64, o->op_load_access_list.access_list[i].access_element.index.index);
						break;
					case ACCESS_MEMBER:
						offset += sprintf(&accesses[offset], "%s", get_name(o->op_load_access_list.access_list[i].access_member.name));
						break;
					case ACCESS_SWIZZLE: {
						swizzle swizzle          = o->op_load_access_list.access_list[i].access_swizzle.swizzle;
						char    swizzle_chars[4] = "xyzw";

						for (uint32_t swizzle_index = 0; swizzle_index < swizzle.size; ++swizzle_index) {
							offset += sprintf(&accesses[offset], "%c", swizzle_chars[swizzle.indices[swizzle_index]]);
						}
						break;
					}
					}

					if (i < o->op_load_access_list.access_list_size - 1) {
						offset += sprintf(&accesses[offset], ", ");
					}
				}

				kong_log(LOG_LEVEL_INFO, "$%zu = LOAD_ACCESS_LIST $%zu[%s]", o->op_load_access_list.to.index, o->op_load_access_list.from.index, accesses);
				break;
			}
			case OPCODE_CALL: {
				char parameters[256];
				int  offset = 0;

				parameters[0] = 0;

				for (int i = 0; i < o->op_call.parameters_size; ++i) {
					offset += sprintf(&parameters[offset], "$%" PRIu64, o->op_call.parameters[i].index);

					if (i < o->op_call.parameters_size - 1) {
						offset += sprintf(&parameters[offset], ", ");
					}
				}

				kong_log(LOG_LEVEL_INFO, "$%zu = CALL %s(%s)", o->op_call.var.index, get_name(o->op_call.func), parameters);
				break;
			}
			case OPCODE_VAR:
				break;
			case OPCODE_NOT:
				kong_log(LOG_LEVEL_INFO, "$%zu = NOT $%zu", o->op_negate.to.index, o->op_negate.from.index);
				break;
			case OPCODE_STORE_VARIABLE:
				kong_log(LOG_LEVEL_INFO, "$%zu = STORE_VARIABLE $%zu", o->op_store_var.to.index, o->op_store_var.from.index);
				break;
			case OPCODE_SUB_AND_STORE_VARIABLE:
				break;
			case OPCODE_ADD_AND_STORE_VARIABLE:
				break;
			case OPCODE_DIVIDE_AND_STORE_VARIABLE:
				break;
			case OPCODE_MULTIPLY_AND_STORE_VARIABLE:
				break;
			case OPCODE_STORE_ACCESS_LIST: {
				char accesses[1024];
				int  offset = 0;

				for (int i = 0; i < o->op_store_access_list.access_list_size; ++i) {
					switch (o->op_store_access_list.access_list[i].kind) {
					case ACCESS_ELEMENT:
						offset += sprintf(&accesses[offset], "$%" PRIu64, o->op_store_access_list.access_list[i].access_element.index.index);
						break;
					case ACCESS_MEMBER:
						offset += sprintf(&accesses[offset], "%s", get_name(o->op_store_access_list.access_list[i].access_member.name));
						break;
					case ACCESS_SWIZZLE: {
						swizzle swizzle          = o->op_store_access_list.access_list[i].access_swizzle.swizzle;
						char    swizzle_chars[4] = "xyzw";

						for (uint32_t swizzle_index = 0; swizzle_index < swizzle.size; ++swizzle_index) {
							offset += sprintf(&accesses[offset], "%c", swizzle_chars[swizzle.indices[swizzle_index]]);
						}
						break;
					}
					}

					if (i < o->op_store_access_list.access_list_size - 1) {
						offset += sprintf(&accesses[offset], ", ");
					}
				}

				kong_log(LOG_LEVEL_INFO, "$%zu[%s] = STORE_ACCESS_LIST $%zu", o->op_store_access_list.to.index, accesses, o->op_store_access_list.from.index);
				break;
			}
			case OPCODE_SUB_AND_STORE_ACCESS_LIST:
				break;
			case OPCODE_ADD_AND_STORE_ACCESS_LIST:
				break;
			case OPCODE_DIVIDE_AND_STORE_ACCESS_LIST:
				break;
			case OPCODE_MULTIPLY_AND_STORE_ACCESS_LIST:
				break;
			case OPCODE_LOAD_FLOAT_CONSTANT:
				kong_log(LOG_LEVEL_INFO, "$%zu = LOAD_FLOAT_CONSTANT %f", o->op_load_float_constant.to.index, o->op_load_float_constant.number);
				break;
			case OPCODE_LOAD_INT_CONSTANT:
				kong_log(LOG_LEVEL_INFO, "$%zu = LOAD_INT_CONSTANT %i", o->op_load_int_constant.to.index, o->op_load_int_constant.number);
				break;
			case OPCODE_LOAD_BOOL_CONSTANT:
				kong_log(LOG_LEVEL_INFO, "$%zu = LOAD_BOOL_CONSTANT %i", o->op_load_bool_constant.to.index, o->op_load_bool_constant.boolean ? 1 : 0);
				break;
			case OPCODE_ADD:
				kong_log(LOG_LEVEL_INFO, "$%zu = ADD $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_SUB:
				kong_log(LOG_LEVEL_INFO, "$%zu = SUB $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_MULTIPLY:
				kong_log(LOG_LEVEL_INFO, "$%zu = MULTIPLY $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_DIVIDE:
				kong_log(LOG_LEVEL_INFO, "$%zu = DIVIDE $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_MOD:
				kong_log(LOG_LEVEL_INFO, "$%zu = MOD $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_EQUALS:
				kong_log(LOG_LEVEL_INFO, "$%zu = EQUALS $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_NOT_EQUALS:
				kong_log(LOG_LEVEL_INFO, "$%zu = NOT_EQUALS $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_GREATER:
				kong_log(LOG_LEVEL_INFO, "$%zu = GREATER $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_GREATER_EQUAL:
				kong_log(LOG_LEVEL_INFO, "$%zu = GREATER_OR_EQUAL $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_LESS:
				kong_log(LOG_LEVEL_INFO, "$%zu = LESS $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_LESS_EQUAL:
				kong_log(LOG_LEVEL_INFO, "$%zu = LESS_OR_EQUAL $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_AND:
				kong_log(LOG_LEVEL_INFO, "$%zu = AND $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_OR:
				kong_log(LOG_LEVEL_INFO, "$%zu = OR $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_BITWISE_XOR:
				kong_log(LOG_LEVEL_INFO, "$%zu = BITWISE_XOR $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_BITWISE_AND:
				kong_log(LOG_LEVEL_INFO, "$%zu = BITWISE_AND $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_BITWISE_OR:
				kong_log(LOG_LEVEL_INFO, "$%zu = BITWISE_OR $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_LEFT_SHIFT:
				kong_log(LOG_LEVEL_INFO, "$%zu = LEFT_SHIFT $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_RIGHT_SHIFT:
				kong_log(LOG_LEVEL_INFO, "$%zu = RIGHT_SHIFT $%zu, $%zu", o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
				break;
			case OPCODE_IF:
				kong_log(LOG_LEVEL_INFO, "IF $%zu -> BLOCK [start=$%zu, end=$%zu]", o->op_if.condition.index, o->op_if.start_id, o->op_if.end_id);
				break;
			case OPCODE_WHILE_START:
				kong_log(LOG_LEVEL_INFO, "WHILE_START [ID: $%zu] -> BLOCK [start=$%zu, continue=$%zu, end=$%zu]", o->op_while_start.start_id, o->op_while_start.start_id, o->op_while_start.continue_id, o->op_while_start.end_id);
				break;
			case OPCODE_WHILE_CONDITION:
				kong_log(LOG_LEVEL_INFO, "WHILE_COND $%zu", o->op_while.condition.index, o->op_while.end_id);
				break;
			case OPCODE_WHILE_END:
				kong_log(LOG_LEVEL_INFO, "WHILE_END [ID: $%zu] -> BLOCK [start=$%zu, continue=$%zu, end=$%zu]", o->op_while_end.end_id, o->op_while_end.start_id, o->op_while_end.continue_id, o->op_while_end.end_id);
				break;
			case OPCODE_BLOCK_START:
				kong_log(LOG_LEVEL_INFO, "BLOCK_START [ID: $%zu]", o->op_block.id);
				break;
			case OPCODE_BLOCK_END:
				kong_log(LOG_LEVEL_INFO, "BLOCK_END [ID: $%zu]", o->op_block.id);
				break;
			default: {
				debug_context context = {0};
				error(context, "Unknown opcode");
				break;
			}
			}
			index += o->size;
		}

		kong_log(LOG_LEVEL_INFO, "");
	}
}

void disassemble(void) {
	write_functions();
}
