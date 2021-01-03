#include <exception>

#include "mpp/Config.h"
#include "mpp/ResourceManager.h"
#include "mpp/RenderTexture.h"
#include "mpp/RenderTextureStream.h"
#include "mpp/MppException.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{
	/*
	* Constructor.
	*
	*/
	RenderTexture::RenderTexture(string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream)
		: RenderTarget(0, 0)
		, Texture(name, renderSystem, resourceMgr, resourceStream)
		, mRenderSystem(renderSystem)
		, mUseDepthBuffer(false)
		, mFrameBuffer(0)
		, mDepthBuffer(0)
	{
	}

	/*
	 * Destructor.
	 *
	 */
	RenderTexture::~RenderTexture()
	{
		destroy();
	}

	/*
	 * Create the data required.
	 *
	 */
	void RenderTexture::createImpl()
	{
		RenderTextureStream* rtStr = dynamic_cast<RenderTextureStream*>(getResourceStream().get());
		if (!rtStr)
		{
			THROW_MPP("Could not cast to type 'RenderTextureStream'.", __LINE__, __FILE__, __func__);
		}

		mInternalFormat = rtStr->getInternalFormat();
		mTarget = rtStr->getTarget();
		mParams = rtStr->getParams();

		Texture::mWidth = RenderTarget::mWidth = rtStr->getWidth();
		Texture::mHeight = RenderTarget::mHeight = rtStr->getHeight();
		Texture::mDepth = rtStr->getDepth();
		mBitsPerPixel = rtStr->getBitsPerPixel();
		mPixelFormat = rtStr->getPixelFormat();
		mDataType = rtStr->getPixelDataType();

		auto sampler = rtStr->getSampler();
		if (sampler != "")
		{
			mSampler = getResourceManager()->getResource(sampler);
			mSampler->create();
		}

		if (mInternalFormat == 0)
		{
			// If we haven't specified the format, work it out from bpp/pixelformat/datatype
			size_t channels{ 0 };

			switch (mDataType)
			{
			case GL_BYTE:
				// Signed, normalised
				channels = mBitsPerPixel / (sizeof(int8_t) * 8);
				switch (channels)
				{
				case 1:
					mInternalFormat = GL_R8_SNORM; break;
				case 2:
					mInternalFormat = GL_RG8_SNORM; break;
				case 3:
					mInternalFormat = GL_RGB8_SNORM; break;
				case 4:
					mInternalFormat = GL_RGBA8_SNORM; break;
				}
				break;

			case GL_UNSIGNED_BYTE:
				// Unsigned, normalised
				channels = mBitsPerPixel / (sizeof(uint8_t) * 8);
				switch (channels)
				{
				case 1:
					mInternalFormat = GL_R8; break;
				case 2:
					mInternalFormat = GL_RG8; break;
				case 3:
					mInternalFormat = GL_RGB8; break;
				case 4:
					mInternalFormat = GL_RGBA8; break;
				}
				break;

			case GL_SHORT:
				// Signed, normalised
				channels = mBitsPerPixel / (sizeof(int16_t) * 8);
				switch (channels)
				{
				case 1:
					mInternalFormat = GL_R16_SNORM; break;
				case 2:
					mInternalFormat = GL_RG16_SNORM; break;
				case 3:
					mInternalFormat = GL_RGB16_SNORM; break;
				case 4:
					mInternalFormat = GL_RGBA16_SNORM; break;
				}
				break;

			case GL_UNSIGNED_SHORT:
				// Unsigned, normalised
				channels = mBitsPerPixel / (sizeof(uint16_t) * 8);
				switch (channels)
				{
				case 1:
					mInternalFormat = GL_R16; break;
				case 2:
					mInternalFormat = GL_RG16; break;
				case 3:
					mInternalFormat = GL_RGB16; break;
				case 4:
					mInternalFormat = GL_RGBA16; break;
				}
				break;

			case GL_HALF_FLOAT:
				// Signed, unnormalised
				channels = mBitsPerPixel / ((sizeof(float) / 2) * 8);
				switch (channels)
				{
				case 1:
					mInternalFormat = GL_R16F; break;
				case 2:
					mInternalFormat = GL_RG16F; break;
				case 3:
					mInternalFormat = GL_RGB16F; break;
				case 4:
					mInternalFormat = GL_RGBA16F; break;
				}
				break;

			case GL_FLOAT:
				// Signed, unnormalised
				channels = mBitsPerPixel / (sizeof(float) * 8);
				switch (channels)
				{
				case 1:
					mInternalFormat = GL_R32F; break;
				case 2:
					mInternalFormat = GL_RG32F; break;
				case 3:
					mInternalFormat = GL_RGB32F; break;
				case 4:
					mInternalFormat = GL_RGBA32F; break;
				}
				break;

			default:
				THROW_MPP("Unsupported data type.", __LINE__, __FILE__, __func__);
			}
		}

		mUseDepthBuffer = rtStr->useDepthBuffer();
		mNumAttachments = rtStr->getNumAttachments();
	}

	/*
	 * Create OpenGL rendertexture.
	 *
	 */
	void RenderTexture::loadImpl()
	{
		auto width = getWidth();
		auto height = getHeight();

		// Create framebuffer
		GL_CHECK(glGenFramebuffers(1, &mFrameBuffer));
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffer));

		// Create textures
		for (size_t i = 0; i < mNumAttachments; ++i)
		{
			GLuint texId;
			GL_CHECK(glGenTextures(1, &texId));
			GL_CHECK(glBindTexture(GL_TEXTURE_2D, texId));

			GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));

			GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
			GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));

			GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));

			mTextureIds.push_back(texId);
		}

		setId(mTextureIds.front());

		// Create depth buffer
		if (mUseDepthBuffer)
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

		GLenum* drawBuffers = new GLenum[mNumAttachments];
		for (size_t i = 0; i < mNumAttachments; ++i)
		{
			GLenum attachment = GL_COLOR_ATTACHMENT0 + i;
			GL_CHECK(glFramebufferTexture(GL_FRAMEBUFFER, attachment, mTextureIds[i], 0));
			drawBuffers[i] = attachment;
		}

		GL_CHECK(glDrawBuffers(mNumAttachments, drawBuffers));
		delete[] drawBuffers;

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			THROW_MPP("Could not create framebuffer.", __LINE__, __FILE__, __func__);
		}

		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	}

	/*
	 * Destroy the OpenGL rendertexture.
	 *
	 */
	void RenderTexture::unloadImpl()
	{
		if (mFrameBuffer != 0)
		{
			GL_CHECK(glDeleteFramebuffers(1, &mFrameBuffer));
			mFrameBuffer = 0;
		}
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
	 * Get render target width.
	 *
	 */
	int RenderTexture::getWidth() const
	{
		return RenderTarget::getWidth();
	}

	/*
	 * Get render target height.
	 *
	 */
	int RenderTexture::getHeight() const
	{
		return RenderTarget::getHeight();
	}

	/*
	 * Override Texture functionality.
	 *
	 */
	int RenderTexture::getBitsPerPixel() const
	{
		return 32;
	}

	bool RenderTexture::hasDepthBuffer() const
	{
		return mUseDepthBuffer;
	}

	bool RenderTexture::hasStencilBuffer() const
	{
		return false;
	}
}

