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

		private:

			void setup();

			void loadImpl();

			std::string parseShader(utils::StructuredData const& data);

			void parseMeshSpecificationBufferChannel(utils::StructuredData const& data, mesh::VertexBufferAttributeLayout* layout);

			void parseMeshSpecificationBuffer(utils::StructuredData const& data, mesh::MeshSpecification& meshSpec);

			mesh::Vertex::Component parseMeshSpecificationBufferChannelComponent(std::string const& value);

			mesh::Vertex::DataType parseMeshSpecificationBufferChannelType(std::string const& value);

			mesh::Primitive::Type parseMeshSpecificationPrimitive(std::string const& value);

			mesh::VertexBufferStorageType parseMeshSpecificationStorage(std::string const& value);

			mesh::MeshSpecification parseMeshSpecification(utils::StructuredData const& data);
			
			void parseQualitySetting(utils::StructuredData const& data);

		public:

			FileProgramStream(ResourceManager* resourceMgr, std::string const& filepath);

			FileProgramStream(ResourceManager* resourceMgr, std::string const& filepath, utils::StructuredData const& data);
		};

	}
}