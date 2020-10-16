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

		FileMaterialStream(ResourceManager* resourceMgr, FileDataStream const& dataStream);

		FileMaterialStream(ResourceManager* resourceMgr, std::string const& xmlDef);
	};
}