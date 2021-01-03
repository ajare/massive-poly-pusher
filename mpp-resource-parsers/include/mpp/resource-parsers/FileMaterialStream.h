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
			bool mUseSpecifiedMeshSpec;

			mesh::MeshSpecification mMeshSpec;

		private:

			void createChildResourceStreamsImpl();

			static void parseForChildResourceStreams(utils::StructuredData const& data, ResourceManager* resourceMgr, std::string const& filepath, bool useSpecifiedMesh, mesh::MeshSpecification const* meshSpec);

			static void parseUniform(utils::StructuredData const& data, UniformCollection& uniforms, std::string const& filepath);

			static void parseUniformVectorType(std::string const& name, std::string const& type, size_t count, std::string const& value, UniformCollection &uniforms, std::string const& filepath);

			static void parseUniformMatrixType(std::string const& name, std::string const& type, size_t count, std::string const& value, UniformCollection &uniforms, std::string const& filepath);

		protected:

			void loadImpl();

		public:

			FileMaterialStream(ResourceManager* resourceMgr, std::string const& filepath);

			FileMaterialStream(ResourceManager* resourceMgr, std::string const& filepath, utils::StructuredData const& data);

			FileMaterialStream(ResourceManager* resourceMgr, std::string const& filepath, mesh::MeshSpecification const& meshSpec);

			FileMaterialStream(ResourceManager* resourceMgr, std::string const& filepath, utils::StructuredData const& data, mesh::MeshSpecification const& meshSpec);

			static std::pair<std::string, QualitySetting> parseQualitySetting(utils::StructuredData const& data, ResourceManager* resourceMgr, std::string const& filepath);
		};

	}
}