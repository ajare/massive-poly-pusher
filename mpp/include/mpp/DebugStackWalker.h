#pragma once

#include <string>

#include "mpp/StackWalker.h"

namespace mpp
{

	class DebugStackWalker : public StackWalker
	{
		std::string* mTrace;

	protected:

		void OnOutput(LPCSTR szText);

	public:

		explicit DebugStackWalker(std::string* trace);
	};
}
