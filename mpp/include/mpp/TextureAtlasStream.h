#pragma once

#include "mpp/TextureStream.h"

namespace mpp
{
	class _MPPAPI TextureAtlasStream : public TextureStream
	{
		size_t mImagesX, mImagesY;

	public:

		TextureAtlasStream(uint8 const* data, int width, int height, int bitsPerPixel, bool filtered, size_t imagesX, size_t imagesY);

		size_t getImagesX() const;

		size_t getImagesY() const;
	};
}