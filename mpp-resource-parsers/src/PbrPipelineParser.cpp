#include <memory>

#include "utils/XmlReader.h"
#include "utils/StringUtils.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"
#include "mpp/resource-parsers/PbrPipelineParser.h"
#include "mpp/resource-parsers/RenderGraphParser.h"

using namespace std;

namespace mpp::resource_parsers
{
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
