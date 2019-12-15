#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/gl.h>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/type_ptr.hpp>
#pragma warning(pop)

#include "mpp/UniformCollection.h"

using namespace std;

namespace mpp
{
	
	void UniformCollection::setUniform(string const& name, int value)
	{
		mIntegerUniforms[name] = value;
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void UniformCollection::setUniform(string const& name, float value)
	{
		mFloatUniforms[name] = value;
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void UniformCollection::setUniform(string const& name, glm::vec2 const& value)
	{
		mVec2Uniforms[name] = value;
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void UniformCollection::setUniform(string const& name, glm::vec3 const& value)
	{
		mVec3Uniforms[name] = value;
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void UniformCollection::setUniform(string const& name, glm::vec4 const& value)
	{
		mVec4Uniforms[name] = value;
	}

	/*
	 * Upload uniform values for rendering.
	 *
	 */
	void UniformCollection::bindUniforms(Program const* program)
	{
		for (auto it: mIntegerUniforms)
		{
			glUniform1i(program->getUniformId(it.first), it.second);
		}

		for (auto it: mFloatUniforms)
		{
			glUniform1f(program->getUniformId(it.first), it.second);
		}

		for (auto it: mVec2Uniforms)
		{
			glUniform2f(program->getUniformId(it.first), it.second.x, it.second.y);
		}

		for (auto it: mVec3Uniforms)
		{
			glUniform3f(program->getUniformId(it.first), it.second.x, it.second.y, it.second.z);
		}

		for (auto it: mVec4Uniforms)
		{
			glUniform4f(program->getUniformId(it.first), it.second.x, it.second.y, it.second.z, it.second.w);
		}
	}

}