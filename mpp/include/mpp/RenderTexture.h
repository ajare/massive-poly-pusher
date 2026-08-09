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
	class RenderOutputProcessor;

	class _MPPAPI RenderTexture : public RenderTarget, public Texture
	{
		friend class RenderOutputProcessor;
		RenderSystem* mRenderSystem{ nullptr };

		RenderTextureDepthAttachment mDepthAttachment;
		RenderTextureDepthParams mDepthParams;
		RenderTextureDepthFormat mDepthFormat{ RenderTextureDepthFormat::Depth24 };

		GLuint mFrameBuffer;

		GLuint mDepthBuffer;

		GLuint mDepthTexture;
		uint32_t mSamples{ 1 };
		uint32_t mMipLevels{ 1 };

	private:

		void deactivate();

		void activate();
		void copyDepthTo(RenderTexture* destination);

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
		// `force` generates even when the target did not opt into automatic
		// mipmaps. Cubemap IBL targets render one face at a time, so they must
		// generate once after every face exists rather than on each target pop.
		void generateMipMaps(bool force = false);

		void applyMipView(uint32_t mipLevel);
		void restoreMipView();

		// Selects the cubemap face and mip attached to a colour output. The
		// caller owns render-state/attachment restoration (Phase 4.4).
		void attachColourFace(size_t attachment, uint32_t face, uint32_t mipLevel);
		// Restores every colour attachment to the conventional +X, mip-zero view.
		void restoreColourFaces();
		uint32_t getMipLevels() const override;

		uint32_t getColourAttachmentId(size_t attachment) const;

	};
}
