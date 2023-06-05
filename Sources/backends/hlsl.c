#include "hlsl.h"

#include "../compiler.h"

#include <inttypes.h>
#include <stdio.h>

void hlsl_export(uint8_t *data, size_t size) {
	FILE *output = fopen("test.hlsl", "wb");

	size_t index = 0;
	while (index < size) {
		opcode *o = (opcode *)&data[index];
		switch (o->type) {
		case OPCODE_VAR:
			fprintf(output, "float %s;\n", get_name(o->op_var.name));
			break;
		case OPCODE_NOT:
			fprintf(output, "_%" PRIu64 " = !_%" PRIu64 ";\n", o->op_not.to.index, o->op_not.from.index);
			break;
		case OPCODE_STORE_VARIABLE:
			fprintf(output, "_%" PRIu64 " = _%" PRIu64 ";\n", o->op_store_var.to.index, o->op_store_var.from.index);
			break;
		case OPCODE_STORE_MEMBER:
			fprintf(output, "something.something = _%" PRIu64 ";\n", o->op_store_member.from.index);
			break;
		case OPCODE_LOAD_CONSTANT:
			fprintf(output, "_%" PRIu64 " = %f;\n", o->op_load_constant.to.index, o->op_load_constant.number);
			break;
		case OPCODE_LOAD_MEMBER:
			fprintf(output, "_%" PRIu64 " = something.something;\n", o->op_load_member.to.index);
			break;
		case OPCODE_RETURN:
			fprintf(output, "return;\n");
			break;
		}

		index += o->size;
	}

	fclose(output);
}
