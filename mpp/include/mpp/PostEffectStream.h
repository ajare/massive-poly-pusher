#pragma once

#include <string>
#include <functional>

#include "mpp/ResourceStream.h"

namespace mpp
{
	class _MPPAPI PostEffectStream : public ResourceStream
	{
	private:

		void loadImpl();

	public:

		explicit PostEffectStream(ResourceManager* resourceMgr);

		virtual ~PostEffectStream() = default;

	};
}