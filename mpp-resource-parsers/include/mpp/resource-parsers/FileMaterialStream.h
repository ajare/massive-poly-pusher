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

		private:

			void parseQualitySetting(utils::StructuredData const& data);

		protected:

			void loadImpl();

		public:

			FileMaterialStream(ResourceManager* resourceMgr, std::string const& filepath);
		};

	}
}