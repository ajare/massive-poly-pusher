#pragma once

#include "mpp/ResourceStream.h"

namespace mpp
{
	class _MPPAPI RenderTextureStream : public ResourceStream
	{
		int mWidth, mHeight;

		bool mUseDepthBuffer;

		size_t mNumAttachments;

	private:

		void loadImpl();

	public:

		RenderTextureStream(ResourceManager* resourceMgr, int width, int height, bool useDepthBuffer, size_t numAttachments);

		std::string getType();

		int getWidth() const;

		int getHeight() const;

		bool useDepthBuffer() const;

		size_t getNumAttachments() const;
	};
}