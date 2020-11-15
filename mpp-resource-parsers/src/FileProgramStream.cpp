#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#	include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/GL.h>

#include "utils/FileSystem.h"

#include "mpp/resource-parsers/FileProgramStream.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"

namespace mpp
{
	namespace resource_parsers
	{

		using namespace std;

		FileProgramStream::FileProgramStream(ResourceManager* resourceMgr, string const& filepath)
			: ProgramStream(resourceMgr)
			, mFilepath(filepath)
		{
		}

		void FileProgramStream::loadImpl()
		{
			// Get file type from extension
			auto fi = utils::FileSystem::getFile(mFilepath);
			auto ext = fi.getExtension();

			auto ser = getSerializer(ext);

			ser->loadFromFile(mFilepath);
			auto const& data = ser->getData();

			// Parse data.  Root element should be 'Program'
			auto rootName = data.getName();

			if (rootName != "Program")
			{
				string errMsg = "Error loading " + mFilepath + ".  Root element is not 'Program'.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "")
				{

				}
				else
				{
					string errMsg = "Error loading " + mFilepath + ".  Unknown element '" + entry.first + "'.";
					THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
				}
			}

			ProgramStream::loadImpl();
		}
	}
}