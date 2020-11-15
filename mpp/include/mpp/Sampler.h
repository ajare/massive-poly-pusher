#pragma once

#include "mpp/Resource.h"
#include "mpp/SamplerParams.h"

namespace mpp
{
	class _MPPAPI Sampler : public Resource
	{
		SamplerParams mParams;

	protected:

		void createImpl();

		void destroyImpl();

		void loadImpl();

		void unloadImpl();

	public:

		Sampler(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		void bind(uint32_t unit);
	};

}
