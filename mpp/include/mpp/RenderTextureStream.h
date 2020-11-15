#pragma once

#include <vector>

#include "mpp/ResourceStream.h"

namespace mpp
{
	class _MPPAPI RenderTextureStream : public ResourceStream
	{
		struct QualitySetting
		{
		};

	private:

		int mWidth, mHeight;

		bool mUseDepthBuffer;

		size_t mNumAttachments;

	protected:

		std::vector<QualitySetting> mQualitySettings;

	private:

		void loadImpl();

	public:

		RenderTextureStream(ResourceManager* resourceMgr, int width, int height, bool useDepthBuffer, size_t numAttachments);

		int getWidth() const;

		int getHeight() const;

		bool useDepthBuffer() const;

		size_t getNumAttachments() const;

		uint32_t createQualitySetting(std::string const& name);
	};
}