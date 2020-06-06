#pragma once

#include <map>

#include <mpp/mesh/MeshSpecification.h>

#include "utils/XmlReader.h"

#include "mpp/mesh-specification-parser/Config.h"

#include "mpp/mesh-specification-parser/ProgramInformation.h"
#include "mpp/mesh-specification-parser/MaterialInformation.h"

namespace mpp
{
	namespace mesh_specification_parser
	{

		class _MPPMESHSPECIFICATIONPARSERAPI SpecificationParser
		{
			std::string mFilename;

			std::map<std::string, mpp::mesh::Vertex::Component> mComponentTypes;
			std::map<std::string, mpp::mesh::Vertex::DataType> mDataTypes;

		public:

			explicit SpecificationParser(std::string const& filename);

			std::map<std::string, ProgramInformation> parseProgramInformation();

			std::map<std::string, MaterialInformation> parseMaterialInformation();

			mpp::mesh::MeshSpecification parseMeshSpecification(uint32& maxVerticesPerMesh);
		};

	}
}