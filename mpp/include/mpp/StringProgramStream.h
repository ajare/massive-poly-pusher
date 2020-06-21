#pragma once

#include "mpp/ProgramStream.h"

namespace mpp
{
	class _MPPAPI StringProgramStream : public ProgramStream
	{
		void loadImpl();

	public:

		StringProgramStream(std::string const& vertString, std::string const& fragString);
	};
}