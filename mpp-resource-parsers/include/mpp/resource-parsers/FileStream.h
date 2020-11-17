#pragma once

#include <string>
#include <map>
#include <functional>

#include "Config.h"
#include "Serializer.h"

namespace mpp
{
	namespace resource_parsers
	{

		class _MPPRESOURCEPARSERSAPI FileStream
		{
			typedef std::function<Serializer*()> SerializerFactory;

			std::map<std::string, SerializerFactory> mFactories;

		private:

			virtual void parseQualitySetting(utils::StructuredData const& data) = 0;

		protected:

			SerializerPtr getSerializer(std::string const& type);

		public:

			FileStream();
		};

	}
}