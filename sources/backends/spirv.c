#include "spirv.h"

#include "../analyzer.h"
#include "../compiler.h"
#include "../errors.h"
#include "../functions.h"
#include "../hashmap.h"
#include "../log.h"
#include "../parser.h"
#include "../shader_stage.h"
#include "../types.h"

#include "../libs/stb_ds.h"

#include "util.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct spirv_id {
	uint32_t id;
} spirv_id;

typedef struct instructions_buffer {
	uint32_t *instructions;
	size_t    offset;
} instructions_buffer;

static void write_buffer(FILE *file, uint8_t *output, size_t output_size) {
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
}

static void write_bytecode(char *directory, const char *filename, const char *name, instructions_buffer *header, instructions_buffer *decorations,
                           instructions_buffer *constants, instructions_buffer *instructions, bool debug) {
	uint8_t *output_header      = (uint8_t *)header->instructions;
	size_t   output_header_size = header->offset * 4;

	uint8_t *output_decorations      = (uint8_t *)decorations->instructions;
	size_t   output_decorations_size = decorations->offset * 4;

	uint8_t *output_constants      = (uint8_t *)constants->instructions;
	size_t   output_constants_size = constants->offset * 4;

	uint8_t *output_instructions      = (uint8_t *)instructions->instructions;
	size_t   output_instructions_size = instructions->offset * 4;

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
		write_buffer(file, output_header, output_header_size);
		write_buffer(file, output_decorations, output_decorations_size);
		write_buffer(file, output_constants, output_constants_size);
		write_buffer(file, output_instructions, output_instructions_size);
		fprintf(file, "\";\n");

		fprintf(file, "size_t %s_size = %zu;\n\n", name, output_header_size + output_decorations_size + output_constants_size + output_instructions_size);

		fclose(file);
	}

#ifndef NDEBUG
	debug = true;
#endif

	if (debug) {
		sprintf(full_filename, "%s/%s.spirv", directory, filename);

		FILE *file = fopen(full_filename, "wb");
		fwrite(output_header, 1, output_header_size, file);
		fwrite(output_decorations, 1, output_decorations_size, file);
		fwrite(output_constants, 1, output_constants_size, file);
		fwrite(output_instructions, 1, output_instructions_size, file);
		fclose(file);

		char command[1024];
		snprintf(command, 1024, "spirv-val %s", full_filename);

		uint32_t exit_code = 0;
		bool     executed  = execute_sync(command, &exit_code);

		if (!executed) {
			kong_log(LOG_LEVEL_WARNING, "Could not run spirv_val.");
		}
		else if (exit_code != 0) {
			debug_context context = {0};
			error(context, "spirv_val check of %s failed with exit code %u.", filename, exit_code);
		}
	}
}

typedef enum spirv_opcode {
	SPIRV_OPCODE_EXT_INST_IMPORT           = 11,
	SPIRV_OPCODE_MEMORY_MODEL              = 14,
	SPIRV_OPCODE_ENTRY_POINT               = 15,
	SPIRV_OPCODE_EXECUTION_MODE            = 16,
	SPIRV_OPCODE_CAPABILITY                = 17,
	SPIRV_OPCODE_TYPE_VOID                 = 19,
	SPIRV_OPCODE_TYPE_BOOL                 = 20,
	SPIRV_OPCODE_TYPE_INT                  = 21,
	SPIRV_OPCODE_TYPE_FLOAT                = 22,
	SPIRV_OPCODE_TYPE_VECTOR               = 23,
	SPIRV_OPCODE_TYPE_MATRIX               = 24,
	SPIRV_OPCODE_TYPE_IMAGE                = 25,
	SPIRV_OPCODE_TYPE_SAMPLER              = 26,
	SPIRV_OPCODE_TYPE_SAMPLED_IMAGE        = 27,
	SPIRV_OPCODE_TYPE_STRUCT               = 30,
	SPIRV_OPCODE_TYPE_POINTER              = 32,
	SPIRV_OPCODE_TYPE_FUNCTION             = 33,
	SPIRV_OPCODE_CONSTANT                  = 43,
	SPIRV_OPCODE_FUNCTION                  = 54,
	SPIRV_OPCODE_FUNCTION_END              = 56,
	SPIRV_OPCODE_VARIABLE                  = 59,
	SPIRV_OPCODE_LOAD                      = 61,
	SPIRV_OPCODE_STORE                     = 62,
	SPIRV_OPCODE_ACCESS_CHAIN              = 65,
	SPIRV_OPCODE_DECORATE                  = 71,
	SPIRV_OPCODE_MEMBER_DECORATE           = 72,
	SPIRV_OPCODE_COMPOSITE_CONSTRUCT       = 80,
	SPIRV_OPCODE_COMPOSITE_EXTRACT         = 81,
	SPIRV_OPCODE_SAMPLED_IMAGE             = 86,
	SPIRV_OPCODE_IMAGE_SAMPLE_IMPLICIT_LOD = 87,
	SPIRV_OPCODE_F_MUL                     = 133,
	SPIRV_OPCODE_VECTOR_TIMES_MATRIX       = 144,
	SPIRV_OPCODE_MATRIX_TIMES_VECTOR       = 145,
	SPIRV_OPCODE_MATRIX_TIMES_MATRIX       = 146,
	SPIRV_OPCODE_F_ORD_LESS_THAN           = 184,
	SPIRV_OPCODE_LOOP_MERGE                = 246,
	SPIRV_OPCODE_SELECTION_MERGE           = 247,
	SPIRV_OPCODE_LABEL                     = 248,
	SPIRV_OPCODE_BRANCH                    = 249,
	SPIRV_OPCODE_BRANCH_CONDITIONAL        = 250,
	SPIRV_OPCODE_RETURN                    = 253,
} spirv_opcode;

static type_id find_access_type(int *indices, int indices_size, type_id base_type) {
	if (indices_size == 1) {
		if (is_vector(base_type)) {
			return vector_base_type(base_type);
		}
		else {
			type *t = get_type(base_type);
			assert(indices[0] < t->members.size);
			return t->members.m[indices[0]].type.type;
		}
	}
	else {
		type *t = get_type(base_type);
		assert(indices[0] < t->members.size);
		return find_access_type(&indices[1], indices_size - 1, t->members.m[indices[0]].type.type);
	}
}

typedef enum addressing_model { ADDRESSING_MODEL_LOGICAL = 0 } addressing_model;

typedef enum memory_model { MEMORY_MODEL_SIMPLE = 0, MEMORY_MODEL_GLSL450 = 1 } memory_model;

typedef enum capability { CAPABILITY_SHADER = 1 } capability;

typedef enum execution_model { EXECUTION_MODEL_VERTEX = 0, EXECUTION_MODEL_FRAGMENT = 4 } execution_model;

typedef enum decoration {
	DECORATION_BLOCK          = 2,
	DECORATION_COL_MAJOR      = 5,
	DECORATION_MATRIX_STRIDE  = 7,
	DECORATION_BUILTIN        = 11,
	DECORATION_LOCATION       = 30,
	DECORATION_BINDING        = 33,
	DECORATION_DESCRIPTOR_SET = 34,
	DECORATION_OFFSET         = 35
} decoration;

typedef enum builtin { BUILTIN_POSITION = 0 } builtin;

typedef enum storage_class {
	STORAGE_CLASS_UNIFORM_CONSTANT = 0,
	STORAGE_CLASS_INPUT            = 1,
	STORAGE_CLASS_UNIFORM          = 2,
	STORAGE_CLASS_OUTPUT           = 3,
	STORAGE_CLASS_FUNCTION         = 7,
	STORAGE_CLASS_NONE             = 9999
} storage_class;

typedef enum selection_control { SELECTION_CONTROL_NONE = 0, SELCTION_CONTROL_FLATTEN = 1, SELECTION_CONTROL_DONT_FLATTEN = 2 } selection_control;

typedef enum loop_control { LOOP_CONTROL_NONE = 0, LOOP_CONTROL_UNROLL = 1, LOOP_CONTROL_DONT_UNROLL = 2 } loop_control;

typedef enum function_control { FUNCTION_CONTROL_NONE } function_control;

typedef enum execution_mode { EXECUTION_MODE_ORIGIN_UPPER_LEFT = 7 } execution_mode;

typedef enum dim {
	DIM_1D   = 0,
	DIM_2D   = 1,
	DIM_3D   = 2,
	DIM_CUBE = 3,
} dim;

typedef enum image_format {
	IMAGE_FORMAT_UNKNOWN     = 0,
	IMAGE_FORMAT_RGBA32F     = 1,
	IMAGE_FORMAT_RGBA16F     = 2,
	IMAGE_FORMAT_R32F        = 3,
	IMAGE_FORMAT_RGBA8       = 4,
	IMAGE_FORMAT_RGBA8_SNORM = 5,
} image_format;

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
	instructions->instructions[instructions->offset++] = 44;
}

static uint32_t next_index = 1;

static void write_bound(instructions_buffer *instructions) {
	instructions->instructions[instructions->offset++] = next_index;
}

static void write_instruction_schema(instructions_buffer *instructions) {
	instructions->instructions[instructions->offset++] = 0; // reserved in SPIR-V for later use, currently always zero
}

