#include "mpp/Memory.h"

namespace mpp
{

	void dumpTrackedMemory()
	{
		utils::MemTracker::dump("mpp.memory.log");
	}

} // mpp