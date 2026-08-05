#include <glew/glew.h>

#include <algorithm>
#include <vector>

#include "mpp/RenderGraphTargets.h"

#include "mpp/MppException.h"
#include "mpp/RenderSystem.h"
#include "mpp/RenderTextureStream.h"

using namespace std;

namespace mpp
{
	namespace
	{
		bool compatibleForAliasing(GraphImageLifetime const& left, GraphImageLifetime const& right)
		{
			return left.size == right.size && left.desc.format == right.desc.format &&
				left.desc.samples == right.desc.samples && left.desc.mipLevels == right.desc.mipLevels &&
				left.desc.colourSpace == right.desc.colourSpace &&
				left.desc.params.minFilter == right.desc.params.minFilter && left.desc.params.magFilter == right.desc.params.magFilter &&
				left.desc.params.wrap == right.desc.params.wrap && left.desc.params.useMipmaps == right.desc.params.useMipmaps &&
				left.desc.params.lodBaseLevel == right.desc.params.lodBaseLevel && left.desc.params.lodMaxLevel == right.desc.params.lodMaxLevel &&
				left.desc.params.lodBias == right.desc.params.lodBias && left.desc.params.maxAnisotropy == right.desc.params.maxAnisotropy;
		}

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
		mTargets.clear();
		struct PoolEntry
		{
			GraphImageLifetime lifetime;
			RenderTargetPtr target;
		};
		vector<PoolEntry> pool;
		for (auto const& lifetime : plan.allocatedImages)
		{
			auto reusable = find_if(pool.begin(), pool.end(), [&](PoolEntry const& candidate)
			{
				return candidate.lifetime.lastPass < lifetime.firstPass && compatibleForAliasing(candidate.lifetime, lifetime);
			});
			if (reusable != pool.end())
			{
				reusable->lifetime.lastPass = lifetime.lastPass;
				mTargets.emplace(makeKey(lifetime.image), reusable->target);
				continue;
			}
			auto const options = makeOptions(lifetime.desc);
			string const name = "RenderGraph_Image" + to_string(lifetime.image.id) + "_v" + to_string(lifetime.image.version);
			auto target = mRenderSystem->createRenderTexture(name, lifetime.size.x, lifetime.size.y, options);
			pool.push_back({ lifetime, target });
			mTargets.emplace(makeKey(lifetime.image), move(target));
		}
	}

	void RenderGraphTargets::bindImported(GraphImageHandle image, RenderTargetPtr target)
	{
		if (!image.isValid() || !target)
		{
			THROW_MPP("Render graph import requires a valid image handle and target.", __LINE__, __FILE__, __func__);
		}
		mImportedTargets[image.id] = target;
	}

	void RenderGraphTargets::clear()
	{
		mTargets.clear();
		mImportedTargets.clear();
	}

	RenderTargetPtr RenderGraphTargets::get(GraphImageHandle image) const
	{
		auto const found = mTargets.find(makeKey(image));
		if (found != mTargets.end()) return found->second;
		auto const imported = mImportedTargets.find(image.id);
		return imported == mImportedTargets.end() ? nullptr : imported->second;
	}
}
