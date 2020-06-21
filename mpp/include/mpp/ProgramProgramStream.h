#pragma once

#include <vector>
#include <fstream>

#include "mpp/program/Parser.h"
#include "mpp/program/Attribute.h"

#include "mpp/ProgramStream.h"

namespace mpp
{
	class _MPPAPI ProgramProgramStream : public ProgramStream
	{
		program::Parser* mParser{ nullptr };

	private:

		void loadImpl();

	public:

		explicit ProgramProgramStream(program::Parser* parser);

		std::string getType();

		std::vector<program::Attribute> getInAttributes() const;

		std::vector<std::string> getUniforms() const;

		std::vector<std::string> getTextures() const;
	};
}