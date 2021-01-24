#include "mpp/DebugStackWalker.h"
#include "mpp/MppException.h"

namespace mpp
{
	using namespace std;

	DebugStackWalker::DebugStackWalker(string* trace)
		: mTrace(trace)
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
			*mTrace = string(szText);
		}
	}
}