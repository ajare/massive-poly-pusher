#pragma once

#include "mpp/TextureStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticTextureStream : public TextureStream
	{

		void setTarget(TextureStream::Target target);

	public:

		explicit ProgrammaticTextureStream(ResourceManager* resourceMgr);

		void setInternalFormat(InternalType type, bool normalized, size_t bitSize, size_t channels);

		void setData(TextureStream::Target target, ImageLoadFunction creator, uint32_t quality = 0);

		void setFile(TextureStream::Target target, std::string const& filename, ImageLoadFunction loader, uint32_t quality = 0);

		void setFiltering(TextureParams::MinFilter minFilter, TextureParams::MagFilter magFilter, uint32_t quality = 0);

		void setWrapping(TextureParams::Wrapping wrapping, uint32_t quality = 0);

		void enableMipMaps(bool enable, uint32_t quality = 0);

		void setLodBaseLevel(float level, uint32_t quality = 0);

		void setLodMaxLevel(float level, uint32_t quality = 0);

		void setLodBias(float bias, uint32_t quality = 0);
		
		void setMaxAnisotropy(float maxAnisotropy, uint32_t quality = 0);

		void setSampler(std::string const& sampler, uint32_t quality = 0);

	};
}