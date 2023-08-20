#include "hlsl.h"

#include "../compiler.h"
#include "../functions.h"
#include "../parser.h"
#include "../types.h"
#include "d3d11.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static char *type_string(type_id type) {
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
	return get_name(get_type(type)->name);
}

static void write_bytecode(const char *filename, const char *name, uint8_t *output, size_t output_size) {
	char full_filename[256];

	{
		sprintf(full_filename, "%s.h", filename);
		FILE *file = fopen(full_filename, "wb");
		fprintf(file, "#include <stdint.h>\n\n");
		fprintf(file, "extern uint8_t *%s;\n", name);
		fprintf(file, "extern size_t %s_size;\n", name);
		fclose(file);
	}

	{
		sprintf(full_filename, "%s.c", filename);

		FILE *file = fopen(full_filename, "wb");
		fprintf(file, "#include \"%s.h\"\n\n", filename);

		fprintf(file, "uint8_t *%s = \"", name);
		for (size_t i = 0; i < output_size; ++i) {
			// based on the encoding described in https://github.com/adobe/bin2c
			if (output[i] == '!' || output[i] == '#' || (output[i] >= '%' && output[i] <= '>') || (output[i] >= 'A' && output[i] <= '[') ||
			    (output[i] >= ']' && output[i] <= '~')) {
				fprintf(file, "%c", output[i]);
			}
			else if (output[i] == '\a') {
				fprintf(file, "\\a");
			}
			else if (output[i] == '\b') {
				fprintf(file, "\\b");
			}
			else if (output[i] == '\t') {
				fprintf(file, "\\t");
			}
			else if (output[i] == '\v') {
				fprintf(file, "\\v");
			}
			else if (output[i] == '\f') {
				fprintf(file, "\\f");
			}
			else if (output[i] == '\r') {
				fprintf(file, "\\r");
			}
			else if (output[i] == '\"') {
				fprintf(file, "\\\"");
			}
			else if (output[i] == '\\') {
				fprintf(file, "\\\\");
			}
			else {
				fprintf(file, "\\%03o", output[i]);
			}
		}
		fprintf(file, "\";\n");

		fprintf(file, "size_t %s_size = %" PRIu64 ";\n", name, output_size);

		fclose(file);
	}
}

