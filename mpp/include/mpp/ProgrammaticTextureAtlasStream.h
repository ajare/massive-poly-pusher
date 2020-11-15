#pragma once

#include "mpp/ProgrammaticTextureStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticTextureAtlasStream : public ProgrammaticTextureStream
	{
	public:

		explicit ProgrammaticTextureAtlasStream(ResourceManager* resourceMgr);

		void addTile(string const& name, int x, int y, int w, int h);
	};
}