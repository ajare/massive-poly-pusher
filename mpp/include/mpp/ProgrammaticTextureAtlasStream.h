#pragma once

#include "mpp/ProgrammaticTextureStream.h"

namespace mpp
{
	class _MPPAPI ProgrammaticTextureAtlasStream : public ProgrammaticTextureStream
	{
	public:

		explicit ProgrammaticTextureAtlasStream(ResourceManager* resourceMgr);
	};
}