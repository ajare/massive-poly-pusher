#include "utils/FileSystem.h"

#include "mpp/DefaultShaders.h"
#include "mpp/ProgrammaticProgramStream.h"
#include "mpp/ProgrammaticTextureStream.h"

#include "mpp/resource-parsers/FileMaterialStream.h"
#include "mpp/resource-parsers/FileProgramStream.h"
#include "mpp/resource-parsers/FileTextureStream.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"

namespace mpp
{
	namespace resource_parsers
	{

		using namespace std;

		FileMaterialStream::FileMaterialStream(ResourceManager* resourceMgr, string const& filepath)
			: MaterialStream(resourceMgr)
			, FileStream(filepath)
			, mUseSpecifiedMeshSpec(false)
		{
		}

		FileMaterialStream::FileMaterialStream(ResourceManager* resourceMgr, string const& filepath, utils::StructuredData const& data)
			: MaterialStream(resourceMgr)
			, FileStream(filepath, data)
			, mUseSpecifiedMeshSpec(false)
		{
		}

		FileMaterialStream::FileMaterialStream(ResourceManager* resourceMgr, string const& filepath, mesh::MeshSpecification const& meshSpec)
			: MaterialStream(resourceMgr)
			, FileStream(filepath)
			, mUseSpecifiedMeshSpec(true)
			, mMeshSpec(meshSpec)
		{
		}

		FileMaterialStream::FileMaterialStream(ResourceManager* resourceMgr, string const& filepath, utils::StructuredData const& data, mesh::MeshSpecification const& meshSpec)
			: MaterialStream(resourceMgr)
			, FileStream(filepath, data)
			, mUseSpecifiedMeshSpec(true)
			, mMeshSpec(meshSpec)
		{
		}

