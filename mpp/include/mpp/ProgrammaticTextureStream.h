#pragma once

#include "mpp/TextureStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticTextureStream : public TextureStream
	{

		void setTarget(TextureStream::Target target);

	public:

		explicit ProgrammaticTextureStream(ResourceManager* resourceMgr);

		void setTargetFormat(InternalType type, bool normalized, size_t bitSize, size_t channels);

		void setData(TextureStream::Target target, uint8_t const* data, size_t width, size_t height, size_t bitsPerPixel, uint32_t pixelFormat, uint32_t dataType);

		void setFile(TextureStream::Target target, std::string const& filename, ImageLoadFunction loader);

		void setFiltered(bool filtered);

	};
}