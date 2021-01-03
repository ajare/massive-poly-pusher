#pragma once

#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#include <Windows.h>
#endif

#include <vector>

#include <glew/glew.h>
#include <gl/gl.h>

#include "mpp/Config.h"
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

		int getWidth() const;

		int getHeight() const;

		int getBitsPerPixel() const;

		bool hasDepthBuffer() const;

		bool hasStencilBuffer() const;

	};
}
