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

		void setTexture(std::string const& sampler, std::string const& texture);

		void useDefaultTexture();

		void setFloatUniform(std::string const& name, float value);

		void setFloatUniform(std::string const& name, glm::vec2 const& value);

		void setFloatUniform(std::string const& name, glm::vec3 const& value);

		void setFloatUniform(std::string const& name, glm::vec4 const& value);
	};
}