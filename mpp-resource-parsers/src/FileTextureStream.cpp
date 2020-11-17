#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#	include <Windows.h>
#endif

#include <filesystem>

#include <glew/glew.h>
#include <gl/GL.h>

#include "utils/FileSystem.h"

#include "mpp/resource-parsers/FileTextureStream.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"

namespace mpp
{
	namespace resource_parsers
	{

		using namespace std;

		FileTextureStream::FileTextureStream(ResourceManager* resourceMgr, string const& filepath)
			: TextureStream(resourceMgr)
			, mFilepath(filepath)
		{
			// Internal formats
			mInternalFormats["R8_SNORM"] = GL_R8_SNORM;
			mInternalFormats["RG8_SNORM"] = GL_RG8_SNORM;
			mInternalFormats["RGB8_SNORM"] = GL_RGB8_SNORM;
			mInternalFormats["RGBA8_SNORM"] = GL_RGBA8_SNORM;
			mInternalFormats["R16_SNORM"] = GL_RG16_SNORM;
			mInternalFormats["RG16_SNORM"] = GL_RGB16_SNORM;
			mInternalFormats["RGB16_SNORM"] = GL_RGB16_SNORM;
			mInternalFormats["RGBA16_SNORM"] = GL_RGBA16_SNORM;
			mInternalFormats["R8"] = GL_R8;
			mInternalFormats["RG8"] = GL_RG8;
			mInternalFormats["RGB8"] = GL_RGB8;
			mInternalFormats["RGBA8"] = GL_RGBA8;
			mInternalFormats["R16"] = GL_R16;
			mInternalFormats["RG16"] = GL_RG16;
			mInternalFormats["RGB16"] = GL_RGB16;
			mInternalFormats["RGBA16"] = GL_RGBA16;
			mInternalFormats["R16F"] = GL_R16F;
			mInternalFormats["RG16F"] = GL_RG16F;
			mInternalFormats["RGB16F"] = GL_RGB16F;
			mInternalFormats["RGBA16F"] = GL_RGBA16F;
			mInternalFormats["R32F"] = GL_R32F;
			mInternalFormats["RG32F"] = GL_RG32F;
			mInternalFormats["RGB32F"] = GL_RGB32F;
			mInternalFormats["RGBA32F"] = GL_RGBA32F;

			// Filtering methods
			mMinFilters["LINEAR"] = GL_LINEAR;
			mMinFilters["NEAREST"] = GL_NEAREST;
			mMinFilters["NEAREST_MIPMAP_NEAREST"] = GL_NEAREST_MIPMAP_NEAREST;
			mMinFilters["NEAREST_MIPMAP_LINEAR"] = GL_NEAREST_MIPMAP_LINEAR;
			mMinFilters["LINEAR_MIPMAP_NEAREST"] = GL_LINEAR_MIPMAP_NEAREST;
			mMinFilters["LINEAR_MIPMAP_LINEAR"] = GL_LINEAR_MIPMAP_LINEAR;

			mMagFilters["LINEAR"] = GL_LINEAR;
			mMagFilters["NEAREST"] = GL_NEAREST;

			// Texture targets
			mTargets["1D"] = GL_TEXTURE_1D;
			mTargets["2D"] = GL_TEXTURE_2D;
			mTargets["3D"] = GL_TEXTURE_3D;
			mTargets["CUBEMAP"] = GL_TEXTURE_CUBE_MAP;

			// Wrapping
			mWrapping["REPEAT"] = GL_REPEAT;
			mWrapping["MIRRORED_REPEAT"] = GL_MIRRORED_REPEAT;
			mWrapping["CLAMP_TO_EDGE"] = GL_CLAMP_TO_EDGE;
			mWrapping["CLAMP_TO_BORDER"] = GL_CLAMP_TO_BORDER;
		}

