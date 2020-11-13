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
	{
	}

	/*
	* Destructor.
	*
	*/
	TextureStream::~TextureStream()
	{
		delete[] mData.data;
	}

	/*
	 * Load data.
	 *
	 */
	void TextureStream::loadImpl()
	{
		if (mLoadFunc)
		{
			mData = mLoadFunc(mSource);
		}
	}

	/*
	 * Get raw texture data.
	 *
	 */
	uint8_t const* TextureStream::getData() const
	{
		return mData.data;
	}

	/*
	 * Get texture width.
	 *
	 */
	size_t TextureStream::getWidth() const
	{
		return mData.width;
	}

	/*
	 * Get texture height.
	 *
	 */
	size_t TextureStream::getHeight() const
	{
		return mData.height;
	}

	/*
	 * Get texture bits per pixel.
	 *
	 */
	size_t TextureStream::getBitsPerPixel() const
	{
		return mData.bitsPerPixel;
	}

	uint32_t TextureStream::getPixelFormat() const
	{
		return mData.pixelFormat;
	}

	uint32_t TextureStream::getPixelDataType() const
	{
		return mData.dataType;
	}

	/*
	 * Get texture data size in bytes.
	 *
	 */
	size_t TextureStream::getDataSize() const
	{
		return getWidth() * getHeight() * getBitsPerPixel() / 8;
	}

	TextureParams const& TextureStream::getParams() const
	{
		return mParams;
	}
}