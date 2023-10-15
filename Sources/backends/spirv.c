#include "spirv.h"

#include "../compiler.h"
#include "../errors.h"
#include "../functions.h"
#include "../parser.h"
#include "../shader_stage.h"
#include "../types.h"

#include "../libs/stb_ds.h"

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

#ifndef NDEBUG
	{
		sprintf(full_filename, "%s/%s.spirv", directory, filename);
		FILE *file = fopen(full_filename, "wb");
		fwrite(output, 1, output_size, file);
		fclose(file);
	}
#endif
}

typedef enum spirv_opcode {
	SPIRV_OPCODE_EXT_INST_IMPORT = 11,
	SPIRV_OPCODE_MEMORY_MODEL = 14,
	SPIRV_OPCODE_ENTRY_POINT = 15,
	SPIRV_OPCODE_CAPABILITY = 17,
	SPIRV_OPCODE_TYPE_VOID = 19,
	SPIRV_OPCODE_TYPE_INT = 21,
	SPIRV_OPCODE_TYPE_FLOAT = 22,
	SPIRV_OPCODE_TYPE_VECTOR = 23,
	SPIRV_OPCODE_TYPE_STRUCT = 30,
	SPIRV_OPCODE_TYPE_POINTER = 32,
	SPIRV_OPCODE_TYPE_FUNCTION = 33,
	SPIRV_OPCODE_CONSTANT = 43,
	SPIRV_OPCODE_FUNCTION = 54,
	SPIRV_OPCODE_FUNCTION_END = 56,
	SPIRV_OPCODE_LOAD = 61,
	SPIRV_OPCODE_ACCESS_CHAIN = 65,
	SPIRV_OPCODE_DECORATE = 71,
	SPIRV_OPCODE_MEMBER_DECORATE = 72,
	SPIRV_OPCODE_RETURN = 253,
	SPIRV_OPCODE_LABEL = 248
} spirv_opcode;

typedef enum addressing_model { ADDRESSING_MODEL_LOGICAL = 0 } addressing_model;

typedef enum memory_model { MEMORY_MODEL_SIMPLE = 0, MEMORY_MODEL_GLSL450 = 1 } memory_model;

typedef enum capability { CAPABILITY_SHADER = 1 } capability;

typedef enum execution_model { EXECUTION_MODEL_VERTEX = 0, EXECUTION_MODEL_FRAGMENT = 4 } execution_model;

typedef enum decoration { DECORATION_BUILTIN = 11, DECORATION_LOCATION = 30 } decoration;

typedef enum builtin { BUILTIN_POSITION = 0 } builtin;

typedef enum storage_class { STORAGE_CLASS_INPUT = 1, STORAGE_CLASS_OUTPUT = 3 } storage_class;

typedef enum function_control { FUNCTION_CONTROL_NONE } function_control;

static uint32_t operands_buffer[4096];

static void write_simple_instruction(instructions_buffer *instructions, spirv_opcode o) {
	instructions->instructions[instructions->offset++] = (1 << 16) | (uint16_t)o;
}

static void write_instruction(instructions_buffer *instructions, uint16_t word_count, spirv_opcode o, uint32_t *operands) {
	instructions->instructions[instructions->offset++] = (word_count << 16) | (uint16_t)o;
	for (uint16_t i = 0; i < word_count - 1; ++i) {
		instructions->instructions[instructions->offset++] = operands[i];
	}
}

static void write_magic_number(instructions_buffer *instructions) {
	instructions->instructions[instructions->offset++] = 0x07230203;
}

static void write_version_number(instructions_buffer *instructions) {
	instructions->instructions[instructions->offset++] = 0x00010000;
}

static void write_generator_magic_number(instructions_buffer *instructions) {
	instructions->instructions[instructions->offset++] = 0; // TODO: Register a number at https://github.com/KhronosGroup/SPIRV-Headers
}

static void write_bound(instructions_buffer *instructions) {
	instructions->instructions[instructions->offset++] = 256; // TODO: Exclusive upper bound of used IDs
}

static void write_instruction_schema(instructions_buffer *instructions) {
	instructions->instructions[instructions->offset++] = 0; // reserved in SPIR-V for later use, currently always zero
}

