#include "mpp/resource-parsers/FilePbrPipelineStream.h"
#include "mpp/resource-parsers/PbrPipelineParser.h"
namespace mpp::resource_parsers { FilePbrPipelineStream::FilePbrPipelineStream(ResourceManager*m,std::string f):PbrPipelineStream(m),mFilepath(std::move(f)){} void FilePbrPipelineStream::loadImpl(){setDocument(std::make_shared<PbrPipelineDocument>(PbrPipelineParser::fromFile(mFilepath)));} }
