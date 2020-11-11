#pragma once

#include <string>
#include <memory>

#include "utils/StructuredData.h"

#include "Config.h"

namespace mpp
{
	namespace resource_parsers
	{

		class Serializer
		{
		protected:

			utils::StructuredData mData;

		public:

			Serializer();

			virtual void loadFromFile(std::string const& filepath) = 0;

			utils::StructuredData const& getData() const;
		};

		typedef std::shared_ptr<Serializer> SerializerPtr;

	}
}