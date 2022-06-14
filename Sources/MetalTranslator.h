#pragma once

#include "CStyleTranslator.h"

namespace krafix {
	class MetalTranslator : public CStyleTranslator {
	public:
		MetalTranslator(std::vector<unsigned> &spirv, ShaderStage stage) : CStyleTranslator(spirv, stage) {}
		void outputCode(const Target &target, const char *sourcefilename, const char *filename, std::map<std::string, int> &attributes);
		void outputInstruction(const Target &target, std::map<std::string, int> &attributes, Instruction &inst);

	protected:
		const char *builtInName(BuiltIn builtin);
		std::string builtInTypeName(Variable &variable);

		std::string name;
		std::string positionName = "position";
	};
}
