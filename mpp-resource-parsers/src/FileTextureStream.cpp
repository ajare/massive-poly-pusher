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
			, FileStream(filepath)
		{
			setup();
		}

		FileTextureStream::FileTextureStream(ResourceManager* resourceMgr, string const& filepath, utils::StructuredData const& data)
			: TextureStream(resourceMgr)
			, FileStream(filepath, data)
		{
			setup();
		}

		void FileTextureStream::setup()
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

			// Texture targets
			mTargets["1D"] = GL_TEXTURE_1D;
			mTargets["2D"] = GL_TEXTURE_2D;
			mTargets["3D"] = GL_TEXTURE_3D;
			mTargets["CUBEMAP"] = GL_TEXTURE_CUBE_MAP;
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
				string errMsg = "Error loading " + getFilepath() + ".  Unknown/unsupported internal format '" + value + "' specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __FUNCTION__);
			}
		}

		uint32_t FileTextureStream::parseMinFilter(string const& value, string const& filepath)
		{
			// Filtering methods
			map<string, uint32_t> minFilters;

			minFilters["LINEAR"] = GL_LINEAR;
			minFilters["NEAREST"] = GL_NEAREST;
			minFilters["NEAREST_MIPMAP_NEAREST"] = GL_NEAREST_MIPMAP_NEAREST;
			minFilters["NEAREST_MIPMAP_LINEAR"] = GL_NEAREST_MIPMAP_LINEAR;
			minFilters["LINEAR_MIPMAP_NEAREST"] = GL_LINEAR_MIPMAP_NEAREST;
			minFilters["LINEAR_MIPMAP_LINEAR"] = GL_LINEAR_MIPMAP_LINEAR;

			auto it = minFilters.find(value);

			if (it != minFilters.end())
			{
				return it->second;
			}
			else
			{
				string errMsg = "Error loading " + filepath + ".  Unknown/unsupported min-filter method '" + value + "' specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __FUNCTION__);
			}
		}

		uint32_t FileTextureStream::parseMagFilter(string const& value, string const& filepath)
		{
			// Filtering methods
			map<string, uint32_t> magFilters;

			magFilters["LINEAR"] = GL_LINEAR;
			magFilters["NEAREST"] = GL_NEAREST;

			auto it = magFilters.find(value);

			if (it != magFilters.end())
			{
				return it->second;
			}
			else
			{
				string errMsg = "Error loading " + filepath + ".  Unknown/unsupported mag-filter method '" + value + "' specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __FUNCTION__);
			}
		}

		uint32_t FileTextureStream::parseWrapping(string const& value, string const& filepath)
		{
			// Wrapping
			map<string, uint32_t> wrapping;

			wrapping["REPEAT"] = GL_REPEAT;
			wrapping["MIRRORED_REPEAT"] = GL_MIRRORED_REPEAT;
			wrapping["CLAMP_TO_EDGE"] = GL_CLAMP_TO_EDGE;
			wrapping["CLAMP_TO_BORDER"] = GL_CLAMP_TO_BORDER;

			auto it = wrapping.find(value);

			if (it != wrapping.end())
			{
				return it->second;
			}
			else
			{
				string errMsg = "Error loading " + filepath + ".  Unknown/unsupported wrap method '" + value + "' specified.";
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
				string errMsg = "Error loading " + getFilepath() + ".  Unknown/unsupported target '" + value + "' specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __FUNCTION__);
			}
		}

		pair<string, FileTextureStream::QualitySetting> FileTextureStream::parseQualitySetting(utils::StructuredData const& data, ResourceManager* resourceMgr, string const& filepath)
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
						filesystem::path fileFp(filepath);
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
					qs.params.minFilter = parseMinFilter(value, filepath);
				}
				else if (entry.first == "magFilter")
				{
					qs.params.magFilter = parseMagFilter(value, filepath);
				}
				else if (entry.first == "lodBaseLevel")
				{
					qs.params.lodBaseLevel = utils::StringUtils::parseInt(value);
				}
				else if (entry.first == "lodMaxLevel")
				{
					qs.params.lodMaxLevel = utils::StringUtils::parseInt(value);
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
					qs.params.wrap = parseWrapping(value, filepath);
				}
			}

			return make_pair(name, qs);
		}

		void FileTextureStream::loadImpl()
		{
			auto const& data = getStructuredData();

			// Parse data.  Root element should be 'Texture'
			auto rootName = data.getName();

			if (rootName != "Texture" && rootName != "Resource")
			{
				string errMsg = "Error loading " + getFilepath() + ".  Root element is neither 'Texture' nor 'Resource'.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			auto qs = parseQualitySetting(data, getResourceMgr(), getFilepath());
			mQualitySettings[createQualitySetting(qs.first)] = qs.second;

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
					auto qs = parseQualitySetting(entry.second, getResourceMgr(), getFilepath());
					mQualitySettings[createQualitySetting(qs.first)] = qs.second;
				}
			}

			if (mTarget == 0)
			{
				string errMsg = "Error loading " + getFilepath() + ".  'target' not specified.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			for (auto& qs: mQualitySettings)
			{
				if (qs.source == "")
				{
					string errMsg = "Error loading " + getFilepath() + ".  'filename' not specified for quality setting.";
					THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
				}
				
				auto resourceMgr = getResourceMgr();
				if (resourceMgr)
				{
					qs.loadFunc = getResourceMgr()->getImageLoadFunction();
				}
			}

			TextureStream::loadImpl();
		}
	}
}