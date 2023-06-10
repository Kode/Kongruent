#include "hlsl.h"

#include "../compiler.h"
#include "../structs.h"

#include <inttypes.h>
#include <stdio.h>

static char *type_string(struct_id type) {
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
	return get_name(get_struct(type)->name);
}

void hlsl_export(uint8_t *data, size_t size) {
	FILE *output = fopen("test.hlsl", "wb");

	size_t index = 0;
	while (index < size) {
		opcode *o = (opcode *)&data[index];
		switch (o->type) {
		case OPCODE_VAR:
			fprintf(output, "%s %s;\n", type_string(o->op_var.type), get_name(o->op_var.name));
			break;
		case OPCODE_NOT:
			fprintf(output, "_%" PRIu64 " = !_%" PRIu64 ";\n", o->op_not.to.index, o->op_not.from.index);
			break;
		case OPCODE_STORE_VARIABLE:
			fprintf(output, "_%" PRIu64 " = _%" PRIu64 ";\n", o->op_store_var.to.index, o->op_store_var.from.index);
			break;
		case OPCODE_STORE_MEMBER:
			fprintf(output, "_%" PRIu64 ".something = _%" PRIu64 ";\n", o->op_store_member.to.index, o->op_store_member.from.index);
			break;
		case OPCODE_LOAD_CONSTANT:
			fprintf(output, "float _%" PRIu64 " = %f;\n", o->op_load_constant.to.index, o->op_load_constant.number);
			break;
		case OPCODE_LOAD_MEMBER: {
			fprintf(output, "%s _%" PRIu64 " = _%" PRIu64, type_string(o->op_load_member.to.type), o->op_load_member.to.index, o->op_load_member.from.index);
			structy *s = get_struct(o->op_load_member.member_parent_type);
			for (size_t i = 0; i < o->op_load_member.member_indices_size; ++i) {
				fprintf(output, ".%s", get_name(s->members.m[o->op_load_member.member_indices[i]].name));
				s = get_struct(s->members.m[o->op_load_member.member_indices[i]].type.type);
			}
			fprintf(output, ";\n");
			break;
		}
		case OPCODE_RETURN:
			fprintf(output, "return;\n");
			break;
		}

		index += o->size;
	}

	fclose(output);
}
