#pragma once

#include <map>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

#include "mpp/Config.h"
#include "mpp/Program.h"

namespace mpp
{
	class _MPPAPI UniformCollection
	{
		struct UniformData
		{
			std::string name;
			GLuint glType;
			char data[64];
		};

		std::map<std::string, UniformData> mUniformData;

	public:

		void setUniform(std::string const& name, int value);

		void setUniform(std::string const& name, float value);

		void setUniform(std::string const& name, glm::vec2 const& value);

		void setUniform(std::string const& name, glm::vec3 const& value);

		void setUniform(std::string const& name, glm::vec4 const& value);

		void bindUniforms(Program const* program);
	};
}

