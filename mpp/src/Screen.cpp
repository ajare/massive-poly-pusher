#include <cassert>

#include "mpp/Config.h"

#if MPP_PLATFORM == MPP_PLATFORM_WINDOWS
#include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/gl.h>

#include "mpp/Screen.h"
#include "mpp/GLErrorCheck.h"

namespace mpp
{
	/*
	 * Constructor.
	 *
	 */
	Screen::Screen(size_t width, size_t height)
		: RenderTarget(width, height)
	{
	}

	/*
	 * Set the screen as the active RenderTarget.
	 *
	 */
	void Screen::activate()
	{
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	}

	/*
	 * Unset the screen from being the active RenderTarget.
	 *
	 */
	void Screen::deactivate()
	{
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	}
}

