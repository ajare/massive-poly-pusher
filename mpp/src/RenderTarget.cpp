#include "mpp/Config.h"

#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/gl.h>

#include "mpp/RenderTarget.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	RenderTarget::RenderTarget(int width, int height)
		: mWidth(width)
		, mHeight(height)
	{
	}

	/*
	 * Destructor.
	 *
	 */
	RenderTarget::~RenderTarget()
	{
	}

	/*
	 * Set dimensions.
	 *
	 */
	void RenderTarget::setDimensions(int width, int height)
	{
		mWidth = width;
		mHeight = height;
		glViewport(0, 0, mWidth, mHeight);
	}

	/*
	 * Get render target width.
	 *
	 */
	int RenderTarget::getWidth() const
	{
		return mWidth;
	}

	/*
	 * Get render target height.
	 *
	 */
	int RenderTarget::getHeight() const
	{
		return mHeight;
	}
}