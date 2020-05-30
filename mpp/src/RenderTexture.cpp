#include <exception>

#include "mpp/Config.h"
#include "mpp/RenderTexture.h"
#include "mpp/MppException.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{
	/*
	* Constructor.
	*
	*/
	RenderTexture::RenderTexture(int width, int height, int numAttachments, bool depthBuffer, RenderSystem* renderSystem)
		: RenderTarget(width, height)
	{
		// Create framebuffer
		GL_CHECK(glGenFramebuffers(1, &mFrameBuffer));
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffer));

		// Create textures
		for (int i = 0; i < numAttachments; ++i)
		{
			GLuint texId;
			GL_CHECK(glGenTextures(1, &texId));
			GL_CHECK(glBindTexture(GL_TEXTURE_2D, texId));

			GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, nullptr));

			GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
			GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));

			GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));

			mTextureIds.push_back(texId);
		}

		// Create depth buffer
		if (depthBuffer)
		{
			GL_CHECK(glGenRenderbuffers(1, &mDepthBuffer));
			GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, mDepthBuffer));

			GL_CHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height));
			GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepthBuffer));
		}
		else
		{
			mDepthBuffer = 0;
		}

		GLenum* drawBuffers = new GLenum[numAttachments]; 
		for (int i = 0; i < numAttachments; ++i)
		{
			GLenum attachment = GL_COLOR_ATTACHMENT0 + i;
			GL_CHECK(glFramebufferTexture(GL_FRAMEBUFFER, attachment, mTextureIds[i], 0));
			drawBuffers[i] = attachment;
		}

		GL_CHECK(glDrawBuffers(numAttachments, drawBuffers));
		delete[] drawBuffers;

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			THROW_MPP("Could not create framebuffer.", __LINE__, __FILE__, __FUNCTION__);
		}

		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	}

	/*
	 * Destructor.
	 *
	 */
	RenderTexture::~RenderTexture()
	{
		GL_CHECK(glDeleteFramebuffers(1, &mFrameBuffer));
	}

	/*
	 * Set the texture as the active RenderTarget.
	 *
	 */
	void RenderTexture::activate()
	{
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffer));
		GL_CHECK(glViewport(0, 0, getWidth(), getHeight()));
	}

	/*
	 * Unset the texture from being the active RenderTarget.
	 *
	 */
	void RenderTexture::deactivate()
	{
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	}

	/*
	 * Override Texture functionality.
	 *
	 */
	int RenderTexture::getBitsPerPixel() const
	{
		return 32;
	}

	/*
	 * Bind as texture for use.
	 *
	 */
	void RenderTexture::bind(int attachment, int unit)
	{
		GL_CHECK(glActiveTexture(GL_TEXTURE0 + unit));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, mTextureIds[attachment]));
	}

}

