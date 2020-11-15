#pragma once

#include <vector>

#include "mpp/ResourceStream.h"

namespace mpp
{
	class _MPPAPI RenderTextureStream : public ResourceStream
	{
		struct QualitySetting
		{
			size_t width, height;
		};

	protected:

		std::vector<QualitySetting> mQualitySettings;

		bool mUseDepthBuffer;

		size_t mNumAttachments;

	private:

		void loadImpl();

	public:

		RenderTextureStream(ResourceManager* resourceMgr);

		size_t getWidth() const;

		size_t getHeight() const;

		bool useDepthBuffer() const;

		size_t getNumAttachments() const;

		uint32_t createQualitySetting(std::string const& name);
	};
}