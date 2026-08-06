#pragma once

#include "mpp/RenderTextureStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticRenderTextureStream : public RenderTextureStream
	{
	public:

		explicit ProgrammaticRenderTextureStream(ResourceManager* resourceMgr);

		void setParams(TextureParams const& params);

		void setTarget(TextureTarget target);

		void setInternalFormat(TextureInternalType type, bool normalized, size_t bitSize, size_t channels);

		void setWidth(size_t width);

		void setHeight(size_t height);

		void setDepth(size_t depth);

		void setFiltering(TextureParams::MinFilter minFilter, TextureParams::MagFilter magFilter);

		void setWrapping(TextureParams::Wrapping wrapping);

		void enableMipMaps(bool enable);

		void setLodBaseLevel(int32_t level);

		void setLodMaxLevel(int32_t level);

		void setLodBias(float bias);

		void setMaxAnisotropy(float maxAnisotropy);

		void setSampler(std::string const& sampler);

		void setDepthBuffer(bool use);

		void setDepthAttachment(RenderTextureDepthAttachment attachment);

		void setDepthParams(RenderTextureDepthParams const& params);

		void setNumAttachments(size_t numAttachments);

		void setSamples(uint32_t samples);
	};
}