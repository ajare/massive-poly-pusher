#if _MSC_VER < 1930
#  include <vld.h> // Memory tracking
#endif

#include "mpp/resource-parsers/Serializer.h"

namespace mpp
{
	namespace resource_parsers
	{

		using namespace std;

		Serializer::Serializer()
			: mData("")
		{
		}

		utils::StructuredData const& Serializer::getData() const
		{
			return mData;
		}

	}
}