static void write_capability(instructions_buffer *instructions, capability c) {
	uint32_t operand = (uint32_t)c;
	write_instruction(instructions, 2, SPIRV_OPCODE_CAPABILITY, &operand);
}

static spirv_id allocate_index(void) {
	uint32_t result = next_index;
	++next_index;

	spirv_id id;
	id.id = result;
	return id;
}

static uint16_t write_string(uint32_t *operands, const char *string) {
	uint16_t length = (uint16_t)strlen(string);
	memcpy(&operands[0], string, length + 1);
	return (length + 1) / 4 + 1;
}

static spirv_id write_op_ext_inst_import(instructions_buffer *instructions, const char *name) {
	spirv_id result = allocate_index();

	operands_buffer[0] = result.id;

	uint32_t name_length = write_string(&operands_buffer[1], name);

	write_instruction(instructions, 2 + name_length, SPIRV_OPCODE_EXT_INST_IMPORT, operands_buffer);

	return result;
}

static void write_op_memory_model(instructions_buffer *instructions, uint32_t addressing_model, uint32_t memory_model) {
	uint32_t args[2] = {addressing_model, memory_model};
	write_instruction(instructions, 3, SPIRV_OPCODE_MEMORY_MODEL, args);
}

static void write_op_entry_point(instructions_buffer *instructions, execution_model em, spirv_id entry_point, const char *name, spirv_id *interfaces,
                                 uint16_t interfaces_size) {
	operands_buffer[0] = (uint32_t)em;
	operands_buffer[1] = entry_point.id;

	uint32_t name_length = write_string(&operands_buffer[2], name);

	for (uint16_t i = 0; i < interfaces_size; ++i) {
		operands_buffer[2 + name_length + i] = interfaces[i].id;
	}

	write_instruction(instructions, 3 + name_length + interfaces_size, SPIRV_OPCODE_ENTRY_POINT, operands_buffer);
}

static void write_op_execution_mode(instructions_buffer *instructions, spirv_id entry_point, execution_mode mode) {
	operands_buffer[0] = entry_point.id;
	operands_buffer[1] = (uint32_t)mode;

	write_instruction(instructions, 3, SPIRV_OPCODE_EXECUTION_MODE, operands_buffer);
}

static void write_capabilities(instructions_buffer *instructions) {
	write_capability(instructions, CAPABILITY_SHADER);
}

static spirv_id write_type_void(instructions_buffer *instructions) {
	spirv_id void_type = allocate_index();
	write_instruction(instructions, 2, SPIRV_OPCODE_TYPE_VOID, &void_type.id);
	return void_type;
}

#define WORD_COUNT(operands) (1 + sizeof(operands) / 4)

static spirv_id write_type_function(instructions_buffer *instructions, spirv_id return_type, spirv_id *parameter_types, uint16_t parameter_types_size) {
	spirv_id function_type = allocate_index();

	operands_buffer[0] = function_type.id;
	operands_buffer[1] = return_type.id;
	for (uint16_t i = 0; i < parameter_types_size; ++i) {
		operands_buffer[i + 2] = parameter_types[0].id;
	}
	write_instruction(instructions, 3 + parameter_types_size, SPIRV_OPCODE_TYPE_FUNCTION, operands_buffer);
	return function_type;
}

static spirv_id write_type_float(instructions_buffer *instructions, uint32_t width) {
	spirv_id float_type = allocate_index();

	uint32_t operands[] = {float_type.id, width};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_TYPE_FLOAT, operands);
	return float_type;
}

// static spirv_id write_type_vector(instructions_buffer *instructions, spirv_id component_type, uint32_t component_count) {
//	spirv_id vector_type = allocate_index();
//
//	uint32_t operands[] = {vector_type.id, component_type.id, component_count};
//	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_TYPE_VECTOR, operands);
//	return vector_type;
// }

static spirv_id write_type_vector_preallocated(instructions_buffer *instructions, spirv_id component_type, uint32_t component_count, spirv_id vector_type) {
	uint32_t operands[] = {vector_type.id, component_type.id, component_count};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_TYPE_VECTOR, operands);
	return vector_type;
}

static spirv_id write_type_matrix(instructions_buffer *instructions, spirv_id column_type, uint32_t column_count) {
	spirv_id matrix_type = allocate_index();

	uint32_t operands[] = {matrix_type.id, column_type.id, column_count};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_TYPE_MATRIX, operands);
	return matrix_type;
}

static spirv_id write_type_int(instructions_buffer *instructions, uint32_t width, bool signedness) {
	spirv_id int_type = allocate_index();

	uint32_t operands[] = {int_type.id, width, signedness ? 1 : 0};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_TYPE_INT, operands);
	return int_type;
}

static spirv_id write_type_bool(instructions_buffer *instructions) {
	spirv_id bool_type = allocate_index();

	uint32_t operands[] = {bool_type.id};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_TYPE_BOOL, operands);
	return bool_type;
}

static spirv_id write_type_image(instructions_buffer *instructions, spirv_id sampled_type, dim dimensionality, uint32_t depth, uint32_t arrayed, uint32_t ms,
                                 uint32_t sampled, image_format img_format) {
	spirv_id image_type = allocate_index();

	uint32_t operands[] = {image_type.id, sampled_type.id, (uint32_t)dimensionality, depth, arrayed, ms, sampled, (uint32_t)img_format};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_TYPE_IMAGE, operands);
	return image_type;
}

static spirv_id write_type_sampler(instructions_buffer *instructions) {
	spirv_id sampler_type = allocate_index();

	uint32_t operands[] = {sampler_type.id};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_TYPE_SAMPLER, operands);
	return sampler_type;
}

static spirv_id write_type_sampled_image(instructions_buffer *instructions, spirv_id image_type) {
	spirv_id sampled_image_type = allocate_index();

	uint32_t operands[] = {sampled_image_type.id, image_type.id};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_TYPE_SAMPLED_IMAGE, operands);
	return sampled_image_type;
}

static spirv_id write_type_struct(instructions_buffer *instructions, spirv_id *types, uint16_t types_size) {
	spirv_id struct_type = allocate_index();

	operands_buffer[0] = struct_type.id;
	for (uint16_t i = 0; i < types_size; ++i) {
		operands_buffer[i + 1] = types[i].id;
	}
	write_instruction(instructions, 2 + types_size, SPIRV_OPCODE_TYPE_STRUCT, operands_buffer);
	return struct_type;
}

static spirv_id write_type_pointer(instructions_buffer *instructions, storage_class storage, spirv_id type) {
	spirv_id pointer_type = allocate_index();

	uint32_t operands[] = {pointer_type.id, (uint32_t)storage, type.id};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_TYPE_POINTER, operands);
	return pointer_type;
}

static spirv_id write_type_pointer_preallocated(instructions_buffer *instructions, storage_class storage, spirv_id type, spirv_id pointer_type) {
	uint32_t operands[] = {pointer_type.id, (uint32_t)storage, type.id};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_TYPE_POINTER, operands);
	return pointer_type;
}

static spirv_id void_type;
static spirv_id void_function_type;
static spirv_id spirv_float_type;
static spirv_id spirv_int_type;
static spirv_id spirv_uint_type;
static spirv_id spirv_float2_type;
static spirv_id spirv_float3_type;
static spirv_id spirv_float4_type;
static spirv_id spirv_bool_type;
static spirv_id spirv_sampler_type;
static spirv_id spirv_image_type;
static spirv_id spirv_sampled_image_type;

typedef struct complex_type {
	type_id  type;
	uint16_t pointer;
	uint16_t storage;
} complex_type;

static struct {
	complex_type key;
	spirv_id     value;
} *type_map = NULL;

static spirv_id convert_type_to_spirv_id(type_id type) {
	complex_type ct;
	ct.type    = type;
	ct.pointer = false;
	ct.storage = (uint16_t)STORAGE_CLASS_NONE;

	spirv_id spirv_index = hmget(type_map, ct);
	if (spirv_index.id == 0) {
		spirv_index = allocate_index();
		hmput(type_map, ct, spirv_index);
	}
	return spirv_index;
}

static spirv_id convert_pointer_type_to_spirv_id(type_id type, storage_class storage) {
	complex_type ct;
	ct.type    = type;
	ct.pointer = true;
	ct.storage = (uint16_t)storage;

	spirv_id spirv_index = hmget(type_map, ct);
	if (spirv_index.id == 0) {
		spirv_index = allocate_index();
		hmput(type_map, ct, spirv_index);
	}
	return spirv_index;
}

static spirv_id output_struct_pointer_type = {0};

static void store_base_type_mapping(instructions_buffer *constants_block, type_id type, spirv_id spirv_type) {
	complex_type ct;
	ct.pointer = false;
	ct.storage = (uint16_t)STORAGE_CLASS_NONE;
	ct.type    = type;

	hmput(type_map, ct, spirv_type);
}

