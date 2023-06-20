#include "hlsl.h"

#include "../compiler.h"
#include "../functions.h"
#include "../types.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

static char *type_string(type_id type) {
	if (type == f32_id) {
		return "f32";
	}
	if (type == vec2_id) {
		return "vec2";
	}
	if (type == vec3_id) {
		return "vec3";
	}
	if (type == vec4_id) {
		return "vec4";
	}
	return get_name(get_type(type)->name);
}

void hlsl_export() {
	FILE *output = fopen("test.hlsl", "wb");

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in) {
			fprintf(output, "struct %s {\n", get_name(t->name));
			for (size_t j = 0; j < t->members.size; ++j) {
				fprintf(output, "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
			}
			fprintf(output, "}\n\n");
		}
	}

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		uint8_t *data = get_function(i)->code.o;
		size_t size = get_function(i)->code.size;

		fprintf(output, "void %s() {\n", get_name(get_function(i)->name));

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];
			switch (o->type) {
			case OPCODE_VAR:
				fprintf(output, "\t%s %s;\n", type_string(o->op_var.type), get_name(o->op_var.name));
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
				fprintf(output, "\tfloat _%" PRIu64 " = %f;\n", o->op_load_constant.to.index, o->op_load_constant.number);
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
