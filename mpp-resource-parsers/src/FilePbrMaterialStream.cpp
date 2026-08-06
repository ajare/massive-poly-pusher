#include "utils/FileSystem.h"

#include "mpp/DefaultShaders.h"
#include "mpp/ProgrammaticProgramStream.h"
#include "mpp/ProgrammaticTextureStream.h"

#include "mpp/resource-parsers/FilePbrMaterialStream.h"
#include "mpp/resource-parsers/FileProgramStream.h"
#include "mpp/resource-parsers/FileTextureStream.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"

namespace mpp
{
	namespace resource_parsers
	{

		using namespace std;

		FilePbrMaterialStream::FilePbrMaterialStream(ResourceManager* resourceMgr, string const& filepath, bool relativisePaths)
			: PbrMaterialStream(resourceMgr)
			, FileStream(filepath)
			, mUseSpecifiedMeshSpec(false)
			, mRelativisePaths(relativisePaths)
		{
		}

		FilePbrMaterialStream::FilePbrMaterialStream(ResourceManager* resourceMgr, string const& filepath, utils::StructuredData const& data, bool relativisePaths)
			: PbrMaterialStream(resourceMgr)
			, FileStream(filepath, data)
			, mUseSpecifiedMeshSpec(false)
			, mRelativisePaths(relativisePaths)
		{
		}

		FilePbrMaterialStream::FilePbrMaterialStream(ResourceManager* resourceMgr, string const& filepath, mesh::MeshSpecification const& meshSpec, bool relativisePaths)
			: PbrMaterialStream(resourceMgr)
			, FileStream(filepath)
			, mUseSpecifiedMeshSpec(true)
			, mMeshSpec(meshSpec)
			, mRelativisePaths(relativisePaths)
		{
		}

		FilePbrMaterialStream::FilePbrMaterialStream(ResourceManager* resourceMgr, string const& filepath, utils::StructuredData const& data, mesh::MeshSpecification const& meshSpec, bool relativisePaths)
			: PbrMaterialStream(resourceMgr)
			, FileStream(filepath, data)
			, mUseSpecifiedMeshSpec(true)
			, mMeshSpec(meshSpec)
			, mRelativisePaths(relativisePaths)
		{
		}

