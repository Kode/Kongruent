#include "Translator.h"

using namespace krafix;

Instruction::Instruction(std::vector<unsigned> &spirv, unsigned &index) {
	int wordCount = spirv[index] >> 16;
	opcode        = (Opcode)(spirv[index] & 0xffff);

	operands = wordCount > 1 ? &spirv[index + 1] : NULL;
	length   = wordCount - 1;

	switch (opcode) {
	case OpString:
		string = (char *)&spirv[index + 2];
		break;
	case OpName:
		string = (char *)&spirv[index + 2];
		break;
	case OpMemberName:
		string = (char *)&spirv[index + 3];
		break;
	case OpEntryPoint:
		string = (char *)&spirv[index + 3];
		break;
	case OpSourceExtension:
		string = (char *)&spirv[index + 1];
		break;
	default:
		string = NULL;
		break;
	}

	index += wordCount;
}

Instruction::Instruction(int opcode, unsigned *operands, unsigned length) : opcode((Opcode)opcode), operands(operands), length(length), string(NULL) {}

Translator::Translator(std::vector<unsigned> &spirv, ShaderStage stage) : stage(stage), spirv(spirv) {
	if (spirv.size() < 5) {
		return;
	}

	unsigned index = 0;
	magicNumber    = spirv[index++];
	version        = spirv[index++];
	generator      = spirv[index++];
	bound          = spirv[index++];
	schema         = spirv[index++];

	while (index < spirv.size()) {
		instructions.push_back(Instruction(spirv, index));
	}

	// printf("Read %i instructions.\n", instructions.size());
}

ExecutionModel Translator::executionModel() {
	switch (stage) {
	case StageVertex:
		return ExecutionModelVertex;
	case StageTessControl:
		return ExecutionModelTessellationControl;
	case StageTessEvaluation:
		return ExecutionModelTessellationEvaluation;
	case StageGeometry:
		return ExecutionModelGeometry;
	case StageFragment:
		return ExecutionModelFragment;
	case StageCompute:
		return ExecutionModelGLCompute;
	default:
		throw "Unknown shader stage";
	}
}