		void FileMaterialStream::parseForChildResourceStreams(utils::StructuredData const& data, ResourceManager* resourceMgr, string const& filepath, bool useSpecifiedMesh, mesh::MeshSpecification const* meshSpec, map<string, ProgramStream::QualitySetting> &programSettings, map<string, map<string, TextureStream::QualitySetting>> &textureSettings)
		{
			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "Program")
				{
					// This can either be a reference to another resource, or an actual program definition.
					auto const& programEntry = entry.second;
					if (programEntry.hasEntry("Resource"))
					{
						auto qs = FileProgramStream::parseQualitySetting(programEntry.getEntry("Resource"), resourceMgr, filepath, !useSpecifiedMesh, meshSpec);
						programSettings[qs.first] = qs.second;
					}
				}
				else if (entry.first == "Textures")
				{
					int textureId = 0;
					auto const& textures = it->second;
					for (auto tit = textures.begin(); tit != textures.end(); ++tit, ++textureId)
					{
						auto const& entry = *tit;

						if (entry.first == "Texture")
						{
							auto const& textureEntry = entry.second;

							if (textureEntry.hasEntry("Resource"))
							{
								// Set texture options: peak sampler name
								auto samplerEntry = textureEntry.getEntry("Variable");
								auto samplerName = samplerEntry.getValue();

								auto texIt = textureSettings.find(samplerName);
								if (texIt == textureSettings.end())
								{
									textureSettings[samplerName] = map<string, TextureStream::QualitySetting>();
								}

								auto& qualitySettings = textureSettings.find(samplerName)->second;

								auto qs = FileTextureStream::parseQualitySetting(textureEntry.getEntry("Resource"), resourceMgr, filepath);
								qualitySettings[qs.first] = qs.second;
							}
						}
					}
				}
			}
		}

		void FileMaterialStream::createChildResourceStreamsImpl()
		{
			auto const& data = getStructuredData();

			// Parse data.  Root element should be 'Material'
			auto rootName = data.getName();

			if (rootName != "Material" && rootName != "Resource")
			{
				string errMsg = "Error loading " + getFilepath() + ".  Root element is neither 'Material' nor 'Resource'.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			map<string, ProgramStream::QualitySetting> programSettings;
			map<string, map<string, TextureStream::QualitySetting>> textureSettings;

			// Default quality setting
			parseForChildResourceStreams(data, getResourceMgr(), getFilepath(), mUseSpecifiedMeshSpec, &mMeshSpec, programSettings, textureSettings);

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "Quality")
				{
					// Additional quality setting
					parseForChildResourceStreams(entry.second, getResourceMgr(), getFilepath(), mUseSpecifiedMeshSpec, &mMeshSpec, programSettings, textureSettings);
				}
			}

			// Create child resource streams
			if (!programSettings.empty())
			{
				auto pStr = new ProgrammaticProgramStream(getResourceMgr());

				auto const& settingNames = pStr->getQualityNames();
				for (auto const& kvp: programSettings)
				{
					uint32_t settingId;
					auto settingIt = settingNames.find(kvp.first);
					if (settingIt != settingNames.end())
					{
						settingId = settingIt->second;
					}
					else
					{
						settingId = pStr->createQualitySetting(kvp.first);
					}

					pStr->setAttribs(kvp.second.attribs, settingId);
					pStr->setParser(kvp.second.parser, settingId);
				}

				addChild("Program", ResourceStreamPtr(pStr));
			}

			for (auto const& kvp: textureSettings)
			{
				auto const& samplerName = kvp.first;
				auto const& qualitySettings = kvp.second;

				auto tStr = new ProgrammaticTextureStream(getResourceMgr());

				// Set target
				tStr->setTarget(

				auto const& settingNames = tStr->getQualityNames();
				for (auto const& kvp : qualitySettings)
				{
					uint32_t settingId;
					auto settingIt = settingNames.find(kvp.first);
					if (settingIt != settingNames.end())
					{
						settingId = settingIt->second;
					}
					else
					{
						settingId = tStr->createQualitySetting(kvp.first);
					}

					tStr->setFile(kvp.second.source, getResourceMgr()->getImageLoadFunction(), settingId);
					tStr->setParams(kvp.second.params, settingId);
					tStr->setSampler(kvp.second.sampler, settingId);
				}

				addChild("Texture_" + samplerName, ResourceStreamPtr(tStr));
			}
		}

		void FileMaterialStream::parseUniformVectorType(string const& name, string const& type, size_t count, string const& value, UniformCollection &uniforms, string const& filepath)
		{
			auto values = utils::StringUtils::split(value, " ,");
			if (values.size() != count)
			{
				string errMsg = "Error loading " + filepath + ".  '" + type + "' specified for uniform '" + name + "'  but "
					+ utils::StringUtils::toString(values.size()) + " values found.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			if (type == "vec2")
			{
				uniforms.setUniform(name, glm::vec2(
					utils::StringUtils::parseFloat(values[0]),
					utils::StringUtils::parseFloat(values[1])));
			}
			else if (type == "vec3")
			{
				uniforms.setUniform(name, glm::vec3(
					utils::StringUtils::parseFloat(values[0]),
					utils::StringUtils::parseFloat(values[1]),
					utils::StringUtils::parseFloat(values[2])));
			}
			else if (type == "vec4")
			{
				uniforms.setUniform(name, glm::vec4(
					utils::StringUtils::parseFloat(values[0]),
					utils::StringUtils::parseFloat(values[1]),
					utils::StringUtils::parseFloat(values[2]),
					utils::StringUtils::parseFloat(values[3])));
			}
		}

		void FileMaterialStream::parseUniformMatrixType(string const& name, string const& type, size_t count, string const& value, UniformCollection &uniforms, string const& filepath)
		{
			auto values = utils::StringUtils::split(value, " ,");
			if (values.size() != count)
			{
				string errMsg = "Error loading " + filepath + ".  '" + type + "' specified for uniform '" + name + "'  but "
					+ utils::StringUtils::toString(values.size()) + " values found.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			auto fvalues = new float[count];
			for (size_t i = 0; i < count; ++i)
			{
				fvalues[i] = utils::StringUtils::parseFloat(values[i]);
			}

			uniforms.setUniform(name, count, fvalues);
			delete[] fvalues;
		}

		void FileMaterialStream::parseUniform(utils::StructuredData const& data, UniformCollection& uniforms, string const& filepath)
		{
			string name, type, value;
			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;

				if (entry.first == "name")
				{
					name = entry.second.getValue();
				}
				else if (entry.first == "type")
				{
					type = entry.second.getValue();
				}
				else if (entry.first == "value")
				{
					value = entry.second.getValue();
				}
			}

			if (name == "")
			{
				string errMsg = "Error loading " + filepath + ".  'name' not specified for uniform '" + name + "'.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			if (type == "")
			{
				string errMsg = "Error loading " + filepath + ".  'type' not specified for uniform '" + name + "'.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			if (value == "")
			{
				string errMsg = "Error loading " + filepath + ".  'value' not specified for uniform '" + name + "'.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			if (type == "int")
			{
				uniforms.setUniform(name, utils::StringUtils::parseInt(value));
			}
			else if (type == "uint")
			{
				uniforms.setUniform(name, utils::StringUtils::parseUInt(value));
			}
			else if (type == "float")
			{
				uniforms.setUniform(name, utils::StringUtils::parseFloat(value));
			}
			else if (type == "vec2")
			{
				parseUniformVectorType(name, type, 2, value, uniforms, filepath);
			}
			else if (type == "vec3")
			{
				parseUniformVectorType(name, type, 3, value, uniforms, filepath);
			}
			else if (type == "vec4")
			{
				parseUniformVectorType(name, type, 4, value, uniforms, filepath);
			}
			else if (type == "mat2")
			{
				parseUniformMatrixType(name, type, 2 * 2, value, uniforms, filepath);
			}
			else if (type == "mat2x3")
			{
				parseUniformMatrixType(name, type, 2 * 3, value, uniforms, filepath);
			}
			else if (type == "mat2x4")
			{
				parseUniformMatrixType(name, type, 2 * 4, value, uniforms, filepath);
			}
			else if (type == "mat3x2")
			{
				parseUniformMatrixType(name, type, 3 * 2, value, uniforms, filepath);
			}
			else if (type == "mat3")
			{
				parseUniformMatrixType(name, type, 3 * 3, value, uniforms, filepath);
			}
			else if (type == "mat3x4")
			{
				parseUniformMatrixType(name, type, 3 * 4, value, uniforms, filepath);
			}
			else if (type == "mat4x2")
			{
				parseUniformMatrixType(name, type, 4 * 2, value, uniforms, filepath);
			}
			else if (type == "mat4x3")
			{
				parseUniformMatrixType(name, type, 4 * 3, value, uniforms, filepath);
			}
			else if (type == "mat4")
			{
				parseUniformMatrixType(name, type, 4 * 4, value, uniforms, filepath);
			}
		}

		pair<string, FileMaterialStream::QualitySetting> FileMaterialStream::parseQualitySetting(utils::StructuredData const& data, ResourceManager* resourceMgr, string const& filepath)
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
				else if (entry.first == "Program")
				{
					// This can either be a reference to another resource, or an actual program definition.
					auto const& programEntry = entry.second;
					if (programEntry.hasEntry("Resource"))
					{
						qs.spec.program.resourceExists = true;
						qs.spec.program.isChild = true;
						qs.spec.program.existingResource = "Program";
					}
					else if (programEntry.hasEntry("Ref"))
					{
						auto refName = programEntry.getEntry("Resource").getValue();

						// Set program options
						qs.spec.program.resourceExists = true;
						qs.spec.program.isChild = false;
						qs.spec.program.existingResource = refName;
					}
					else
					{
						string errMsg = "Error loading " + filepath + ".  Neither 'Resource' nor 'Ref' specified for material program.";
						THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
					}
				}
				else if (entry.first == "Textures")
				{
					auto const& textures = it->second;
					for (auto const& entry: textures)
					{
						if (entry.first == "Texture")
						{
							auto const& textureEntry = entry.second;

							// Set texture options: peak sampler name
							auto samplerEntry = textureEntry.getEntry("Variable");
							auto samplerName = samplerEntry.getValue();

							// This can either be a reference to another resource, or an actual texture definition.
							MaterialSpecification::TextureOptions textureOptions;

							textureOptions.resourceExists = true;
							textureOptions.sampler = samplerName;

							if (textureEntry.hasEntry("Resource"))
							{
								textureOptions.isChild = true;
								textureOptions.existingResource = "Texture_" + samplerName;
							}
							else if (textureEntry.hasEntry("Ref"))
							{
								textureOptions.isChild = false;
								textureOptions.existingResource = textureEntry.getEntry("Ref").getValue();
							}
						
							qs.spec.textures.push_back(textureOptions);
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
							parseUniform(entry.second, qs.spec.uniforms, filepath);
						}
					}
				}
			}

			return make_pair(name, qs);
		}

		void FileMaterialStream::loadImpl()
		{
			auto const& data = getStructuredData();

			// Parse data.  Root element should be 'Material'
			auto rootName = data.getName();

			if (rootName != "Material" && rootName != "Resource")
			{
				string errMsg = "Error loading " + getFilepath() + ".  Root element is neither 'Material' nor 'Resource'.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			// Default quality setting
			auto qs = parseQualitySetting(data, getResourceMgr(), getFilepath());
			mQualitySettings[createQualitySetting(qs.first)] = qs.second;

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "Quality")
				{
					// Additional quality setting
					auto qs = parseQualitySetting(entry.second, getResourceMgr(), getFilepath());
					mQualitySettings[createQualitySetting(qs.first)] = qs.second;
				}
			}
		}
	}
}