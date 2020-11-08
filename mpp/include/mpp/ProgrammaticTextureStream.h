#pragma once

#include "mpp/TextureStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticTextureStream : public TextureStream
	{
	public:

		explicit ProgrammaticTextureStream(ResourceManager* resourceMgr);

		void setData(uint8_t const* data, size_t width, size_t height, size_t bitsPerPixel);

		void setFile(std::string const& filename, ImageLoadFunction loader);

		void setFiltered(bool filtered);

	};
}