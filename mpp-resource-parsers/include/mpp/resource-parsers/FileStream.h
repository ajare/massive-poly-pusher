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
			std::string mFilepath;

			mutable utils::StructuredData mData;

			typedef std::function<Serializer*()> SerializerFactory;

			std::map<std::string, SerializerFactory> mFactories;

		private:

			virtual void parseQualitySetting(utils::StructuredData const& data) {};

		protected:

			SerializerPtr getSerializer(std::string const& type) const;

			static std::string readTextFile(std::string const& filepath, std::string const& root);

		public:

			explicit FileStream(std::string const& filepath);

			FileStream(std::string const& filepath, utils::StructuredData const& data);

			std::string const& getFilepath() const;

			utils::StructuredData const& getStructuredData() const;
		};

	}
}