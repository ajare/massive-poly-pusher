#pragma once

#include "mpp/program/Parser.h"

#include "mpp/ProgramStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticProgramStream : public ProgramStream
	{
	public:

		explicit ProgrammaticProgramStream(ResourceManager* resourceMgr);

		void setParser(std::shared_ptr<program::Parser> parser, uint32_t quality = 0);

		void setAttribs(std::set<std::string> const& attribs, uint32_t quality = 0);
	};
}