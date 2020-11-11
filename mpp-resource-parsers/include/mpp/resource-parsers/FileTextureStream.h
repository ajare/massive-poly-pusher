#pragma once

#include <string>

#include "mpp/TextureStream.h"
#include "mpp/ResourceManager.h"

#include "Config.h"
#include "FileStream.h"

namespace mpp
{
	namespace resource_parsers
	{

		class _MPPRESOURCEPARSERSAPI FileTextureStream : public mpp::TextureStream, public FileStream
		{
			std::string mFilepath;

		private:

			void loadImpl();

		public:

			FileTextureStream(ResourceManager* resourceMgr, std::string const& filepath);
		};

	}
}