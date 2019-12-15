#pragma once

#include "mpp/ProgramStream.h"

namespace mpp
{
	class _MPPAPI FileProgramStream : public ProgramStream
	{
		std::string mVertFile, mFragFile;

	private:

		void loadImpl();

	public:

		FileProgramStream(std::string const& vertFile, std::string const& fragFile);
	};
}