static void write_base_types(instructions_buffer *constants_block) {
	void_type = write_type_void(constants_block);

	void_function_type = write_type_function(constants_block, void_type, NULL, 0);

	complex_type ct;
	ct.pointer = false;
	ct.storage = (uint16_t)STORAGE_CLASS_NONE;

	spirv_float_type = write_type_float(constants_block, 32);
	store_base_type_mapping(constants_block, float_id, spirv_float_type);

	spirv_float2_type = convert_type_to_spirv_id(float2_id);
	write_type_vector_preallocated(constants_block, spirv_float_type, 2, spirv_float2_type);
	store_base_type_mapping(constants_block, float2_id, spirv_float2_type);

	spirv_float3_type = convert_type_to_spirv_id(float3_id);
	write_type_vector_preallocated(constants_block, spirv_float_type, 3, spirv_float3_type);
	store_base_type_mapping(constants_block, float3_id, spirv_float3_type);

	spirv_float4_type = convert_type_to_spirv_id(float4_id);
	write_type_vector_preallocated(constants_block, spirv_float_type, 4, spirv_float4_type);
	store_base_type_mapping(constants_block, float4_id, spirv_float4_type);

	spirv_uint_type = write_type_int(constants_block, 32, false);
	store_base_type_mapping(constants_block, uint_id, spirv_uint_type);

	spirv_int_type = write_type_int(constants_block, 32, true);
	store_base_type_mapping(constants_block, int_id, spirv_int_type);

	spirv_bool_type = write_type_bool(constants_block);
	store_base_type_mapping(constants_block, bool_id, spirv_bool_type);

	spirv_sampler_type = write_type_sampler(constants_block);

	spirv_image_type = write_type_image(constants_block, spirv_float_type, DIM_2D, 0, 0, 0, 1, IMAGE_FORMAT_UNKNOWN);

	spirv_sampled_image_type = write_type_sampled_image(constants_block, spirv_image_type);

	store_base_type_mapping(constants_block, float3x3_id, write_type_matrix(constants_block, spirv_float3_type, 3));
	store_base_type_mapping(constants_block, float4x4_id, write_type_matrix(constants_block, spirv_float4_type, 4));
}

static void write_types(instructions_buffer *constants, function *main) {
	type_id types[256];
	size_t  types_size = 0;
	find_referenced_types(main, types, &types_size);

	for (size_t i = 0; i < types_size; ++i) {
		type *t = get_type(types[i]);

		if (!t->built_in && !has_attribute(&t->attributes, add_name("pipe"))) {
			spirv_id member_types[256];
			uint16_t member_types_size = 0;
			for (size_t j = 0; j < t->members.size; ++j) {
				member_types[member_types_size] = convert_type_to_spirv_id(t->members.m[j].type.type);
				member_types_size += 1;
				assert(member_types_size < 256);
			}
			spirv_id struct_type = write_type_struct(constants, member_types, member_types_size);

			complex_type ct;
			ct.type    = types[i];
			ct.pointer = false;
			ct.storage = (uint16_t)STORAGE_CLASS_NONE;
			hmput(type_map, ct, struct_type);
		}
	}

	size_t size = hmlenu(type_map);
	for (size_t i = 0; i < size; ++i) {
		complex_type type = type_map[i].key;
		if (type.pointer && type.storage != STORAGE_CLASS_UNIFORM && type.storage != STORAGE_CLASS_UNIFORM_CONSTANT) {
			write_type_pointer_preallocated(constants, type.storage, convert_type_to_spirv_id(type.type), type_map[i].value);
		}
	}
}

static spirv_id write_constant(instructions_buffer *instructions, spirv_id type, spirv_id value_id, uint32_t value) {
	uint32_t operands[] = {type.id, value_id.id, value};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_CONSTANT, operands);
	return value_id;
}

static spirv_id write_constant_int(instructions_buffer *instructions, spirv_id value_id, int32_t value) {
	uint32_t uint32_value = *(uint32_t *)&value;
	return write_constant(instructions, spirv_int_type, value_id, uint32_value);
}

static spirv_id write_constant_float(instructions_buffer *instructions, spirv_id value_id, float value) {
	uint32_t uint32_value = *(uint32_t *)&value;
	return write_constant(instructions, spirv_float_type, value_id, uint32_value);
}

static spirv_id write_constant_bool(instructions_buffer *instructions, spirv_id value_id, bool value) {
	uint32_t uint32_value = *(uint32_t *)&value;
	return write_constant(instructions, spirv_bool_type, value_id, uint32_value);
}

static void write_vertex_output_decorations(instructions_buffer *instructions, spirv_id output_struct, spirv_id *outputs, uint32_t outputs_size) {
	{
		uint32_t operands[] = {output_struct.id, 0, (uint32_t)DECORATION_BUILTIN, (uint32_t)BUILTIN_POSITION};
		write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_MEMBER_DECORATE, operands);
	}

	{
		uint32_t operands[] = {output_struct.id, (uint32_t)DECORATION_BLOCK};
		write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_DECORATE, operands);
	}

	for (uint32_t i = 1; i < outputs_size; ++i) {
		uint32_t operands[] = {outputs[i].id, (uint32_t)DECORATION_LOCATION, i - 1};
		write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_DECORATE, operands);
	}
}

static void write_vertex_input_decorations(instructions_buffer *instructions, spirv_id *inputs, uint32_t inputs_size) {
	for (uint32_t i = 0; i < inputs_size; ++i) {
		uint32_t operands[] = {inputs[i].id, (uint32_t)DECORATION_LOCATION, i};
		write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_DECORATE, operands);
	}
}

static void write_fragment_input_decorations(instructions_buffer *instructions, spirv_id *inputs, uint32_t inputs_size) {
	for (uint32_t i = 0; i < inputs_size; ++i) {
		uint32_t operands[] = {inputs[i].id, (uint32_t)DECORATION_LOCATION, i};
		write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_DECORATE, operands);
	}
}

static void write_fragment_output_decorations(instructions_buffer *instructions, spirv_id output) {
	uint32_t operands[] = {output.id, (uint32_t)DECORATION_LOCATION, 0};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_DECORATE, operands);
}

static void write_op_decorate(instructions_buffer *instructions, spirv_id target, decoration decor) {
	uint32_t operands[] = {target.id, (uint32_t)decor};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_DECORATE, operands);
}

static void write_op_decorate_value(instructions_buffer *instructions, spirv_id target, decoration decor, uint32_t value) {
	uint32_t operands[] = {target.id, (uint32_t)decor, value};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_DECORATE, operands);
}

static void write_op_member_decorate(instructions_buffer *instructions, spirv_id structure_type, uint32_t member, decoration decor) {
	uint32_t operands[] = {structure_type.id, member, (uint32_t)decor};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_MEMBER_DECORATE, operands);
}

static void write_op_member_decorate_value(instructions_buffer *instructions, spirv_id structure_type, uint32_t member, decoration decor, uint32_t value) {
	uint32_t operands[] = {structure_type.id, member, (uint32_t)decor, value};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_MEMBER_DECORATE, operands);
}

static spirv_id write_op_function_preallocated(instructions_buffer *instructions, spirv_id result_type, function_control control, spirv_id function_type,
                                               spirv_id result) {
	uint32_t operands[] = {result_type.id, result.id, (uint32_t)control, function_type.id};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_FUNCTION, operands);
	return result;
}

// static spirv_id write_op_function(instructions_buffer *instructions, spirv_id result_type, function_control control, spirv_id function_type) {
//	spirv_id result = allocate_index();
//	write_op_function_preallocated(instructions, result_type, control, function_type, result);
//	return result;
// }

static spirv_id write_op_label(instructions_buffer *instructions) {
	spirv_id result = allocate_index();

	uint32_t operands[] = {result.id};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_LABEL, operands);
	return result;
}

static void write_op_label_preallocated(instructions_buffer *instructions, spirv_id result) {
	uint32_t operands[] = {result.id};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_LABEL, operands);
}

static void write_op_branch(instructions_buffer *instructions, spirv_id target) {
	uint32_t operands[] = {target.id};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_BRANCH, operands);
}

static void write_op_loop_merge(instructions_buffer *instructions, spirv_id merge_block, spirv_id continue_target, loop_control control) {
	uint32_t operands[] = {merge_block.id, continue_target.id, (uint32_t)control};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_LOOP_MERGE, operands);
}

static void write_op_return(instructions_buffer *instructions) {
	write_simple_instruction(instructions, SPIRV_OPCODE_RETURN);
}

static void write_op_function_end(instructions_buffer *instructions) {
	write_simple_instruction(instructions, SPIRV_OPCODE_FUNCTION_END);
}

typedef struct int_constant_container {
	struct container container;
	spirv_id         value;
} int_constant_container;

static struct hash_map *int_constants = NULL;

static spirv_id get_int_constant(int value) {
	int_constant_container *container = (int_constant_container *)hash_map_get(int_constants, value);

	if (container == NULL) {
		container = (int_constant_container *)malloc(sizeof(int_constant_container));
		assert(container != NULL);
		container->container.key = value;
		container->value         = allocate_index();
		hash_map_add(int_constants, (struct container *)container);
	}

	return container->value;
}

static struct {
	float    key;
	spirv_id value;
} *float_constants = NULL;

static spirv_id get_float_constant(float value) {
	spirv_id index = hmget(float_constants, value);
	if (index.id == 0) {
		index = allocate_index();
		hmput(float_constants, value, index);
	}
	return index;
}

static struct {
	bool     key;
	spirv_id value;
} *bool_constants = NULL;

static spirv_id get_bool_constant(bool value) {
	spirv_id index = hmget(bool_constants, value);
	if (index.id == 0) {
		index = allocate_index();
		hmput(bool_constants, value, index);
	}
	return index;
}

