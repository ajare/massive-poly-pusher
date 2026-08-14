#include <memory>

#include "utils/XmlReader.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"
#include "mpp/resource-parsers/LegacyPipelineDocumentLoader.h"
#include "mpp/resource-parsers/LegacyPipelineParser.h"

namespace mpp::resource_parsers
{
	LegacyPipelineDocument LegacyPipelineDocumentLoader::fromFile(std::string const& filepath)
	{
		std::unique_ptr<utils::XmlReader> reader(utils::XmlReader::fromFile(filepath));
		auto root = reader->readTree().getName();
		if (root == "LegacyPipeline") return LegacyPipelineParser::fromFile(filepath);
		THROW_MPP_RESOURCE_PARSERS("Expected LegacyPipeline XML root: " + filepath, __LINE__, __FILE__, __func__);
	}

	bool LegacyPipelineDocumentLoader::isLegacyPipelineFile(std::string const& filepath)
	{
		std::unique_ptr<utils::XmlReader> reader(utils::XmlReader::fromFile(filepath));
		return reader->readTree().getName() == "LegacyPipeline";
	}
}
