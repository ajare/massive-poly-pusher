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

using namespace std;

namespace mpp
{
	/*
	 * Constructor.
	 *
	 */
	Texture::Texture(string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream)
		: Resource(name, "Texture", renderSystem, resourceMgr, resourceStream)
		, mData(nullptr)
		, mWidth(0)
		, mHeight(0)
		, mBitsPerPixel(0)
		, mFiltered(true)
		, mSortId(0)
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
			throw exception("Texture::createImpl() could not cast to type 'TextureStream'.");
		}

		int dataSize = tStr->getDataSize();

		delete[] mData;
		mData = new uint8[dataSize];
		memcpy(mData, tStr->getData(), dataSize);

		mWidth = tStr->getWidth();
		mHeight = tStr->getHeight();
		mBitsPerPixel = tStr->getBitsPerPixel();
		mFiltered = tStr->isFiltered();
	}

	/*
	 * Destroy the texture data.
	 *
	 */
	void Texture::destroyImpl()
	{
		delete[] mData;
		mData = nullptr;

		mWidth = 0;
		mHeight = 0;
		mBitsPerPixel = 0;
		mSortId = 0;
	}

	/*
	 * Create OpenGL texture.
	 *
	 */
	void Texture::loadImpl()
	{
		uint32 texId;

		glGenTextures(1, &texId);

		glBindTexture(GL_TEXTURE_2D, texId);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mFiltered ? GL_LINEAR : GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mFiltered ? GL_LINEAR : GL_NEAREST);

		if (mBitsPerPixel == 24)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, mWidth, mHeight, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, mData);
		}
		else if (mBitsPerPixel == 32)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mWidth, mHeight, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, mData);
		}

		glBindTexture(GL_TEXTURE_2D, 0);
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
			glDeleteTextures(1, &id);
			setId(0);
		}
	}
	
	/*
	 * Get texture width.
	 *
	 */
	int Texture::getWidth() const
	{
		return mWidth;
	}

	/*
	 * Get texture height.
	 *
	 */
	int Texture::getHeight() const
	{
		return mHeight;
	}

	/*
	 * Get texture bits per pixel.
	 *
	 */
	int Texture::getBitsPerPixel() const
	{
		return mBitsPerPixel;
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

		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_2D, getId());
	}

	/*
	 * Have a separate id for sorting to the GL id, as the GL id may not fit in however many bytes
	 * we have assigned to texture in the sort key.
	 *
	 */
	void Texture::setSortId(uint32 sortId)
	{
		mSortId = sortId;
	}

	/*
	 * Get the sort id.
	 *
	 */
	uint32 Texture::getSortId() const
	{
		return mSortId;
	}

	/*
	 * Set a texel.
	 *
	 */
	void Texture::setTexel(int x, int y, uint8 red, uint8 green, uint8 blue)
	{
		uint8 data[4];
		data[0] = red;
		data[1] = green;
		data[2] = blue;
		data[3] = 255;

		glBindTexture(GL_TEXTURE_2D, getId());

		if (mBitsPerPixel == 24)
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, data);
		}
		else if (mBitsPerPixel == 32)
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}

		glBindTexture(GL_TEXTURE_2D, 0);
	}
}