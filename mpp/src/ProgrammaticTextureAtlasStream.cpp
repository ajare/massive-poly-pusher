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

	void ProgrammaticTextureAtlasStream::addTile(string const& name, float u0, float v0, float u1, float v1)
	{
		Tile t;

		t.u[0] = u0;
		t.v[0] = v0;

		t.u[1] = u1;
		t.v[1] = v1;

		mTiles[name] = t;
	}
}