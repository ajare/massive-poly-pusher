#pragma once

#include <map>
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

			std::map<std::string, uint32_t> mInternalFormats, mFiltering, mTargets, mWrapping;

		private:

			void loadImpl();

			uint32_t parseInternalFormat(std::string const& value);

			uint32_t parseFiltering(std::string const& value);

			uint32_t parseWrapping(std::string const& value);

			uint32_t parseTarget(std::string const& value);

		public:

			FileTextureStream(ResourceManager* resourceMgr, std::string const& filepath);
		};

	}
}