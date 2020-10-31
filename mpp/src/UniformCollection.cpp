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
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{
	// Uniforms are used by shaders, which are part of Materials.
	// So, it should be a member of Material, not MeshInstance.
	// However, we may want to override them for individual MeshInstances.
	// We also want to be able to have them set programmatically, via a
	// lambda, which should take the Model resource, and mesh id, as arguments.

	// When the program changes in flushVertexBuffers, need to set the material uniforms
	// When the program stays the same, but the material changes, need to set the
	// material uniforms.

	// Batches should set uniforms such as Diffuse in the Material directly.
	
	// Then need to set mesh overrides

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
			GL_CHECK(glUniform1i(program->getUniformId(it.first), it.second));
		}

		for (auto it: mFloatUniforms)
		{
			GL_CHECK(glUniform1f(program->getUniformId(it.first), it.second));
		}

		for (auto it: mVec2Uniforms)
		{
			GL_CHECK(glUniform2f(program->getUniformId(it.first), it.second.x, it.second.y));
		}

		for (auto it: mVec3Uniforms)
		{
			GL_CHECK(glUniform3f(program->getUniformId(it.first), it.second.x, it.second.y, it.second.z));
		}

		for (auto it: mVec4Uniforms)
		{
			GL_CHECK(glUniform4f(program->getUniformId(it.first), it.second.x, it.second.y, it.second.z, it.second.w));
		}
	}

}