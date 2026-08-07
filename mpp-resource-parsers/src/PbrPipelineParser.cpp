#include <memory>

#include "utils/XmlReader.h"
#include "utils/StringUtils.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"
#include "mpp/resource-parsers/PbrPipelineParser.h"
#include "mpp/resource-parsers/RenderGraphParser.h"

using namespace std;

namespace mpp::resource_parsers
{
	namespace
	{
		GraphImageFormat importFormat(std::string value)
		{
			utils::StringUtils::toUpper(value);
			if(value=="R8")return GraphImageFormat::R8;if(value=="RG8")return GraphImageFormat::Rg8;if(value=="RGBA8")return GraphImageFormat::Rgba8;if(value=="SRGB8_ALPHA8")return GraphImageFormat::Srgb8Alpha8;
			if(value=="R16F")return GraphImageFormat::R16f;if(value=="RG16F")return GraphImageFormat::Rg16f;if(value=="RGBA16F")return GraphImageFormat::Rgba16f;if(value=="R32F")return GraphImageFormat::R32f;if(value=="RG32F")return GraphImageFormat::Rg32f;if(value=="RGBA32F")return GraphImageFormat::Rgba32f;
			if(value=="R11G11B10F")return GraphImageFormat::R11g11b10f;if(value=="RGB10_A2")return GraphImageFormat::Rgb10a2;if(value=="DEPTH16")return GraphImageFormat::Depth16;if(value=="DEPTH24")return GraphImageFormat::Depth24;if(value=="DEPTH32F")return GraphImageFormat::Depth32f;if(value=="DEPTH24_STENCIL8")return GraphImageFormat::Depth24Stencil8;if(value=="DEPTH32F_STENCIL8")return GraphImageFormat::Depth32fStencil8;
			THROW_MPP_RESOURCE_PARSERS("Unknown PbrPipeline import format '"+value+"'.",__LINE__,__FILE__,__func__);
		}
		GraphImageUsage importUsage(std::string value){utils::StringUtils::toUpper(value);GraphImageUsage usage=GraphImageUsage::None;if(value.find("SAMPLED")!=std::string::npos)usage=usage|GraphImageUsage::Sampled;if(value.find("COLOURATTACHMENT")!=std::string::npos)usage=usage|GraphImageUsage::ColourAttachment;if(value.find("DEPTHATTACHMENT")!=std::string::npos)usage=usage|GraphImageUsage::DepthAttachment;if(value.find("PRESENTATION")!=std::string::npos)usage=usage|GraphImageUsage::Presentation;return usage;}
		bool boolean(std::string value){utils::StringUtils::toUpper(value);return value=="TRUE"||value=="1"||value=="YES";}
	}
	PbrPipelineDocument PbrPipelineParser::fromFile(string const& filepath)
	{
		unique_ptr<utils::XmlReader> reader(utils::XmlReader::fromFile(filepath));
		auto data = reader->readTree();
		if (data.getName() != "PbrPipeline") THROW_MPP_RESOURCE_PARSERS("Pipeline root must be PbrPipeline: " + filepath, __LINE__, __FILE__, __func__);
		PbrPipelineDocument document;
		document.sourcePath = filepath;
		document.version = data.hasEntry("version") ? utils::StringUtils::parseUInt(data.getEntry("version").getValue()) : 1;
		document.name = data.hasEntry("name") ? data.getEntry("name").getValue() : "";
		if (data.hasEntry("PreviewScene")) document.previewScene = data.getEntry("PreviewScene").getEntry("file").getValue();
		if (data.hasEntry("ResourceLibraries"))
			for (auto const& entry : data.getEntry("ResourceLibraries")) if (entry.first == "Library") document.resourceLibraries.push_back(entry.second.getEntry("file").getValue());
		if (data.hasEntry("Imports")) for(auto const& entry:data.getEntry("Imports")) if(entry.first=="Import")
		{
			auto const& value=entry.second; PbrPipelineImportDocument import; import.id=value.getEntry("id").getValue(); import.semantic=value.getEntry("semantic").getValue(); import.format=importFormat(value.getEntry("format").getValue()); import.usage=importUsage(value.getEntry("usage").getValue()); if(value.hasEntry("required"))import.required=boolean(value.getEntry("required").getValue()); if(value.hasEntry("fallback"))import.fallback=value.getEntry("fallback").getValue(); document.imports.push_back(import);
		}
		if (data.hasEntry("Environment"))
		{
			auto const& environment = data.getEntry("Environment");
			auto read = [&](char const* key) { return environment.hasEntry(key) ? environment.getEntry(key).getValue() : string(); };
			document.environment.binding = read("binding"); document.environment.irradiance = read("irradiance");
			document.environment.prefilteredSpecular = read("prefilteredSpecular"); document.environment.brdfLut = read("brdfLut"); document.environment.background = read("background");
		}
		if (data.hasEntry("PreviewBindings"))
			for (auto const& entry : data.getEntry("PreviewBindings")) if (entry.first == "Material")
				document.previewBindings.push_back({ entry.second.getEntry("binding").getValue(), entry.second.getEntry("resource").getValue() });
		if (!data.hasEntry("RenderGraph")) THROW_MPP_RESOURCE_PARSERS("PbrPipeline has no embedded RenderGraph: " + filepath, __LINE__, __FILE__, __func__);
		document.graph = make_shared<RenderGraph>(RenderGraphParser::fromData(data.getEntry("RenderGraph"), filepath));
		return document;
	}
}
