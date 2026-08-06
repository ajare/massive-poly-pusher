#pragma once

#include "mpp/Config.h"

#include <vector>

#include <glew/glew.h>
#include <gl/gl.h>

#include "mpp/RenderTarget.h"
#include "mpp/RenderTextureStream.h"
#include "mpp/Texture.h"

namespace mpp
{
	class _MPPAPI RenderTexture : public RenderTarget, public Texture
	{
		RenderSystem* mRenderSystem{ nullptr };

		RenderTextureDepthAttachment mDepthAttachment;
		RenderTextureDepthParams mDepthParams;

		GLuint mFrameBuffer;

		GLuint mDepthBuffer;

		GLuint mDepthTexture;
		uint32_t mSamples{ 1 };

	private:

		void deactivate();

		void activate();

	protected:

		void createImpl();

		void loadImpl();

		void unloadImpl();

	public:

		RenderTexture(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		~RenderTexture();

		size_t getWidth() const;

		size_t getHeight() const;

		size_t getBitsPerPixel() const;

		bool hasDepthBuffer() const;

		bool hasStencilBuffer() const;

		bool isMultisampled() const;

		uint32_t getSamples() const;

		uint32_t getAttachmentTextureTarget() const;

		void resolveTo(RenderTexture* destination, bool colour, bool depth);

		bool resize(size_t width, size_t height) override;

		uint32_t getDepthTextureId() const;

		void bindDepth(uint32_t unit);

		// Regenerates declared colour/depth mip chains after level-zero writes.
		void generateMipMaps();

		void applyMipView(uint32_t mipLevel);
		void restoreMipView();

		uint32_t getColourAttachmentId(size_t attachment) const;

	};
}
