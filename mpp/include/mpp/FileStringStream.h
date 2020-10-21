#pragma once

#include "mpp/StringStream.h"

namespace mpp
{
	class _MPPAPI FileStringStream : public StringStream
	{
		std::string mFilename;

		std::string mData;

	private:

		void loadImpl();

		void unloadImpl();

	public:

		FileStringStream(ResourceManager* resourceMgr, std::string const& filename);

		std::string getData() const;
	};
}
