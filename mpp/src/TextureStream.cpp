#include <cassert>

#include "mpp/TextureStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	TextureStream::TextureStream(ResourceManager* resourceMgr, uint8 const* data, int width, int height, int bitsPerPixel, bool filtered)
		: ResourceStream(resourceMgr)
		, mData(nullptr)
		, mWidth(width)
		, mHeight(height)
		, mBitsPerPixel(bitsPerPixel)
		, mFiltered(filtered)
	{
		assert((bitsPerPixel == 24 || bitsPerPixel == 32) && "TextureStream::TextureStream() 'bitsPerPixel' is invalid.");

		int dataSize = getDataSize();
		mData = new uint8[dataSize];
		memcpy(mData, data, dataSize);
	}

	TextureStream::TextureStream(ResourceManager* resourceMgr, string const& filename, ImageLoadFunction function, bool filtered)
		: ResourceStream(resourceMgr)
		, mData(nullptr)
		, mWidth(0)
		, mHeight(0)
		, mBitsPerPixel(0)
		, mFiltered(filtered)
	{
		auto textureData = function(filename);
		mData = textureData.data;
		mWidth = textureData.width;
		mHeight = textureData.height;
		mBitsPerPixel = textureData.bitsPerPixel;
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
	 * Load data.  Already loaded in constructor!
	 *
	 */
	void TextureStream::loadImpl()
	{
	}

	/*
	 * Get resource stream type.
	 *
	 */
	string TextureStream::getType()
	{
		return "Texture";
	}

	/*
	 * Get raw texture data.
	 *
	 */
	uint8 const* TextureStream::getData() const
	{
		return mData;
	}

	/*
	 * Get texture width.
	 *
	 */
	int TextureStream::getWidth() const
	{
		return mWidth;
	}

	/*
	 * Get texture height.
	 *
	 */
	int TextureStream::getHeight() const
	{
		return mHeight;
	}

	/*
	 * Get texture bits per pixel.
	 *
	 */
	int TextureStream::getBitsPerPixel() const
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
	int TextureStream::getDataSize() const
	{
		return getWidth() * getHeight() * getBitsPerPixel() / 8;
	}

}