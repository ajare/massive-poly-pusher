#pragma once

#include "mpp/StringStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticStringStream : public StringStream
	{
		void loadImpl();

	public:

		explicit ProgrammaticStringStream(ResourceManager* resourceMgr);

		void setString(std::string const& data, uint32_t quality = 0);

		void setFile(std::string const& filepath, uint32_t quality = 0);
	};
}