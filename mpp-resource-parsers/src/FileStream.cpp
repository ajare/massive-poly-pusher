#include "mpp/resource-parsers/FileStream.h"
#include "mpp/resource-parsers/XmlSerializer.h"

namespace mpp
{
	namespace resource_parsers
	{

		using namespace std;

		FileStream::FileStream()
		{
			mFactories[".xml"] = []() {return new XmlSerializer(); };
		}

		SerializerPtr FileStream::getSerializer(string const& type)
		{
			auto it = mFactories.find(type);

			if (it == mFactories.end())
			{
				string errMsg = "Can't find serializer of type '" + type + "'.";
				throw exception(errMsg.c_str());
			}

			return shared_ptr<Serializer>(it->second());
		}

	}
}