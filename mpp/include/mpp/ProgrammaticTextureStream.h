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

		void setData(TextureStream::Target target, ImageLoadFunction creator);

		void setFile(TextureStream::Target target, std::string const& filename, ImageLoadFunction loader);

		void setFiltering(TextureParams::MinFilter minFilter, TextureParams::MagFilter magFilter);

		void setWrapping(TextureParams::Wrapping wrapping);

		void enableMipMaps(bool enable);

		void setLodBaseLevel(float level);

		void setLodMaxLevel(float level);

		void setLodBias(float bias);

	};
}