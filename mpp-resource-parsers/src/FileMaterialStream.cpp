#include "utils/FileSystem.h"

#include "mpp/DefaultShaders.h"

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
		{
		}

		FileMaterialStream::FileMaterialStream(ResourceManager* resourceMgr, string const& filepath, utils::StructuredData const& data)
			: MaterialStream(resourceMgr)
			, FileStream(filepath, data)
		{
		}

		void FileMaterialStream::parseUniformVectorType(string const& name, string const& type, size_t count, string const& value, UniformCollection &uniforms)
		{
			auto values = utils::StringUtils::split(value, " ,");
			if (values.size() != count)
			{
				string errMsg = "Error loading " + getFilepath() + ".  '" + type + "' specified for uniform '" + name + "'  but "
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

		void FileMaterialStream::parseUniformMatrixType(string const& name, string const& type, size_t count, string const& value, UniformCollection &uniforms)
		{
			auto values = utils::StringUtils::split(value, " ,");
			if (values.size() != count)
			{
				string errMsg = "Error loading " + getFilepath() + ".  '" + type + "' specified for uniform '" + name + "'  but "
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

		void FileMaterialStream::parseUniform(utils::StructuredData const& data, UniformCollection& uniforms)
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
				string errMsg = "Error loading " + getFilepath() + ".  'name' not specified for uniform '" + name + "'.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			if (type == "")
			{
				string errMsg = "Error loading " + getFilepath() + ".  'type' not specified for uniform '" + name + "'.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			if (value == "")
			{
				string errMsg = "Error loading " + getFilepath() + ".  'value' not specified for uniform '" + name + "'.";
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
				parseUniformVectorType(name, type, 2, value, uniforms);
			}
			else if (type == "vec3")
			{
				parseUniformVectorType(name, type, 3, value, uniforms);
			}
			else if (type == "vec4")
			{
				parseUniformVectorType(name, type, 4, value, uniforms);
			}
			else if (type == "mat2")
			{
				parseUniformMatrixType(name, type, 2 * 2, value, uniforms);
			}
			else if (type == "mat2x3")
			{
				parseUniformMatrixType(name, type, 2 * 3, value, uniforms);
			}
			else if (type == "mat2x4")
			{
				parseUniformMatrixType(name, type, 2 * 4, value, uniforms);
			}
			else if (type == "mat3x2")
			{
				parseUniformMatrixType(name, type, 3 * 2, value, uniforms);
			}
			else if (type == "mat3")
			{
				parseUniformMatrixType(name, type, 3 * 3, value, uniforms);
			}
			else if (type == "mat3x4")
			{
				parseUniformMatrixType(name, type, 3 * 4, value, uniforms);
			}
			else if (type == "mat4x2")
			{
				parseUniformMatrixType(name, type, 4 * 2, value, uniforms);
			}
			else if (type == "mat4x3")
			{
				parseUniformMatrixType(name, type, 4 * 3, value, uniforms);
			}
			else if (type == "mat4")
			{
				parseUniformMatrixType(name, type, 4 * 4, value, uniforms);
			}
		}

		void FileMaterialStream::parseQualitySetting(utils::StructuredData const& data)
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
						// If it's a definition, create a child FileProgramStream with this node and load it
						auto programStream = new FileProgramStream(getResourceMgr(), getFilepath(), programEntry.getEntry("Resource"));
						addChild("Program", ResourceStreamPtr(programStream));

						// Set program options
						qs.program.resourceExists = true;
						qs.program.isChild = true;
					}
					else if (programEntry.hasEntry("Ref"))
					{
						auto refName = programEntry.getEntry("Resource").getValue();

						// Set program options
						qs.program.resourceExists = true;
						qs.program.existingResource = refName;
					}
					else
					{
						string errMsg = "Error loading " + getFilepath() + ".  Neither 'Resource' nor 'Ref' specified for material program.";
						THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
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

							// Set texture options: peak sampler name
							auto samplerEntry = textureEntry.getEntry("Variable");
							auto samplerName = samplerEntry.getValue();

							// This can either be a reference to another resource, or an actual texture definition.
							if (textureEntry.hasEntry("Resource"))
							{
								string textureName = "Texture" + utils::StringUtils::toString(textureId);
								auto textureStream = new FileTextureStream(getResourceMgr(), getFilepath(), textureEntry.getEntry("Resource"));
								addChild(textureName, ResourceStreamPtr(textureStream));
							
								qs.textures[samplerName] = make_pair(textureName, true);
							}
							else if (textureEntry.hasEntry("Ref"))
							{
								qs.textures[samplerName] = make_pair(textureEntry.getEntry("Ref").getValue(), false);
							}
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
							parseUniform(entry.second, qs.uniforms);
						}
					}
				}
			}

			auto newSettingId = createQualitySetting(name);
			mQualitySettings[newSettingId] = qs;
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