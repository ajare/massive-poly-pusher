#pragma once

#include <memory>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/vec3.hpp>
#pragma warning(pop)

#include "mpp/Config.h"

namespace mpp
{
	class BatchDataProvider
	{
		size_t mNumPrimitives{ 0 };

	public:

		virtual void getBounds(glm::vec3& bMin, glm::vec3& bMax) = 0;

		virtual bool update(float frameTime)
		{
			MPP_UNUSED(frameTime);
			return false;
		}

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