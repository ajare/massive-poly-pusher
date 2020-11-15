#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#	include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/GL.h>

#include "mpp/TextureParams.h"

using namespace std;

namespace mpp
{

	TextureParams::TextureParams()
		: minFilter(GL_NEAREST)
		, magFilter(GL_NEAREST)
		, wrap(GL_REPEAT)
		, useMipmaps(false)
		, lodBaseLevel(0.0f)
		, lodMaxLevel(1000.0f)
		, lodBias(0.0f)
		, maxAnisotropy(1.0f)
	{
	}

}