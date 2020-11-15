#pragma once

#include "mpp/RenderTextureStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticRenderTextureStream : public RenderTextureStream
	{
	public:

		explicit ProgrammaticRenderTextureStream(ResourceManager* resourceMgr);

		void setWidth(size_t width, uint32_t quality = 0);

		void setHeight(size_t height, uint32_t quality = 0);

		void setDepthBuffer(bool use);

		void setNumAttachments(size_t numAttachments);
	};
}