		void FilePbrMaterialStream::parseForChildResourceStreams(utils::StructuredData const& data, string const& filepath, bool useSpecifiedMesh, mesh::MeshSpecification const* meshSpec)
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
						// Create FileProgramStream
						if (useSpecifiedMesh)
						{
							auto fpStream = new FileProgramStream(getResourceMgr(), filepath, programEntry.getEntry("Resource"), *meshSpec, mRelativisePaths);
							addChild("Program", ResourceStreamPtr(fpStream));
						}
						else
						{
							auto fpStream = new FileProgramStream(getResourceMgr(), filepath, programEntry.getEntry("Resource"), mRelativisePaths);
							addChild("Program", ResourceStreamPtr(fpStream));
						}
					}
				}
				else if (entry.first == "BaseColourMap" || entry.first == "MetallicRoughnessMap" || entry.first == "NormalMap" || entry.first == "OcclusionMap" || entry.first == "EmissiveMap")
				{
					static map<string, string> const samplers = {
						{ "BaseColourMap", "PBR_BASE_COLOUR_MAP" }, { "MetallicRoughnessMap", "PBR_METALLIC_ROUGHNESS_MAP" },
						{ "NormalMap", "PBR_NORMAL_MAP" }, { "OcclusionMap", "PBR_OCCLUSION_MAP" }, { "EmissiveMap", "PBR_EMISSIVE_MAP" }
					};
					if (entry.second.hasEntry("Resource"))
					{
						auto const& sampler = samplers.at(entry.first);
						auto ftStream = new FileTextureStream(getResourceMgr(), filepath, entry.second.getEntry("Resource"), mRelativisePaths);
						addChild("Textures/" + sampler, ResourceStreamPtr(ftStream));
					}
				}
				else if (entry.first == "Extensions")
				{
					for (auto const& extension : entry.second)
					{
						if (extension.first != "Texture" || !extension.second.hasEntry("Resource")) continue;
						auto const& name = extension.second.hasEntry("name") ? extension.second.getEntry("name").getValue() : extension.second.getEntry("Variable").getValue();
						if (name.rfind("PBR_EXT_", 0) != 0) THROW_MPP_RESOURCE_PARSERS("PBR extension sampler must use the PBR_EXT_ namespace.", __LINE__, __FILE__, __func__);
						auto stream = new FileTextureStream(getResourceMgr(), filepath, extension.second.getEntry("Resource"), mRelativisePaths);
						addChild("Extensions/" + name, ResourceStreamPtr(stream));
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

								// Create FileTextureStream
								auto ftStream = new FileTextureStream(getResourceMgr(), filepath, textureEntry.getEntry("Resource"), mRelativisePaths);
								addChild("Textures/" + samplerName, ResourceStreamPtr(ftStream));
							}
						}
					}
				}
			}
		}

		void FilePbrMaterialStream::createChildResourceStreamsImpl()
		{
			auto const& data = getStructuredData();

			// Parse data.  Root element should be 'Material'
			auto rootName = data.getName();

			if (rootName != "PbrMaterial" && rootName != "Material" && rootName != "Resource")
			{
				string errMsg = "Error loading " + getFilepath() + ".  Root element is neither 'PbrMaterial' nor 'Resource'.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			// Default quality setting
			parseForChildResourceStreams(data, getFilepath(), mUseSpecifiedMeshSpec, &mMeshSpec);

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "Quality")
				{
					// Additional quality setting
					parseForChildResourceStreams(entry.second, getFilepath(), mUseSpecifiedMeshSpec, &mMeshSpec);
				}
			}
		}

		void FilePbrMaterialStream::parseUniformVectorType(string const& name, string const& type, size_t count, string const& value, UniformCollection &uniforms, string const& filepath)
		{
			auto values = utils::StringUtils::split(value, " ,");
			if (values.size() != count)
			{
				string errMsg = STR_FORMAT("Error loading {}.  '{}' specified for uniform '{}'  but {} values found.",
					filepath, type, name, values.size());
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

		void FilePbrMaterialStream::parseUniformMatrixType(string const& name, string const& type, size_t count, string const& value, UniformCollection &uniforms, string const& filepath)
		{
			auto values = utils::StringUtils::split(value, " ,");
			if (values.size() != count)
			{
				string errMsg = STR_FORMAT("Error loading {}. '{}' specified for uniform {} but {} values found.", filepath, type, name, values.size());
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			auto fvalues = new float[count];
			for (size_t i = 0; i < count; ++i)
			{
				fvalues[i] = utils::StringUtils::parseFloat(values[i]);
			}

			uniforms.setUniform(name, count, 1, fvalues);
			delete[] fvalues;
		}

		void FilePbrMaterialStream::parseUniform(utils::StructuredData const& data, UniformCollection& uniforms, string const& filepath)
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

		pair<string, FilePbrMaterialStream::QualitySetting> FilePbrMaterialStream::parseQualitySetting(utils::StructuredData const& data, ResourceManager* resourceMgr, string const& filepath)
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
				else if (entry.first == "Pbr" || entry.first == "Surface")
				{
					auto& pbr = qs.spec.pbr;
					pbr.enabled = true;
					for (auto pit = entry.second.begin(); pit != entry.second.end(); ++pit)
					{
						auto const& pbrEntry = *pit;
						auto const rawValue = pbrEntry.second.getValue();
						auto const pbrValue = utils::StringUtils::toUpper(rawValue);
						if (pbrEntry.first == "baseColourFactor")
						{
							auto values = utils::StringUtils::split(rawValue, " ,");
							if (values.size() != 4) THROW_MPP_RESOURCE_PARSERS("Pbr baseColourFactor requires four values.", __LINE__, __FILE__, __func__);
							pbr.baseColourFactor = glm::vec4(utils::StringUtils::parseFloat(values[0]), utils::StringUtils::parseFloat(values[1]), utils::StringUtils::parseFloat(values[2]), utils::StringUtils::parseFloat(values[3]));
						}
						else if (pbrEntry.first == "metallicFactor") pbr.metallicFactor = utils::StringUtils::parseFloat(rawValue);
						else if (pbrEntry.first == "roughnessFactor") pbr.roughnessFactor = utils::StringUtils::parseFloat(rawValue);
						else if (pbrEntry.first == "normalScale") pbr.normalScale = utils::StringUtils::parseFloat(rawValue);
						else if (pbrEntry.first == "occlusionStrength") pbr.occlusionStrength = utils::StringUtils::parseFloat(rawValue);
						else if (pbrEntry.first == "alphaCutoff") pbr.alphaCutoff = utils::StringUtils::parseFloat(rawValue);
						else if (pbrEntry.first == "doubleSided") pbr.doubleSided = utils::StringUtils::parseBool(pbrValue);
						else if (pbrEntry.first == "emissiveFactor")
						{
							auto values = utils::StringUtils::split(rawValue, " ,");
							if (values.size() != 3) THROW_MPP_RESOURCE_PARSERS("Pbr emissiveFactor requires three values.", __LINE__, __FILE__, __func__);
							pbr.emissiveFactor = glm::vec3(utils::StringUtils::parseFloat(values[0]), utils::StringUtils::parseFloat(values[1]), utils::StringUtils::parseFloat(values[2]));
						}
						else if (pbrEntry.first == "alphaMode")
						{
							if (pbrValue == "OPAQUE") pbr.alphaMode = PbrMaterialSpecification::PbrAlphaMode::Opaque;
							else if (pbrValue == "MASK") pbr.alphaMode = PbrMaterialSpecification::PbrAlphaMode::Mask;
							else if (pbrValue == "BLEND") pbr.alphaMode = PbrMaterialSpecification::PbrAlphaMode::Blend;
							else THROW_MPP_RESOURCE_PARSERS("Unknown Pbr alphaMode.", __LINE__, __FILE__, __func__);
						}
					}

					// Preserve PBR values through the legacy material stream format.
					// Unknown uniforms are harmless to legacy shaders and therefore keep
					// old Release builds able to load regenerated models.
					qs.spec.uniforms.setUniform("PBR_ENABLED", (int32_t)1);
					qs.spec.uniforms.setUniform("PBR_BASE_COLOUR_FACTOR", pbr.baseColourFactor);
					qs.spec.uniforms.setUniform("PBR_METALLIC_FACTOR", pbr.metallicFactor);
					qs.spec.uniforms.setUniform("PBR_ROUGHNESS_FACTOR", pbr.roughnessFactor);
					qs.spec.uniforms.setUniform("PBR_EMISSIVE_FACTOR", pbr.emissiveFactor);
					qs.spec.uniforms.setUniform("PBR_NORMAL_SCALE", pbr.normalScale);
					qs.spec.uniforms.setUniform("PBR_OCCLUSION_STRENGTH", pbr.occlusionStrength);
					qs.spec.uniforms.setUniform("PBR_ALPHA_MODE", (int32_t)pbr.alphaMode);
					qs.spec.uniforms.setUniform("PBR_ALPHA_CUTOFF", pbr.alphaCutoff);
					qs.spec.uniforms.setUniform("PBR_DOUBLE_SIDED", (int32_t)(pbr.doubleSided ? 1 : 0));
				}
				else if (entry.first == "BaseColourMap" || entry.first == "MetallicRoughnessMap" || entry.first == "NormalMap" || entry.first == "OcclusionMap" || entry.first == "EmissiveMap")
				{
					static map<string, string> const samplers = {
						{ "BaseColourMap", "PBR_BASE_COLOUR_MAP" }, { "MetallicRoughnessMap", "PBR_METALLIC_ROUGHNESS_MAP" },
						{ "NormalMap", "PBR_NORMAL_MAP" }, { "OcclusionMap", "PBR_OCCLUSION_MAP" }, { "EmissiveMap", "PBR_EMISSIVE_MAP" }
					};
					PbrMaterialSpecification::TextureOptions textureOptions;
					textureOptions.resourceExists = true;
					textureOptions.sampler = samplers.at(entry.first);
					if (entry.second.hasEntry("Resource")) { textureOptions.isChild = true; textureOptions.existingResource = "Textures/" + textureOptions.sampler; }
					else if (entry.second.hasEntry("Ref")) { textureOptions.isChild = false; textureOptions.existingResource = entry.second.getEntry("Ref").getValue(); }
					else THROW_MPP_RESOURCE_PARSERS("PbrMaterial semantic map requires Resource or Ref.", __LINE__, __FILE__, __func__);
					qs.spec.textures.push_back(textureOptions);
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
							PbrMaterialSpecification::TextureOptions textureOptions;

							textureOptions.resourceExists = true;
							textureOptions.sampler = samplerName;

							if (textureEntry.hasEntry("Resource"))
							{
								textureOptions.isChild = true;
								textureOptions.existingResource = "Textures/" + samplerName;
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
				else if (entry.first == "Extensions")
				{
					for (auto const& extension : entry.second)
					{
						if (extension.first == "Uniform")
						{
							auto const& name = extension.second.getEntry("name").getValue();
							if (name.rfind("PBR_EXT_", 0) != 0) THROW_MPP_RESOURCE_PARSERS("PBR extension uniform must use the PBR_EXT_ namespace.", __LINE__, __FILE__, __func__);
							parseUniform(extension.second, qs.spec.uniforms, filepath);
						}
						else if (extension.first == "Texture")
						{
							PbrMaterialSpecification::TextureOptions texture;
							texture.resourceExists = true;
							texture.sampler = extension.second.hasEntry("name") ? extension.second.getEntry("name").getValue() : extension.second.getEntry("Variable").getValue();
							if (texture.sampler.rfind("PBR_EXT_", 0) != 0) THROW_MPP_RESOURCE_PARSERS("PBR extension sampler must use the PBR_EXT_ namespace.", __LINE__, __FILE__, __func__);
							if (extension.second.hasEntry("Resource"))
							{
								texture.isChild = true; texture.existingResource = "Extensions/" + texture.sampler;
								auto const& resource = extension.second.getEntry("Resource");
								if (resource.hasEntry("target")) { auto target = utils::StringUtils::toUpper(resource.getEntry("target").getValue()); texture.target = target == "CUBE" || target == "CUBEMAP" ? TextureTarget::CubeMap : TextureTarget::Texture2D; }
							}
							else if (extension.second.hasEntry("Ref"))
							{
								texture.existingResource = extension.second.getEntry("Ref").getValue();
								if (extension.second.hasEntry("target")) { auto target = utils::StringUtils::toUpper(extension.second.getEntry("target").getValue()); texture.target = target == "CUBE" || target == "CUBEMAP" ? TextureTarget::CubeMap : TextureTarget::Texture2D; }
							}
							else THROW_MPP_RESOURCE_PARSERS("PBR extension sampler requires Resource or Ref.", __LINE__, __FILE__, __func__);
							qs.spec.textures.push_back(texture);
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

		void FilePbrMaterialStream::loadImpl()
		{
			auto const& data = getStructuredData();

			// Parse data. Root element identifies the concrete PBR resource.
			auto rootName = data.getName();

			if (rootName != "PbrMaterial" && rootName != "Material" && rootName != "Resource")
			{
				string errMsg = "Error loading " + getFilepath() + ". Root element is neither 'PbrMaterial' nor 'Resource'.";
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