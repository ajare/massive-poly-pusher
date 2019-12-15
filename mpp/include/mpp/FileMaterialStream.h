#pragma once

#include "mpp/MaterialStream.h"
#include "mpp/FileDataStream.h"

namespace mpp
{
	class _MPPAPI FileMaterialStream : public MaterialStream
	{
		std::string mXmlDefinition;

	private:

		void loadImpl();

	public:

		explicit FileMaterialStream(FileDataStream const& dataStream);

		explicit FileMaterialStream(std::string const& xmlDef);
	};
}