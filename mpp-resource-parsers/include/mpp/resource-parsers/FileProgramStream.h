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

		private:

			void setup();

			void loadImpl();

			Shader parseShader(utils::StructuredData const& data);

			void parseQualitySetting(utils::StructuredData const& data);

		public:

			FileProgramStream(ResourceManager* resourceMgr, std::string const& filepath);

			FileProgramStream(ResourceManager* resourceMgr, std::string const& filepath, utils::StructuredData const& data);

			FileProgramStream(ResourceManager* resourceMgr, std::string const& filepath, mesh::MeshSpecification const& meshSpec);

			FileProgramStream(ResourceManager* resourceMgr, std::string const& filepath, utils::StructuredData const& data, mesh::MeshSpecification const& meshSpec);
		};

	}
}