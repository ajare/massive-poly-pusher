#include <cassert>

#include "mpp/TextureAtlasStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	TextureAtlasStream::TextureAtlasStream(uint8 const* data, int width, int height, int bitsPerPixel, bool filtered, size_t imagesX, size_t imagesY)
		: TextureStream(data, width, height, bitsPerPixel, filtered)
		, mImagesX(imagesX)
		, mImagesY(imagesY)
	{
	}

	size_t TextureAtlasStream::getImagesX() const
	{
		return mImagesX;
	}

	size_t TextureAtlasStream::getImagesY() const
	{
		return mImagesY;
	}
}