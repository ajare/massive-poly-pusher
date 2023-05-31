#pragma once

#include "mpp/Config.h"

#include <vector>

#include <glew/glew.h>
#include <gl/gl.h>

#include "mpp/RenderTarget.h"
#include "mpp/Texture.h"

namespace mpp
{
	class _MPPAPI RenderTexture : public RenderTarget, public Texture
	{
		RenderSystem* mRenderSystem{ nullptr };

		bool mUseDepthBuffer;

		GLuint mFrameBuffer;

		GLuint mDepthBuffer;

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

		GLuint getFrameBuffer() const;

		bool hasDepthBuffer() const;

		bool hasStencilBuffer() const;

	};
}
