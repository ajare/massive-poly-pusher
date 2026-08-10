#include <glew/glew.h>

#include <algorithm>
#include <vector>

#include "mpp/RenderGraphTargets.h"

#include "mpp/MppException.h"
#include "mpp/RenderGraphImportRegistry.h"
#include "mpp/RenderSystem.h"
#include "mpp/RenderTexture.h"
#include "mpp/RenderTextureStream.h"

using namespace std;

namespace mpp
{
	namespace
	{
		RenderTextureOptions makeOptions(GraphImageDesc const& desc)
		{
			RenderTextureOptions options;
			options.params = desc.params;
			options.params.colourSpace = desc.colourSpace;
			options.params.useMipmaps = desc.mipLevels > 1;
			options.params.lodBaseLevel = 0;
			options.params.lodMaxLevel = (int32_t)desc.mipLevels - 1;
			switch (desc.format)
			{
			case GraphImageFormat::R8: options.colourBitSize = 8; options.colourChannels = 1; break;
			case GraphImageFormat::Rg8: options.colourBitSize = 8; options.colourChannels = 2; break;
			case GraphImageFormat::Rgba8: options.colourBitSize = 8; options.colourChannels = 4; break;
			case GraphImageFormat::Srgb8Alpha8: options.colourInternalFormat = GL_SRGB8_ALPHA8; break;
			case GraphImageFormat::R16f: options.colourType = TextureInternalType::Float; options.colourNormalised = false; options.colourBitSize = 16; options.colourChannels = 1; break;
			case GraphImageFormat::Rg16f: options.colourType = TextureInternalType::Float; options.colourNormalised = false; options.colourBitSize = 16; options.colourChannels = 2; break;
			case GraphImageFormat::Rgba16f: options.colourType = TextureInternalType::Float; options.colourNormalised = false; options.colourBitSize = 16; options.colourChannels = 4; break;
			case GraphImageFormat::R32f: options.colourType = TextureInternalType::Float; options.colourNormalised = false; options.colourBitSize = 32; options.colourChannels = 1; break;
			case GraphImageFormat::Rg32f: options.colourType = TextureInternalType::Float; options.colourNormalised = false; options.colourBitSize = 32; options.colourChannels = 2; break;
			case GraphImageFormat::Rgba32f: options.colourType = TextureInternalType::Float; options.colourNormalised = false; options.colourBitSize = 32; options.colourChannels = 4; break;
			case GraphImageFormat::R11g11b10f: options.colourInternalFormat = GL_R11F_G11F_B10F; break;
			case GraphImageFormat::Rgb10a2: options.colourInternalFormat = GL_RGB10_A2; break;
			case GraphImageFormat::Depth16: options.numAttachments = 0; options.depthAttachment = RenderTextureDepthAttachment::DepthTexture; options.depthFormat = RenderTextureDepthFormat::Depth16; options.depthParams.params = options.params; break;
			case GraphImageFormat::Depth24: options.numAttachments = 0; options.depthAttachment = RenderTextureDepthAttachment::DepthTexture; options.depthFormat = RenderTextureDepthFormat::Depth24; options.depthParams.params = options.params; break;
			case GraphImageFormat::Depth32f: options.numAttachments = 0; options.depthAttachment = RenderTextureDepthAttachment::DepthTexture; options.depthFormat = RenderTextureDepthFormat::Depth32f; options.depthParams.params = options.params; break;
			case GraphImageFormat::Depth24Stencil8: options.numAttachments = 0; options.depthAttachment = RenderTextureDepthAttachment::DepthStencilTexture; options.depthFormat = RenderTextureDepthFormat::Depth24Stencil8; options.depthParams.params = options.params; break;
			case GraphImageFormat::Depth32fStencil8: options.numAttachments = 0; options.depthAttachment = RenderTextureDepthAttachment::DepthStencilTexture; options.depthFormat = RenderTextureDepthFormat::Depth32fStencil8; options.depthParams.params = options.params; break;
			}
			return options;
		}
	}

