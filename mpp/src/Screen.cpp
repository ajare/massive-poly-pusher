#include <cassert>

#include "mpp/Config.h"

#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/gl.h>

#include "mpp/Screen.h"

namespace mpp
{
	/*
	 * Constructor.
	 *
	 */
	Screen::Screen(int width, int height)
		: RenderTarget(width, height)
	{
	}

	/*
	 * Set the screen as the active RenderTarget.
	 *
	 */
	void Screen::activate()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, getWidth(), getHeight());
	}

	/*
	 * Unset the screen from being the active RenderTarget.
	 *
	 */
	void Screen::deactivate()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, getWidth(), getHeight());
	}
}

