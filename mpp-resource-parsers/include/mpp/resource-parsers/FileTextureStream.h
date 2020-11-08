#pragma once

#include <string>

#include "mpp/TextureStream.h"
#include "mpp/ResourceManager.h"

#include "Config.h"

namespace mpp
{
	namespace resource_parsers
	{

		class _MPPRESOURCEPARSERSAPI FileTextureStream : public mpp::TextureStream
		{
			std::string mFilepath;

		public:

			explicit FileTextureStream(ResourceManager* resourceMgr, std::string const& filepath);
		};

	}
}