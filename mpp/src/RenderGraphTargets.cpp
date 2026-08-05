#include <glew/glew.h>

#include "mpp/RenderGraphTargets.h"

#include "mpp/MppException.h"
#include "mpp/RenderSystem.h"
#include "mpp/RenderTextureStream.h"

using namespace std;

namespace mpp
{
	namespace
	{
		RenderTextureOptions makeOptions(GraphImageDesc const& desc)
		{
			if (desc.samples != 1 || desc.mipLevels != 1)
			{
				THROW_MPP("Render graph target allocation currently supports only one sample and one mip level.", __LINE__, __FILE__, __func__);
			}

			RenderTextureOptions options;
			options.params = desc.params;
			options.params.colourSpace = desc.colourSpace;
			switch (desc.format)
			{
			case GraphImageFormat::Rgba8:
				options.colourType = TextureInternalType::UnsignedInteger;
				options.colourNormalised = true;
				options.colourBitSize = 8;
				options.colourChannels = 4;
				break;
			case GraphImageFormat::Rgba16f:
				options.colourType = TextureInternalType::Float;
				options.colourNormalised = false;
				options.colourBitSize = 16;
				options.colourChannels = 4;
				break;
			case GraphImageFormat::Rg16f:
				options.colourType = TextureInternalType::Float;
				options.colourNormalised = false;
				options.colourBitSize = 16;
				options.colourChannels = 2;
				break;
			case GraphImageFormat::Depth24:
				options.numAttachments = 0;
				options.depthAttachment = RenderTextureDepthAttachment::DepthTexture;
				break;
			case GraphImageFormat::Depth24Stencil8:
				options.numAttachments = 0;
				options.depthAttachment = RenderTextureDepthAttachment::DepthStencilTexture;
				break;
			}
			return options;
		}
	}

	RenderGraphTargets::RenderGraphTargets(RenderSystem* renderSystem)
		: mRenderSystem(renderSystem)
	{
		if (!mRenderSystem)
		{
			THROW_MPP("Render graph targets require a RenderSystem.", __LINE__, __FILE__, __func__);
		}
	}

	uint64_t RenderGraphTargets::makeKey(GraphImageHandle image)
	{
		return ((uint64_t)image.id << 32) | image.version;
	}

	void RenderGraphTargets::allocate(RenderGraphAllocationPlan const& plan)
	{
		if (!plan.valid)
		{
			THROW_MPP("Cannot allocate an invalid render graph allocation plan.", __LINE__, __FILE__, __func__);
		}
		clear();
		for (auto const& lifetime : plan.allocatedImages)
		{
			auto const options = makeOptions(lifetime.desc);
			string const name = "RenderGraph_Image" + to_string(lifetime.image.id) + "_v" + to_string(lifetime.image.version);
			mTargets.emplace(makeKey(lifetime.image), mRenderSystem->createRenderTexture(name, lifetime.size.x, lifetime.size.y, options));
		}
	}

	void RenderGraphTargets::clear()
	{
		mTargets.clear();
	}

	RenderTargetPtr RenderGraphTargets::get(GraphImageHandle image) const
	{
		auto const found = mTargets.find(makeKey(image));
		return found == mTargets.end() ? nullptr : found->second;
	}
}
