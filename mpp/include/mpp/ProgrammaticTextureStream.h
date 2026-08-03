#pragma once

#include "mpp/TextureStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticTextureStream : public TextureStream
	{

	public:

		ProgrammaticTextureStream(ResourceManager* resourceMgr, std::string const& type = "Texture");

		void setAtlas(bool isAtlas);

		void setTarget(TextureTarget target, uint32_t quality = 0);

		void setParams(TextureParams const& params, uint32_t quality = 0);

		void setInternalFormat(TextureInternalType type, bool normalized, size_t bitSize, size_t channels, uint32_t quality = 0);

		void setData(ImageLoadFunction creator, uint32_t quality = 0);

		void setFile(std::string const& filename, ImageLoadFunction loader, uint32_t quality = 0);

		void setFiltering(TextureParams::MinFilter minFilter, TextureParams::MagFilter magFilter, uint32_t quality = 0);

		void setWrapping(TextureParams::Wrapping wrapping, uint32_t quality = 0);

		void setColourSpace(TextureColourSpace colourSpace, uint32_t quality = 0);

		void enableMipMaps(bool enable, uint32_t quality = 0);

		void setLodBaseLevel(int32_t level, uint32_t quality = 0);

		void setLodMaxLevel(int32_t level, uint32_t quality = 0);

		void setLodBias(float bias, uint32_t quality = 0);
		
		void setMaxAnisotropy(float maxAnisotropy, uint32_t quality = 0);

		void setSampler(std::string const& sampler, uint32_t quality = 0);

		void setImageLoadFunction(ImageLoadFunction function);

	};
}