#pragma once

#include <GL/gl.h>

namespace mpp
{

	enum class BlendMode
	{
		Zero = GL_ZERO,
		One = GL_ONE,
		SrcColour = GL_SRC_COLOR,
		OneMinusSrcColour = GL_ONE_MINUS_SRC_COLOR,
		DstColour = GL_DST_COLOR,
		OneMinusDstColour = GL_ONE_MINUS_DST_COLOR,
		SrcAlpha = GL_SRC_ALPHA,
		OneMinusSrcAlpha = GL_ONE_MINUS_SRC_ALPHA,
		DstAlpha = GL_DST_ALPHA,
		OneMinusDstAlpha = GL_ONE_MINUS_DST_ALPHA
	};

}