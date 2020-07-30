#pragma once

#include <vector>
#include <fstream>
#include <memory>

#include "mpp/program/Parser.h"
#include "mpp/program/Attribute.h"

#include "mpp/ProgramStream.h"

namespace mpp
{
	class _MPPAPI ProgramProgramStream : public ProgramStream
	{
		std::shared_ptr<program::Parser> mParser;

		std::set<std::string> mAttribs;

	private:

		void loadImpl();

	public:

		ProgramProgramStream(std::shared_ptr<program::Parser> parser, std::set<std::string> const& attribs);

		std::string getType();

		std::vector<program::Attribute> getInAttributes() const;

		std::vector<std::string> getUniforms() const;

		std::vector<std::string> getTextures() const;
	};
}