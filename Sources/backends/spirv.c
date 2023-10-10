#include "spirv.h"

#include "../errors.h"
#include "../functions.h"
#include "../shader_stage.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct instructions_buffer {
	uint32_t *instructions;
	size_t offset;
} instructions_buffer;

static void write_bytecode(char *directory, const char *filename, const char *name, instructions_buffer *instructions) {
	uint8_t *output = (uint8_t *)instructions->instructions;
	size_t output_size = instructions->offset * 4;

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

typedef enum spirv_opcode {
	OPCODE_EXT_INST_IMPORT = 11,
	OPCODE_MEMORY_MODEL = 14,
	OPCODE_ENTRY_POINT = 15,
	OPCODE_CAPABILITY = 17,
	OPCODE_TYPE_VOID = 19,
	OPCODE_TYPE_INT = 21,
	OPCODE_TYPE_FLOAT = 22,
	OPCODE_TYPE_VECTOR = 23,
	OPCODE_TYPE_FUNCTION = 33,
	OPCODE_CONSTANT = 43
} spirv_opcode;

typedef enum addressing_model { ADDRESSING_MODEL_LOGICAL = 0 } addressing_model;

typedef enum memory_model { MEMORY_MODEL_SIMPLE = 0, MEMORY_MODEL_GLSL450 = 1 } memory_model;

typedef enum capability { CAPABILITY_SHADER = 1 } capability;

typedef enum execution_model { EXECUTION_MODEL_VERTEX = 0, EXECUTION_MODEL_FRAGMENT = 4 } execution_model;

static uint32_t operands_buffer[4096];

static void write_instruction(instructions_buffer *instructions, uint16_t word_count, spirv_opcode o, uint32_t *operands) {
	instructions->instructions[instructions->offset++] = (word_count << 16) | (uint16_t)o;
	for (uint16_t i = 0; i < word_count - 1; ++i) {
		instructions->instructions[instructions->offset++] = operands[i];
	}
}

static void write_capability(instructions_buffer *instructions, capability c) {
	uint32_t operand = (uint32_t)c;
	write_instruction(instructions, 2, OPCODE_CAPABILITY, &operand);
}

static uint32_t next_index = 1;

static uint32_t allocate_index(void) {
	uint32_t result = next_index;
	++next_index;
	return result;
}

static uint16_t write_string(uint32_t *operands, const char *string) {
	uint16_t length = (uint16_t)strlen(string);
	memcpy(&operands[0], string, length + 1);
	return (length + 1) / 4 + 1;
}

static uint32_t write_op_ext_inst_import(instructions_buffer *instructions, const char *name) {
	uint32_t result = allocate_index();

	operands_buffer[0] = result;

	uint32_t name_length = write_string(&operands_buffer[1], name);

	write_instruction(instructions, 2 + name_length, OPCODE_EXT_INST_IMPORT, operands_buffer);

	return result;
}

static void write_op_memory_model(instructions_buffer *instructions, uint32_t addressing_model, uint32_t memory_model) {
	uint32_t args[2] = {addressing_model, memory_model};
	write_instruction(instructions, 3, OPCODE_MEMORY_MODEL, args);
}

static void write_op_entry_point(instructions_buffer *instructions, execution_model em, uint32_t entry_point, const char *name, uint32_t *interfaces,
                                 uint16_t interfaces_size) {
	operands_buffer[0] = (uint32_t)em;
	operands_buffer[1] = entry_point;

	uint32_t name_length = write_string(&operands_buffer[2], name);

	for (uint16_t i = 0; i < interfaces_size; ++i) {
		operands_buffer[2 + name_length + i] = interfaces[i];
	}

	write_instruction(instructions, 3 + name_length + interfaces_size, OPCODE_ENTRY_POINT, operands_buffer);
}

static void write_capabilities(instructions_buffer *instructions) {
	write_capability(instructions, CAPABILITY_SHADER);
}

static uint32_t write_type_void(instructions_buffer *instructions) {
	uint32_t void_type = allocate_index();
	write_instruction(instructions, 2, OPCODE_TYPE_VOID, &void_type);
	return void_type;
}

#define WORD_COUNT(operands) (1 + sizeof(operands) / 4)

static uint32_t write_type_function(instructions_buffer *instructions, uint32_t return_type, uint32_t *parameter_types, uint16_t parameter_types_size) {
	uint32_t function_type = allocate_index();

	operands_buffer[0] = function_type;
	operands_buffer[1] = return_type;
	for (uint16_t i = 0; i < parameter_types_size; ++i) {
		operands_buffer[i + 2] = parameter_types[0];
	}
	write_instruction(instructions, 3 + parameter_types_size, OPCODE_TYPE_FUNCTION, operands_buffer);
	return function_type;
}

static uint32_t write_type_float(instructions_buffer *instructions, uint32_t width) {
	uint32_t float_type = allocate_index();

	uint32_t operands[2];
	operands[0] = float_type;
	operands[1] = width;
	write_instruction(instructions, WORD_COUNT(operands), OPCODE_TYPE_FLOAT, operands);
	return float_type;
}

static uint32_t write_type_vector(instructions_buffer *instructions, uint32_t component_type, uint32_t component_count) {
	uint32_t vector_type = allocate_index();

	uint32_t operands[3];
	operands[0] = vector_type;
	operands[1] = component_type;
	operands[2] = component_count;
	write_instruction(instructions, WORD_COUNT(operands), OPCODE_TYPE_VECTOR, operands);
	return vector_type;
}

static uint32_t write_type_int(instructions_buffer *instructions, uint32_t width, bool signedness) {
	uint32_t int_type = allocate_index();

	uint32_t operands[3];
	operands[0] = int_type;
	operands[1] = width;
	operands[2] = signedness ? 1 : 0;
	write_instruction(instructions, WORD_COUNT(operands), OPCODE_TYPE_INT, operands);
	return int_type;
}

static uint32_t spirv_float_type;

static void write_types(instructions_buffer *instructions) {
	uint32_t void_type = write_type_void(instructions);

	uint32_t void_function_type = write_type_function(instructions, void_type, NULL, 0);

	spirv_float_type = write_type_float(instructions, 32);

	uint32_t float4_type = write_type_vector(instructions, spirv_float_type, 4);

	uint32_t uint_type = write_type_int(instructions, 32, false);

	uint32_t int_type = write_type_int(instructions, 32, true);
}

static uint32_t write_constant(instructions_buffer *instructions, uint32_t type, uint32_t value) {
	uint32_t value_id = allocate_index();

	uint32_t operands[3];
	operands[0] = type;
	operands[1] = value_id;
	operands[2] = value;
	write_instruction(instructions, WORD_COUNT(operands), OPCODE_CONSTANT, operands);
	return value_id;
}

static uint32_t write_constant_float(instructions_buffer *instructions, float value) {
	uint32_t uint32_value = *(uint32_t *)&value;
	return write_constant(instructions, spirv_float_type, uint32_value);
}

void write_vertex_output_decorations(instructions_buffer *instructions) {}

static void spirv_export_vertex(char *directory, function *main) {
	instructions_buffer instructions = {0};
	instructions.instructions = (uint32_t *)calloc(1024 * 1024, 1);

	type_id vertex_input = main->parameter_type.type;
	type_id vertex_output = main->return_type.type;

	debug_context context = {0};
	check(vertex_input != NO_TYPE, context, "vertex input missing");
	check(vertex_output != NO_TYPE, context, "vertex output missing");

	write_capabilities(&instructions);
	write_op_ext_inst_import(&instructions, "GLSL.std.450");
	write_op_memory_model(&instructions, ADDRESSING_MODEL_LOGICAL, MEMORY_MODEL_GLSL450);
	uint32_t entry_point = 2;
	uint32_t interfaces[] = {3, 4};
	write_op_entry_point(&instructions, EXECUTION_MODEL_VERTEX, entry_point, "main", interfaces, sizeof(interfaces) / 4);

	write_vertex_output_decorations(&instructions);

	write_types(&instructions);

	char *name = get_name(main->name);

	char filename[512];
	sprintf(filename, "kong_%s", name);

	char var_name[256];
	sprintf(var_name, "%s_code", name);

	write_bytecode(directory, filename, var_name, &instructions);
}

static void spirv_export_fragment(char *directory, function *main) {
	instructions_buffer instructions = {0};
	instructions.instructions = (uint32_t *)calloc(1024 * 1024, 1);

	type_id pixel_input = main->parameter_type.type;

	debug_context context = {0};
	check(pixel_input != NO_TYPE, context, "fragment input missing");

	write_capabilities(&instructions);

	char *name = get_name(main->name);

	char filename[512];
	sprintf(filename, "kong_%s", name);

	char var_name[256];
	sprintf(var_name, "%s_code", name);

	write_bytecode(directory, filename, var_name, &instructions);
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
