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

	enum class RenderTextureDepthFormat
	{
		Depth16,
		Depth24,
		Depth32f,
		Depth24Stencil8,
		Depth32fStencil8
	};

	// Depth textures need sampler state even when the render target has no
	// colour attachment, as is the case for a shadow map.
	struct _MPPAPI RenderTextureDepthParams
	{
		TextureParams params;
		bool compareRefToTexture{ false };
	};

	struct _MPPAPI RenderTextureOptions
	{
		size_t numAttachments{ 1 };
		RenderTextureDepthAttachment depthAttachment{ RenderTextureDepthAttachment::None };
		RenderTextureDepthParams depthParams;
		RenderTextureDepthFormat depthFormat{ RenderTextureDepthFormat::Depth24 };
		// Non-zero selects an exact packed/sRGB OpenGL internal format.
		uint32_t colourInternalFormat{ 0 };
		TextureInternalType colourType{ TextureInternalType::UnsignedInteger };
		bool colourNormalised{ true };
		size_t colourBitSize{ 8 };
		size_t colourChannels{ 4 };
		uint32_t samples{ 1 };
		TextureParams params;
	};

	class _MPPAPI RenderTextureStream : public ResourceStream
	{
		struct Definition
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

		Definition mDefinition;

		RenderTextureDepthAttachment mDepthAttachment;
		RenderTextureDepthParams mDepthParams;
		RenderTextureDepthFormat mDepthFormat{ RenderTextureDepthFormat::Depth24 };

		size_t mNumAttachments;
		uint32_t mSamples{ 1 };

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

		RenderTextureDepthParams const& getDepthParams() const;

		RenderTextureDepthFormat getDepthFormat() const;

		size_t getNumAttachments() const;

		uint32_t getSamples() const;

	};
}