	RenderTextureOptions makeGraphRenderTextureOptions(GraphImageDesc const& desc)
	{
		return makeOptions(desc);
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

	void RenderGraphTargets::allocate(RenderGraphAllocationPlan const& plan){allocatePhysical(plan,1);}

	void RenderGraphTargets::allocatePhysical(RenderGraphAllocationPlan const& plan,uint32_t samples)
	{
		if (!plan.valid)
		{
			THROW_MPP("Cannot allocate an invalid render graph allocation plan.", __LINE__, __FILE__, __func__);
		}
		map<uint64_t,RenderTargetPtr> candidateTargets;
		map<uint64_t,RenderTargetPtr> candidateWriteTargets;
		auto candidatePool=mPool;
		struct Assignment
		{
			uint32_t firstPass;
			uint32_t lastPass;
			bool transient;
		};
		vector<GraphImageLifetime const*> lifetimes;
		lifetimes.reserve(plan.allocatedImages.size());
		for (auto const& lifetime : plan.allocatedImages) lifetimes.push_back(&lifetime);
		sort(lifetimes.begin(), lifetimes.end(), [](GraphImageLifetime const* left, GraphImageLifetime const* right)
		{
			return left->firstPass < right->firstPass;
		});
		vector<vector<Assignment>> assignments(candidatePool.size());
		auto intervalsOverlap = [](Assignment const& left, GraphImageLifetime const& right)
		{
			return !(left.lastPass < right.firstPass || right.lastPass < left.firstPass);
		};
		for (auto const* lifetime : lifetimes)
		{
			bool rasterAttachment=hasGraphImageUsage(lifetime->desc.usage,GraphImageUsage::ColourAttachment)||hasGraphImageUsage(lifetime->desc.usage,GraphImageUsage::DepthAttachment);uint32_t physicalSamples=rasterAttachment?samples:1;if(physicalSamples>1&&lifetime->desc.mipLevels>1)THROW_MPP("Multisampled graph attachment '"+lifetime->debugName+"' cannot declare mip levels.",__LINE__,__FILE__,__func__);
			size_t poolIndex = SIZE_MAX;
			for (size_t index = 0; index < candidatePool.size(); ++index)
			{
				if (candidatePool[index].samples!=physicalSamples||!graphImagesCanAlias(candidatePool[index].lifetime, *lifetime)) continue;
				auto const& used = assignments[index];
				if (used.empty())
				{
					poolIndex = index; // Cross-frame reuse, not same-frame aliasing.
					break;
				}
				if (!lifetime->desc.transient) continue;
				bool safe = all_of(used.begin(), used.end(), [&](Assignment const& previous)
				{
					return previous.transient && !intervalsOverlap(previous, *lifetime);
				});
				if (safe)
				{
					poolIndex = index;
					break;
				}
			}
			if (poolIndex == SIZE_MAX)
			{
				string const name = "RenderGraph." + (lifetime->debugName.empty() ? "Image" + to_string(lifetime->image.id) : lifetime->debugName) + ".v" + to_string(lifetime->image.version);auto options=makeGraphRenderTextureOptions(lifetime->desc);
				auto target=mRenderSystem->createRenderTexture(name+".Resolved",lifetime->size.x,lifetime->size.y,options);auto writeTarget=physicalSamples>1?mRenderSystem->createPhysicalRenderTexture(name+".MSAA"+to_string(physicalSamples),lifetime->size.x,lifetime->size.y,options,physicalSamples):target;
				candidatePool.push_back({ *lifetime, target, writeTarget,physicalSamples });
				assignments.emplace_back();
				poolIndex = candidatePool.size() - 1;
			}
			for (auto const& previous : assignments[poolIndex])
			{
				if (intervalsOverlap(previous, *lifetime))
					THROW_MPP("Render graph allocator attempted to alias overlapping image lifetimes.", __LINE__, __FILE__, __func__);
			}
			assignments[poolIndex].push_back({ lifetime->firstPass, lifetime->lastPass, lifetime->desc.transient });
			candidateTargets.emplace(makeKey(lifetime->image), candidatePool[poolIndex].target);
			candidateWriteTargets.emplace(makeKey(lifetime->image), candidatePool[poolIndex].writeTarget);
		}
		vector<PoolEntry> activePool;for(size_t index=0;index<candidatePool.size();++index)if(!assignments[index].empty())activePool.push_back(std::move(candidatePool[index]));
		mTargets.swap(candidateTargets);mWriteTargets.swap(candidateWriteTargets);mPool.swap(activePool);
	}

	void RenderGraphTargets::bindImported(GraphImageHandle image, RenderTargetPtr target)
	{
		if (!image.isValid() || !target)
		{
			THROW_MPP("Render graph import requires a valid image handle and target.", __LINE__, __FILE__, __func__);
		}
		mImportedTargets[image.id] = target;
	}

	void RenderGraphTargets::bindImports(RenderGraph const& graph, RenderGraphImportRegistry const& imports)
	{
		for (auto const image : graph.getImportedImages())
		{
			auto const info = graph.getImageInfo(image);
			if (info.importName.empty())
			{
				THROW_MPP("External render graph image '" + info.name + "' has no import name.", __LINE__, __FILE__, __func__);
			}
			auto target = imports.findImport(info.importName);
			if (!target)
			{
				THROW_MPP("No target registered for render graph import '" + info.importName + "'.", __LINE__, __FILE__, __func__);
			}
			bindImported(image, target);
		}
	}

	void RenderGraphTargets::clear()
	{
		mTargets.clear();
		mWriteTargets.clear();
		mImportedTargets.clear();
		mPool.clear();
	}

	RenderTargetPtr RenderGraphTargets::get(GraphImageHandle image) const
	{
		auto const found = mTargets.find(makeKey(image));
		if (found != mTargets.end()) return found->second;
		auto const imported = mImportedTargets.find(image.id);
		return imported == mImportedTargets.end() ? nullptr : imported->second;
	}

	RenderTargetPtr RenderGraphTargets::getWriteTarget(GraphImageHandle image) const
	{
		auto const found = mWriteTargets.find(makeKey(image));
		return found == mWriteTargets.end() ? get(image) : found->second;
	}

	bool RenderGraphTargets::resolve(GraphImageHandle image, bool depth) const
	{
		auto source = dynamic_cast<RenderTexture*>(getWriteTarget(image).get());
		auto destination = dynamic_cast<RenderTexture*>(get(image).get());
		if (source && destination && source != destination && source->isMultisampled())
		{
			source->resolveTo(destination, !depth, depth);
			return true;
		}
		return false;
	}
}
