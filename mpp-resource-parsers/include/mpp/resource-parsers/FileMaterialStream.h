#pragma once

#include <map>
#include <string>

#include "mpp/MaterialStream.h"
#include "mpp/ResourceManager.h"

#include "Config.h"
#include "FileStream.h"

namespace mpp
{
	namespace resource_parsers
	{

		class _MPPRESOURCEPARSERSAPI FileMaterialStream : public mpp::MaterialStream, public FileStream
		{
			std::string mFilepath;

			utils::StructuredData mData;

		private:

			void parseQualitySetting(utils::StructuredData const& data);

		protected:

			void loadImpl();

		public:

			FileMaterialStream(ResourceManager* resourceMgr, std::string const& filepath);

			FileMaterialStream(ResourceManager* resourceMgr, utils::StructuredData const& data);
		};

	}
}