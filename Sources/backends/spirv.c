#include "spirv.h"

#include "../errors.h"
#include "../functions.h"
#include "../shader_stage.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void write_bytecode(char *directory, const char *filename, const char *name, uint32_t *spirv, size_t spirv_size) {
	uint8_t *output = (uint8_t *)spirv;
	size_t output_size = spirv_size * 4;

	char full_filename[512];

	{
		sprintf(full_filename, "%s/%s.h", directory, filename);
		FILE *file = fopen(full_filename, "wb");
		fprintf(file, "#include <stddef.h>\n");
		fprintf(file, "#include <stdint.h>\n\n");
		fprintf(file, "extern uint8_t *%s;\n", name);
		fprintf(file, "extern size_t %s_size;\n", name);
		fclose(file);
	}

	{
		sprintf(full_filename, "%s/%s.c", directory, filename);

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

		fprintf(file, "size_t %s_size = %zu;\n\n", name, output_size);

		fclose(file);
	}
}

typedef enum spirv_opcode { OPCODE_EXT_INST_IMPORT = 11, OPCODE_MEMORY_MODEL = 14, OPCODE_ENTRY_POINT = 15, OPCODE_CAPABILITY = 17 } spirv_opcode;

typedef enum addressing_model { ADDRESSING_MODEL_LOGICAL = 0 } addressing_model;

typedef enum memory_model { MEMORY_MODEL_SIMPLE = 0, MEMORY_MODEL_GLSL450 = 1 } memory_model;

typedef enum capability { CAPABILITY_SHADER = 1 } capability;

typedef enum execution_model { EXECUTION_MODEL_VERTEX = 0, EXECUTION_MODEL_FRAGMENT = 4 } execution_model;

static void write_instruction(uint32_t *instructions, size_t *offset, uint16_t word_count, spirv_opcode o, uint32_t *operands) {
	instructions[(*offset)++] = (word_count << 16) | (uint16_t)o;
	for (uint16_t i = 0; i < word_count - 1; ++i) {
		instructions[(*offset)++] = operands[i];
	}
}

static void write_capability(uint32_t *instructions, size_t *offset, capability c) {
	uint32_t operand = (uint32_t)c;
	write_instruction(instructions, offset, 2, OPCODE_CAPABILITY, &operand);
}

static uint32_t next_index = 1;

static uint16_t write_string(uint32_t *operands, const char *string) {
	uint16_t length = (uint16_t)strlen(string);
	memcpy(&operands[0], string, length + 1);
	return (length + 1) / 4 + 1;
}

static uint32_t write_op_ext_inst_import(uint32_t *instructions, size_t *offset, const char *name) {
	uint32_t result = next_index;
	++next_index;

	uint32_t operands[256] = {0};
	operands[0] = result;

	uint32_t name_length = write_string(&operands[1], name);

	write_instruction(instructions, offset, 2 + name_length, OPCODE_EXT_INST_IMPORT, operands);

	return result;
}

static void write_op_memory_model(uint32_t *instructions, size_t *offset, uint32_t addressing_model, uint32_t memory_model) {
	uint32_t args[2] = {addressing_model, memory_model};
	write_instruction(instructions, offset, 3, OPCODE_MEMORY_MODEL, args);
}

static void write_op_entry_point(uint32_t *instructions, size_t *offset, execution_model em, uint32_t entry_point, const char *name, uint32_t *interfaces,
                                 uint16_t interfaces_size) {
	uint32_t operands[256] = {0};
	operands[0] = (uint32_t)em;
	operands[1] = entry_point;

	uint32_t name_length = write_string(&operands[2], name);

	for (uint16_t i = 0; i < interfaces_size; ++i) {
		operands[2 + name_length + i] = interfaces[i];
	}

	write_instruction(instructions, offset, 3 + name_length + interfaces_size, OPCODE_ENTRY_POINT, operands);
}

static void write_capabilities(uint32_t *instructions, size_t *offset) {
	write_capability(instructions, offset, CAPABILITY_SHADER);
}

static void spirv_export_vertex(char *directory, function *main) {
	uint32_t *instructions = (uint32_t *)calloc(1024 * 1024, 1);
	size_t offset = 0;

	type_id vertex_input = main->parameter_type.type;
	type_id vertex_output = main->return_type.type;

	debug_context context = {0};
	check(vertex_input != NO_TYPE, context, "vertex input missing");
	check(vertex_output != NO_TYPE, context, "vertex output missing");

	write_capabilities(instructions, &offset);
	write_op_ext_inst_import(instructions, &offset, "GLSL.std.450");
	write_op_memory_model(instructions, &offset, ADDRESSING_MODEL_LOGICAL, MEMORY_MODEL_GLSL450);
	uint32_t entry_point = 2;
	uint32_t interfaces[] = {3, 4};
	write_op_entry_point(instructions, &offset, EXECUTION_MODEL_VERTEX, entry_point, "main", interfaces, sizeof(interfaces) / 4);

	char *name = get_name(main->name);

	char filename[512];
	sprintf(filename, "kong_%s", name);

	char var_name[256];
	sprintf(var_name, "%s_code", name);

	write_bytecode(directory, filename, var_name, instructions, offset);
}

static void spirv_export_fragment(char *directory, function *main) {
	uint32_t *instructions = (uint32_t *)calloc(1024 * 1024, 1);
	size_t offset = 0;

	type_id pixel_input = main->parameter_type.type;

	debug_context context = {0};
	check(pixel_input != NO_TYPE, context, "fragment input missing");

	write_capabilities(instructions, &offset);

	char *name = get_name(main->name);

	char filename[512];
	sprintf(filename, "kong_%s", name);

	char var_name[256];
	sprintf(var_name, "%s_code", name);

	write_bytecode(directory, filename, var_name, instructions, offset);
}

void spirv_export(char *directory) {
	function *vertex_shaders[256];
	size_t vertex_shaders_size = 0;

	function *fragment_shaders[256];
	size_t fragment_shaders_size = 0;

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in && t->attribute == add_name("pipe")) {
			name_id vertex_shader_name = NO_NAME;
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
			check(vertex_shader_name != NO_NAME, context, "vertex shader missing");
			check(fragment_shader_name != NO_NAME, context, "fragment shader missing");

			for (function_id i = 0; get_function(i) != NULL; ++i) {
				function *f = get_function(i);
				if (f->name == vertex_shader_name) {
					vertex_shaders[vertex_shaders_size] = f;
					vertex_shaders_size += 1;
				}
				else if (f->name == fragment_shader_name) {
					fragment_shaders[fragment_shaders_size] = f;
					fragment_shaders_size += 1;
				}
			}
		}
	}

	for (size_t i = 0; i < vertex_shaders_size; ++i) {
		spirv_export_vertex(directory, vertex_shaders[i]);
	}

	for (size_t i = 0; i < fragment_shaders_size; ++i) {
		spirv_export_fragment(directory, fragment_shaders[i]);
	}
}
