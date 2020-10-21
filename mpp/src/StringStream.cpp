#include "mpp/StringStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	StringStream::StringStream(ResourceManager* resourceMgr)
		: ResourceStream(resourceMgr)
	{
	}

	/*
	 * Get resource type.
	 *
	 */
	std::string StringStream::getType()
	{
		return "String";
	}

}