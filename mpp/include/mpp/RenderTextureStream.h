#pragma once

#include <vector>

#include "mpp/ResourceStream.h"
#include "mpp/TextureStreamBase.h"
#include "mpp/TextureParams.h"

namespace mpp
{
	enum class RenderTextureDepthAttachment
	{
		None,
		DepthRenderbuffer,
		DepthStencilRenderbuffer,
		DepthTexture,
		DepthStencilTexture
	};

	struct _MPPAPI RenderTextureOptions
	{
		size_t numAttachments{ 1 };
		RenderTextureDepthAttachment depthAttachment{ RenderTextureDepthAttachment::None };
		TextureInternalType colourType{ TextureInternalType::UnsignedInteger };
		bool colourNormalised{ true };
		size_t colourBitSize{ 8 };
		size_t colourChannels{ 4 };
		TextureParams params;
	};

	class _MPPAPI RenderTextureStream : public ResourceStream
	{
		struct QualitySetting
		{
			uint32_t internalFormat{ 0 };
			uint32_t target{ 0 };
			size_t width{ 0 }, height{ 0 }, depth{ 0 };
			size_t bitsPerPixel{ 0 };
			uint32_t pixelFormat{ 0 }, pixelDataType{ 0 };
			TextureParams params;
			std::string sampler;
		};

	protected:

		std::vector<QualitySetting> mQualitySettings;

		RenderTextureDepthAttachment mDepthAttachment;

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

		RenderTextureDepthAttachment getDepthAttachment() const;

		size_t getNumAttachments() const;

		uint32_t createQualitySetting(std::string const& name);
	};
}