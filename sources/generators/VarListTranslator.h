#pragma once

#include "Translator.h"

namespace krafix {
	class VarListTranslator : public Translator {
	public:
		VarListTranslator(std::vector<unsigned>& spirv, ShaderStage stage) : Translator(spirv, stage) {}
		void outputCode(const Target& target, const char* sourcefilename, const char* filename, char* output, std::map<std::string, int>& attributes) override;
		void print();
	};
}
