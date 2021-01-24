#pragma once

#include "mpp/StackWalker.h"

namespace mpp
{

	class MppException;

	class DebugStackWalker : public StackWalker
	{
		MppException* mException;

	protected:

		void OnOutput(LPCSTR szText);

	public:

		explicit DebugStackWalker(MppException* exc);
	};
}
