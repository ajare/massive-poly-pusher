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
			, mFilepath(filepath)
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
				string errMsg = "Error loading " + mFilepath + ".  Unknown/unsupported min-filter method '" + value + "' specified.";
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
				string errMsg = "Error loading " + mFilepath + ".  Unknown/unsupported mag-filter method '" + value + "' specified.";
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
				string errMsg = "Error loading " + mFilepath + ".  Unknown/unsupported wrap method '" + value + "' specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __FUNCTION__);
			}
		}

		void FileSamplerStream::loadImpl()
		{
			// Get file type from extension
			auto fi = utils::FileSystem::getFile(mFilepath);
			auto ext = fi.getExtension();

			auto ser = getSerializer(ext);

			ser->loadFromFile(mFilepath);
			auto const& data = ser->getData();

			// Parse data.  Root element should be 'Sampler'
			auto rootName = data.getName();

			if (rootName != "Sampler")
			{
				string errMsg = "Error loading " + mFilepath + ".  Root element is not 'Sampler'.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "minFilter")
				{
					mQualitySettings[0].params.minFilter = parseMinFilter(value);
				}
				else if (entry.first == "magFilter")
				{
					mQualitySettings[0].params.magFilter = parseMagFilter(value);
				}
				else if (entry.first == "lodBaseLevel")
				{
					mQualitySettings[0].params.lodBaseLevel = utils::StringUtils::parseFloat(value);
				}
				else if (entry.first == "lodMaxLevel")
				{
					mQualitySettings[0].params.lodMaxLevel = utils::StringUtils::parseFloat(value);
				}
				else if (entry.first == "lodBias")
				{
					mQualitySettings[0].params.lodBias = utils::StringUtils::parseFloat(value);
				}
				else if (entry.first == "wrap")
				{
					mQualitySettings[0].params.wrap = parseWrapping(value);
				}
				else if (entry.first == "maxAnisotropy")
				{
					mQualitySettings[0].params.maxAnisotropy = utils::StringUtils::parseFloat(value);
				}
				else
				{
					string errMsg = "Error loading " + mFilepath + ".  Unknown element '" + entry.first + "'.";
					THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
				}
			}

			SamplerStream::loadImpl();
		}
	}
}