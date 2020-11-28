#include "utils/FileSystem.h"

#include "mpp/resource-parsers/FileStringStream.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"

namespace mpp
{
	namespace resource_parsers
	{

		using namespace std;

		FileStringStream::FileStringStream(ResourceManager* resourceMgr, string const& filepath)
			: StringStream(resourceMgr)
			, FileStream(filepath)
		{
		}

		FileStringStream::FileStringStream(ResourceManager* resourceMgr, string const& filepath, utils::StructuredData const& data)
			: StringStream(resourceMgr)
			, FileStream(filepath, data)
		{
		}

		void FileStringStream::parseQualitySetting(utils::StructuredData const& data)
		{
			string name, value, file;

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;

				if (entry.first == "name")
				{
					name = entry.second.getValue();
				}
				else if (entry.first == "value")
				{
					value = entry.second.getValue();
				}
				else if (entry.first == "file")
				{
					file = entry.second.getValue();
				}
			}

			if (value == "" & file == "")
			{
				string errMsg = "Error loading " + getFilepath() + ".  Neither 'value' nor 'file' specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}
			if (value != "" && file != "")
			{
				string errMsg = "Error loading " + getFilepath() + ".  Both 'value' and 'file' specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			auto newSettingId = createQualitySetting(name);
			auto &qs = mQualitySettings[newSettingId];

			if (value != "")
			{
				qs.data = value;
			}
			else
			{
				// if the filepath is relative, prepend the path of the xml file
				filesystem::path fp(file);
				if (fp.is_relative())
				{
					filesystem::path fileFp(getFilepath());
					file = utils::FileSystem::concatPaths(fileFp.parent_path().string(), file);
				}

				// Load file
				qs.data = utils::FileSystem::readTextFile(file);
			}
		}

		void FileStringStream::loadImpl()
		{
			auto const& data = getStructuredData();

			// Parse data.  Root element should be 'String'
			auto rootName = data.getName();

			if (rootName != "String" && rootName != "Resource")
			{
				string errMsg = "Error loading " + getFilepath() + ".  Root element is neither 'String' nor 'Resource'.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			parseQualitySetting(data);

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "Quality")
				{
					parseQualitySetting(entry.second);
				}
			}
		}
	}
}