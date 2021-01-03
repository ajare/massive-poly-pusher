#pragma once

#include <vector>

#include "mpp/ResourceStream.h"
#include "mpp/TextureStreamBase.h"
#include "mpp/TextureParams.h"

namespace mpp
{
	class _MPPAPI RenderTextureStream : public ResourceStream, public TextureStreamBase
	{
		struct QualitySetting
		{
			size_t width, height, depth;
			size_t bitsPerPixel;
			uint32_t pixelFormat, pixelDataType;
			TextureParams params;
			std::string sampler;
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

		size_t getDepth() const;

		size_t getBitsPerPixel() const;

		uint32_t getPixelFormat() const;

		uint32_t getPixelDataType() const;

		TextureParams const& getParams() const;

		std::string const& getSampler() const;

		bool useDepthBuffer() const;

		size_t getNumAttachments() const;

		uint32_t createQualitySetting(std::string const& name);
	};
}