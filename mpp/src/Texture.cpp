#include "mpp/Config.h"

#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#include <Windows.h>
#endif

#include <cassert>
#include <glew/glew.h>
#include <gl/gl.h>

#include "mpp/RenderSystem.h"
#include "mpp/Texture.h"
#include "mpp/TextureStream.h"
#include "mpp/MppException.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{
	/*
	 * Constructor.
	 *
	 */
	Texture::Texture(string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream)
		: Resource(name, "Texture", renderSystem, resourceMgr, resourceStream)
		, mSortId(0)
		, mInternalFormat(0)
	{
	}

	/*
	 * Create the data required for the program from the resource stream.
	 *
	 */
	void Texture::createImpl()
	{
		TextureStream* tStr = dynamic_cast<TextureStream*>(getResourceStream().get());
		if (!tStr)
		{
			THROW_MPP("Could not cast to type 'TextureStream'.", __LINE__, __FILE__, __func__);
		}

		auto dataSize = tStr->getDataSize();

		delete[] mData.data;
		mData.data = new uint8_t[dataSize];
		memcpy(mData.data, tStr->getData(), dataSize);

		mInternalFormat = tStr->getInternalFormat();
		mTarget = tStr->getTarget();
		mParams = tStr->getParams(0);

		mData.width = tStr->getWidth();
		mData.height = tStr->getHeight();
		mData.bitsPerPixel = tStr->getBitsPerPixel();
		mData.pixelFormat = tStr->getPixelFormat();
		mData.dataType = tStr->getPixelDataType();

		if (mInternalFormat == 0)
		{
			// If we haven't specified the format, work it out from bpp/pixelformat/datatype
			size_t channels{ 0 };

			switch (mData.dataType)
			{
			case GL_BYTE:
				// Signed, normalised
				channels = mData.bitsPerPixel / (sizeof(int8_t) * 8);
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
				channels = mData.bitsPerPixel / (sizeof(uint8_t) * 8);
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
				channels = mData.bitsPerPixel / (sizeof(int16_t) * 8);
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
				channels = mData.bitsPerPixel / (sizeof(uint16_t) * 8);
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
				channels = mData.bitsPerPixel / ((sizeof(float) / 2) * 8);
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
				channels = mData.bitsPerPixel / (sizeof(float) * 8);
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
	}

	/*
	 * Destroy the texture data.
	 *
	 */
	void Texture::destroyImpl()
	{
		delete[] mData.data;
		mData.data = nullptr;

		mData.width = 0;
		mData.height = 0;
		mData.bitsPerPixel = 0;
		mData.pixelFormat = 0;
		mData.dataType = 0;
		mSortId = 0;
	}

	/*
	 * Create OpenGL texture.
	 *
	 */
	void Texture::loadImpl()
	{
		uint32_t texId;

		// Create and bind
		GL_CHECK(glGenTextures(1, &texId));
		GL_CHECK(glBindTexture(mTarget, texId));

		// Set parameters
		switch (mTarget)
		{
		case GL_TEXTURE_3D:
		case GL_TEXTURE_CUBE_MAP:
			GL_CHECK(glTexParameteri(mTarget, GL_TEXTURE_WRAP_R, mParams.wrap));
		case GL_TEXTURE_2D:
			GL_CHECK(glTexParameteri(mTarget, GL_TEXTURE_WRAP_T, mParams.wrap));
		case GL_TEXTURE_1D:
			GL_CHECK(glTexParameteri(mTarget, GL_TEXTURE_MIN_FILTER, mParams.minFilter));
			GL_CHECK(glTexParameteri(mTarget, GL_TEXTURE_MAG_FILTER, mParams.magFilter));
			GL_CHECK(glTexParameteri(mTarget, GL_TEXTURE_WRAP_S, mParams.wrap));
			break;
		default:
			THROW_MPP("Invalid target.", __LINE__, __FILE__, __func__);
		}

		// Set data
		switch (mTarget)
		{
		case GL_TEXTURE_1D:
			if (mData.bitsPerPixel == 24)
			{
				GL_CHECK(glTexImage1D(mTarget, 0, mInternalFormat, mData.width, 0, mData.pixelFormat, mData.dataType, mData.data));
			}
			else if (mData.bitsPerPixel == 32)
			{
				GL_CHECK(glTexImage1D(mTarget, 0, mInternalFormat, mData.width, 0, mData.pixelFormat, mData.dataType, mData.data));
			}
			break;

		case GL_TEXTURE_2D:
			if (mData.bitsPerPixel == 24)
			{
				GL_CHECK(glTexImage2D(mTarget, 0, mInternalFormat, mData.width, mData.height, 0, mData.pixelFormat, mData.dataType, mData.data));
			}
			else if (mData.bitsPerPixel == 32)
			{
				GL_CHECK(glTexImage2D(mTarget, 0, mInternalFormat, mData.width, mData.height, 0, mData.pixelFormat, mData.dataType, mData.data));
			}
			break;

		case GL_TEXTURE_3D:
			if (mData.bitsPerPixel == 24)
			{
				GL_CHECK(glTexImage3D(mTarget, 0, mInternalFormat, mData.width, mData.height, mData.depth, 0, mData.pixelFormat, mData.dataType, mData.data));
			}
			else if (mData.bitsPerPixel == 32)
			{
				GL_CHECK(glTexImage3D(mTarget, 0, mInternalFormat, mData.width, mData.height, mData.depth, 0, mData.pixelFormat, mData.dataType, mData.data));
			}
			break;

		case GL_TEXTURE_CUBE_MAP:
			if (mData.bitsPerPixel == 24)
			{
				//GL_CHECK(glTexImage3D(mTarget, 0, mInternalFormat, mData.width, mData.height, mData.depth, 0, mData.pixelFormat, mData.dataType, mData.data));
			}
			else if (mData.bitsPerPixel == 32)
			{
				//GL_CHECK(glTexImage3D(mTarget, 0, mInternalFormat, mData.width, mData.height, mData.depth, 0, mData.pixelFormat, mData.dataType, mData.data));
			}
			break;

		default:
			THROW_MPP("Invalid target.", __LINE__, __FILE__, __func__);
		}

		GL_CHECK(glBindTexture(mTarget, 0));
		setId(texId);
	}

	/*
	 * Destroy the OpenGL texture.
	 *
	 */
	void Texture::unloadImpl()
	{
		GLuint id = getId();
		if (id != 0)
		{
			GL_CHECK(glDeleteTextures(1, &id));
			setId(0);
		}
	}
	
	/*
	 * Get texture width.
	 *
	 */
	int Texture::getWidth() const
	{
		return mData.width;
	}

	/*
	 * Get texture height.
	 *
	 */
	int Texture::getHeight() const
	{
		return mData.height;
	}

	/*
	 * Get texture bits per pixel.
	 *
	 */
	int Texture::getBitsPerPixel() const
	{
		return mData.bitsPerPixel;
	}

	/*
	 * Bind texture.
	 *
	 */
	void Texture::bind(int unit)
	{
		if (!isLoaded())
		{
			load();
		}

		GL_CHECK(glActiveTexture(GL_TEXTURE0 + unit));
		GL_CHECK(glBindTexture(mTarget, getId()));
	}

	/*
	 * Have a separate id for sorting to the GL id, as the GL id may not fit in however many bytes
	 * we have assigned to texture in the sort key.
	 *
	 */
	void Texture::setSortId(uint32_t sortId)
	{
		mSortId = sortId;
	}

	/*
	 * Get the sort id.
	 *
	 */
	uint32_t Texture::getSortId() const
	{
		return mSortId;
	}

}