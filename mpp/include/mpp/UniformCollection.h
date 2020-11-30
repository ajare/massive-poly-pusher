#pragma once

#include <map>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

#include "mpp/program/glslTypes.h"

#include "mpp/Config.h"
#include "mpp/Program.h"

namespace mpp
{
	class _MPPAPI UniformCollection
	{
	public:

		struct UniformData
		{
			std::string name;
			program::GLSLType type;
			size_t size;
			char data[64];
		};

		std::map<std::string, UniformData> mUniformData;

	public:

		size_t getNumUniforms() const;

		std::map<std::string, UniformData> const& getUniformData() const;

		void setUniform(std::string const& name, int32_t value);

		void setUniform(std::string const& name, uint32_t value);

		void setUniform(std::string const& name, float value);

		void setUniform(std::string const& name, glm::vec2 const& value);

		void setUniform(std::string const& name, glm::vec3 const& value);

		void setUniform(std::string const& name, glm::vec4 const& value);

		void setUniform(std::string const& name, size_t count, int32_t const* values);

		void setUniform(std::string const& name, size_t count, uint32_t const* values);

		void setUniform(std::string const& name, size_t count, float const* values);

		void setUniform(std::string const& name, program::GLSLType type, size_t count, char* data);

		void updateUniform(std::string const& name, int32_t value);

		void updateUniform(std::string const& name, uint32_t value);

		void updateUniform(std::string const& name, float value);

		void updateUniform(std::string const& name, glm::vec2 const& value);

		void updateUniform(std::string const& name, glm::vec3 const& value);

		void updateUniform(std::string const& name, glm::vec4 const& value);

		void updateUniform(std::string const& name, size_t count, int32_t const* values);

		void updateUniform(std::string const& name, size_t count, uint32_t const* values);

		void updateUniform(std::string const& name, size_t count, float const* values);

		void bindUniforms(ResourcePtr program);
	};
}