static void write_capability(instructions_buffer *instructions, capability c) {
	uint32_t operand = (uint32_t)c;
	write_instruction(instructions, 2, SPIRV_OPCODE_CAPABILITY, &operand);
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

	write_instruction(instructions, 2 + name_length, SPIRV_OPCODE_EXT_INST_IMPORT, operands_buffer);

	return result;
}

static void write_op_memory_model(instructions_buffer *instructions, uint32_t addressing_model, uint32_t memory_model) {
	uint32_t args[2] = {addressing_model, memory_model};
	write_instruction(instructions, 3, SPIRV_OPCODE_MEMORY_MODEL, args);
}

static void write_op_entry_point(instructions_buffer *instructions, execution_model em, uint32_t entry_point, const char *name, uint32_t *interfaces,
                                 uint16_t interfaces_size) {
	operands_buffer[0] = (uint32_t)em;
	operands_buffer[1] = entry_point;

	uint32_t name_length = write_string(&operands_buffer[2], name);

	for (uint16_t i = 0; i < interfaces_size; ++i) {
		operands_buffer[2 + name_length + i] = interfaces[i];
	}

	write_instruction(instructions, 3 + name_length + interfaces_size, SPIRV_OPCODE_ENTRY_POINT, operands_buffer);
}

static void write_capabilities(instructions_buffer *instructions) {
	write_capability(instructions, CAPABILITY_SHADER);
}

static uint32_t write_type_void(instructions_buffer *instructions) {
	uint32_t void_type = allocate_index();
	write_instruction(instructions, 2, SPIRV_OPCODE_TYPE_VOID, &void_type);
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
	write_instruction(instructions, 3 + parameter_types_size, SPIRV_OPCODE_TYPE_FUNCTION, operands_buffer);
	return function_type;
}

static uint32_t write_type_float(instructions_buffer *instructions, uint32_t width) {
	uint32_t float_type = allocate_index();

	uint32_t operands[2];
	operands[0] = float_type;
	operands[1] = width;
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_TYPE_FLOAT, operands);
	return float_type;
}

static uint32_t write_type_vector(instructions_buffer *instructions, uint32_t component_type, uint32_t component_count) {
	uint32_t vector_type = allocate_index();

	uint32_t operands[3];
	operands[0] = vector_type;
	operands[1] = component_type;
	operands[2] = component_count;
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_TYPE_VECTOR, operands);
	return vector_type;
}

static uint32_t write_type_int(instructions_buffer *instructions, uint32_t width, bool signedness) {
	uint32_t int_type = allocate_index();

	uint32_t operands[3];
	operands[0] = int_type;
	operands[1] = width;
	operands[2] = signedness ? 1 : 0;
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_TYPE_INT, operands);
	return int_type;
}

static uint32_t write_type_struct(instructions_buffer *instructions, uint32_t *types, uint16_t types_size) {
	uint32_t struct_type = allocate_index();

	operands_buffer[0] = struct_type;
	for (uint16_t i = 0; i < types_size; ++i) {
		operands_buffer[i + 1] = types[i];
	}
	write_instruction(instructions, 2 + types_size, SPIRV_OPCODE_TYPE_STRUCT, operands_buffer);
	return struct_type;
}

static uint32_t write_type_pointer(instructions_buffer *instructions, storage_class storage, uint32_t type) {
	uint32_t pointer_type = allocate_index();

	uint32_t operands[3];
	operands[0] = pointer_type;
	operands[1] = (uint32_t)storage;
	operands[2] = type;
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_TYPE_POINTER, operands);
	return pointer_type;
}

static uint32_t void_type;
static uint32_t void_function_type;
static uint32_t spirv_float_type;
static uint32_t float_input_pointer_type;

