#pragma once

#include "mpp/TextureAtlasStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticTextureAtlasStream : public TextureAtlasStream
	{
	public:

		explicit ProgrammaticTextureAtlasStream(ResourceManager* resourceMgr);

		void setData(uint8_t const* data, size_t width, size_t height, size_t bitsPerPixel);

		void setFile(std::string const& filename, ImageLoadFunction loader);

		void setFiltered(bool filtered);

	};
}