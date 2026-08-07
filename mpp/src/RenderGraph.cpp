#include <algorithm>
#include <queue>
#include <set>
#include <sstream>

#include "mpp/RenderGraph.h"
#include "mpp/Caps.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{
	namespace
	{
		bool isDepthFormat(GraphImageFormat format)
		{
			return format == GraphImageFormat::Depth16 || format == GraphImageFormat::Depth24 ||
				format == GraphImageFormat::Depth32f || format == GraphImageFormat::Depth24Stencil8 ||
				format == GraphImageFormat::Depth32fStencil8;
		}

		char const* formatName(GraphImageFormat format)
		{
			switch (format)
			{
			case GraphImageFormat::R8: return "R8"; case GraphImageFormat::Rg8: return "RG8"; case GraphImageFormat::Rgba8: return "RGBA8";
			case GraphImageFormat::Srgb8Alpha8: return "SRGB8_ALPHA8"; case GraphImageFormat::R16f: return "R16F";
			case GraphImageFormat::Rg16f: return "RG16F"; case GraphImageFormat::Rgba16f: return "RGBA16F";
			case GraphImageFormat::R32f: return "R32F"; case GraphImageFormat::Rg32f: return "RG32F"; case GraphImageFormat::Rgba32f: return "RGBA32F";
			case GraphImageFormat::R11g11b10f: return "R11G11B10F"; case GraphImageFormat::Rgb10a2: return "RGB10_A2";
			case GraphImageFormat::Depth16: return "DEPTH16"; case GraphImageFormat::Depth24: return "DEPTH24"; case GraphImageFormat::Depth32f: return "DEPTH32F";
			case GraphImageFormat::Depth24Stencil8: return "DEPTH24_STENCIL8"; case GraphImageFormat::Depth32fStencil8: return "DEPTH32F_STENCIL8";
			}
			return "UNKNOWN";
		}

		uint32_t formatBits(GraphImageFormat format)
		{
			switch (format)
			{
			case GraphImageFormat::R8: return 8; case GraphImageFormat::Rg8: return 16; case GraphImageFormat::Rgba8: return 32;
			case GraphImageFormat::Srgb8Alpha8: return 32;
			case GraphImageFormat::R16f: return 16; case GraphImageFormat::Rg16f: return 32; case GraphImageFormat::Rgba16f: return 64;
			case GraphImageFormat::R32f: return 32; case GraphImageFormat::Rg32f: return 64; case GraphImageFormat::Rgba32f: return 128;
			case GraphImageFormat::R11g11b10f: return 32; case GraphImageFormat::Rgb10a2: return 32;
			case GraphImageFormat::Depth16: return 16; case GraphImageFormat::Depth24: return 24; case GraphImageFormat::Depth32f: return 32;
			case GraphImageFormat::Depth24Stencil8: return 32; case GraphImageFormat::Depth32fStencil8: return 64;
			}
			return 0;
		}

		bool aliasCompatible(GraphImageLifetime const& left, GraphImageLifetime const& right)
		{
			return left.size == right.size && left.desc.format == right.desc.format &&
				left.desc.samples == right.desc.samples && left.desc.mipLevels == right.desc.mipLevels &&
				left.desc.colourSpace == right.desc.colourSpace &&
				left.desc.params.minFilter == right.desc.params.minFilter && left.desc.params.magFilter == right.desc.params.magFilter &&
				left.desc.params.wrap == right.desc.params.wrap;
		}

		char const* passTypeName(GraphPassType type)
		{
			switch (type) { case GraphPassType::Scene: return "scene"; case GraphPassType::Fullscreen: return "fullscreen"; default: return "present"; }
		}
	}

	char const* graphImageFormatName(GraphImageFormat format)
	{
		return formatName(format);
	}

	struct RenderGraph::Image
	{
		string name;
		GraphImageDesc desc;
		string importName;
		uint32_t latestVersion{ 0 };
		// Producer pass per version. UINT32_MAX means imported/external version 0.
		vector<uint32_t> producers{ UINT32_MAX };
		vector<string> valueIds;
	};

	struct RenderGraph::Pass
	{
		string name;
		bool enabled{ true };
		GraphPassType type{ GraphPassType::Scene };
		string callbackFactory;
		string programResource;
		vector<GraphImageHandle> sampledInputs;
		vector<GraphSamplerBinding> samplerBindings;
		UniformCollection parameters;
		vector<GraphColourOutput> colourOutputs;
		vector<GraphDepthOutput> depthOutputs;
		GraphRasterState rasterState;
	};

	RenderGraph::RenderGraph() = default;
	RenderGraph::~RenderGraph() = default;
	RenderGraph::RenderGraph(RenderGraph&&) noexcept = default;
	RenderGraph& RenderGraph::operator =(RenderGraph&&) noexcept = default;
	RenderGraph::RenderGraph(RenderGraph const&) = default;
	RenderGraph& RenderGraph::operator =(RenderGraph const&) = default;

	bool RenderGraph::validImage(GraphImageHandle image) const
	{
		return image.isValid() && image.id < mImages.size() && image.version < mImages[image.id].producers.size();
	}

	bool RenderGraph::validPass(GraphPassHandle pass) const
	{
		return pass.isValid() && pass.id < mPasses.size();
	}

	GraphImageHandle RenderGraph::createImage(string const& name, GraphImageDesc const& desc)
	{
		bool const depthFormat = isDepthFormat(desc.format);
		bool const colourUsage = hasGraphImageUsage(desc.usage, GraphImageUsage::ColourAttachment);
		bool const depthUsage = hasGraphImageUsage(desc.usage, GraphImageUsage::DepthAttachment);
		if (name.empty() || desc.samples == 0 || desc.mipLevels == 0 ||
			(desc.absoluteSize.x == 0 && desc.relativeSize.x <= 0.0f) ||
			(desc.absoluteSize.y == 0 && desc.relativeSize.y <= 0.0f) ||
			(depthFormat && (!depthUsage || colourUsage)) || (!depthFormat && depthUsage))
		{
			THROW_MPP("Invalid render graph image descriptor.", __LINE__, __FILE__, __func__);
		}
		if (find_if(mImages.begin(), mImages.end(), [&](Image const& image) { return image.name == name; }) != mImages.end())
		{
			THROW_MPP("Duplicate render graph image name.", __LINE__, __FILE__, __func__);
		}
		Image image{ name, desc };
		image.valueIds.push_back(name + ".Import");
		mImages.push_back(move(image));
		return { (uint32_t)mImages.size() - 1, 0 };
	}

	void RenderGraph::setImageImportName(GraphImageHandle image, string const& importName)
	{
		if (!validImage(image) || !mImages[image.id].desc.external || importName.empty())
		{
			THROW_MPP("Render graph import requires an external image and name.", __LINE__, __FILE__, __func__);
		}
		mImages[image.id].importName = importName;
	}

	void RenderGraph::clearImageImportName(GraphImageHandle image)
	{
		if (!validImage(image)) THROW_MPP("Invalid render graph image handle.", __LINE__, __FILE__, __func__);
		mImages[image.id].importName.clear();
	}

	void RenderGraph::setImageName(GraphImageHandle image, string const& name)
	{
		if (!validImage(image) || name.empty() || any_of(mImages.begin(), mImages.end(), [&](Image const& value) { return value.name == name && &value != &mImages[image.id]; }))
			THROW_MPP("Invalid or duplicate render graph image name.", __LINE__, __FILE__, __func__);
		mImages[image.id].name = name;
	}

	void RenderGraph::setImageDesc(GraphImageHandle image,GraphImageDesc const& desc)
	{
		if(!validImage(image)||desc.samples==0||desc.mipLevels==0||(desc.absoluteSize.x==0&&desc.relativeSize.x<=0)||(desc.absoluteSize.y==0&&desc.relativeSize.y<=0))THROW_MPP("Invalid render graph image descriptor.",__LINE__,__FILE__,__func__);bool depth=isDepthFormat(desc.format),colour=hasGraphImageUsage(desc.usage,GraphImageUsage::ColourAttachment),depthUsage=hasGraphImageUsage(desc.usage,GraphImageUsage::DepthAttachment);if((depth&&(!depthUsage||colour))||(!depth&&depthUsage))THROW_MPP("Render graph image format and usage are incompatible.",__LINE__,__FILE__,__func__);mImages[image.id].desc=desc;if(!desc.external)mImages[image.id].importName.clear();
	}

	void RenderGraph::removeImage(GraphImageHandle image)
	{
		if (!validImage(image) || image.version != 0) THROW_MPP("Invalid render graph image removal.", __LINE__, __FILE__, __func__);
		for (auto& pass : mPasses)
		{
			pass.sampledInputs.erase(remove_if(pass.sampledInputs.begin(), pass.sampledInputs.end(), [&](auto value) { return value.id == image.id; }), pass.sampledInputs.end());
			pass.samplerBindings.erase(remove_if(pass.samplerBindings.begin(), pass.samplerBindings.end(), [&](auto const& value) { return value.image.id == image.id; }), pass.samplerBindings.end());
			pass.colourOutputs.erase(remove_if(pass.colourOutputs.begin(), pass.colourOutputs.end(), [&](auto const& value) { return value.image.id == image.id; }), pass.colourOutputs.end());
			pass.depthOutputs.erase(remove_if(pass.depthOutputs.begin(), pass.depthOutputs.end(), [&](auto const& value) { return value.image.id == image.id; }), pass.depthOutputs.end());
			auto remap = [&](auto& values) { for (auto& value : values) if (value.id > image.id) --value.id; };
			remap(pass.sampledInputs);
			for (auto& value : pass.samplerBindings) if (value.image.id > image.id) --value.image.id;
			for (auto& value : pass.colourOutputs) if (value.image.id > image.id) --value.image.id;
			for (auto& value : pass.depthOutputs) if (value.image.id > image.id) --value.image.id;
		}
		mImages.erase(mImages.begin() + image.id);
	}

	GraphImageInfo RenderGraph::getImageInfo(GraphImageHandle image) const
	{
		if (!validImage(image)) THROW_MPP("Invalid render graph image handle.", __LINE__, __FILE__, __func__);
		auto const& source = mImages[image.id];
		return { source.name, source.desc, source.importName };
	}

	vector<GraphImageHandle> RenderGraph::getImportedImages() const
	{
		vector<GraphImageHandle> result;
		for (uint32_t id = 0; id < mImages.size(); ++id)
			if (mImages[id].desc.external) result.push_back({ id, 0 });
		return result;
	}

	GraphPassHandle RenderGraph::addPass(string const& name, GraphPassType type)
	{
		if (name.empty() || find_if(mPasses.begin(), mPasses.end(), [&](Pass const& pass) { return pass.name == name; }) != mPasses.end())
		{
			THROW_MPP("Invalid or duplicate render graph pass name.", __LINE__, __FILE__, __func__);
		}
		Pass pass;
		pass.name = name;
		pass.type = type;
		mPasses.push_back(move(pass));
		return { (uint32_t)mPasses.size() - 1 };
	}

	void RenderGraph::setPassEnabled(GraphPassHandle pass, bool enabled)
	{
		if (!validPass(pass)) THROW_MPP("Invalid render graph pass handle.", __LINE__, __FILE__, __func__);
		mPasses[pass.id].enabled = enabled;
	}

	void RenderGraph::setPassName(GraphPassHandle pass, string const& name)
	{
		if (!validPass(pass) || name.empty() || any_of(mPasses.begin(), mPasses.end(), [&](Pass const& value) { return value.name == name && &value != &mPasses[pass.id]; }))
			THROW_MPP("Invalid or duplicate render graph pass name.", __LINE__, __FILE__, __func__);
		mPasses[pass.id].name = name;
	}

	void RenderGraph::setPassProgramResource(GraphPassHandle pass, string const& resourceName)
	{
		if (!validPass(pass) || resourceName.empty())
		{
			THROW_MPP("Render graph pass program resource requires a valid pass and name.", __LINE__, __FILE__, __func__);
		}
		mPasses[pass.id].programResource = resourceName;
	}

	void RenderGraph::clearPassProgramResource(GraphPassHandle pass)
	{
		if (!validPass(pass)) THROW_MPP("Invalid render graph pass handle.", __LINE__, __FILE__, __func__);
		mPasses[pass.id].programResource.clear();
	}

	void RenderGraph::setPassCallbackFactory(GraphPassHandle pass, string const& factoryName)
	{
		if (!validPass(pass) || factoryName.empty())
		{
			THROW_MPP("Render graph pass callback factory requires a valid pass and name.", __LINE__, __FILE__, __func__);
		}
		mPasses[pass.id].callbackFactory = factoryName;
	}

	void RenderGraph::setPassRasterState(GraphPassHandle pass, GraphRasterState const& state)
	{
		if (!validPass(pass)) THROW_MPP("Invalid render graph pass handle.", __LINE__, __FILE__, __func__);
		mPasses[pass.id].rasterState = state;
	}

	void RenderGraph::removeProducedValue(GraphImageHandle image)
	{
		if (!validImage(image) || image.version == 0) return;
		for (auto& pass : mPasses)
		{
			pass.sampledInputs.erase(remove_if(pass.sampledInputs.begin(), pass.sampledInputs.end(), [&](auto value) { return value.id == image.id && value.version == image.version; }), pass.sampledInputs.end());
			pass.samplerBindings.erase(remove_if(pass.samplerBindings.begin(), pass.samplerBindings.end(), [&](auto const& value) { return value.image.id == image.id && value.image.version == image.version; }), pass.samplerBindings.end());
			for (auto& value : pass.sampledInputs) if (value.id == image.id && value.version > image.version) --value.version;
			for (auto& value : pass.samplerBindings) if (value.image.id == image.id && value.image.version > image.version) --value.image.version;
			for (auto& value : pass.colourOutputs) if (value.image.id == image.id && value.image.version > image.version) --value.image.version;
			for (auto& value : pass.depthOutputs) if (value.image.id == image.id && value.image.version > image.version) --value.image.version;
		}
		auto& target = mImages[image.id];
		target.producers.erase(target.producers.begin() + image.version);
		target.valueIds.erase(target.valueIds.begin() + image.version);
		--target.latestVersion;
	}

	void RenderGraph::removePass(GraphPassHandle pass)
	{
		if (!validPass(pass)) THROW_MPP("Invalid render graph pass removal.", __LINE__, __FILE__, __func__);
		vector<GraphImageHandle> outputs;
		for (auto const& value : mPasses[pass.id].colourOutputs) outputs.push_back(value.image);
		for (auto const& value : mPasses[pass.id].depthOutputs) outputs.push_back(value.image);
		sort(outputs.begin(), outputs.end(), [](auto left, auto right) { return left.id == right.id ? left.version > right.version : left.id > right.id; });
		for (auto value : outputs) removeProducedValue(value);
		mPasses.erase(mPasses.begin() + pass.id);
		for (auto& image : mImages) for (auto& producer : image.producers) if (producer != UINT32_MAX && producer > pass.id) --producer;
	}

	GraphPassHandle RenderGraph::duplicatePass(GraphPassHandle pass, string const& name)
	{
		if (!validPass(pass)) THROW_MPP("Invalid render graph pass duplication.", __LINE__, __FILE__, __func__);
		auto source = mPasses[pass.id];
		auto colour = source.colourOutputs;
		auto depth = source.depthOutputs;
		source.name = name;
		source.colourOutputs.clear();
		source.depthOutputs.clear();
		if (name.empty() || any_of(mPasses.begin(), mPasses.end(), [&](Pass const& value) { return value.name == name; })) THROW_MPP("Invalid or duplicate render graph pass name.", __LINE__, __FILE__, __func__);
		mPasses.push_back(move(source));
		GraphPassHandle result{ static_cast<uint32_t>(mPasses.size() - 1) };
		for (auto const& value : colour) writeColour(result, { value.image.id, mImages[value.image.id].latestVersion }, value.load, value.store, value.clearColour, value.mipLevel);
		for (auto const& value : depth) writeDepth(result, { value.image.id, mImages[value.image.id].latestVersion }, value.load, value.store, value.clearDepth, value.mipLevel);
		return result;
	}

	void RenderGraph::movePass(GraphPassHandle pass, uint32_t destination)
	{
		if (!validPass(pass) || destination >= mPasses.size()) THROW_MPP("Invalid render graph pass move.", __LINE__, __FILE__, __func__);
		vector<GraphPassHandle> order;
		for (uint32_t index = 0; index < mPasses.size(); ++index) if (index != pass.id) order.push_back({ index });
		order.insert(order.begin() + destination, pass);
		reorderPasses(order);
	}

	void RenderGraph::readSampled(GraphPassHandle pass, GraphImageHandle image)
	{
		if (!validPass(pass) || !validImage(image))
		{
			THROW_MPP("Invalid render graph sampled input.", __LINE__, __FILE__, __func__);
		}
		mPasses[pass.id].sampledInputs.push_back(image);
	}

	void RenderGraph::bindSampler(GraphPassHandle pass, string const& sampler, GraphImageHandle image, uint32_t mipLevel)
	{
		if (!validPass(pass) || !validImage(image) || sampler.empty() || (mipLevel != UINT32_MAX && mipLevel >= mImages[image.id].desc.mipLevels))
		{
			THROW_MPP("Invalid render graph sampler binding.", __LINE__, __FILE__, __func__);
		}
		auto& bindings = mPasses[pass.id].samplerBindings;
		if (find_if(bindings.begin(), bindings.end(), [&](GraphSamplerBinding const& binding) { return binding.sampler == sampler; }) != bindings.end())
		{
			THROW_MPP("Duplicate render graph sampler binding.", __LINE__, __FILE__, __func__);
		}
		readSampled(pass, image);
		bindings.push_back({ sampler, image, mipLevel });
	}

	void RenderGraph::setPassParameters(GraphPassHandle pass, UniformCollection const& parameters)
	{
		if (!validPass(pass)) THROW_MPP("Invalid render graph pass handle.", __LINE__, __FILE__, __func__);
		mPasses[pass.id].parameters = parameters;
	}

	GraphImageHandle RenderGraph::writeColour(GraphPassHandle pass, GraphImageHandle image, GraphLoadOp load, GraphStoreOp store, glm::vec4 const& clear, uint32_t mipLevel)
	{
		if (!validPass(pass) || !validImage(image) || image.version != mImages[image.id].latestVersion || mipLevel >= mImages[image.id].desc.mipLevels ||
			!hasGraphImageUsage(mImages[image.id].desc.usage, GraphImageUsage::ColourAttachment) || isDepthFormat(mImages[image.id].desc.format))
		{
			THROW_MPP("Invalid render graph colour output.", __LINE__, __FILE__, __func__);
		}
		auto& target = mImages[image.id];
		GraphImageHandle output{ image.id, ++target.latestVersion };
		target.producers.push_back(pass.id);
		target.valueIds.push_back(target.name + ".v" + to_string(output.version));
		mPasses[pass.id].colourOutputs.push_back({ output, mipLevel, load, store, clear });
		return output;
	}

	GraphImageHandle RenderGraph::writeDepth(GraphPassHandle pass, GraphImageHandle image, GraphLoadOp load, GraphStoreOp store, float clear, uint32_t mipLevel)
	{
		if (!validPass(pass) || !validImage(image) || image.version != mImages[image.id].latestVersion || mipLevel >= mImages[image.id].desc.mipLevels ||
			!hasGraphImageUsage(mImages[image.id].desc.usage, GraphImageUsage::DepthAttachment) || !isDepthFormat(mImages[image.id].desc.format))
		{
			THROW_MPP("Invalid render graph depth output.", __LINE__, __FILE__, __func__);
		}
		auto& target = mImages[image.id];
		GraphImageHandle output{ image.id, ++target.latestVersion };
		target.producers.push_back(pass.id);
		target.valueIds.push_back(target.name + ".v" + to_string(output.version));
		mPasses[pass.id].depthOutputs.push_back({ output, mipLevel, load, store, clear });
		return output;
	}

	void RenderGraph::setColourOutput(GraphPassHandle pass, size_t output, GraphLoadOp load, GraphStoreOp store, glm::vec4 const& clear, uint32_t mipLevel)
	{
		if (!validPass(pass) || output >= mPasses[pass.id].colourOutputs.size()) THROW_MPP("Invalid render graph colour output edit.", __LINE__, __FILE__, __func__);
		auto& value = mPasses[pass.id].colourOutputs[output];
		if (mipLevel >= mImages[value.image.id].desc.mipLevels) THROW_MPP("Invalid render graph colour output mip.", __LINE__, __FILE__, __func__);
		value.load = load; value.store = store; value.clearColour = clear; value.mipLevel = mipLevel;
	}

	GraphImageHandle RenderGraph::retargetColourOutput(GraphPassHandle pass, size_t output, GraphImageHandle image)
	{
		if (!validPass(pass) || output >= mPasses[pass.id].colourOutputs.size() || !validImage(image) || image.version != mImages[image.id].latestVersion || !hasGraphImageUsage(mImages[image.id].desc.usage, GraphImageUsage::ColourAttachment) || isDepthFormat(mImages[image.id].desc.format)) THROW_MPP("Invalid render graph colour output target.", __LINE__, __FILE__, __func__);
		auto value = mPasses[pass.id].colourOutputs[output]; auto old = value.image;
		if (old.id == image.id) return old;
		auto stableId = getValueId(old); auto replacement = writeColour(pass, image, value.load, value.store, value.clearColour, value.mipLevel);
		mPasses[pass.id].colourOutputs.pop_back(); mPasses[pass.id].colourOutputs[output].image = replacement;
		for (auto& candidate : mPasses) { for (auto& input : candidate.sampledInputs) if (input.id == old.id && input.version == old.version) input = replacement; for (auto& binding : candidate.samplerBindings) if (binding.image.id == old.id && binding.image.version == old.version) binding.image = replacement; }
		removeProducedValue(old); setValueId(replacement, stableId); return replacement;
	}

	void RenderGraph::removeColourOutput(GraphPassHandle pass, size_t output)
	{
		if (!validPass(pass) || output >= mPasses[pass.id].colourOutputs.size()) THROW_MPP("Invalid render graph colour output removal.", __LINE__, __FILE__, __func__);
		auto image = mPasses[pass.id].colourOutputs[output].image;
		mPasses[pass.id].colourOutputs.erase(mPasses[pass.id].colourOutputs.begin() + output);
		removeProducedValue(image);
	}

	void RenderGraph::setDepthOutput(GraphPassHandle pass, size_t output, GraphLoadOp load, GraphStoreOp store, float clear, uint32_t mipLevel)
	{
		if (!validPass(pass) || output >= mPasses[pass.id].depthOutputs.size()) THROW_MPP("Invalid render graph depth output edit.", __LINE__, __FILE__, __func__);
		auto& value = mPasses[pass.id].depthOutputs[output];
		if (mipLevel >= mImages[value.image.id].desc.mipLevels) THROW_MPP("Invalid render graph depth output mip.", __LINE__, __FILE__, __func__);
		value.load = load; value.store = store; value.clearDepth = clear; value.mipLevel = mipLevel;
	}

	GraphImageHandle RenderGraph::retargetDepthOutput(GraphPassHandle pass, size_t output, GraphImageHandle image)
	{
		if (!validPass(pass) || output >= mPasses[pass.id].depthOutputs.size() || !validImage(image) || image.version != mImages[image.id].latestVersion || !hasGraphImageUsage(mImages[image.id].desc.usage, GraphImageUsage::DepthAttachment) || !isDepthFormat(mImages[image.id].desc.format)) THROW_MPP("Invalid render graph depth output target.", __LINE__, __FILE__, __func__);
		auto value = mPasses[pass.id].depthOutputs[output]; auto old = value.image;
		if (old.id == image.id) return old;
		auto stableId = getValueId(old); auto replacement = writeDepth(pass, image, value.load, value.store, value.clearDepth, value.mipLevel);
		mPasses[pass.id].depthOutputs.pop_back(); mPasses[pass.id].depthOutputs[output].image = replacement;
		for (auto& candidate : mPasses) { for (auto& input : candidate.sampledInputs) if (input.id == old.id && input.version == old.version) input = replacement; for (auto& binding : candidate.samplerBindings) if (binding.image.id == old.id && binding.image.version == old.version) binding.image = replacement; }
		removeProducedValue(old); setValueId(replacement, stableId); return replacement;
	}

	void RenderGraph::removeDepthOutput(GraphPassHandle pass, size_t output)
	{
		if (!validPass(pass) || output >= mPasses[pass.id].depthOutputs.size()) THROW_MPP("Invalid render graph depth output removal.", __LINE__, __FILE__, __func__);
		auto image = mPasses[pass.id].depthOutputs[output].image;
		mPasses[pass.id].depthOutputs.erase(mPasses[pass.id].depthOutputs.begin() + output);
		removeProducedValue(image);
	}

	void RenderGraph::setSamplerBinding(GraphPassHandle pass, size_t binding, string const& sampler, GraphImageHandle image, uint32_t mipLevel)
	{
		if (!validPass(pass) || binding >= mPasses[pass.id].samplerBindings.size() || !validImage(image) || sampler.empty() || (mipLevel != UINT32_MAX && mipLevel >= mImages[image.id].desc.mipLevels))
			THROW_MPP("Invalid render graph sampler binding edit.", __LINE__, __FILE__, __func__);
		auto& source = mPasses[pass.id];
		for (size_t index = 0; index < source.samplerBindings.size(); ++index) if (index != binding && source.samplerBindings[index].sampler == sampler) THROW_MPP("Duplicate render graph sampler binding.", __LINE__, __FILE__, __func__);
		auto old = source.samplerBindings[binding].image;
		auto sampled = find_if(source.sampledInputs.begin(), source.sampledInputs.end(), [&](auto value) { return value.id == old.id && value.version == old.version; });
		if (sampled != source.sampledInputs.end()) *sampled = image; else source.sampledInputs.push_back(image);
		source.samplerBindings[binding] = { sampler, image, mipLevel };
	}

	void RenderGraph::removeSamplerBinding(GraphPassHandle pass, size_t binding)
	{
		if (!validPass(pass) || binding >= mPasses[pass.id].samplerBindings.size()) THROW_MPP("Invalid render graph sampler binding removal.", __LINE__, __FILE__, __func__);
		auto& source = mPasses[pass.id]; auto image = source.samplerBindings[binding].image;
		source.samplerBindings.erase(source.samplerBindings.begin() + binding);
		if (none_of(source.samplerBindings.begin(), source.samplerBindings.end(), [&](auto const& value) { return value.image.id == image.id && value.image.version == image.version; }))
			source.sampledInputs.erase(remove_if(source.sampledInputs.begin(), source.sampledInputs.end(), [&](auto value) { return value.id == image.id && value.version == image.version; }), source.sampledInputs.end());
	}

	void RenderGraph::setValueId(GraphImageHandle image, string const& valueId)
	{
		if (!validImage(image) || valueId.empty()) THROW_MPP("Invalid render graph value ID.", __LINE__, __FILE__, __func__);
		if (mImages[image.id].valueIds[image.version] == valueId) return;
		for (auto const& candidate : mImages)
			if (find(candidate.valueIds.begin(), candidate.valueIds.end(), valueId) != candidate.valueIds.end())
				THROW_MPP("Duplicate render graph value ID.", __LINE__, __FILE__, __func__);
		mImages[image.id].valueIds[image.version] = valueId;
	}

	string const& RenderGraph::getValueId(GraphImageHandle image) const
	{
		if (!validImage(image)) THROW_MPP("Invalid render graph image handle.", __LINE__, __FILE__, __func__);
		return mImages[image.id].valueIds[image.version];
	}

	GraphImageHandle RenderGraph::findValue(string const& valueId) const
	{
		for (uint32_t image = 0; image < mImages.size(); ++image)
		{
			auto const found = find(mImages[image].valueIds.begin(), mImages[image].valueIds.end(), valueId);
			if (found != mImages[image].valueIds.end()) return { image, static_cast<uint32_t>(found - mImages[image].valueIds.begin()) };
		}
		return {};
	}

	size_t RenderGraph::getImageCount() const
	{
		return mImages.size();
	}

	size_t RenderGraph::getImageVersionCount(uint32_t imageId) const
	{
		if(imageId>=mImages.size())THROW_MPP("Invalid render graph image ID.",__LINE__,__FILE__,__func__);return mImages[imageId].producers.size();
	}

	size_t RenderGraph::getPassCount() const
	{
		return mPasses.size();
	}

	GraphPassInfo RenderGraph::getPassInfo(GraphPassHandle pass) const
	{
		if (!validPass(pass))
		{
			THROW_MPP("Invalid render graph pass handle.", __LINE__, __FILE__, __func__);
		}
		auto const& source = mPasses[pass.id];
		return { source.name, source.enabled, source.type, source.callbackFactory, source.programResource, source.sampledInputs, source.samplerBindings, source.parameters, source.colourOutputs, source.depthOutputs, source.rasterState };
	}

	RenderGraphCompileResult RenderGraph::compile() const
	{
		RenderGraphCompileResult result;
		for (uint32_t passId = 0; passId < mPasses.size(); ++passId)
		{
			auto const& pass = mPasses[passId];
			if (!pass.enabled) continue;
			for (auto const& input : pass.sampledInputs)
			{
				if (!validImage(input))
				{
					result.diagnostics.push_back("Pass '" + pass.name + "' reads an invalid image handle.");
					continue;
				}
				auto const& image = mImages[input.id];
				if (!hasGraphImageUsage(image.desc.usage, GraphImageUsage::Sampled))
				{
					result.diagnostics.push_back("Pass '" + pass.name + "' samples image '" + image.name + "' without Sampled usage.");
				}
				uint32_t producer = image.producers[input.version];
				if (producer == UINT32_MAX && !image.desc.external)
				{
					result.diagnostics.push_back("Pass '" + pass.name + "' reads unwritten non-external image '" + image.name + "'.");
				}
				else if (producer != UINT32_MAX && producer != passId)
				{
					if (!mPasses[producer].enabled)
						result.diagnostics.push_back("Pass '" + pass.name + "' reads value '" + image.valueIds[input.version] + "' produced by disabled pass '" + mPasses[producer].name + "'.");
					else
					{
						if (producer > passId)
							result.diagnostics.push_back("Pass '" + pass.name + "' appears before producer '" + mPasses[producer].name + "' for value '" + image.valueIds[input.version] + "'.");
					}
				}
				else if (producer == passId)
				{
					result.diagnostics.push_back("Pass '" + pass.name + "' reads and writes the same image version.");
				}
			}

			auto effectiveSize = [&](GraphImageDesc const& desc, uint32_t mip)
			{
				glm::vec2 size = desc.absoluteSize.x && desc.absoluteSize.y ? glm::vec2(desc.absoluteSize) : desc.relativeSize;
				return size / (float)(1u << mip);
			};
			if (pass.colourOutputs.size() > 1)
			{
				auto const& firstOutput = pass.colourOutputs.front();
				auto const& first = mImages[firstOutput.image.id].desc;
				for (auto const& output : pass.colourOutputs)
				{
					auto const& desc = mImages[output.image.id].desc;
					if (effectiveSize(desc, output.mipLevel) != effectiveSize(first, firstOutput.mipLevel) || desc.samples != first.samples)
					{
						result.diagnostics.push_back("MRT pass '" + pass.name + "' has incompatible colour attachment mip dimensions or sample counts.");
					}
				}
			}
			if (pass.depthOutputs.size() > 1)
			{
				result.diagnostics.push_back("Pass '" + pass.name + "' declares more than one depth output.");
			}
			if (!pass.colourOutputs.empty() && !pass.depthOutputs.empty())
			{
				auto const& colourOutput = pass.colourOutputs.front();
				auto const& depthOutput = pass.depthOutputs.front();
				auto const& colour = mImages[colourOutput.image.id].desc;
				auto const& depth = mImages[depthOutput.image.id].desc;
				if (effectiveSize(colour, colourOutput.mipLevel) != effectiveSize(depth, depthOutput.mipLevel) || colour.samples != depth.samples)
				{
					result.diagnostics.push_back("Pass '" + pass.name + "' has incompatible colour and depth attachment dimensions or sample counts.");
				}
			}
		}

		if (!result.diagnostics.empty()) return result;
		for (uint32_t pass = 0; pass < mPasses.size(); ++pass)
			if (mPasses[pass].enabled) result.passOrder.push_back({ pass });
		result.valid = true;
		return result;
	}

	RenderGraphCompileResult RenderGraph::buildDependencyOrder() const
	{
		RenderGraphCompileResult result;
		vector<set<uint32_t>> edges(mPasses.size());
		vector<uint32_t> indegree(mPasses.size());
		size_t enabledCount = 0;
		for (uint32_t passId = 0; passId < mPasses.size(); ++passId)
		{
			auto const& pass = mPasses[passId];
			if (!pass.enabled) continue;
			++enabledCount;
			for (auto const& input : pass.sampledInputs)
			{
				if (!validImage(input))
				{
					result.diagnostics.push_back("Pass '" + pass.name + "' reads an invalid image handle.");
					continue;
				}
				auto const producer = mImages[input.id].producers[input.version];
				if (producer == UINT32_MAX)
				{
					if (!mImages[input.id].desc.external)
						result.diagnostics.push_back("Pass '" + pass.name + "' reads unwritten non-external value '" + mImages[input.id].valueIds[input.version] + "'.");
				}
				else if (producer == passId)
					result.diagnostics.push_back("Pass '" + pass.name + "' reads and writes the same image value.");
				else if (!mPasses[producer].enabled)
					result.diagnostics.push_back("Pass '" + pass.name + "' depends on disabled pass '" + mPasses[producer].name + "'.");
				else if (edges[producer].insert(passId).second)
					++indegree[passId];
			}
		}
		if (!result.diagnostics.empty()) return result;
		priority_queue<uint32_t, vector<uint32_t>, greater<uint32_t>> ready;
		for (uint32_t pass = 0; pass < mPasses.size(); ++pass)
			if (mPasses[pass].enabled && indegree[pass] == 0) ready.push(pass);
		while (!ready.empty())
		{
			auto const pass = ready.top(); ready.pop();
			result.passOrder.push_back({ pass });
			for (auto const next : edges[pass]) if (--indegree[next] == 0) ready.push(next);
		}
		if (result.passOrder.size() != enabledCount)
		{
			result.diagnostics.push_back("Render graph contains a pass dependency cycle.");
			result.passOrder.clear();
			return result;
		}
		result.valid = true;
		return result;
	}

	void RenderGraph::reorderPasses(vector<GraphPassHandle> const& order)
	{
		if(order.size()!=mPasses.size())THROW_MPP("Render graph pass order must include every pass.",__LINE__,__FILE__,__func__);vector<uint32_t> oldToNew(mPasses.size(),UINT32_MAX);for(uint32_t next=0;next<order.size();++next){if(!validPass(order[next])||oldToNew[order[next].id]!=UINT32_MAX)THROW_MPP("Render graph pass order contains an invalid or duplicate pass.",__LINE__,__FILE__,__func__);oldToNew[order[next].id]=next;}vector<Pass> reordered;reordered.reserve(mPasses.size());for(auto handle:order)reordered.push_back(std::move(mPasses[handle.id]));mPasses=std::move(reordered);for(auto& image:mImages)for(auto& producer:image.producers)if(producer!=UINT32_MAX)producer=oldToNew[producer];
	}

	RenderGraphAllocationPlan RenderGraph::buildAllocationPlan(glm::uvec2 const& viewport) const
	{
		RenderGraphAllocationPlan plan;
		if (viewport.x == 0 || viewport.y == 0)
		{
			plan.diagnostics.push_back("Render graph allocation viewport must be non-zero.");
			return plan;
		}
		auto compiled = compile();
		if (!compiled.valid)
		{
			plan.diagnostics = move(compiled.diagnostics);
			return plan;
		}

		vector<vector<uint32_t>> allocationIndex;
		allocationIndex.reserve(mImages.size());
		for (auto const& image : mImages)
			allocationIndex.emplace_back(image.producers.size(), UINT32_MAX);

		auto markImage = [&](GraphImageHandle handle, uint32_t passPosition)
		{
			auto const& image = mImages[handle.id];
			if (image.desc.external)
			{
				if (find_if(plan.importedImages.begin(), plan.importedImages.end(), [&](GraphImageHandle current)
					{ return current.id == handle.id && current.version == handle.version; }) == plan.importedImages.end())
				{
					plan.importedImages.push_back(handle);
				}
				return;
			}
			auto& index = allocationIndex[handle.id][handle.version];
			if (index == UINT32_MAX)
			{
				GraphImageLifetime lifetime;
				lifetime.image = handle;
				lifetime.debugName = image.name;
				lifetime.desc = image.desc;
				lifetime.size.x = image.desc.absoluteSize.x ? image.desc.absoluteSize.x : max(1u, (uint32_t)(viewport.x * image.desc.relativeSize.x));
				lifetime.size.y = image.desc.absoluteSize.y ? image.desc.absoluteSize.y : max(1u, (uint32_t)(viewport.y * image.desc.relativeSize.y));
				uint32_t maxMipLevels = 1;
				for (uint32_t dimension = max(lifetime.size.x, lifetime.size.y); dimension > 1; dimension >>= 1) ++maxMipLevels;
				if (image.desc.mipLevels > maxMipLevels)
				{
					plan.diagnostics.push_back("Image '" + image.name + "' requests " + to_string(image.desc.mipLevels) +
						" mip levels but its resolved dimensions support at most " + to_string(maxMipLevels) + ".");
					return;
				}
				lifetime.firstPass = passPosition;
				lifetime.lastPass = passPosition;
				uint64_t texels = 0;
				for (uint32_t mip = 0; mip < image.desc.mipLevels; ++mip)
					texels += static_cast<uint64_t>(max(1u, lifetime.size.x >> mip)) * max(1u, lifetime.size.y >> mip);
				lifetime.estimatedBytes = (texels * formatBits(image.desc.format) * image.desc.samples + 7) / 8;
				index = (uint32_t)plan.allocatedImages.size();
				plan.allocatedImages.push_back(lifetime);
			}
			else
			{
				plan.allocatedImages[index].firstPass = min(plan.allocatedImages[index].firstPass, passPosition);
				plan.allocatedImages[index].lastPass = max(plan.allocatedImages[index].lastPass, passPosition);
			}
		};

		for (uint32_t position = 0; position < compiled.passOrder.size(); ++position)
		{
			auto const& pass = mPasses[compiled.passOrder[position].id];
			for (auto const& input : pass.sampledInputs) markImage(input, position);
			for (auto const& output : pass.colourOutputs) markImage(output.image, position);
			for (auto const& output : pass.depthOutputs) markImage(output.image, position);
		}
		if (plan.diagnostics.empty())
		{
			vector<vector<uint32_t>> allocationMembers;
			for (uint32_t imageIndex = 0; imageIndex < plan.allocatedImages.size(); ++imageIndex)
			{
				auto& lifetime = plan.allocatedImages[imageIndex];
				uint32_t allocation = UINT32_MAX;
				if (lifetime.desc.transient)
				{
					for (uint32_t candidate = 0; candidate < allocationMembers.size(); ++candidate)
					{
						auto const& representative = plan.allocatedImages[allocationMembers[candidate].front()];
						if (!aliasCompatible(representative, lifetime)) continue;
						bool overlaps = false;
						for (auto member : allocationMembers[candidate])
						{
							auto const& previous = plan.allocatedImages[member];
							if (!(previous.lastPass < lifetime.firstPass || lifetime.lastPass < previous.firstPass)) { overlaps = true; break; }
						}
						if (!overlaps) { allocation = candidate; break; }
					}
				}
				if (allocation == UINT32_MAX)
				{
					allocation = static_cast<uint32_t>(allocationMembers.size());
					allocationMembers.push_back({});
					plan.estimatedPhysicalBytes += lifetime.estimatedBytes;
				}
				lifetime.physicalAllocation = allocation;
				allocationMembers[allocation].push_back(imageIndex);
			}
		}
		plan.valid = plan.diagnostics.empty();
		if (!plan.valid)
		{
			plan.allocatedImages.clear();
			plan.importedImages.clear();
		}
		return plan;
	}

	string RenderGraph::describe() const
	{
		ostringstream output;
		output << "RenderGraph: " << mImages.size() << " image(s), " << mPasses.size() << " pass(es)\n";
		for (uint32_t id = 0; id < mImages.size(); ++id)
		{
			auto const& image = mImages[id];
			output << "  Image[" << id << "] '" << image.name << "': format=" << formatName(image.desc.format)
				<< ", versions=0.." << image.latestVersion << ", size=" << image.desc.absoluteSize.x << "x" << image.desc.absoluteSize.y
				<< " @ " << image.desc.relativeSize.x << "x" << image.desc.relativeSize.y << ", samples=" << image.desc.samples
				<< ", mips=" << image.desc.mipLevels << ", colourSpace=" << (image.desc.colourSpace == TextureColourSpace::Srgb ? "sRGB" : "linear")
				<< ", filters=" << image.desc.params.minFilter << "/" << image.desc.params.magFilter << ", wrap=" << image.desc.params.wrap
				<< ", transient=" << (image.desc.transient ? "true" : "false") << ", external=" << (image.desc.external ? "true" : "false");
			if (!image.importName.empty()) output << ", import='" << image.importName << "'";
			output << "\n";
		}
		for (uint32_t id = 0; id < mPasses.size(); ++id)
		{
			auto const& pass = mPasses[id];
			output << "  Pass[" << id << "] '" << pass.name << "': enabled=" << (pass.enabled ? "true" : "false") << ", type=" << passTypeName(pass.type)
				<< ", sampled=" << pass.sampledInputs.size() << ", colour=" << pass.colourOutputs.size() << ", depth=" << pass.depthOutputs.size()
				<< ", parameters=" << pass.parameters.getNumUniforms();
			if (!pass.programResource.empty()) output << ", program='" << pass.programResource << "'";
			if (!pass.callbackFactory.empty()) output << ", factory='" << pass.callbackFactory << "'";
			output << "\n";
			for (auto const& binding : pass.samplerBindings)
				output << "    sampler '" << binding.sampler << "' = Image[" << binding.image.id << "]@" << binding.image.version << (binding.mipLevel == UINT32_MAX ? " full chain" : " mip " + to_string(binding.mipLevel)) << "\n";
			for (auto const& input : pass.sampledInputs)
				output << "    reads '" << mImages[input.id].valueIds[input.version] << "' Image[" << input.id << "]@" << input.version << "\n";
			for (auto const& target : pass.colourOutputs)
				output << "    writes colour '" << mImages[target.image.id].valueIds[target.image.version] << "' Image[" << target.image.id << "]@" << target.image.version << " mip " << target.mipLevel << "\n";
			for (auto const& target : pass.depthOutputs)
				output << "    writes depth '" << mImages[target.image.id].valueIds[target.image.version] << "' Image[" << target.image.id << "]@" << target.image.version << " mip " << target.mipLevel << "\n";
		}
		return output.str();
	}

	RenderGraphCompileResult RenderGraph::compile(Caps const& caps) const
	{
		auto result = compile();
		for (auto const& image : mImages)
		{
			if (image.desc.samples > caps.maxSamples)
				result.diagnostics.push_back("Image '" + image.name + "' requests " + to_string(image.desc.samples) + " samples, exceeding the renderer capability.");
			if (image.desc.samples > 1 && image.desc.mipLevels > 1)
				result.diagnostics.push_back("Image '" + image.name + "' cannot combine multisampling and mip levels.");
		}
		for (auto const& pass : mPasses)
		{
			if (pass.colourOutputs.size() > caps.maxColourAttachments || pass.colourOutputs.size() > caps.maxDrawBuffers)
			{
				result.diagnostics.push_back("Pass '" + pass.name + "' declares " + to_string(pass.colourOutputs.size()) +
					" colour outputs, exceeding the renderer MRT capability.");
			}
		}
		if (!result.diagnostics.empty())
		{
			result.valid = false;
			result.passOrder.clear();
		}
		return result;
	}
}
