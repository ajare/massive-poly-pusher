#include "utils/FileSystem.h"

#include "mpp/DefaultShaders.h"

#include "mpp/resource-parsers/FileMaterialStream.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"

namespace mpp
{
	namespace resource_parsers
	{

		using namespace std;

		FileMaterialStream::FileMaterialStream(ResourceManager* resourceMgr, string const& filepath)
			: MaterialStream(resourceMgr)
			, mFilepath(filepath)
		{
		}

		void FileMaterialStream::parseQualitySetting(utils::StructuredData const& data)
		{
			string name;

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "name")
				{
					name = entry.second.getValue();
				}
				else if (entry.first == "Program")
				{
					// This can either be a reference to another resource, or
					// an actual program definition.
				}
				else if (entry.first == "Textures")
				{
					auto const& textures = it->second;
					for (auto tit = textures.begin(); tit != textures.end(); ++tit)
					{
						auto const& entry = *tit;

						if (entry.first == "Texture")
						{
							// This can either be a reference to another resource, or
							// an actual texture definition.
						}
					}
				}
				else if (entry.first == "Uniforms")
				{
					auto const& uniforms = it->second;
					for (auto uit = uniforms.begin(); uit != uniforms.end(); ++uit)
					{
						auto const& entry = *uit;

						if (entry.first == "Uniform")
						{
							// Parse uniform
						}
					}
				}
			}

			auto newSettingId = createQualitySetting(name);

			//QualitySetting qs;
			//qs.parser = make_shared<program::Parser>();

			//mQualitySettings[newSettingId] = qs;
		}

		void FileMaterialStream::loadImpl()
		{
			// Get file type from extension
			auto fi = utils::FileSystem::getFile(mFilepath);
			auto ext = fi.getExtension();

			auto ser = getSerializer(ext);

			ser->loadFromFile(mFilepath);
			auto const& data = ser->getData();

			// Parse data.  Root element should be 'Material'
			auto rootName = data.getName();

			if (rootName != "Material")
			{
				string errMsg = "Error loading " + mFilepath + ".  Root element is not 'Material'.";
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