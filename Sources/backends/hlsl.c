#include "hlsl.h"

#include "../compiler.h"
#include "../functions.h"
#include "../parser.h"
#include "../types.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

static char *type_string(type_id type) {
	if (type == f32_id) {
		return "float";
	}
	if (type == vec2_id) {
		return "float2";
	}
	if (type == vec3_id) {
		return "float3";
	}
	if (type == vec4_id) {
		return "float4";
	}
	return get_name(get_type(type)->name);
}

void hlsl_export(void) {
	FILE *output = fopen("test.hlsl", "wb");

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in && t->attribute != add_name("pipe")) {
			fprintf(output, "struct %s {\n", get_name(t->name));
			for (size_t j = 0; j < t->members.size; ++j) {
				fprintf(output, "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
			}
			fprintf(output, "}\n\n");
		}
	}

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);
		uint8_t *data = f->code.o;
		size_t size = f->code.size;

		uint64_t parameter_id = 0;
		for (size_t i = 0; i < f->block->block.vars.size; ++i) {
			if (f->parameter_name == f->block->block.vars.v[i].name) {
				parameter_id = f->block->block.vars.v[i].variable_id;
				break;
			}
		}

		assert(parameter_id != 0);
		fprintf(output, "%s %s(%s _%" PRIu64 ") {\n", type_string(f->return_type.type), get_name(f->name), type_string(f->parameter_type.type), parameter_id);

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];
			switch (o->type) {
			case OPCODE_VAR:
				fprintf(output, "\t%s _%" PRIu64 ";\n", type_string(o->op_var.var.type), o->op_var.var.index);
				break;
			case OPCODE_NOT:
				fprintf(output, "\t_%" PRIu64 " = !_%" PRIu64 ";\n", o->op_not.to.index, o->op_not.from.index);
				break;
			case OPCODE_STORE_VARIABLE:
				fprintf(output, "\t_%" PRIu64 " = _%" PRIu64 ";\n", o->op_store_var.to.index, o->op_store_var.from.index);
				break;
			case OPCODE_STORE_MEMBER:
				fprintf(output, "\t_%" PRIu64, o->op_store_member.to.index);
				type *s = get_type(o->op_store_member.member_parent_type);
				for (size_t i = 0; i < o->op_store_member.member_indices_size; ++i) {
					fprintf(output, ".%s", get_name(s->members.m[o->op_store_member.member_indices[i]].name));
					s = get_type(s->members.m[o->op_store_member.member_indices[i]].type.type);
				}
				fprintf(output, " = _%" PRIu64 ";\n", o->op_store_member.from.index);
				break;
			case OPCODE_LOAD_CONSTANT:
				fprintf(output, "\t%s _%" PRIu64 " = %f;\n", type_string(o->op_load_constant.to.type), o->op_load_constant.to.index,
				        o->op_load_constant.number);
				break;
			case OPCODE_LOAD_MEMBER: {
				fprintf(output, "\t%s _%" PRIu64 " = _%" PRIu64, type_string(o->op_load_member.to.type), o->op_load_member.to.index,
				        o->op_load_member.from.index);
				type *s = get_type(o->op_load_member.member_parent_type);
				for (size_t i = 0; i < o->op_load_member.member_indices_size; ++i) {
					fprintf(output, ".%s", get_name(s->members.m[o->op_load_member.member_indices[i]].name));
					s = get_type(s->members.m[o->op_load_member.member_indices[i]].type.type);
				}
				fprintf(output, ";\n");
				break;
			}
			case OPCODE_RETURN: {
				if (o->size > offsetof(opcode, op_return)) {
					fprintf(output, "\treturn _%" PRIu64 ";\n", o->op_return.var.index);
				}
				else {
					fprintf(output, "\treturn;\n");
				}
				break;
			}
			}

			index += o->size;
		}

		fprintf(output, "}\n\n");
	}

	fclose(output);
}
