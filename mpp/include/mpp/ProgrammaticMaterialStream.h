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

		ProgrammaticMaterialStream(ResourceManager* resourceMgr, std::string const& program);

		ProgrammaticMaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec, std::string const& vertexShader, bool vertexShaderIsFile, std::string const& fragmentShader, bool fragmentShaderIsFile);

		ProgrammaticMaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec);

		ProgrammaticMaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec, std::set<std::string> const& tags);

		void setTextureChild(std::string const& sampler, std::string const& resource);

		void setTexture(std::string const& sampler, std::string const& texture);

		void useDefaultTexture();

		void setUniform(std::string const& name, int32 value);

		void setUniform(std::string const& name, uint32 value);

		void setUniform(std::string const& name, float value);

		void setUniform(std::string const& name, glm::vec2 const& value);

		void setUniform(std::string const& name, glm::vec3 const& value);

		void setUniform(std::string const& name, glm::vec4 const& value);

		void setUniform(std::string const& name, size_t count, int32 const* values);

		void setUniform(std::string const& name, size_t count, uint32 const* values);

		void setUniform(std::string const& name, size_t count, float const* values);

		Resource* createResource(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr);
	};
}