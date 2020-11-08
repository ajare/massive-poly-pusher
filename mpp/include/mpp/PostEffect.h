#pragma once

#include <vector>

#include "mpp/Resource.h"
#include "mpp/RenderTarget.h"

namespace mpp
{
	class _MPPAPI PostEffect : public Resource
	{
		enum class ImageType
		{
			Colour,
			Depth,
			Stencil
		};

		struct Input
		{
			ImageType type;
			RenderTargetPtr target;
			uint32_t attachment;
		};

		struct Output
		{
			ImageType type;
			RenderTargetPtr target;
		};

	private:

		std::vector<Input> mInputs;

		Output mOutput;

	protected:

		void createImpl();

		void destroyImpl();

		void loadImpl();

		void unloadImpl();

	public:

		PostEffect(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		RenderTargetPtr getOuputRenderTarget();
	};

}
