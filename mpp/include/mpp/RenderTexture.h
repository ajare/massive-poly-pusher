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
	class _MPPAPI RenderTexture : public RenderTarget
	{
		GLuint mFrameBuffer;

		GLuint mDepthBuffer;

		std::vector<GLuint> mTextureIds;

	private:

		void deactivate();

		void activate();

	public:

		RenderTexture(int width, int height, int numAttachments, bool depthBuffer, RenderSystem* renderSystem);

		~RenderTexture();

		int getBitsPerPixel() const;

		void bind(int attachment, int unit);

	};
}
