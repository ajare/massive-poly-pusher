#pragma once

#include "mpp/ProgrammaticTextureStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticTextureAtlasStream : public ProgrammaticTextureStream
	{
	public:

		explicit ProgrammaticTextureAtlasStream(ResourceManager* resourceMgr);

		void addTile(std::string const& name, float u0, float v0, float u1, float v1);
	};
}