		uint32_t FileTextureStream::parseInternalFormat(string const& value)
		{
			auto it = mInternalFormats.find(value);
			
			if (it != mInternalFormats.end())
			{
				return it->second;
			}
			else
			{
				string errMsg = "Error loading " + mFilepath + ".  Unknown/unsupported internal format '" + value + "' specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __FUNCTION__);
			}
		}

		uint32_t FileTextureStream::parseMinFilter(string const& value)
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

		uint32_t FileTextureStream::parseMagFilter(string const& value)
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

		uint32_t FileTextureStream::parseWrapping(string const& value)
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

		uint32_t FileTextureStream::parseTarget(string const& value)
		{
			auto it = mTargets.find(value);

			if (it != mTargets.end())	
			{
				return it->second;
			}
			else
			{
				string errMsg = "Error loading " + mFilepath + ".  Unknown/unsupported target '" + value + "' specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __FUNCTION__);
			}
		}

		void FileTextureStream::parseQualitySetting(utils::StructuredData const& data)
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
				else if (entry.first == "filename")
				{
					// If filename is specified, then load from disk
					string filepath = entry.second.getValue();

					// if the filepath is relative, prepend the path of the xml file
					filesystem::path fp(filepath);
					if (fp.is_relative())
					{
						filesystem::path fileFp(mFilepath);
						filepath = utils::FileSystem::concatPaths(fileFp.parent_path().string(), filepath);
					}

					qs.source = filepath;
				}
				else if (entry.first == "sampler")
				{
					qs.sampler = entry.second.getValue();
				}
				else if (entry.first == "mipmaps")
				{
					qs.params.useMipmaps = utils::StringUtils::parseBool(value);
				}
				else if (entry.first == "minFilter")
				{
					qs.params.minFilter = parseMinFilter(value);
				}
				else if (entry.first == "magFilter")
				{
					qs.params.magFilter = parseMagFilter(value);
				}
				else if (entry.first == "lodBaseLevel")
				{
					qs.params.lodBaseLevel = utils::StringUtils::parseFloat(value);
				}
				else if (entry.first == "lodMaxLevel")
				{
					qs.params.lodMaxLevel = utils::StringUtils::parseFloat(value);
				}
				else if (entry.first == "lodBias")
				{
					qs.params.lodBias = utils::StringUtils::parseFloat(value);
				}
				else if (entry.first == "maxAnisotropy")
				{
					qs.params.maxAnisotropy = utils::StringUtils::parseFloat(value);
				}
				else if (entry.first == "wrap")
				{
					qs.params.wrap = parseWrapping(value);
				}
			}

			auto newSettingId = createQualitySetting(name);
			mQualitySettings[newSettingId] = qs;
		}

		void FileTextureStream::loadImpl()
		{
			// Get file type from extension
			auto fi = utils::FileSystem::getFile(mFilepath);
			auto ext = fi.getExtension();

			auto ser = getSerializer(ext);

			ser->loadFromFile(mFilepath);
			auto const& data = ser->getData();

			// Parse data.  Root element should be 'Texture'
			auto rootName = data.getName();

			if (rootName != "Texture")
			{
				string errMsg = "Error loading " + mFilepath + ".  Root element is not 'Texture'.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			parseQualitySetting(data);

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "target")
				{
					mTarget = parseTarget(value);
				}
				else if (entry.first == "internalFormat")
				{
					// Optionally, specify OpenGL internal format.  Otherwise calculate from loaded image.
					mInternalFormat = parseInternalFormat(value);
				}
				else if (entry.first == "Quality")
				{
					parseQualitySetting(entry.second);
				}
			}

			if (mTarget == 0)
			{
				string errMsg = "Error loading " + mFilepath + ".  'target' not specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			for (auto& qs: mQualitySettings)
			{
				if (qs.source == "")
				{
					string errMsg = "Error loading " + mFilepath + ".  'filename' not specified for quality setting.";
					THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
				}
				
				qs.loadFunc = getResourceMgr()->getImageLoadFunction();
			}

			TextureStream::loadImpl();
		}
	}
}