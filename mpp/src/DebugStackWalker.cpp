#ifdef MPP_DEBUG_BUILD

#include "mpp/DebugStackWalker.h"
#include "mpp/RenderSystem.h"

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	DebugStackWalker::DebugStackWalker(RenderSystem* renderSystem)
		: mwRenderSystem(renderSystem)
	{
	}

	/*
	 * Overridden from StackWalker.
	 *
	 */
	void DebugStackWalker::OnOutput(LPCSTR szText)
	{
		mwRenderSystem->logMessage(std::string(szText));
	}

}

#endif