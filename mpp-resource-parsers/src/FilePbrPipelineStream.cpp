#include "mpp/resource-parsers/FilePbrPipelineStream.h"
#include "mpp/resource-parsers/FilePbrMaterialStream.h"
#include "mpp/resource-parsers/FileProgramStream.h"
#include "mpp/resource-parsers/FileSamplerStream.h"
#include "mpp/resource-parsers/FileTextureStream.h"
#include "mpp/resource-parsers/PbrPipelineParser.h"

namespace mpp::resource_parsers
{
	FilePbrPipelineStream::FilePbrPipelineStream(ResourceManager* manager,std::string filepath):PbrPipelineStream(manager),mFilepath(std::move(filepath)){}

	void FilePbrPipelineStream::createChildResourceStreamsImpl()
	{
		if(!getDocument())setDocument(std::make_shared<PbrPipelineDocument>(PbrPipelineParser::fromFile(mFilepath)));
		for(auto const& resource:getDocument()->localResources)
		{
			ResourceStreamPtr stream;
			switch(resource.kind)
			{
			case PbrPipelineResourceKind::PbrMaterial:stream=std::make_shared<FilePbrMaterialStream>(getResourceMgr(),mFilepath,resource.definition);break;
			case PbrPipelineResourceKind::Program:stream=std::make_shared<FileProgramStream>(getResourceMgr(),mFilepath,resource.definition);break;
			case PbrPipelineResourceKind::Texture:stream=std::make_shared<FileTextureStream>(getResourceMgr(),mFilepath,resource.definition);break;
			case PbrPipelineResourceKind::Sampler:stream=std::make_shared<FileSamplerStream>(getResourceMgr(),mFilepath,resource.definition);break;
			}
			addChild(resource.name,stream);
		}
	}

	void FilePbrPipelineStream::loadImpl()
	{
		if(!getDocument())setDocument(std::make_shared<PbrPipelineDocument>(PbrPipelineParser::fromFile(mFilepath)));
	}
}
