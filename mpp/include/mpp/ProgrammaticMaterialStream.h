#pragma once

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#pragma warning(pop)

#include "mpp/MaterialStream.h"
#include "mpp/mesh/Vertex.h"
#include "mpp/mesh/MeshSpecification.h"

namespace mpp
{
	class _MPPAPI ProgrammaticMaterialStream : public MaterialStream
	{

		void loadImpl() {}

	public:

		explicit ProgrammaticMaterialStream(ResourceManager* resourceMgr);

		void setProgram(std::string const& program, uint32_t quality = 0);

		void setMeshSpecification(mesh::MeshSpecification const& spec, uint32_t quality = 0);

		void setProgram2d(bool is2d, size_t quality = 0);

		void setProgramVertexShaderFile(std::string const& file, uint32_t quality = 0);

		void setProgramVertexShaderString(std::string const& data, uint32_t quality = 0);

		void setProgramVertexShaderResource(std::string const& resource, uint32_t quality = 0);

		void setProgramFragmentShaderFile(std::string const& file, uint32_t quality = 0);

		void setProgramFragmentShaderString(std::string const& data, uint32_t quality = 0);

		void setProgramFragmentShaderResource(std::string const& resource, uint32_t quality = 0);

		void setTextureChild(std::string const& sampler, std::string const& resource, uint32_t quality = 0);

		void setTexture(std::string const& sampler, std::string const& texture, uint32_t quality = 0);

		void setDefaultTexture(uint32_t quality = 0);

		void setUniform(std::string const& name, int32_t value, uint32_t quality = 0);

		void setUniform(std::string const& name, uint32_t value, uint32_t quality = 0);

		void setUniform(std::string const& name, float value, uint32_t quality = 0);

		void setUniform(std::string const& name, glm::vec2 const& value, uint32_t quality = 0);

		void setUniform(std::string const& name, glm::vec3 const& value, uint32_t quality = 0);

		void setUniform(std::string const& name, glm::vec4 const& value, uint32_t quality = 0);

		void setUniform(std::string const& name, size_t count, int32_t const* values, uint32_t quality = 0);

		void setUniform(std::string const& name, size_t count, uint32_t const* values, uint32_t quality = 0);

		void setUniform(std::string const& name, size_t count, float const* values, uint32_t quality = 0);
	};
}