#pragma once

#ifdef MPP_DEBUG_BUILD

#include "mpp/StackWalker.h"

namespace mpp
{
	class RenderSystem;

	class DebugStackWalker : public StackWalker
	{
		RenderSystem* mwRenderSystem;
	
	protected:

		virtual void OnOutput(LPCSTR szText);

	public:

		explicit DebugStackWalker(RenderSystem* renderSystem);
	};

}

#endif