static void write_types(instructions_buffer *instructions, type_id vertex_input) {
	void_type = write_type_void(instructions);

	void_function_type = write_type_function(instructions, void_type, NULL, 0);

	spirv_float_type = write_type_float(instructions, 32);

	uint32_t float2_type = write_type_vector(instructions, spirv_float_type, 2);
	uint32_t float3_type = write_type_vector(instructions, spirv_float_type, 3);
	uint32_t float4_type = write_type_vector(instructions, spirv_float_type, 4);
	uint32_t uint_type = write_type_int(instructions, 32, false);
	uint32_t int_type = write_type_int(instructions, 32, true);

	uint32_t types[] = {float4_type};
	uint32_t input_struct_type = write_type_struct(instructions, types, 1);
	uint32_t input_struct_pointer_type = write_type_pointer(instructions, STORAGE_CLASS_OUTPUT, input_struct_type);
	uint32_t input_float4_pointer_type = write_type_pointer(instructions, STORAGE_CLASS_OUTPUT, float4_type);

	uint32_t input_pointer_types[256];

	type *input = get_type(vertex_input);
	size_t input_pointer_types_size = input->members.size;

	for (size_t i = 0; i < input->members.size; ++i) {
		member m = input->members.m[i];
		if (m.type.type == float2_id) {
			input_pointer_types[i] = write_type_pointer(instructions, STORAGE_CLASS_INPUT, float2_type);
		}
		else if (m.type.type == float3_id) {
			input_pointer_types[i] = write_type_pointer(instructions, STORAGE_CLASS_INPUT, float3_type);
		}
		else if (m.type.type == float4_id) {
			input_pointer_types[i] = write_type_pointer(instructions, STORAGE_CLASS_INPUT, float4_type);
		}
		else {
			debug_context context = {0};
			error(context, "Type unsupported for input in SPIR-V");
		}
	}

	float_input_pointer_type = write_type_pointer(instructions, STORAGE_CLASS_INPUT, spirv_float_type);
}

static uint32_t write_constant(instructions_buffer *instructions, uint32_t type, uint32_t value) {
	uint32_t value_id = allocate_index();

	uint32_t operands[3];
	operands[0] = type;
	operands[1] = value_id;
	operands[2] = value;
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_CONSTANT, operands);
	return value_id;
}

static uint32_t write_constant_float(instructions_buffer *instructions, float value) {
	uint32_t uint32_value = *(uint32_t *)&value;
	return write_constant(instructions, spirv_float_type, uint32_value);
}

void write_vertex_output_decorations(instructions_buffer *instructions, uint32_t output_struct) {
	uint32_t operands[4];
	operands[0] = output_struct;
	operands[1] = 0;
	operands[2] = (uint32_t)DECORATION_BUILTIN;
	operands[3] = (uint32_t)BUILTIN_POSITION;
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_MEMBER_DECORATE, operands);
}

void write_vertex_input_decorations(instructions_buffer *instructions, uint32_t *inputs, uint32_t inputs_size) {
	for (uint32_t i = 0; i < inputs_size; ++i) {
		uint32_t operands[3];
		operands[0] = inputs[i];
		operands[1] = (uint32_t)DECORATION_LOCATION;
		operands[2] = i;
		write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_DECORATE, operands);
	}
}

uint32_t write_op_function(instructions_buffer *instructions, uint32_t result_type, function_control control, uint32_t function_type) {
	uint32_t result = allocate_index();

	uint32_t operands[4];
	operands[0] = result_type;
	operands[1] = result;
	operands[2] = (uint32_t)control;
	operands[3] = function_type;
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_FUNCTION, operands);
	return result;
}

uint32_t write_label(instructions_buffer *instructions) {
	uint32_t result = allocate_index();

	uint32_t operands[1];
	operands[0] = result;
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_LABEL, operands);
	return result;
}

void write_return(instructions_buffer *instructions) {
	write_simple_instruction(instructions, SPIRV_OPCODE_RETURN);
}

void write_function_end(instructions_buffer *instructions) {
	write_simple_instruction(instructions, SPIRV_OPCODE_FUNCTION_END);
}

uint32_t write_op_access_chain(instructions_buffer *instructions, uint32_t result_type, uint32_t base, uint32_t *indices, uint16_t indices_size) {
	uint32_t pointer = allocate_index();

	operands_buffer[0] = result_type;
	operands_buffer[1] = pointer;
	operands_buffer[2] = base;
	for (uint16_t i = 0; i < indices_size; ++i) {
		operands_buffer[i + 3] = indices[i];
	}

	write_instruction(instructions, 4 + indices_size, SPIRV_OPCODE_ACCESS_CHAIN, operands_buffer);
	return pointer;
}

uint32_t write_op_load(instructions_buffer *instructions, uint32_t result_type, uint32_t pointer) {
	uint32_t result = allocate_index();

	uint32_t operands[3];
	operands[0] = result_type;
	operands[1] = result;
	operands[2] = pointer;
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_LOAD, operands);
	return result;
}

static struct {
	uint64_t key;
	uint32_t value;
} *hash = NULL;

