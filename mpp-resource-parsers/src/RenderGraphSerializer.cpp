#include <glew/glew.h>

#include <algorithm>
#include <cstring>
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
		std::string colourSpace(TextureColourSpace value) { return value == TextureColourSpace::Srgb ? "SRGB" : "LINEAR"; }
		std::string minFilter(uint32_t value)
		{
			switch (value) { case GL_NEAREST: return "NEAREST"; case GL_LINEAR: return "LINEAR"; case GL_NEAREST_MIPMAP_NEAREST: return "NEAREST_MIPMAP_NEAREST"; case GL_LINEAR_MIPMAP_NEAREST: return "LINEAR_MIPMAP_NEAREST"; case GL_NEAREST_MIPMAP_LINEAR: return "NEAREST_MIPMAP_LINEAR"; default: return "LINEAR_MIPMAP_LINEAR"; }
		}
		std::string magFilter(uint32_t value) { return value == GL_NEAREST ? "NEAREST" : "LINEAR"; }
		std::string wrap(uint32_t value)
		{
			switch (value) { case GL_REPEAT: return "REPEAT"; case GL_MIRRORED_REPEAT: return "MIRRORED_REPEAT"; case GL_CLAMP_TO_BORDER: return "CLAMP_TO_BORDER"; default: return "CLAMP_TO_EDGE"; }
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
			image->createChild("colourSpace")->setValue(colourSpace(info.desc.colourSpace));
			image->createChild("minFilter")->setValue(minFilter(info.desc.params.minFilter));
			image->createChild("magFilter")->setValue(magFilter(info.desc.params.magFilter));
			image->createChild("wrap")->setValue(wrap(info.desc.params.wrap));
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
			if (!info.sampledInputs.empty())
			{
				auto inputs = pass->createChild("Inputs");
				for (auto const& image : info.sampledInputs)
				{
					auto sampled = inputs->createChild("Sampled");
					auto binding = std::find_if(info.samplerBindings.begin(), info.samplerBindings.end(), [&](GraphSamplerBinding const& current) { return current.image.id == image.id && current.image.version == image.version; });
					if (binding != info.samplerBindings.end()) { sampled->createChild("sampler")->setValue(binding->sampler); if (binding->mipLevel != UINT32_MAX) sampled->createChild("mipLevel")->setValue(binding->mipLevel); }
					sampled->createChild("image")->setValue(imageName(graph, image));
				}
			}
			if (!info.parameters.getUniformData().empty())
			{
				auto parameters = pass->createChild("Parameters");
				for (auto const& entry : info.parameters.getUniformData())
				{
					auto const& value = entry.second;
					if (value.count != 1) continue;
					auto node = parameters->createChild(value.type == program::GLSLType::Int ? "Int" : (value.type == program::GLSLType::Bool ? "Bool" : value.numElements == 1 ? "Float" : value.numElements == 2 ? "Vec2" : value.numElements == 3 ? "Vec3" : "Vec4"));
					node->createChild("name")->setValue(value.name);
					std::ostringstream text;
					if (value.type == program::GLSLType::Int || value.type == program::GLSLType::Bool) { int32_t v; memcpy(&v, value.data, sizeof(v)); text << v; }
					else { auto values = reinterpret_cast<float const*>(value.data); for (size_t i = 0; i < value.numElements; ++i) { if (i) text << ' '; text << values[i]; } }
					node->createChild("value")->setValue(text.str());
				}
			}
			if (!info.colourOutputs.empty())
			{
				auto colours = pass->createChild("Colours");
				for (auto const& output : info.colourOutputs) { auto node = colours->createChild("Output"); node->createChild("image")->setValue(imageName(graph, output.image)); if (output.mipLevel) node->createChild("mipLevel")->setValue(output.mipLevel); node->createChild("load")->setValue(load(output.load)); node->createChild("store")->setValue(store(output.store)); if (output.load == GraphLoadOp::Clear) node->createChild("clear")->setValue(vec4(output.clearColour)); }
			}
			if (!info.depthOutputs.empty()) { auto const& output = info.depthOutputs.front(); auto node = pass->createChild("Depth"); node->createChild("image")->setValue(imageName(graph, output.image)); if (output.mipLevel) node->createChild("mipLevel")->setValue(output.mipLevel); node->createChild("load")->setValue(load(output.load)); node->createChild("store")->setValue(store(output.store)); if (output.load == GraphLoadOp::Clear) node->createChild("clear")->setValue(output.clearDepth); }
		}
		writer.write(filepath);
	}
}
