#pragma once

#include "mpp/TextureStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticTextureStream : public TextureStream
	{
	public:

		explicit ProgrammaticTextureStream(ResourceManager* resourceMgr);

		void setTargetFormat(InternalType type, bool normalized, size_t bitSize, size_t channels);

		void setData(uint8_t const* data, size_t width, size_t height, size_t bitsPerPixel, uint32_t pixelFormat, uint32_t dataType);

		void setFile(std::string const& filename, ImageLoadFunction loader);

		void setFiltered(bool filtered);

	};
}