static hlsl_export_vertex(void) {
	char *hlsl = (char *)calloc(1024 * 1024, 1);
	size_t offset = 0;

	type_id vertex_input = NO_TYPE;
	type_id vertex_output = NO_TYPE;

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);
		if (f->attribute == add_name("vertex")) {
			vertex_output = f->return_type.type;
			vertex_input = f->parameter_type.type;
			break;
		}
	}

	assert(vertex_input != NO_TYPE);
	assert(vertex_output != NO_TYPE);

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);

		if (!t->built_in && t->attribute != add_name("pipe")) {
			offset += sprintf(&hlsl[offset], "struct %s {\n", get_name(t->name));

			if (i == vertex_input) {
				for (size_t j = 0; j < t->members.size; ++j) {
					offset +=
					    sprintf(&hlsl[offset], "\t%s %s : TEXCOORD%" PRIu64 ";\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name), j);
				}
			}
			else if (i == vertex_output) {
				for (size_t j = 0; j < t->members.size; ++j) {
					offset += sprintf(&hlsl[offset], "\t%s %s : SV_POSITION;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
				}
			}
			else {
				for (size_t j = 0; j < t->members.size; ++j) {
					offset += sprintf(&hlsl[offset], "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
				}
			}
			offset += sprintf(&hlsl[offset], "};\n\n");
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
		if (f->attribute == add_name("vertex")) {
			offset +=
			    sprintf(&hlsl[offset], "%s main(%s _%" PRIu64 ") {\n", type_string(f->return_type.type), type_string(f->parameter_type.type), parameter_id);
		}
		else {
			offset += sprintf(&hlsl[offset], "%s %s(%s _%" PRIu64 ") {\n", type_string(f->return_type.type), get_name(f->name),
			                  type_string(f->parameter_type.type), parameter_id);
		}

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];
			switch (o->type) {
			case OPCODE_VAR:
				offset += sprintf(&hlsl[offset], "\t%s _%" PRIu64 ";\n", type_string(o->op_var.var.type), o->op_var.var.index);
				break;
			case OPCODE_NOT:
				offset += sprintf(&hlsl[offset], "\t_%" PRIu64 " = !_%" PRIu64 ";\n", o->op_not.to.index, o->op_not.from.index);
				break;
			case OPCODE_STORE_VARIABLE:
				offset += sprintf(&hlsl[offset], "\t_%" PRIu64 " = _%" PRIu64 ";\n", o->op_store_var.to.index, o->op_store_var.from.index);
				break;
			case OPCODE_STORE_MEMBER:
				offset += sprintf(&hlsl[offset], "\t_%" PRIu64, o->op_store_member.to.index);
				type *s = get_type(o->op_store_member.member_parent_type);
				for (size_t i = 0; i < o->op_store_member.member_indices_size; ++i) {
					offset += sprintf(&hlsl[offset], ".%s", get_name(s->members.m[o->op_store_member.member_indices[i]].name));
					s = get_type(s->members.m[o->op_store_member.member_indices[i]].type.type);
				}
				offset += sprintf(&hlsl[offset], " = _%" PRIu64 ";\n", o->op_store_member.from.index);
				break;
			case OPCODE_LOAD_CONSTANT:
				offset += sprintf(&hlsl[offset], "\t%s _%" PRIu64 " = %f;\n", type_string(o->op_load_constant.to.type), o->op_load_constant.to.index,
				                  o->op_load_constant.number);
				break;
			case OPCODE_LOAD_MEMBER: {
				offset += sprintf(&hlsl[offset], "\t%s _%" PRIu64 " = _%" PRIu64, type_string(o->op_load_member.to.type), o->op_load_member.to.index,
				                  o->op_load_member.from.index);
				type *s = get_type(o->op_load_member.member_parent_type);
				for (size_t i = 0; i < o->op_load_member.member_indices_size; ++i) {
					offset += sprintf(&hlsl[offset], ".%s", get_name(s->members.m[o->op_load_member.member_indices[i]].name));
					s = get_type(s->members.m[o->op_load_member.member_indices[i]].type.type);
				}
				offset += sprintf(&hlsl[offset], ";\n");
				break;
			}
			case OPCODE_RETURN: {
				if (o->size > offsetof(opcode, op_return)) {
					offset += sprintf(&hlsl[offset], "\treturn _%" PRIu64 ";\n", o->op_return.var.index);
				}
				else {
					offset += sprintf(&hlsl[offset], "\treturn;\n");
				}
				break;
			}
			}

			index += o->size;
		}

		offset += sprintf(&hlsl[offset], "}\n\n");
	}

	char *output;
	size_t output_size;
	compile_hlsl_to_d3d11(hlsl, &output, &output_size, EShLangVertex, false);

	write_bytecode("vert", "kong_vert_code", output, output_size);
}

