#include <map>
#include <sstream>

#include "utils/XmlReader.h"
#include "utils/StringUtils.h"

#include "mpp/resource-parsers/RenderGraphParser.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"

using namespace std;

namespace mpp
{
	namespace resource_parsers
	{
		namespace
		{
			GraphImageFormat parseFormat(string value, string const& filepath)
			{
				utils::StringUtils::toUpper(value);
				if (value == "RGBA8") return GraphImageFormat::Rgba8;
				if (value == "RGBA16F") return GraphImageFormat::Rgba16f;
				if (value == "RG16F") return GraphImageFormat::Rg16f;
				if (value == "DEPTH24") return GraphImageFormat::Depth24;
				if (value == "DEPTH24_STENCIL8") return GraphImageFormat::Depth24Stencil8;
				THROW_MPP_RESOURCE_PARSERS("Unknown RenderGraph image format in " + filepath + ".", __LINE__, __FILE__, __func__);
			}

			GraphLoadOp parseLoad(string value)
			{
				utils::StringUtils::toUpper(value);
				if (value == "LOAD") return GraphLoadOp::Load;
				if (value == "CLEAR") return GraphLoadOp::Clear;
				return GraphLoadOp::DontCare;
			}

			GraphStoreOp parseStore(string value)
			{
				utils::StringUtils::toUpper(value);
				return value == "STORE" ? GraphStoreOp::Store : GraphStoreOp::DontCare;
			}

			glm::vec2 parseVec2(string const& value)
			{
				istringstream input(value); glm::vec2 result; input >> result.x >> result.y; return result;
			}

			glm::vec4 parseVec4(string const& value)
			{
				istringstream input(value); glm::vec4 result; input >> result.x >> result.y >> result.z >> result.w; return result;
			}

			bool parseBool(string value)
			{
				utils::StringUtils::toUpper(value);
				return value == "TRUE" || value == "1";
			}
		}

		RenderGraph RenderGraphParser::fromFile(string const& filepath)
		{
			auto reader = utils::XmlReader::fromFile(filepath);
			auto data = reader->readTree();
			delete reader;
			if (data.getName() != "RenderGraph")
			{
				THROW_MPP_RESOURCE_PARSERS("Render graph root must be RenderGraph: " + filepath, __LINE__, __FILE__, __func__);
			}

			RenderGraph graph;
			map<string, GraphImageHandle> images;
			if (!data.hasEntry("Images"))
			{
				THROW_MPP_RESOURCE_PARSERS("Render graph has no Images block: " + filepath, __LINE__, __FILE__, __func__);
			}
			for (auto const& entry : data.getEntry("Images"))
			{
				if (entry.first != "Image") continue;
				auto const& image = entry.second;
				GraphImageDesc desc;
				desc.format = parseFormat(image.getEntry("format").getValue(), filepath);
				desc.relativeSize = image.hasEntry("scale") ? parseVec2(image.getEntry("scale").getValue()) : glm::vec2(1.0f);
				desc.external = image.hasEntry("external") && parseBool(image.getEntry("external").getValue());
				desc.transient = !image.hasEntry("transient") || parseBool(image.getEntry("transient").getValue());
				string usage = image.getEntry("usage").getValue();
				utils::StringUtils::toUpper(usage);
				if (usage.find("SAMPLED") != string::npos) desc.usage = desc.usage | GraphImageUsage::Sampled;
				if (usage.find("COLOURATTACHMENT") != string::npos) desc.usage = desc.usage | GraphImageUsage::ColourAttachment;
				if (usage.find("DEPTHATTACHMENT") != string::npos) desc.usage = desc.usage | GraphImageUsage::DepthAttachment;
				if (usage.find("PRESENTATION") != string::npos) desc.usage = desc.usage | GraphImageUsage::Presentation;
				images[image.getEntry("name").getValue()] = graph.createImage(image.getEntry("name").getValue(), desc);
			}

			if (!data.hasEntry("Passes")) return graph;
			for (auto const& entry : data.getEntry("Passes"))
			{
				if (entry.first != "Pass") continue;
				auto const& passData = entry.second;
				auto pass = graph.addPass(passData.getEntry("name").getValue());
				if (passData.hasEntry("factory")) graph.setPassCallbackFactory(pass, passData.getEntry("factory").getValue());
				else if (passData.hasEntry("callback")) graph.setPassCallbackFactory(pass, passData.getEntry("callback").getValue());
				if (passData.hasEntry("program")) graph.setPassProgramResource(pass, passData.getEntry("program").getValue());
				if (passData.hasEntry("Inputs"))
				{
					for (auto const& input : passData.getEntry("Inputs"))
					{
						if (input.first != "Sampled") continue;
						auto it = images.find(input.second.getEntry("image").getValue());
						if (it == images.end()) THROW_MPP_RESOURCE_PARSERS("Unknown sampled graph image in " + filepath, __LINE__, __FILE__, __func__);
						if (input.second.hasEntry("sampler")) graph.bindSampler(pass, input.second.getEntry("sampler").getValue(), it->second);
						else graph.readSampled(pass, it->second);
					}
				}
				if (passData.hasEntry("Colours"))
				{
					for (auto const& output : passData.getEntry("Colours"))
					{
						if (output.first != "Output") continue;
						auto it = images.find(output.second.getEntry("image").getValue());
						if (it == images.end()) THROW_MPP_RESOURCE_PARSERS("Unknown colour graph image in " + filepath, __LINE__, __FILE__, __func__);
						auto next = graph.writeColour(pass, it->second, parseLoad(output.second.getEntry("load").getValue()), parseStore(output.second.getEntry("store").getValue()), output.second.hasEntry("clear") ? parseVec4(output.second.getEntry("clear").getValue()) : glm::vec4(0.0f));
						it->second = next;
					}
				}
				if (passData.hasEntry("Depth"))
				{
					auto const& output = passData.getEntry("Depth");
					auto it = images.find(output.getEntry("image").getValue());
					if (it == images.end()) THROW_MPP_RESOURCE_PARSERS("Unknown depth graph image in " + filepath, __LINE__, __FILE__, __func__);
					auto next = graph.writeDepth(pass, it->second, parseLoad(output.getEntry("load").getValue()), parseStore(output.getEntry("store").getValue()), output.hasEntry("clear") ? utils::StringUtils::parseFloat(output.getEntry("clear").getValue()) : 1.0f);
					it->second = next;
				}
			}
			return graph;
		}
	}
}