static spirv_id write_op_access_chain(instructions_buffer *instructions, spirv_id result_type, spirv_id base, int *indices, uint16_t indices_size) {
	spirv_id pointer = allocate_index();

	operands_buffer[0] = result_type.id;
	operands_buffer[1] = pointer.id;
	operands_buffer[2] = base.id;
	for (uint16_t i = 0; i < indices_size; ++i) {
		operands_buffer[i + 3] = get_int_constant(indices[i]).id;
	}

	write_instruction(instructions, 4 + indices_size, SPIRV_OPCODE_ACCESS_CHAIN, operands_buffer);
	return pointer;
}

static spirv_id write_op_load(instructions_buffer *instructions, spirv_id result_type, spirv_id pointer) {
	spirv_id result = allocate_index();

	uint32_t operands[] = {result_type.id, result.id, pointer.id};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_LOAD, operands);
	return result;
}

static void write_op_store(instructions_buffer *instructions, spirv_id pointer, spirv_id object) {
	uint32_t operands[] = {pointer.id, object.id};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_STORE, operands);
}

static spirv_id write_op_composite_construct(instructions_buffer *instructions, spirv_id type, spirv_id *constituents, uint16_t constituents_size) {
	spirv_id result = allocate_index();

	operands_buffer[0] = type.id;
	operands_buffer[1] = result.id;
	for (uint16_t i = 0; i < constituents_size; ++i) {
		operands_buffer[i + 2] = constituents[i].id;
	}
	write_instruction(instructions, 3 + constituents_size, SPIRV_OPCODE_COMPOSITE_CONSTRUCT, operands_buffer);
	return result;
}

static spirv_id write_op_composite_extract(instructions_buffer *instructions, spirv_id type, spirv_id composite, uint32_t *indices, uint16_t indices_size) {
	spirv_id result = allocate_index();

	operands_buffer[0] = type.id;
	operands_buffer[1] = result.id;
	operands_buffer[2] = composite.id;
	for (uint16_t i = 0; i < indices_size; ++i) {
		operands_buffer[i + 3] = indices[i];
	}
	write_instruction(instructions, 4 + indices_size, SPIRV_OPCODE_COMPOSITE_EXTRACT, operands_buffer);
	return result;
}

static spirv_id write_op_f_ord_less_than(instructions_buffer *instructions, spirv_id type, spirv_id operand1, spirv_id operand2) {
	spirv_id result = allocate_index();

	uint32_t operands[] = {type.id, result.id, operand1.id, operand2.id};

	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_F_ORD_LESS_THAN, operands);

	return result;
}

static spirv_id write_op_f_mul(instructions_buffer *instructions, spirv_id type, spirv_id operand1, spirv_id operand2) {
	spirv_id result = allocate_index();

	uint32_t operands[] = {type.id, result.id, operand1.id, operand2.id};

	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_F_MUL, operands);

	return result;
}

static spirv_id write_op_matrix_times_vector(instructions_buffer *instructions, spirv_id type, spirv_id operand1, spirv_id operand2) {
	spirv_id result = allocate_index();

	uint32_t operands[] = {type.id, result.id, operand1.id, operand2.id};

	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_MATRIX_TIMES_VECTOR, operands);

	return result;
}

static spirv_id write_op_vector_times_matrix(instructions_buffer *instructions, spirv_id type, spirv_id operand1, spirv_id operand2) {
	spirv_id result = allocate_index();

	uint32_t operands[] = {type.id, result.id, operand1.id, operand2.id};

	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_VECTOR_TIMES_MATRIX, operands);

	return result;
}

static spirv_id write_op_matrix_times_matrix(instructions_buffer *instructions, spirv_id type, spirv_id operand1, spirv_id operand2) {
	spirv_id result = allocate_index();

	uint32_t operands[] = {type.id, result.id, operand1.id, operand2.id};

	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_MATRIX_TIMES_MATRIX, operands);

	return result;
}

static void write_op_selection_merge(instructions_buffer *instructions, spirv_id merge_block, selection_control control) {
	uint32_t operands[] = {merge_block.id, (uint32_t)control};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_SELECTION_MERGE, operands);
}

static void write_op_branch_conditional(instructions_buffer *instructions, spirv_id condition, spirv_id pass, spirv_id fail) {
	uint32_t operands[] = {condition.id, pass.id, fail.id};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_BRANCH_CONDITIONAL, operands);
}

static spirv_id write_op_sampled_image(instructions_buffer *instructions, spirv_id result_type, spirv_id image, spirv_id sampler) {
	spirv_id result = allocate_index();

	uint32_t operands[] = {result_type.id, result.id, image.id, sampler.id};

	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_SAMPLED_IMAGE, operands);

	return result;
}

static spirv_id write_op_image_sample_implicit_lod(instructions_buffer *instructions, spirv_id result_type, spirv_id sampled_image, spirv_id coordinate) {
	spirv_id result = allocate_index();

	uint32_t operands[] = {result_type.id, result.id, sampled_image.id, coordinate.id};

	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_IMAGE_SAMPLE_IMPLICIT_LOD, operands);

	return result;
}

static spirv_id write_op_variable(instructions_buffer *instructions, spirv_id result_type, storage_class storage) {
	spirv_id result = allocate_index();

	uint32_t operands[] = {result_type.id, result.id, (uint32_t)storage};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_VARIABLE, operands);
	return result;
}

static void write_op_variable_preallocated(instructions_buffer *instructions, spirv_id result_type, spirv_id result, storage_class storage) {
	uint32_t operands[] = {result_type.id, result.id, (uint32_t)storage};
	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_VARIABLE, operands);
}

// static spirv_id write_op_variable_with_initializer(instructions_buffer *instructions, uint32_t result_type, storage_class storage, uint32_t initializer) {
//	spirv_id result = allocate_index();
//
//	uint32_t operands[] = {result_type, result.id, (uint32_t)storage, initializer};
//	write_instruction(instructions, WORD_COUNT(operands), SPIRV_OPCODE_VARIABLE, operands);
//	return result;
// }

static struct {
	uint64_t key;
	spirv_id value;
} *index_map = NULL;

static spirv_id convert_kong_index_to_spirv_id(uint64_t index) {
	spirv_id id = hmget(index_map, index);
	if (id.id == 0) {
		id = allocate_index();
		hmput(index_map, index, id);
	}
	return id;
}

static spirv_id per_vertex_var    = {0};
static spirv_id output_vars[256]  = {0};
static type_id  output_types[256] = {0};
static size_t   output_vars_count = 0;

static spirv_id input_vars[256]  = {0};
static type_id  input_types[256] = {0};
static size_t   input_vars_count = 0;

