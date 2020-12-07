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
			std::map<std::string, uint32_t> mInternalFormats, mTargets;

		private:

			void setup();

			void loadImpl();

			uint32_t parseInternalFormat(std::string const& value);

			static uint32_t parseMinFilter(std::string const& value, std::string const& filepath);

			static uint32_t parseMagFilter(std::string const& value, std::string const& filepath);

			static uint32_t parseWrapping(std::string const& value, std::string const& filepath);

			uint32_t parseTarget(std::string const& value);

		public:

			FileTextureStream(ResourceManager* resourceMgr, std::string const& filepath);

			FileTextureStream(ResourceManager* resourceMgr, std::string const& filepath, utils::StructuredData const& data);
		
			static std::pair<std::string, QualitySetting> parseQualitySetting(utils::StructuredData const& data, ResourceManager* resourceMgr, std::string const& filepath);
		};

	}
}