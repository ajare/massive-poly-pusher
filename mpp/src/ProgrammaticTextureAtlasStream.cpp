#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#	include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/GL.h>

#include <cassert>

#include "mpp/ProgrammaticTextureAtlasStream.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{

	ProgrammaticTextureAtlasStream::ProgrammaticTextureAtlasStream(ResourceManager* resourceMgr)
		: ProgrammaticTextureStream(resourceMgr)
	{
	}

}