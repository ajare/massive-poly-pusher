#include <vld.h> // Memory tracking

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