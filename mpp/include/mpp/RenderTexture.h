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

		size_t mNumAttachments;

		GLuint mFrameBuffer;

		GLuint mDepthBuffer;

		std::vector<GLuint> mTextureIds;

	private:

		void deactivate();

		void activate();

	protected:

		void createImpl();

		void destroyImpl();

		void loadImpl();

		void unloadImpl();

	public:

		RenderTexture(std::string const& name, int width, int height, size_t numAttachments, bool depthBuffer, RenderSystem* renderSystem);

		~RenderTexture();

		int getWidth() const;

		int getHeight() const;

		int getBitsPerPixel() const;

		void bind(int unit);

		void bind(int attachment, int unit);

	};
}
