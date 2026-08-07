#pragma once
#include <string>
#include "Config.h"
#include "mpp/PbrPipelineStream.h"
namespace mpp::resource_parsers { class _MPPRESOURCEPARSERSAPI FilePbrPipelineStream : public PbrPipelineStream { std::string mFilepath; void createChildResourceStreamsImpl() override; protected: void loadImpl() override; public: FilePbrPipelineStream(ResourceManager* manager,std::string filepath); }; }
