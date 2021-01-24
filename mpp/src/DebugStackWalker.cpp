#include "mpp/DebugStackWalker.h"
#include "mpp/MppException.h"

namespace mpp
{
	using namespace std;

	DebugStackWalker::DebugStackWalker(MppException* exc)
		: mException(exc)
	{
	}

	void DebugStackWalker::OnOutput(LPCSTR szText)
	{
		if (IsDebuggerPresent())
		{
			StackWalker::OnOutput(szText);
		}
		else
		{
			mException->setStackTrace(string(szText));
		}
	}
}