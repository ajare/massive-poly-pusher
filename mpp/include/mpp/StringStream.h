#pragma once

#include "mpp/ResourceStream.h"

namespace mpp
{
	class _MPPAPI StringStream : public ResourceStream
	{
	public:

		explicit StringStream(ResourceManager* resourceMgr);

		std::string getType();

		virtual std::string getData() const = 0;
	};
}