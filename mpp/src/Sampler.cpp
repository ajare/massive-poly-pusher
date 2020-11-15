#include "mpp/Config.h"

#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#include <Windows.h>
#endif

#include <cassert>
#include <glew/glew.h>
#include <gl/gl.h>

#include "mpp/RenderSystem.h"
#include "mpp/Sampler.h"
#include "mpp/SamplerStream.h"
#include "mpp/MppException.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{
	/*
	 * Constructor.
	 *
	 */
	Sampler::Sampler(string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream)
		: Resource(name, "Texture", renderSystem, resourceMgr, resourceStream)
	{
	}

	/*
	 * Create the data required for the program from the resource stream.
	 *
	 */
	void Sampler::createImpl()
	{
		SamplerStream* sStr = dynamic_cast<SamplerStream*>(getResourceStream().get());
		if (!sStr)
		{
			THROW_MPP("Could not cast to type 'SamplerStream'.", __LINE__, __FILE__, __func__);
		}

		mParams = sStr->getParams(0);
	}

	/*
	 * Destroy the texture data.
	 *
	 */
	void Sampler::destroyImpl()
	{
	}

	/*
	 * Create OpenGL sampler.
	 *
	 */
	void Sampler::loadImpl()
	{
		SamplerStream* sStr = dynamic_cast<SamplerStream*>(getResourceStream().get());
		if (!sStr)
		{
			THROW_MPP("Could not cast to type 'SamplerStream'.", __LINE__, __FILE__, __func__);
		}

		uint32_t samplerId;

		// Create: don't need to bind for samplers
		GL_CHECK(glGenSamplers(1, &samplerId));

		GL_CHECK(glSamplerParameteri(samplerId, GL_TEXTURE_WRAP_R, mParams.wrap));
		GL_CHECK(glSamplerParameteri(samplerId, GL_TEXTURE_WRAP_S, mParams.wrap));
		GL_CHECK(glSamplerParameteri(samplerId, GL_TEXTURE_WRAP_T, mParams.wrap));
		GL_CHECK(glSamplerParameteri(samplerId, GL_TEXTURE_MIN_FILTER, mParams.minFilter));
		GL_CHECK(glSamplerParameteri(samplerId, GL_TEXTURE_MAG_FILTER, mParams.magFilter));
		GL_CHECK(glSamplerParameterf(samplerId, GL_TEXTURE_BASE_LEVEL, mParams.lodBaseLevel));
		GL_CHECK(glSamplerParameterf(samplerId, GL_TEXTURE_MAX_LEVEL, mParams.lodMaxLevel));
		GL_CHECK(glSamplerParameterf(samplerId, GL_TEXTURE_LOD_BIAS, mParams.lodBias));

		setId(samplerId);
	}

	/*
	 * Destroy the OpenGL sampler.
	 *
	 */
	void Sampler::unloadImpl()
	{
		GLuint id = getId();
		if (id != 0)
		{
			GL_CHECK(glDeleteSamplers(1, &id));
			setId(0);
		}
	}


	/*
	 * Bind texture.
	 *
	 */
	void Sampler::bind(uint32_t unit)
	{
		if (!isLoaded())
		{
			load();
		}

		GL_CHECK(glBindSampler(unit, getId()));
	}
}