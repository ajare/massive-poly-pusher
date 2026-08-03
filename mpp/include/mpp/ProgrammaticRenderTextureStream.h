#pragma once

#include "mpp/RenderTextureStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticRenderTextureStream : public RenderTextureStream
	{
	public:

		explicit ProgrammaticRenderTextureStream(ResourceManager* resourceMgr);

		void setParams(TextureParams const& params, uint32_t quality = 0);

		void setTarget(TextureTarget target, uint32_t quality = 0);

		void setInternalFormat(TextureInternalType type, bool normalized, size_t bitSize, size_t channels, uint32_t quality = 0);

		void setWidth(size_t width, uint32_t quality = 0);

		void setHeight(size_t height, uint32_t quality = 0);

		void setDepth(size_t depth, uint32_t quality = 0);

		void setFiltering(TextureParams::MinFilter minFilter, TextureParams::MagFilter magFilter, uint32_t quality = 0);

		void setWrapping(TextureParams::Wrapping wrapping, uint32_t quality = 0);

		void enableMipMaps(bool enable, uint32_t quality = 0);

		void setLodBaseLevel(int32_t level, uint32_t quality = 0);

		void setLodMaxLevel(int32_t level, uint32_t quality = 0);

		void setLodBias(float bias, uint32_t quality = 0);

		void setMaxAnisotropy(float maxAnisotropy, uint32_t quality = 0);

		void setSampler(std::string const& sampler, uint32_t quality = 0);

		void setDepthBuffer(bool use);

		void setDepthAttachment(RenderTextureDepthAttachment attachment);

		void setDepthParams(RenderTextureDepthParams const& params);

		void setNumAttachments(size_t numAttachments);
	};
}