#pragma once

#include <memory>

#include "mpp/Config.h"

namespace mpp
{
	class BatchDataProvider
	{
		size_t mNumPrimitives{ 0 };

	public:

		virtual void update(float frameTime) {}

		void setNumPrimitives(size_t numPrimitives)
		{
			mNumPrimitives = numPrimitives;
		}

		size_t getNumPrimitives() const
		{
			return mNumPrimitives;
		}
	};

	typedef std::shared_ptr<BatchDataProvider> BatchDataProviderPtr;
}