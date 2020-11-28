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
			void parseQualitySetting(utils::StructuredData const& data);

			void parseUniform(utils::StructuredData const& data, UniformCollection& uniforms);

			void parseUniformVectorType(std::string const& name, std::string const& type, size_t count, std::string const& value, UniformCollection &uniforms);

			void parseUniformMatrixType(std::string const& name, std::string const& type, size_t count, std::string const& value, UniformCollection &uniforms);

		protected:

			void loadImpl();

		public:

			FileMaterialStream(ResourceManager* resourceMgr, std::string const& filepath);

			FileMaterialStream(ResourceManager* resourceMgr, std::string const& filepath, utils::StructuredData const& data);
		};

	}
}