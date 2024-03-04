#include "mpp/Config.h"

#if MPP_PLATFORM == MPP_PLATFORM_WINDOWS
#include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/gl.h>

#include "mpp/RenderTarget.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	RenderTarget::RenderTarget(size_t width, size_t height)
		: mWidth(width)
		, mHeight(height)
	{
	}

	/*
	 * Get render target width.
	 *
	 */
	size_t RenderTarget::getWidth() const
	{
		return mWidth;
	}

	/*
	 * Get render target height.
	 *
	 */
	size_t RenderTarget::getHeight() const
	{
		return mHeight;
	}
}