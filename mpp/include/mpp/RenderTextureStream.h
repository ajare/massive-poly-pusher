#pragma once

#include <vector>

#include "mpp/ResourceStream.h"
#include "mpp/TextureStreamBase.h"
#include "mpp/TextureParams.h"

namespace mpp
{
	class _MPPAPI RenderTextureStream : public ResourceStream
	{
		struct QualitySetting
		{
			uint32_t internalFormat{ 0 };
			uint32_t target{ 0 };
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

		uint32_t getInternalFormat() const;

		uint32_t getTarget() const;

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