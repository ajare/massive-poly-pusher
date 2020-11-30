#pragma once

#include <string>
#include <fstream>

#include "mpp/mesh/MeshSpecification.h"

#include "mpp/program/Parser.h"

#include "Config.h"
#include "ResourceStream.h"
#include "UniformCollection.h"

namespace mpp
{
	class _MPPAPI ResourceStreamSerializer
	{
		void writeValue(std::string const& value, std::ofstream& fp);

		void writeValue(int32_t value, std::ofstream& fp);

		void writeValue(uint32_t value, std::ofstream& fp);

		void writeValue(float value, std::ofstream& fp);

		void writeValue(bool value, std::ofstream& fp);

		void writeMeshSpecification(mesh::MeshSpecification const& meshSpec, std::ofstream& fp);

		void writeUniformCollection(UniformCollection const& uniforms, std::ofstream& fp);

		void writeParser(program::Parser const& parser, std::ofstream& fp);

		void writeGLSLDecl(program::GLSLTypeDecl decl, std::ofstream& fp);

		void writeMaterialStream(ResourceStreamPtr resourceStream, std::ofstream& fp);

		void writeProgramStream(ResourceStreamPtr resourceStream, std::ofstream& fp);

		void writeSamplerStream(ResourceStreamPtr resourceStream, std::ofstream& fp);

		void writeStringStream(ResourceStreamPtr resourceStream, std::ofstream& fp);

		void writeTextureStream(ResourceStreamPtr resourceStream, std::ofstream& fp);

		void writeStream(ResourceStreamPtr resourceStream, std::ofstream& fp);

	public:

		ResourceStreamSerializer() = default;

		virtual ~ResourceStreamSerializer() = default;

		void serialize(ResourceStreamPtr resourceStream, std::string const& filename);
	};

}