uint32_t convert_kong_index_to_spirv_index(uint64_t index) {
	uint32_t spirv_index = hmget(hash, index);
	if (spirv_index == 0) {
		spirv_index = allocate_index();
		hmput(hash, index, spirv_index);
	}
	return spirv_index;
}

void write_function(instructions_buffer *instructions, function *f) {
	write_op_function(instructions, void_type, FUNCTION_CONTROL_NONE, void_function_type);
	write_label(instructions);

	debug_context context = {0};
	check(f->block != NULL, context, "Function block missing");

	uint8_t *data = f->code.o;
	size_t size = f->code.size;

	uint64_t parameter_id = 0;
	for (size_t i = 0; i < f->block->block.vars.size; ++i) {
		if (f->parameter_name == f->block->block.vars.v[i].name) {
			parameter_id = f->block->block.vars.v[i].variable_id;
			break;
		}
	}

	check(parameter_id != 0, context, "Parameter not found");

	bool ends_with_return = false;

	size_t index = 0;
	while (index < size) {
		ends_with_return = false;
		opcode *o = (opcode *)&data[index];
		switch (o->type) {
		case OPCODE_VAR: {
			break;
		}
		case OPCODE_LOAD_MEMBER: {
			uint32_t indices[256];
			uint16_t indices_size = o->op_load_member.member_indices_size;
			for (size_t i = 0; i < indices_size; ++i) {
				indices[i] = o->op_load_member.member_indices[i];
			}
			uint32_t pointer = write_op_access_chain(instructions, float_input_pointer_type, convert_kong_index_to_spirv_index(o->op_load_member.from.index),
			                                         indices, indices_size);
			uint32_t value = write_op_load(instructions, spirv_float_type, pointer);
			break;
		}
		case OPCODE_LOAD_CONSTANT: {
			break;
		}
		case OPCODE_CALL: {
			break;
		}
		case OPCODE_STORE_MEMBER: {
			break;
		}
		case OPCODE_RETURN: {
			write_return(instructions);
			ends_with_return = true;
			break;
		}
		default: {
			debug_context context = {0};
			error(context, "Opcode not implemented for SPIR-V");
			break;
		}
		}

		index += o->size;
	}

	if (!ends_with_return) {
		write_return(instructions);
	}
	write_function_end(instructions);
}

void write_functions(instructions_buffer *instructions, function *main) {
	write_function(instructions, main);
}

static void spirv_export_vertex(char *directory, function *main) {
	instructions_buffer instructions = {0};
	instructions.instructions = (uint32_t *)calloc(1024 * 1024, 1);

	type_id vertex_input = main->parameter_type.type;
	type_id vertex_output = main->return_type.type;

	debug_context context = {0};
	check(vertex_input != NO_TYPE, context, "vertex input missing");
	check(vertex_output != NO_TYPE, context, "vertex output missing");

	write_magic_number(&instructions);
	write_version_number(&instructions);
	write_generator_magic_number(&instructions);
	write_bound(&instructions);
	write_instruction_schema(&instructions);

	write_capabilities(&instructions);
	write_op_ext_inst_import(&instructions, "GLSL.std.450");
	write_op_memory_model(&instructions, ADDRESSING_MODEL_LOGICAL, MEMORY_MODEL_GLSL450);
	uint32_t entry_point = allocate_index();
	uint32_t interfaces[] = {3, 4};
	write_op_entry_point(&instructions, EXECUTION_MODEL_VERTEX, entry_point, "main", interfaces, sizeof(interfaces) / 4);

	uint32_t output_struct = allocate_index();
	write_vertex_output_decorations(&instructions, output_struct);

	uint32_t inputs[256];
	inputs[0] = allocate_index();
	write_vertex_input_decorations(&instructions, inputs, 1);

	write_types(&instructions, vertex_input);

	write_functions(&instructions, main);

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

	write_magic_number(&instructions);
	write_version_number(&instructions);
	write_generator_magic_number(&instructions);
	write_bound(&instructions);
	write_instruction_schema(&instructions);

	write_capabilities(&instructions);

	char *name = get_name(main->name);

	char filename[512];
	sprintf(filename, "kong_%s", name);

	char var_name[256];
	sprintf(var_name, "%s_code", name);

	write_bytecode(directory, filename, var_name, &instructions);
}

void spirv_export(char *directory) {
	hmdefault(hash, 0);

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
