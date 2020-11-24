#pragma once

#include <map>
#include <string>

#include "mpp/ProgramStream.h"
#include "mpp/ResourceManager.h"

#include "Config.h"
#include "FileStream.h"

namespace mpp
{
	namespace resource_parsers
	{

		class _MPPRESOURCEPARSERSAPI FileProgramStream : public mpp::ProgramStream, public FileStream
		{
			std::string mFilepath;

			utils::StructuredData mData;

		private:

			void loadImpl();

			void parseQualitySetting(utils::StructuredData const& data);

			std::string readTextFile(std::string const& filepath);

		public:

			FileProgramStream(ResourceManager* resourceMgr, std::string const& filepath);

			FileProgramStream(ResourceManager* resourceMgr, utils::StructuredData const& data);
		};

	}
}