#include <sstream>

#include "utils/XmlWriter.h"
#include "mpp/resource-parsers/RenderGraphSerializer.h"

namespace mpp::resource_parsers
{
	namespace
	{
		std::string format(GraphImageFormat value)
		{
			switch (value) { case GraphImageFormat::Rgba8: return "RGBA8"; case GraphImageFormat::Rgba16f: return "RGBA16F"; case GraphImageFormat::Rg16f: return "RG16F"; case GraphImageFormat::Depth24: return "DEPTH24"; default: return "DEPTH24_STENCIL8"; }
		}
		std::string usage(GraphImageUsage value)
		{
			std::string result;
			auto add = [&](GraphImageUsage flag, char const* name) { if (hasGraphImageUsage(value, flag)) { if (!result.empty()) result += ","; result += name; } };
			add(GraphImageUsage::Sampled, "sampled"); add(GraphImageUsage::ColourAttachment, "colourAttachment"); add(GraphImageUsage::DepthAttachment, "depthAttachment"); add(GraphImageUsage::Presentation, "presentation");
			return result;
		}
		std::string passType(GraphPassType value)
		{
			switch (value) { case GraphPassType::Scene: return "scene"; case GraphPassType::Fullscreen: return "fullscreen"; default: return "present"; }
		}
		std::string load(GraphLoadOp value) { return value == GraphLoadOp::Load ? "load" : (value == GraphLoadOp::Clear ? "clear" : "dontCare"); }
		std::string store(GraphStoreOp value) { return value == GraphStoreOp::Store ? "store" : "dontCare"; }
		std::string imageName(RenderGraph const& graph, GraphImageHandle handle) { return graph.getImageInfo(handle).name; }
		std::string vec4(glm::vec4 const& value) { std::ostringstream out; out << value.x << ' ' << value.y << ' ' << value.z << ' ' << value.w; return out.str(); }
	}

	void RenderGraphSerializer::toFile(RenderGraph const& graph, std::string const& filepath)
	{
		utils::XmlWriter writer("RenderGraph");
		auto images = writer.getRootNode()->createChild("Images");
		for (uint32_t id = 0; id < graph.getImageCount(); ++id)
		{
			auto info = graph.getImageInfo({ id, 0 });
			auto image = images->createChild("Image");
			image->createChild("name")->setValue(info.name);
			image->createChild("format")->setValue(format(info.desc.format));
			image->createChild("scale")->setValue(std::to_string(info.desc.relativeSize.x) + " " + std::to_string(info.desc.relativeSize.y));
			if (info.desc.absoluteSize.x) image->createChild("width")->setValue(info.desc.absoluteSize.x);
			if (info.desc.absoluteSize.y) image->createChild("height")->setValue(info.desc.absoluteSize.y);
			image->createChild("samples")->setValue(info.desc.samples);
			image->createChild("mipLevels")->setValue(info.desc.mipLevels);
			image->createChild("usage")->setValue(usage(info.desc.usage));
			image->createChild("external")->setValue(info.desc.external);
			image->createChild("transient")->setValue(info.desc.transient);
			if (!info.importName.empty()) image->createChild("import")->setValue(info.importName);
		}
		auto passes = writer.getRootNode()->createChild("Passes");
		for (uint32_t id = 0; id < graph.getPassCount(); ++id)
		{
			auto info = graph.getPassInfo({ id });
			auto pass = passes->createChild("Pass");
			pass->createChild("name")->setValue(info.name);
			pass->createChild("type")->setValue(passType(info.type));
			if (!info.callbackFactory.empty()) pass->createChild("factory")->setValue(info.callbackFactory);
			if (!info.programResource.empty()) pass->createChild("program")->setValue(info.programResource);
			if (!info.samplerBindings.empty())
			{
				auto inputs = pass->createChild("Inputs");
				for (auto const& binding : info.samplerBindings) { auto sampled = inputs->createChild("Sampled"); sampled->createChild("sampler")->setValue(binding.sampler); sampled->createChild("image")->setValue(imageName(graph, binding.image)); }
			}
			if (!info.colourOutputs.empty())
			{
				auto colours = pass->createChild("Colours");
				for (auto const& output : info.colourOutputs) { auto node = colours->createChild("Output"); node->createChild("image")->setValue(imageName(graph, output.image)); node->createChild("load")->setValue(load(output.load)); node->createChild("store")->setValue(store(output.store)); if (output.load == GraphLoadOp::Clear) node->createChild("clear")->setValue(vec4(output.clearColour)); }
			}
			if (!info.depthOutputs.empty()) { auto const& output = info.depthOutputs.front(); auto node = pass->createChild("Depth"); node->createChild("image")->setValue(imageName(graph, output.image)); node->createChild("load")->setValue(load(output.load)); node->createChild("store")->setValue(store(output.store)); if (output.load == GraphLoadOp::Clear) node->createChild("clear")->setValue(output.clearDepth); }
		}
		writer.write(filepath);
	}
}
