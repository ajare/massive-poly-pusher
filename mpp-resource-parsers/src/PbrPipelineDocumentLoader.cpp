#include <filesystem>
#include <memory>

#include "utils/XmlReader.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"
#include "mpp/resource-parsers/PbrPipelineDocumentLoader.h"
#include "mpp/resource-parsers/PbrPipelineParser.h"
#include "mpp/resource-parsers/RenderGraphParser.h"

namespace mpp::resource_parsers
{
	PbrPipelineDocument PbrPipelineDocumentLoader::fromFile(std::string const& filepath)
	{
		std::unique_ptr<utils::XmlReader> reader(utils::XmlReader::fromFile(filepath));
		auto root = reader->readTree().getName();
		if (root == "PbrPipeline") return PbrPipelineParser::fromFile(filepath);
		if (root == "RenderGraph")
		{
			PbrPipelineDocument document;
			document.name = std::filesystem::path(filepath).stem().string();
			document.sourcePath = filepath;
			document.importedFromRenderGraph = true;
			document.graph = std::make_shared<RenderGraph>(RenderGraphParser::fromFile(filepath));
			for(uint32_t image=0;image<document.graph->getImageCount();++image)
			{
				auto handle=GraphImageHandle{image,0};auto info=document.graph->getImageInfo(handle);
				if(!hasGraphImageUsage(info.desc.usage,GraphImageUsage::Presentation))continue;
				info.desc.usage=info.desc.usage|GraphImageUsage::Sampled;document.graph->setImageDesc(handle,info.desc);
				RenderPipelineOutput output;output.name="Main";output.image=info.name;document.outputs.push_back(std::move(output));break;
			}
			return document;
		}
		THROW_MPP_RESOURCE_PARSERS("Expected PbrPipeline or RenderGraph XML root: " + filepath, __LINE__, __FILE__, __func__);
	}
}
