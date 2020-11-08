#include <cassert>

#include "mpp/TextureStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	TextureStream::TextureStream(ResourceManager* resourceMgr, string streamType)
		: ResourceStream(resourceMgr, streamType)
		, mData(nullptr)
		, mWidth(0)
		, mHeight(0)
		, mBitsPerPixel(0)
		, mFiltered(false)
	{
	}

	/*
	* Destructor.
	*
	*/
	TextureStream::~TextureStream()
	{
		delete[] mData;
	}

	/*
	 * Load data.
	 *
	 */
	void TextureStream::loadImpl()
	{
		if (mLoadFunc)
		{
			auto textureData = mLoadFunc(mSource);
			mData = textureData.data;
			mWidth = textureData.width;
			mHeight = textureData.height;
			mBitsPerPixel = textureData.bitsPerPixel;
		}
	}

	/*
	 * Get raw texture data.
	 *
	 */
	uint8_t const* TextureStream::getData() const
	{
		return mData;
	}

	/*
	 * Get texture width.
	 *
	 */
	size_t TextureStream::getWidth() const
	{
		return mWidth;
	}

	/*
	 * Get texture height.
	 *
	 */
	size_t TextureStream::getHeight() const
	{
		return mHeight;
	}

	/*
	 * Get texture bits per pixel.
	 *
	 */
	size_t TextureStream::getBitsPerPixel() const
	{
		return mBitsPerPixel;
	}

	/*
	 * Should the texture be filtered or not.
	 *
	 */
	bool TextureStream::isFiltered() const
	{
		return mFiltered;
	}

	/*
	 * Get texture data size in bytes.
	 *
	 */
	size_t TextureStream::getDataSize() const
	{
		return getWidth() * getHeight() * getBitsPerPixel() / 8;
	}

}