static void hlsl_export_pixel(void) {
	char *hlsl = (char *)calloc(1024 * 1024, 1);
	size_t offset = 0;

	type_id pixel_input = NO_TYPE;

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);
		if (f->attribute == add_name("fragment")) {
			pixel_input = f->parameter_type.type;
			break;
		}
	}

	assert(pixel_input != NO_TYPE);

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);

		if (!t->built_in && t->attribute != add_name("pipe")) {
			offset += sprintf(&hlsl[offset], "struct %s {\n", get_name(t->name));

			if (i == pixel_input) {
				for (size_t j = 0; j < t->members.size; ++j) {
					offset += sprintf(&hlsl[offset], "\t%s %s : SV_POSITION;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
				}
			}
			else {
				for (size_t j = 0; j < t->members.size; ++j) {
					offset += sprintf(&hlsl[offset], "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
				}
			}
			offset += sprintf(&hlsl[offset], "};\n\n");
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
		if (f->attribute == add_name("fragment")) {
			offset += sprintf(&hlsl[offset], "%s main(%s _%" PRIu64 ") : SV_Target0 {\n", type_string(f->return_type.type), type_string(f->parameter_type.type),
			                  parameter_id);
		}
		else {
			offset += sprintf(&hlsl[offset], "%s %s(%s _%" PRIu64 ") {\n", type_string(f->return_type.type), get_name(f->name),
			                  type_string(f->parameter_type.type), parameter_id);
		}

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];
			switch (o->type) {
			case OPCODE_VAR:
				offset += sprintf(&hlsl[offset], "\t%s _%" PRIu64 ";\n", type_string(o->op_var.var.type), o->op_var.var.index);
				break;
			case OPCODE_NOT:
				offset += sprintf(&hlsl[offset], "\t_%" PRIu64 " = !_%" PRIu64 ";\n", o->op_not.to.index, o->op_not.from.index);
				break;
			case OPCODE_STORE_VARIABLE:
				offset += sprintf(&hlsl[offset], "\t_%" PRIu64 " = _%" PRIu64 ";\n", o->op_store_var.to.index, o->op_store_var.from.index);
				break;
			case OPCODE_STORE_MEMBER:
				offset += sprintf(&hlsl[offset], "\t_%" PRIu64, o->op_store_member.to.index);
				type *s = get_type(o->op_store_member.member_parent_type);
				for (size_t i = 0; i < o->op_store_member.member_indices_size; ++i) {
					offset += sprintf(&hlsl[offset], ".%s", get_name(s->members.m[o->op_store_member.member_indices[i]].name));
					s = get_type(s->members.m[o->op_store_member.member_indices[i]].type.type);
				}
				offset += sprintf(&hlsl[offset], " = _%" PRIu64 ";\n", o->op_store_member.from.index);
				break;
			case OPCODE_LOAD_CONSTANT:
				offset += sprintf(&hlsl[offset], "\t%s _%" PRIu64 " = %f;\n", type_string(o->op_load_constant.to.type), o->op_load_constant.to.index,
				                  o->op_load_constant.number);
				break;
			case OPCODE_LOAD_MEMBER: {
				offset += sprintf(&hlsl[offset], "\t%s _%" PRIu64 " = _%" PRIu64, type_string(o->op_load_member.to.type), o->op_load_member.to.index,
				                  o->op_load_member.from.index);
				type *s = get_type(o->op_load_member.member_parent_type);
				for (size_t i = 0; i < o->op_load_member.member_indices_size; ++i) {
					offset += sprintf(&hlsl[offset], ".%s", get_name(s->members.m[o->op_load_member.member_indices[i]].name));
					s = get_type(s->members.m[o->op_load_member.member_indices[i]].type.type);
				}
				offset += sprintf(&hlsl[offset], ";\n");
				break;
			}
			case OPCODE_RETURN: {
				if (o->size > offsetof(opcode, op_return)) {
					offset += sprintf(&hlsl[offset], "\treturn _%" PRIu64 ";\n", o->op_return.var.index);
				}
				else {
					offset += sprintf(&hlsl[offset], "\treturn;\n");
				}
				break;
			}
			}

			index += o->size;
		}

		offset += sprintf(&hlsl[offset], "}\n\n");
	}

	uint8_t *output;
	size_t output_size;
	compile_hlsl_to_d3d11(hlsl, &output, &output_size, EShLangFragment, false);

	write_bytecode("frag", "kong_frag_code", output, output_size);
}

void hlsl_export(void) {
	hlsl_export_vertex();
	hlsl_export_pixel();
}
