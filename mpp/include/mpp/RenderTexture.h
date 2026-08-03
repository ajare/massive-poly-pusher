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

		GLuint mFrameBuffer;

		GLuint mDepthBuffer;

		GLuint mDepthTexture;

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

		bool resize(size_t width, size_t height) override;

		uint32_t getDepthTextureId() const;

		uint32_t getColourAttachmentId(size_t attachment) const;

	};
}
