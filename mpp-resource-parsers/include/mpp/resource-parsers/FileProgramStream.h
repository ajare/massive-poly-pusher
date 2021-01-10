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
			std::map<std::string, mesh::VertexBufferStorageType> mMeshSpecificationStorage;
			std::map<std::string, mesh::Primitive::Type>  mMeshSpecificationPrimitive;
			std::map<std::string, mesh::Vertex::Component> mComponentTypes;
			std::map<std::string, mesh::Vertex::DataType> mDataTypes;

			bool mMeshSpecRequired;

			mesh::MeshSpecification mMeshSpecification;

			bool mRelativisePaths;

		private:

			void setup();

			void loadImpl();

			static Shader parseShader(utils::StructuredData const& data, ResourceManager* resourceMgr, std::string const& filepath);

		public:

			FileProgramStream(ResourceManager* resourceMgr, std::string const& filepath, bool relativisePaths = true);

			FileProgramStream(ResourceManager* resourceMgr, std::string const& filepath, utils::StructuredData const& data, bool relativisePaths = true);

			FileProgramStream(ResourceManager* resourceMgr, std::string const& filepath, mesh::MeshSpecification const& meshSpec, bool relativisePaths = true);

			FileProgramStream(ResourceManager* resourceMgr, std::string const& filepath, utils::StructuredData const& data, mesh::MeshSpecification const& meshSpec, bool relativisePaths = true);

			static std::pair<std::string, QualitySetting> parseQualitySetting(utils::StructuredData const& data, ResourceManager* resourceMgr, std::string const& filepath, bool meshSpecRequired, mesh::MeshSpecification const* mainMeshSpec, bool relativisePaths = true);
		};

	}
}