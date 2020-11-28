#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#	include <Windows.h>
#endif

#include <filesystem>

#include <glew/glew.h>
#include <gl/GL.h>

#include "utils/FileSystem.h"

#include "mpp/resource-parsers/FileSamplerStream.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"

namespace mpp
{
	namespace resource_parsers
	{

		using namespace std;

		FileSamplerStream::FileSamplerStream(ResourceManager* resourceMgr, string const& filepath)
			: SamplerStream(resourceMgr)
			, FileStream(filepath)
		{
			setup();
		}

		FileSamplerStream::FileSamplerStream(ResourceManager* resourceMgr, string const& filepath, utils::StructuredData const& data)
			: SamplerStream(resourceMgr)
			, FileStream(filepath, data)
		{
			setup();
		}

		void FileSamplerStream::setup()
		{
			// Filtering methods
			mMinFilters["LINEAR"] = GL_LINEAR;
			mMinFilters["NEAREST"] = GL_NEAREST;
			mMinFilters["NEAREST_MIPMAP_NEAREST"] = GL_NEAREST_MIPMAP_NEAREST;
			mMinFilters["NEAREST_MIPMAP_LINEAR"] = GL_NEAREST_MIPMAP_LINEAR;
			mMinFilters["LINEAR_MIPMAP_NEAREST"] = GL_LINEAR_MIPMAP_NEAREST;
			mMinFilters["LINEAR_MIPMAP_LINEAR"] = GL_LINEAR_MIPMAP_LINEAR;

			mMagFilters["LINEAR"] = GL_LINEAR;
			mMagFilters["NEAREST"] = GL_NEAREST;

			// Wrapping
			mWrapping["REPEAT"] = GL_REPEAT;
			mWrapping["MIRRORED_REPEAT"] = GL_MIRRORED_REPEAT;
			mWrapping["CLAMP_TO_EDGE"] = GL_CLAMP_TO_EDGE;
			mWrapping["CLAMP_TO_BORDER"] = GL_CLAMP_TO_BORDER;
		}

		uint32_t FileSamplerStream::parseMinFilter(string const& value)
		{
			auto it = mMinFilters.find(value);

			if (it != mMinFilters.end())
			{
				return it->second;
			}
			else
			{
				string errMsg = "Error loading " + getFilepath() + ".  Unknown/unsupported min-filter method '" + value + "' specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __FUNCTION__);
			}
		}

		uint32_t FileSamplerStream::parseMagFilter(string const& value)
		{
			auto it = mMagFilters.find(value);

			if (it != mMagFilters.end())
			{
				return it->second;
			}
			else
			{
				string errMsg = "Error loading " + getFilepath() + ".  Unknown/unsupported mag-filter method '" + value + "' specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __FUNCTION__);
			}
		}

		uint32_t FileSamplerStream::parseWrapping(string const& value)
		{
			auto it = mWrapping.find(value);

			if (it != mWrapping.end())
			{
				return it->second;
			}
			else
			{
				string errMsg = "Error loading " + getFilepath() + ".  Unknown/unsupported wrap method '" + value + "' specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __FUNCTION__);
			}
		}

		void FileSamplerStream::parseQualitySetting(utils::StructuredData const& data)
		{
			string name;
			QualitySetting qs;

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "name")
				{
					name = entry.second.getValue();
				}
				else if (entry.first == "minFilter")
				{
					qs.params.minFilter = parseMinFilter(value);
				}
				else if (entry.first == "magFilter")
				{
					qs.params.magFilter = parseMagFilter(value);
				}
				else if (entry.first == "lodMinLevel")
				{
					qs.params.lodMinLevel = utils::StringUtils::parseFloat(value);
				}
				else if (entry.first == "lodMaxLevel")
				{
					qs.params.lodMaxLevel = utils::StringUtils::parseFloat(value);
				}
				else if (entry.first == "lodBias")
				{
					qs.params.lodBias = utils::StringUtils::parseFloat(value);
				}
				else if (entry.first == "wrap")
				{
					qs.params.wrap = parseWrapping(value);
				}
				else if (entry.first == "maxAnisotropy")
				{
					qs.params.maxAnisotropy = utils::StringUtils::parseFloat(value);
				}
			}

			auto newSettingId = createQualitySetting(name);
			mQualitySettings[newSettingId] = qs;
		}

		void FileSamplerStream::loadImpl()
		{
			auto const& data = getStructuredData();

			// Parse data.  Root element should be 'Sampler'
			auto rootName = data.getName();

			if (rootName != "Sampler" && rootName != "Resource")
			{
				string errMsg = "Error loading " + getFilepath() + ".  Root element is neither 'Sampler' nor 'Resource'.";
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

			SamplerStream::loadImpl();
		}
	}
}