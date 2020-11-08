#include "mpp/resource-parsers/FileTextureStream.h"

namespace mpp
{
	namespace resource_parsers
	{

		using namespace std;

		FileTextureStream::FileTextureStream(ResourceManager* resourceMgr, string const& filepath)
			: TextureStream(resourceMgr)
			, mFilepath(filepath)
		{
		}
		
	}
}