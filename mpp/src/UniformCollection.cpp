#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#include <Windows.h>
#endif

#include <functional>

#include <glew/glew.h>
#include <gl/gl.h>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/type_ptr.hpp>
#pragma warning(pop)

#include "mpp/UniformCollection.h"
#include "mpp/GLErrorCheck.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{


	size_t UniformCollection::getNumUniforms() const
	{
		return mUniformData.size();
	}

	map<std::string, UniformCollection::UniformData> const& UniformCollection::getUniformData() const
	{
		return mUniformData;
	}

	void UniformCollection::setUniform(string const& name, int32_t value)
	{
		UniformData ud
		{
			MPP_PROGRAM_MARKUP_UNIFORM(name),
			program::GLSLType::Int,
			1
		};

		memcpy(ud.data, &value, sizeof(int32_t));
		mUniformData[name] = ud;
	}

	void UniformCollection::setUniform(string const& name, uint32_t value)
	{
		UniformData ud
		{
			MPP_PROGRAM_MARKUP_UNIFORM(name),
			program::GLSLType::Uint,
			1
		};

		memcpy(ud.data, &value, sizeof(uint32_t));
		mUniformData[name] = ud;
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void UniformCollection::setUniform(string const& name, float value)
	{
		UniformData ud
		{
			MPP_PROGRAM_MARKUP_UNIFORM(name),
			program::GLSLType::Float,
			1
		};

		memcpy(ud.data, &value, sizeof(float));
		mUniformData[name] = ud;
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void UniformCollection::setUniform(string const& name, glm::vec2 const& value)
	{
		UniformData ud
		{
			MPP_PROGRAM_MARKUP_UNIFORM(name),
			program::GLSLType::Float,
			2
		};

		memcpy(ud.data, glm::value_ptr(value), sizeof(glm::vec2));
		mUniformData[name] = ud;
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void UniformCollection::setUniform(string const& name, glm::vec3 const& value)
	{
		UniformData ud
		{
			MPP_PROGRAM_MARKUP_UNIFORM(name),
			program::GLSLType::Float,
			3
		};

		memcpy(ud.data, glm::value_ptr(value), sizeof(glm::vec3));
		mUniformData[name] = ud;
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void UniformCollection::setUniform(string const& name, glm::vec4 const& value)
	{
		UniformData ud
		{
			MPP_PROGRAM_MARKUP_UNIFORM(name),
			program::GLSLType::Float,
			4
		};

		memcpy(ud.data, glm::value_ptr(value), sizeof(glm::vec4));
		mUniformData[name] = ud;
	}

	void UniformCollection::setUniform(string const& name, size_t count, int32_t const* values)
	{
		UniformData ud
		{
			MPP_PROGRAM_MARKUP_UNIFORM(name),
			program::GLSLType::Int,
			count
		};

		memcpy(ud.data, values, sizeof(int32_t) * count);
		mUniformData[name] = ud;
	}

	void UniformCollection::setUniform(string const& name, size_t count, uint32_t const* values)
	{
		UniformData ud
		{
			MPP_PROGRAM_MARKUP_UNIFORM(name),
			program::GLSLType::Uint,
			count
		};

		memcpy(ud.data, values, sizeof(uint32_t) * count);
		mUniformData[name] = ud;
	}

	void UniformCollection::setUniform(string const& name, size_t count, float const* values)
	{
		UniformData ud
		{
			MPP_PROGRAM_MARKUP_UNIFORM(name),
			program::GLSLType::Float,
			count
		};

		memcpy(&ud.data, values, sizeof(float) * count);
		mUniformData[name] = ud;
	}

	void UniformCollection::updateUniform(string const& name, int32_t value)
	{
		auto data = mUniformData.find(name)->second.data;
		memcpy(data, &value, sizeof(int32_t));
	}

	void UniformCollection::updateUniform(string const& name, uint32_t value)
	{
		auto data = mUniformData.find(name)->second.data;
		memcpy(data, &value, sizeof(uint32_t));
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void UniformCollection::updateUniform(string const& name, float value)
	{
		auto data = mUniformData.find(name)->second.data;
		memcpy(data, &value, sizeof(float));
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void UniformCollection::updateUniform(string const& name, glm::vec2 const& value)
	{
		auto data = mUniformData.find(name)->second.data;
		memcpy(data, &value, sizeof(glm::vec2));
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void UniformCollection::updateUniform(string const& name, glm::vec3 const& value)
	{
		auto data = mUniformData.find(name)->second.data;
		memcpy(data, &value, sizeof(glm::vec3));
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void UniformCollection::updateUniform(string const& name, glm::vec4 const& value)
	{
		auto data = mUniformData.find(name)->second.data;
		memcpy(data, &value, sizeof(glm::vec4));
	}

	void UniformCollection::updateUniform(string const& name, size_t count, int32_t const* values)
	{
		auto data = mUniformData.find(name)->second.data;
		memcpy(data, values, sizeof(int32_t) * count);
	}

	void UniformCollection::updateUniform(string const& name, size_t count, uint32_t const* values)
	{
		auto data = mUniformData.find(name)->second.data;
		memcpy(data, values, sizeof(int32_t) * count);
	}

	void UniformCollection::updateUniform(string const& name, size_t count, float const* values)
	{
		auto data = mUniformData.find(name)->second.data;
		memcpy(data, values, sizeof(int32_t) * count);
	}

	/*
	 * Upload uniform values for rendering.
	 *
	 */
	void UniformCollection::bindUniforms(ResourcePtr program)
	{
		Program* p = static_cast<Program*>(program.get());

		const function<void(GLint, GLsizei, const GLint*)> intFunctions[4]
		{
			glUniform1iv,
			glUniform2iv,
			glUniform3iv,
			glUniform4iv
		};

		const function<void(GLint, GLsizei, const GLuint*)> uintFunctions[4]
		{
			glUniform1uiv,
			glUniform2uiv,
			glUniform3uiv,
			glUniform4uiv
		};

		const function<void(GLint, GLsizei, const GLfloat*)> floatFunctions[4]
		{
			glUniform1fv,
			glUniform2fv,
			glUniform3fv,
			glUniform4fv
		};

		for (auto const& it: mUniformData)
		{
			auto const& ud = it.second;
			auto id = (GLint)p->getUniformId(it.first); // Use non-marked up value
			
			switch (ud.type)
			{
			case program::GLSLType::Int:
				GL_CHECK(intFunctions[ud.size - 1](id, 1, (const GLint*)ud.data));
				break;

			case program::GLSLType::Uint:
				GL_CHECK(uintFunctions[ud.size - 1](id, 1, (const GLuint*)ud.data));
				break;

			case program::GLSLType::Float:
				GL_CHECK(floatFunctions[ud.size - 1](id, 1, (const GLfloat*)ud.data));
				break;

			default:
				THROW_MPP("Unsupported uniform type.", __LINE__, __FILE__, __func__);
			}
		}
	}

}