static void write_function(instructions_buffer *instructions, function *f, spirv_id function_id, shader_stage stage, bool main, type_id input, type_id output) {
	write_op_function_preallocated(instructions, void_type, FUNCTION_CONTROL_NONE, void_function_type, function_id);
	write_op_label(instructions);

	debug_context context = {0};
	check(f->block != NULL, context, "Function block missing");

	uint8_t *data = f->code.o;
	size_t   size = f->code.size;

	uint64_t parameter_ids[256]   = {0};
	type_id  parameter_types[256] = {0};
	for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
		for (size_t i = 0; i < f->block->block.vars.size; ++i) {
			if (f->parameter_names[parameter_index] == f->block->block.vars.v[i].name) {
				parameter_ids[parameter_index]   = f->block->block.vars.v[i].variable_id;
				parameter_types[parameter_index] = f->block->block.vars.v[i].type.type;
				break;
			}
		}
	}

	for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
		check(parameter_ids[parameter_index] != 0, context, "Parameter not found");
	}

	// create variable for the input parameter
	spirv_id spirv_parameter_id = convert_kong_index_to_spirv_id(parameter_ids[0]);
	write_op_variable_preallocated(instructions, convert_pointer_type_to_spirv_id(parameter_types[0], STORAGE_CLASS_FUNCTION), spirv_parameter_id,
	                               STORAGE_CLASS_FUNCTION);

	// all vars have to go first
	size_t index = 0;
	while (index < size) {
		opcode *o = (opcode *)&data[index];
		switch (o->type) {
		case OPCODE_VAR: {
			spirv_id result =
			    write_op_variable(instructions, convert_pointer_type_to_spirv_id(o->op_var.var.type.type, STORAGE_CLASS_FUNCTION), STORAGE_CLASS_FUNCTION);
			hmput(index_map, o->op_var.var.index, result);
			break;
		}
		default:
			break;
		}

		index += o->size;
	}

	// transfer input values into the input variable
	if (stage == SHADER_STAGE_FRAGMENT) {
		for (size_t i = 0; i < input_vars_count; ++i) {
			int      index  = (int)(i + 1); // jump over the pos member
			spirv_id loaded = write_op_load(instructions, convert_type_to_spirv_id(input_types[i]), input_vars[i]);
			spirv_id pointer =
			    write_op_access_chain(instructions, convert_pointer_type_to_spirv_id(input_types[i], STORAGE_CLASS_FUNCTION), spirv_parameter_id, &index, 1);
			write_op_store(instructions, pointer, loaded);
		}
	}
	else {
		for (size_t i = 0; i < input_vars_count; ++i) {
			int      index  = (int)i;
			spirv_id loaded = write_op_load(instructions, convert_type_to_spirv_id(input_types[i]), input_vars[i]);
			spirv_id pointer =
			    write_op_access_chain(instructions, convert_pointer_type_to_spirv_id(input_types[i], STORAGE_CLASS_FUNCTION), spirv_parameter_id, &index, 1);
			write_op_store(instructions, pointer, loaded);
		}
	}

	bool ends_with_return = false;

	index = 0;
	while (index < size) {
		ends_with_return = false;
		opcode *o        = (opcode *)&data[index];
		switch (o->type) {
		case OPCODE_VAR: {
			break;
		}
		case OPCODE_LOAD_ACCESS_LIST: {
			uint16_t indices_size = o->op_load_access_list.access_list_size;

			if (o->op_load_access_list.from.kind == VARIABLE_INTERNAL) {
				uint32_t indices[256];

				type *s = get_type(o->op_load_access_list.from.type.type);

				for (uint16_t i = 0; i < indices_size; ++i) {
					switch (o->op_load_access_list.access_list[i].kind) {
					case ACCESS_ELEMENT:
						assert(false);
						break;
					case ACCESS_MEMBER: {
						uint32_t member_index = 0;
						bool     found        = false;

						for (; member_index < s->members.size; ++member_index) {
							if (s->members.m[member_index].name == o->op_load_access_list.access_list[i].access_member.name) {
								found = true;
								break;
							}
						}

						assert(found);

						indices[i] = member_index;

						break;
					}
					case ACCESS_SWIZZLE: {
						assert(o->op_load_access_list.access_list[i].access_swizzle.swizzle.size == 1);

						indices[i] = o->op_load_access_list.access_list[i].access_swizzle.swizzle.indices[0];

						break;
					}
					}

					s = get_type(o->op_load_access_list.access_list[i].type);
				}

				spirv_id value = write_op_composite_extract(instructions, convert_type_to_spirv_id(o->op_load_access_list.to.type.type),
				                                            convert_kong_index_to_spirv_id(o->op_load_access_list.from.index), indices, indices_size);

				hmput(index_map, o->op_load_access_list.to.index, value);
			}
			else {
				int indices[256];

				type *s = get_type(o->op_load_access_list.from.type.type);

				for (uint16_t i = 0; i < indices_size; ++i) {
					switch (o->op_load_access_list.access_list[i].kind) {
					case ACCESS_ELEMENT:
						assert(false);
						break;
					case ACCESS_MEMBER: {
						int  member_index = 0;
						bool found        = false;

						for (; member_index < s->members.size; ++member_index) {
							if (s->members.m[member_index].name == o->op_load_access_list.access_list[i].access_member.name) {
								found = true;
								break;
							}
						}

						assert(found);

						indices[i] = member_index;

						break;
					}
					case ACCESS_SWIZZLE: {
						assert(o->op_load_access_list.access_list[i].access_swizzle.swizzle.size == 1);

						indices[i] = o->op_load_access_list.access_list[i].access_swizzle.swizzle.indices[0];

						break;
					}
					}

					s = get_type(o->op_load_access_list.access_list[i].type);
				}

				storage_class storage;
				if (o->op_load_access_list.from.index == parameter_ids[0]) {
					storage = STORAGE_CLASS_FUNCTION;
				}
				else {
					if (o->op_load_access_list.from.kind == VARIABLE_GLOBAL) {
						storage = STORAGE_CLASS_UNIFORM;
					}
					else {
						storage = STORAGE_CLASS_INPUT;
					}
				}

				spirv_id pointer = write_op_access_chain(instructions, convert_pointer_type_to_spirv_id(o->op_load_access_list.to.type.type, storage),
				                                         convert_kong_index_to_spirv_id(o->op_load_access_list.from.index), indices, indices_size);

				spirv_id value = write_op_load(instructions, convert_type_to_spirv_id(o->op_load_access_list.to.type.type), pointer);
				hmput(index_map, o->op_load_access_list.to.index, value);
			}
			break;
		}
		case OPCODE_LOAD_FLOAT_CONSTANT: {
			spirv_id id = get_float_constant(o->op_load_float_constant.number);
			hmput(index_map, o->op_load_float_constant.to.index, id);
			break;
		}
		case OPCODE_LOAD_BOOL_CONSTANT: {
			spirv_id id = get_bool_constant(o->op_load_bool_constant.boolean);
			hmput(index_map, o->op_load_bool_constant.to.index, id);
			break;
		}
		case OPCODE_CALL: {
			if (o->op_call.func == add_name("sample")) {
				spirv_id image = write_op_load(instructions, spirv_image_type, convert_kong_index_to_spirv_id(o->op_call.parameters[0].index));

				spirv_id sampler = write_op_load(instructions, spirv_sampler_type, convert_kong_index_to_spirv_id(o->op_call.parameters[1].index));

				spirv_id sampled_image = write_op_sampled_image(instructions, spirv_sampled_image_type, image, sampler);
				spirv_id id            = write_op_image_sample_implicit_lod(instructions, spirv_float4_type, sampled_image,
				                                                            convert_kong_index_to_spirv_id(o->op_call.parameters[2].index));
				hmput(index_map, o->op_call.var.index, id);
			}
			else if (o->op_call.func == add_name("sample_lod")) {
			}
			else if (o->op_call.func == add_name("float2")) {
				spirv_id constituents[2];
				for (int i = 0; i < o->op_call.parameters_size; ++i) {
					constituents[i] = convert_kong_index_to_spirv_id(o->op_call.parameters[i].index);
				}
				spirv_id id = write_op_composite_construct(instructions, spirv_float2_type, constituents, o->op_call.parameters_size);
				hmput(index_map, o->op_call.var.index, id);
			}
			else if (o->op_call.func == add_name("float3")) {
				spirv_id constituents[3];
				for (int i = 0; i < o->op_call.parameters_size; ++i) {
					constituents[i] = convert_kong_index_to_spirv_id(o->op_call.parameters[i].index);
				}
				spirv_id id = write_op_composite_construct(instructions, spirv_float3_type, constituents, o->op_call.parameters_size);
				hmput(index_map, o->op_call.var.index, id);
			}
			else if (o->op_call.func == add_name("float4")) {
				spirv_id constituents[4];
				for (int i = 0; i < o->op_call.parameters_size; ++i) {
					constituents[i] = convert_kong_index_to_spirv_id(o->op_call.parameters[i].index);
				}
				spirv_id id = write_op_composite_construct(instructions, spirv_float4_type, constituents, o->op_call.parameters_size);
				hmput(index_map, o->op_call.var.index, id);
			}
			else {
				assert(false);
			}
			break;
		}
		case OPCODE_STORE_ACCESS_LIST: {
			int indices[256];

			uint16_t indices_size = o->op_store_access_list.access_list_size;

			type *s = get_type(o->op_store_access_list.to.type.type);

			for (uint16_t i = 0; i < indices_size; ++i) {
				switch (o->op_store_access_list.access_list[i].kind) {
				case ACCESS_ELEMENT:
					assert(false);
					break;
				case ACCESS_MEMBER: {
					int  member_index = 0;
					bool found        = false;

					for (; member_index < s->members.size; ++member_index) {
						if (s->members.m[member_index].name == o->op_store_access_list.access_list[i].access_member.name) {
							found = true;
							break;
						}
					}

					assert(found);

					indices[i] = member_index;

					break;
				}
				case ACCESS_SWIZZLE: {
					assert(o->op_store_access_list.access_list[i].access_swizzle.swizzle.size == 1);

					indices[i] = o->op_store_access_list.access_list[i].access_swizzle.swizzle.indices[0];

					break;
				}
				}

				s = get_type(o->op_store_access_list.access_list[i].type);
			}

			type_id access_kong_type = find_access_type(indices, indices_size, o->op_store_access_list.to.type.type);

			spirv_id access_type = {0};

			switch (o->op_store_access_list.to.kind) {
			case VARIABLE_LOCAL:
				access_type = convert_pointer_type_to_spirv_id(access_kong_type, STORAGE_CLASS_FUNCTION);
				break;
			case VARIABLE_GLOBAL:
				access_type = convert_pointer_type_to_spirv_id(access_kong_type, STORAGE_CLASS_OUTPUT);
				break;
			case VARIABLE_INTERNAL:
				assert(false);
				break;
			}

			spirv_id pointer =
			    write_op_access_chain(instructions, access_type, convert_kong_index_to_spirv_id(o->op_store_access_list.to.index), indices, indices_size);
			write_op_store(instructions, pointer, convert_kong_index_to_spirv_id(o->op_store_access_list.from.index));

			break;
		}
		case OPCODE_STORE_VARIABLE: {
			write_op_store(instructions, convert_kong_index_to_spirv_id(o->op_store_var.to.index), convert_kong_index_to_spirv_id(o->op_store_var.from.index));
			break;
		}
		case OPCODE_RETURN: {
			if (stage == SHADER_STAGE_VERTEX && main) {
				type *output_type = get_type(output);

				for (size_t i = 0; i < output_type->members.size; ++i) {
					member m = output_type->members.m[i];

					int      indices = (int)i;
					spirv_id spirv_type;
					if (m.type.type == float2_id) {
						spirv_type = spirv_float2_type;
					}
					else if (m.type.type == float3_id) {
						spirv_type = spirv_float3_type;
					}
					else if (m.type.type == float4_id) {
						spirv_type = spirv_float4_type;
					}
					else {
						debug_context context = {0};
						error(context, "Type unsupported for input in SPIR-V");
					}

					spirv_id load_pointer = write_op_access_chain(instructions, convert_pointer_type_to_spirv_id(m.type.type, STORAGE_CLASS_FUNCTION),
					                                              convert_kong_index_to_spirv_id(o->op_return.var.index), &indices, 1);
					spirv_id value        = write_op_load(instructions, spirv_type, load_pointer);

					if (i == 0) {
						// position
						spirv_id store_pointer = write_op_access_chain(instructions, convert_pointer_type_to_spirv_id(m.type.type, STORAGE_CLASS_OUTPUT),
						                                               output_vars[i], &indices, 1);
						write_op_store(instructions, store_pointer, value);
					}
					else {
						write_op_store(instructions, output_vars[i], value);
					}
				}
				write_op_return(instructions);
			}
			else if (stage == SHADER_STAGE_FRAGMENT && main) {
				if (false /*TODO*/) {
					// spirv_id object = write_op_load(instructions, convert_type_to_spirv_id(o->op_return.var.type.type),
					// convert_kong_index_to_spirv_id(o->op_return.var.index)); write_op_store(instructions, output_var, object);
				}
				else {
					spirv_id loaded;
					if (o->op_return.var.kind == VARIABLE_INTERNAL) {
						loaded = convert_kong_index_to_spirv_id(o->op_return.var.index);
					}
					else {
						loaded = write_op_load(instructions, convert_type_to_spirv_id(float4_id), convert_kong_index_to_spirv_id(o->op_return.var.index));
					}
					write_op_store(instructions, output_vars[0], loaded);
				}
				write_op_return(instructions);
			}
			ends_with_return = true;
			break;
		}
		case OPCODE_LESS: {
			spirv_id result = write_op_f_ord_less_than(instructions, spirv_bool_type, convert_kong_index_to_spirv_id(o->op_binary.left.index),
			                                           convert_kong_index_to_spirv_id(o->op_binary.right.index));
			hmput(index_map, o->op_binary.result.index, result);
			break;
		}
		case OPCODE_MULTIPLY: {
			spirv_id left;
			if (o->op_binary.left.kind != VARIABLE_INTERNAL) {
				left =
				    write_op_load(instructions, convert_type_to_spirv_id(o->op_binary.left.type.type), convert_kong_index_to_spirv_id(o->op_binary.left.index));
			}
			else {
				left = convert_kong_index_to_spirv_id(o->op_binary.left.index);
			}

			spirv_id right;
			if (o->op_binary.right.kind != VARIABLE_INTERNAL) {
				right = write_op_load(instructions, convert_type_to_spirv_id(o->op_binary.right.type.type),
				                      convert_kong_index_to_spirv_id(o->op_binary.right.index));
			}
			else {
				right = convert_kong_index_to_spirv_id(o->op_binary.right.index);
			}

			bool left_is_matrix  = o->op_binary.left.type.type == float3x3_id || o->op_binary.left.type.type == float4x4_id;
			bool right_is_matrix = o->op_binary.right.type.type == float3x3_id || o->op_binary.right.type.type == float4x4_id;

			spirv_id result;

			if (left_is_matrix && right_is_matrix) {
				result = write_op_matrix_times_matrix(instructions, convert_type_to_spirv_id(o->op_binary.result.type.type), left, right);
			}
			else if (left_is_matrix) {
				result = write_op_matrix_times_vector(instructions, convert_type_to_spirv_id(o->op_binary.result.type.type), left, right);
			}
			else if (right_is_matrix) {
				result = write_op_vector_times_matrix(instructions, convert_type_to_spirv_id(o->op_binary.result.type.type), left, right);
			}
			else {
				result = write_op_f_mul(instructions, convert_type_to_spirv_id(o->op_binary.result.type.type), left, right);
			}

			hmput(index_map, o->op_binary.result.index, result);

			break;
		}
		case OPCODE_IF: {
			write_op_selection_merge(instructions, convert_kong_index_to_spirv_id(o->op_if.end_id), SELECTION_CONTROL_NONE);

			write_op_branch_conditional(instructions, convert_kong_index_to_spirv_id(o->op_if.condition.index),
			                            convert_kong_index_to_spirv_id(o->op_if.start_id), convert_kong_index_to_spirv_id(o->op_if.end_id));

			break;
		}
		case OPCODE_WHILE_START: {
			spirv_id while_start_label    = convert_kong_index_to_spirv_id(o->op_while_start.start_id);
			spirv_id while_continue_label = convert_kong_index_to_spirv_id(o->op_while_start.continue_id);
			spirv_id while_end_label      = convert_kong_index_to_spirv_id(o->op_while_start.end_id);

			write_op_branch(instructions, while_start_label);
			write_op_label_preallocated(instructions, while_start_label);

			write_op_loop_merge(instructions, while_end_label, while_continue_label, LOOP_CONTROL_NONE);

			spirv_id loop_start_id = allocate_index();
			write_op_branch(instructions, loop_start_id);
			write_op_label_preallocated(instructions, loop_start_id);
			break;
		}
		case OPCODE_WHILE_CONDITION: {
			spirv_id while_end_label = convert_kong_index_to_spirv_id(o->op_while.end_id);

			spirv_id pass = allocate_index();

			write_op_branch_conditional(instructions, convert_kong_index_to_spirv_id(o->op_while.condition.index), pass, while_end_label);

			write_op_label_preallocated(instructions, pass);
			break;
		}
		case OPCODE_WHILE_END: {
			spirv_id while_start_label    = convert_kong_index_to_spirv_id(o->op_while_end.start_id);
			spirv_id while_continue_label = convert_kong_index_to_spirv_id(o->op_while_end.continue_id);
			spirv_id while_end_label      = convert_kong_index_to_spirv_id(o->op_while_end.end_id);

			write_op_branch(instructions, while_continue_label);
			write_op_label_preallocated(instructions, while_continue_label);

			write_op_branch(instructions, while_start_label);
			write_op_label_preallocated(instructions, while_end_label);
			break;
		}
		case OPCODE_BLOCK_START:
		case OPCODE_BLOCK_END: {
			write_op_label_preallocated(instructions, convert_kong_index_to_spirv_id(o->op_block.id));
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
		if (main) {
			// TODO
		}
		else {
			write_op_return(instructions);
		}
	}
	write_op_function_end(instructions);
}

static void write_functions(instructions_buffer *instructions, function *main, spirv_id entry_point, shader_stage stage, type_id input, type_id output) {
	write_function(instructions, main, entry_point, stage, true, input, output);
}

static void write_int_constant(struct container *container, void *data) {
	int_constant_container *int_constant = (int_constant_container *)container;
	instructions_buffer    *instructions = (instructions_buffer *)data;

	write_constant_int(instructions, int_constant->value, int_constant->container.key);
}

static void write_constants(instructions_buffer *instructions) {
	hash_map_iterate(int_constants, write_int_constant, instructions);

	size_t size = hmlenu(float_constants);
	for (size_t i = 0; i < size; ++i) {
		write_constant_float(instructions, float_constants[i].value, float_constants[i].key);
	}

	size = hmlenu(bool_constants);
	for (size_t i = 0; i < size; ++i) {
		write_constant_bool(instructions, bool_constants[i].value, bool_constants[i].key);
	}
}

static void assign_bindings(uint32_t *bindings, function *shader) {
	descriptor_set_group *set_group = get_descriptor_set_group(shader->descriptor_set_group_index);

	for (size_t group_index = 0; group_index < set_group->size; ++group_index) {
		uint32_t binding = 0;

		descriptor_set *set = set_group->values[group_index];

		if (set->name == add_name("root_constants")) {
			if (set->globals.size != 1) {
				debug_context context = {0};
				error(context, "More than one root constants struct found");
			}

			global_id g_id = set->globals.globals[0];
			global   *g    = get_global(g_id);

			if (get_type(g->type)->built_in) {
				debug_context context = {0};
				error(context, "Unsupported type for a root constant");
			}

			bindings[g_id] = binding;
			binding += 1;

			continue;
		}

		for (size_t g_index = 0; g_index < set->globals.size; ++g_index) {
			global_id global_index = set->globals.globals[g_index];
			bool      writable     = set->globals.writable[g_index];

			global *g = get_global(global_index);

			type   *t         = get_type(g->type);
			type_id base_type = t->array_size > 0 ? t->base : g->type;

			if (base_type == sampler_type_id) {
				bindings[global_index] = binding;
				binding += 1;
			}
			else if (base_type == tex2d_type_id) {
				if (t->array_size == UINT32_MAX) {
					bindings[global_index] = 0;
				}
				else if (writable) {
					bindings[global_index] = binding;
					binding += 1;
				}
				else {
					bindings[global_index] = binding;
					binding += 1;
				}
			}
			else if (base_type == texcube_type_id || base_type == tex2darray_type_id || base_type == bvh_type_id) {
				bindings[global_index] = binding;
				binding += 1;
			}
			else if (get_type(g->type)->built_in) {
				if (get_type(g->type)->array_size > 0) {
					bindings[global_index] = binding;
					binding += 1;
				}
			}
			else {
				if (get_type(g->type)->array_size > 0) {
					bindings[global_index] = binding;
					binding += 1;
				}
				else {
					bindings[global_index] = binding;
					binding += 1;
				}
			}
		}
	}
}

static void write_globals(instructions_buffer *decorations, instructions_buffer *instructions_block, function *main) {
	uint32_t bindings[512] = {0};
	assign_bindings(bindings, main);

	global_array globals = {0};

	if (main != NULL) {
		find_referenced_globals(main, &globals);
	}

	descriptor_set_group *group = find_descriptor_set_group_for_function(main);
	for (size_t descriptor_set_index = 0; descriptor_set_index < group->size; ++descriptor_set_index) {
		descriptor_set *set = group->values[descriptor_set_index];
		for (size_t global_index = 0; global_index < set->globals.size; ++global_index) {
			global_id id = set->globals.globals[global_index];
			for (size_t function_global_index = 0; function_global_index < globals.size; ++function_global_index) {
				global_id function_global_id = globals.globals[function_global_index];

				if (id == function_global_id) {
					if (set->globals.writable[global_index]) {
						globals.writable[function_global_index] = true;
					}
					break;
				}
			}
		}
	}

	for (size_t i = 0; i < globals.size; ++i) {
		global  *g       = get_global(globals.globals[i]);
		uint32_t binding = bindings[globals.globals[i]];

		type   *t         = get_type(g->type);
		type_id base_type = t->array_size > 0 ? t->base : g->type;

		if (base_type == sampler_type_id) {
			spirv_id sampler_pointer_type = write_type_pointer(instructions_block, STORAGE_CLASS_UNIFORM_CONSTANT, spirv_sampler_type);

			complex_type ct;
			ct.type    = g->type;
			ct.pointer = true;
			ct.storage = (uint16_t)STORAGE_CLASS_UNIFORM_CONSTANT;
			hmput(type_map, ct, spirv_sampler_type);

			spirv_id spirv_var_id = convert_kong_index_to_spirv_id(g->var_index);
			write_op_variable_preallocated(instructions_block, sampler_pointer_type, spirv_var_id, STORAGE_CLASS_UNIFORM_CONSTANT);

			write_op_decorate_value(decorations, spirv_var_id, DECORATION_DESCRIPTOR_SET, 0);
			write_op_decorate_value(decorations, spirv_var_id, DECORATION_BINDING, binding);
		}
		else if (base_type == tex2d_type_id) {
			if (has_attribute(&g->attributes, add_name("write"))) {
				assert(false);
			}
			else {
				if (t->array_size == UINT32_MAX) {
					assert(false);
				}
				else {
					spirv_id image_pointer_type = write_type_pointer(instructions_block, STORAGE_CLASS_UNIFORM_CONSTANT, spirv_image_type);

					complex_type ct;
					ct.type    = g->type;
					ct.pointer = true;
					ct.storage = (uint16_t)STORAGE_CLASS_UNIFORM_CONSTANT;
					hmput(type_map, ct, spirv_image_type);

					spirv_id spirv_var_id = convert_kong_index_to_spirv_id(g->var_index);
					write_op_variable_preallocated(instructions_block, image_pointer_type, spirv_var_id, STORAGE_CLASS_UNIFORM_CONSTANT);

					write_op_decorate_value(decorations, spirv_var_id, DECORATION_DESCRIPTOR_SET, 0);
					write_op_decorate_value(decorations, spirv_var_id, DECORATION_BINDING, binding);
				}
			}
		}
		else if (base_type == tex2darray_type_id) {
			assert(false);
		}
		else if (base_type == texcube_type_id) {
			assert(false);
		}
		else if (base_type == bvh_type_id) {
			assert(false);
		}
		else if (base_type == float_id) {
			assert(false);
		}
		else if (base_type == float2_id) {
			assert(false);
		}
		else if (base_type == float3_id) {
			assert(false);
		}
		else if (base_type == float4_id) {
			assert(false);
		}
		else {
			type *t = get_type(g->type);

			spirv_id member_types[256];
			uint16_t member_types_size = 0;
			for (size_t j = 0; j < t->members.size; ++j) {
				type_id member_type = t->members.m[j].type.type;

				member_types[member_types_size] = convert_type_to_spirv_id(member_type);

				spirv_id member_pointer_type = write_type_pointer(instructions_block, STORAGE_CLASS_UNIFORM, member_types[member_types_size]);

				complex_type ct;
				ct.type    = member_type;
				ct.pointer = true;
				ct.storage = (uint16_t)STORAGE_CLASS_UNIFORM;
				hmput(type_map, ct, member_pointer_type);

				member_types_size += 1;
				assert(member_types_size < 256);
			}

			spirv_id struct_type = write_type_struct(instructions_block, member_types, member_types_size);

			uint32_t offset = 0;
			for (uint32_t j = 0; j < (uint32_t)t->members.size; ++j) {
				type_id member_type = t->members.m[j].type.type;

				if (member_type == float3x3_id || member_type == float4x4_id) {
					write_op_member_decorate(decorations, struct_type, j, DECORATION_COL_MAJOR);
				}
				write_op_member_decorate_value(decorations, struct_type, j, DECORATION_OFFSET, offset);
				if (member_type == float3x3_id) {
					write_op_member_decorate_value(decorations, struct_type, j, DECORATION_MATRIX_STRIDE, 12);
					offset += 12;
				}
				if (member_type == float4x4_id) {
					write_op_member_decorate_value(decorations, struct_type, j, DECORATION_MATRIX_STRIDE, 16);
					offset += 16;
				}
			}

			complex_type ct;
			ct.type    = g->type;
			ct.pointer = false;
			ct.storage = (uint16_t)STORAGE_CLASS_NONE;
			hmput(type_map, ct, struct_type);

			spirv_id struct_pointer_type = write_type_pointer(instructions_block, STORAGE_CLASS_UNIFORM, struct_type);

			ct.type    = g->type;
			ct.pointer = true;
			ct.storage = (uint16_t)STORAGE_CLASS_UNIFORM;
			hmput(type_map, ct, struct_pointer_type);

			spirv_id spirv_var_id = convert_kong_index_to_spirv_id(g->var_index);
			write_op_variable_preallocated(instructions_block, struct_pointer_type, spirv_var_id, STORAGE_CLASS_UNIFORM);

			write_op_decorate(decorations, struct_type, DECORATION_BLOCK);
			write_op_decorate_value(decorations, spirv_var_id, DECORATION_DESCRIPTOR_SET, 0);
			write_op_decorate_value(decorations, spirv_var_id, DECORATION_BINDING, binding);
		}
	}
}

static void init_index_map(void) {
	spirv_id default_id = {0};
	hmdefault(index_map, default_id);
	size_t size = hmlenu(index_map);
	for (size_t i = 0; i < size; ++i) {
		hmdel(index_map, index_map[i].key);
	}
}

static void init_type_map(void) {
	spirv_id default_id = {0};
	hmdefault(type_map, default_id);
	size_t size = hmlenu(type_map);
	for (size_t i = 0; i < size; ++i) {
		hmdel(type_map, type_map[i].key);
	}
}

static void init_int_constants(void) {
	hash_map_destroy(int_constants);
	int_constants = hash_map_create();
}

static void init_float_constants(void) {
	spirv_id default_id = {0};
	hmdefault(float_constants, default_id);
	size_t size = hmlenu(float_constants);
	for (size_t i = 0; i < size; ++i) {
		hmdel(float_constants, float_constants[i].key);
	}
}

void init_maps(void) {
	init_index_map();
	init_type_map();
	init_int_constants();
	init_float_constants();
}

static void spirv_export_vertex(char *directory, function *main, bool debug) {
	next_index = 1;
	init_maps();

	instructions_buffer instructions = {
	    .instructions = (uint32_t *)calloc(1024 * 1024, 1),
	};

	instructions_buffer header = {
	    .instructions = (uint32_t *)calloc(1024 * 1024, 1),
	};

	instructions_buffer decorations = {
	    .instructions = (uint32_t *)calloc(1024 * 1024, 1),
	};

	instructions_buffer constants = {
	    .instructions = (uint32_t *)calloc(1024 * 1024, 1),
	};

	assert(main->parameters_size > 0);
	type_id vertex_input  = main->parameter_types[0].type;
	type_id vertex_output = main->return_type.type;

	debug_context context = {0};
	check(vertex_input != NO_TYPE, context, "vertex input missing");
	check(vertex_output != NO_TYPE, context, "vertex output missing");

	write_capabilities(&decorations);
	write_op_ext_inst_import(&decorations, "GLSL.std.450");
	write_op_memory_model(&decorations, ADDRESSING_MODEL_LOGICAL, MEMORY_MODEL_GLSL450);

	type *input  = get_type(vertex_input);
	type *output = get_type(vertex_output);

	spirv_id entry_point = allocate_index();

	input_vars_count = input->members.size;
	for (size_t input_var_index = 0; input_var_index < input_vars_count; ++input_var_index) {
		input_vars[input_var_index] = allocate_index();
	}

	output_vars_count = output->members.size;
	for (size_t output_var_index = 0; output_var_index < output_vars_count; ++output_var_index) {
		output_vars[output_var_index] = allocate_index();
	}
	per_vertex_var = output_vars[0];

	spirv_id interfaces[256];
	size_t   interfaces_count = 0;

	for (size_t input_var_index = 0; input_var_index < input_vars_count; ++input_var_index) {
		interfaces[interfaces_count] = input_vars[input_var_index];
		interfaces_count += 1;
	}

	for (size_t output_var_index = 0; output_var_index < output_vars_count; ++output_var_index) {
		interfaces[interfaces_count] = output_vars[output_var_index];
		interfaces_count += 1;
	}

	write_op_entry_point(&decorations, EXECUTION_MODEL_VERTEX, entry_point, "main", interfaces, (uint16_t)interfaces_count);

	write_vertex_input_decorations(&decorations, input_vars, (uint32_t)input_vars_count);

	write_base_types(&constants);

	write_globals(&decorations, &constants, main);

	for (size_t i = 0; i < input_vars_count; ++i) {
		member m       = input->members.m[i];
		input_types[i] = m.type.type;

		if (m.type.type == float2_id) {
			write_op_variable_preallocated(&instructions, convert_pointer_type_to_spirv_id(float2_id, STORAGE_CLASS_INPUT), input_vars[i], STORAGE_CLASS_INPUT);
		}
		else if (m.type.type == float3_id) {
			write_op_variable_preallocated(&instructions, convert_pointer_type_to_spirv_id(float3_id, STORAGE_CLASS_INPUT), input_vars[i], STORAGE_CLASS_INPUT);
		}
		else if (m.type.type == float4_id) {
			write_op_variable_preallocated(&instructions, convert_pointer_type_to_spirv_id(float4_id, STORAGE_CLASS_INPUT), input_vars[i], STORAGE_CLASS_INPUT);
		}
		else {
			debug_context context = {0};
			error(context, "Type unsupported for input in SPIR-V");
		}
	}

	spirv_id types[]       = {spirv_float4_type};
	spirv_id output_struct = write_type_struct(&constants, types, 1);
	write_vertex_output_decorations(&decorations, output_struct, output_vars, (uint32_t)output_vars_count);

	output_struct_pointer_type = write_type_pointer(&constants, STORAGE_CLASS_OUTPUT, output_struct);
	write_op_variable_preallocated(&instructions, output_struct_pointer_type, per_vertex_var, STORAGE_CLASS_OUTPUT);

	// special handling for the first one (position) via per_vertex_var
	for (size_t i = 1; i < output_vars_count; ++i) {
		member m        = output->members.m[i];
		output_types[i] = m.type.type;

		if (m.type.type == float2_id) {
			write_op_variable_preallocated(&instructions, convert_pointer_type_to_spirv_id(float2_id, STORAGE_CLASS_OUTPUT), output_vars[i],
			                               STORAGE_CLASS_OUTPUT);
		}
		else if (m.type.type == float3_id) {
			write_op_variable_preallocated(&instructions, convert_pointer_type_to_spirv_id(float3_id, STORAGE_CLASS_OUTPUT), output_vars[i],
			                               STORAGE_CLASS_OUTPUT);
		}
		else if (m.type.type == float4_id) {
			write_op_variable_preallocated(&instructions, convert_pointer_type_to_spirv_id(float4_id, STORAGE_CLASS_OUTPUT), output_vars[i],
			                               STORAGE_CLASS_OUTPUT);
		}
		else {
			debug_context context = {0};
			error(context, "Type unsupported for input in SPIR-V");
		}
	}

	write_functions(&instructions, main, entry_point, SHADER_STAGE_VERTEX, vertex_input, vertex_output);

	write_types(&constants, main);

	// header
	write_magic_number(&header);
	write_version_number(&header);
	write_generator_magic_number(&header);
	write_bound(&header);
	write_instruction_schema(&header);

	write_constants(&constants);

	char *name = get_name(main->name);

	char filename[512];
	sprintf(filename, "kong_%s", name);

	char var_name[256];
	sprintf(var_name, "%s_code", name);

	write_bytecode(directory, filename, var_name, &header, &decorations, &constants, &instructions, debug);
}

static void spirv_export_fragment(char *directory, function *main, bool debug) {
	next_index = 1;
	init_maps();

	instructions_buffer instructions = {
	    .instructions = (uint32_t *)calloc(1024 * 1024, 1),
	};

	instructions_buffer header = {
	    .instructions = (uint32_t *)calloc(1024 * 1024, 1),
	};

	instructions_buffer decorations = {
	    .instructions = (uint32_t *)calloc(1024 * 1024, 1),
	};

	instructions_buffer constants = {
	    .instructions = (uint32_t *)calloc(1024 * 1024, 1),
	};

	assert(main->parameters_size > 0);
	type_id pixel_input  = main->parameter_types[0].type;
	type_id pixel_output = main->return_type.type;

	debug_context context = {0};
	check(pixel_input != NO_TYPE, context, "fragment input missing");
	check(pixel_output != NO_TYPE, context, "fragment output missing");

	write_capabilities(&decorations);
	write_op_ext_inst_import(&decorations, "GLSL.std.450");
	write_op_memory_model(&decorations, ADDRESSING_MODEL_LOGICAL, MEMORY_MODEL_GLSL450);

	type *input  = get_type(pixel_input);
	type *output = get_type(pixel_output);

	spirv_id entry_point = allocate_index();

	input_vars_count = input->members.size - 1; // jump over pos input
	for (size_t input_var_index = 0; input_var_index < input_vars_count; ++input_var_index) {
		input_vars[input_var_index] = allocate_index();
	}

	if (output->built_in) {
		output_vars_count = 1;
		output_vars[0]    = allocate_index();
	}
	else {
		output_vars_count = output->members.size;
		for (size_t output_var_index = 0; output_var_index < output_vars_count; ++output_var_index) {
			output_vars[output_var_index] = allocate_index();
		}
	}

	spirv_id interfaces[256];
	size_t   interfaces_count = 0;

	for (size_t input_var_index = 0; input_var_index < input_vars_count; ++input_var_index) {
		interfaces[interfaces_count] = input_vars[input_var_index];
		interfaces_count += 1;
	}

	for (size_t output_var_index = 0; output_var_index < output_vars_count; ++output_var_index) {
		interfaces[interfaces_count] = output_vars[output_var_index];
		interfaces_count += 1;
	}

	write_op_entry_point(&decorations, EXECUTION_MODEL_FRAGMENT, entry_point, "main", interfaces, (uint16_t)interfaces_count);

	write_op_execution_mode(&decorations, entry_point, EXECUTION_MODE_ORIGIN_UPPER_LEFT);

	write_fragment_input_decorations(&decorations, input_vars, (uint32_t)input_vars_count);

	write_base_types(&constants);

	write_globals(&decorations, &constants, main);

	for (size_t i = 0; i < input_vars_count; ++i) {
		member m       = input->members.m[i + 1]; // jump over pos input
		input_types[i] = m.type.type;

		if (m.type.type == float2_id) {
			write_op_variable_preallocated(&instructions, convert_pointer_type_to_spirv_id(float2_id, STORAGE_CLASS_INPUT), input_vars[i], STORAGE_CLASS_INPUT);
		}
		else if (m.type.type == float3_id) {
			write_op_variable_preallocated(&instructions, convert_pointer_type_to_spirv_id(float3_id, STORAGE_CLASS_INPUT), input_vars[i], STORAGE_CLASS_INPUT);
		}
		else if (m.type.type == float4_id) {
			write_op_variable_preallocated(&instructions, convert_pointer_type_to_spirv_id(float4_id, STORAGE_CLASS_INPUT), input_vars[i], STORAGE_CLASS_INPUT);
		}
		else {
			debug_context context = {0};
			error(context, "Type unsupported for input in SPIR-V");
		}
	}

	write_fragment_output_decorations(&decorations, output_vars[0]);

	write_op_variable_preallocated(&instructions, convert_pointer_type_to_spirv_id(float4_id, STORAGE_CLASS_OUTPUT), output_vars[0], STORAGE_CLASS_OUTPUT);

	write_functions(&instructions, main, entry_point, SHADER_STAGE_FRAGMENT, pixel_input, NO_TYPE);

	write_types(&constants, main);

	// header
	write_magic_number(&header);
	write_version_number(&header);
	write_generator_magic_number(&header);
	write_bound(&header);
	write_instruction_schema(&header);

	write_constants(&constants);

	char *name = get_name(main->name);

	char filename[512];
	sprintf(filename, "kong_%s", name);

	char var_name[256];
	sprintf(var_name, "%s_code", name);

	write_bytecode(directory, filename, var_name, &header, &decorations, &constants, &instructions, debug);
}

void spirv_export(char *directory, bool debug) {
	function *vertex_shaders[256];
	size_t    vertex_shaders_size = 0;

	function *fragment_shaders[256];
	size_t    fragment_shaders_size = 0;

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
			name_id vertex_shader_name   = NO_NAME;
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
		input_vars_count = 0;
		spirv_export_vertex(directory, vertex_shaders[i], debug);
	}

	for (size_t i = 0; i < fragment_shaders_size; ++i) {
		input_vars_count = 0;
		spirv_export_fragment(directory, fragment_shaders[i], debug);
	}
}
