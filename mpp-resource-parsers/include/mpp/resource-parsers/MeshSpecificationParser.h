#pragma once

#include <map>
#include <string>

#include "utils/StructuredData.h"

#include "mpp/mesh/MeshSpecification.h"

#include "Config.h"

namespace mpp
{
	namespace resource_parsers
	{

		class _MPPRESOURCEPARSERSAPI MeshSpecificationParser
		{
			std::string mFilepath;

			std::map<std::string, mesh::VertexBufferStorageType> mMeshSpecificationStorage;
			std::map<std::string, mesh::Primitive::Type>  mMeshSpecificationPrimitive;
			std::map<std::string, mesh::Vertex::Component> mComponentTypes;
			std::map<std::string, mesh::Vertex::DataType> mDataTypes;

		private:

			void parseMeshSpecificationBufferChannel(utils::StructuredData const& data, mesh::VertexBufferAttributeLayout* layout);

			void parseMeshSpecificationBuffer(utils::StructuredData const& data, mesh::MeshSpecification& meshSpec);

			mesh::Vertex::Component parseMeshSpecificationBufferChannelComponent(std::string const& value);

			mesh::Vertex::DataType parseMeshSpecificationBufferChannelType(std::string const& value);

			mesh::Primitive::Type parseMeshSpecificationPrimitive(std::string const& value);

			mesh::VertexBufferStorageType parseMeshSpecificationStorage(std::string const& value);

		public:

			explicit MeshSpecificationParser(std::string const& filepath);

			mesh::MeshSpecification parse(utils::StructuredData const& data);
		};

	}
}