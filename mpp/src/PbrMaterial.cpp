#include <cfloat>
#include <cstring>

#include "utils/FileSystem.h"

#include "mpp/PbrMaterial.h"
#include "mpp/PbrMaterialStream.h"
#include "mpp/PbrShaders.h"
#include "mpp/RenderSystem.h"
#include "mpp/ResourceManager.h"
#include "mpp/String.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{
	/*
	* Constructor.
	*
	*/
	PbrMaterial::PbrMaterial(string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream)
		: Material(name, "PbrMaterial", renderSystem, resourceMgr, resourceStream)
	{
	}

	/*
	 * Destructor
	 *
	 */
	PbrMaterial::~PbrMaterial()
	{
		destroy();
	}

	/*
	 * Create material.
	 *
	 */
	void PbrMaterial::createImpl()
	{
		PbrMaterialStream* mStr = dynamic_cast<PbrMaterialStream*>(getResourceStream().get());
		if (!mStr)
		{
			THROW_MPP("Could not cast to type 'PbrMaterialStream'", __LINE__, __FILE__, __func__);
		}

		auto resourceMgr = getResourceManager();

		// Create program and build information about it.  Program is either a named resource, or a MeshSpecification with
		// optional shader strings.
		auto const& progOpts = mStr->getProgramOptions();

		if (progOpts.resourceExists)
		{
			if (progOpts.isChild)
			{
				mProgram = resourceMgr->getResource(getName() + "/Program");
			}
			else
			{
				mProgram = resourceMgr->getResource(progOpts.existingResource);
			}
		}
		else
		{
			// Get texture usage
			uint32_t programFlags{ 0 };

			if (!mStr->getTextures().empty())
			{
				programFlags |= MPP_PROGRAM_TAGS_TEXTURE;
			}

			// Load in shaders if required
			string vertexShaderSrc;
			switch (progOpts.vertexShader.type)
			{
			case PbrMaterialSpecification::ProgramOptions::Shader::Type::Default:
				vertexShaderSrc = "";
				break;

			case PbrMaterialSpecification::ProgramOptions::Shader::Type::File:
				vertexShaderSrc = utils::FileSystem::readTextFile(progOpts.vertexShader.data);
				break;

			case PbrMaterialSpecification::ProgramOptions::Shader::Type::Resource:
				vertexShaderSrc = dynamic_cast<String*>(getResourceManager()->getResource(progOpts.vertexShader.data).get())->getData();
				break;

			default:
				THROW_MPP("Unknown PbrMaterialStream::ProgramOptions::Shader::Type", __LINE__, __FILE__, __func__);
			}

			string geometryShaderSrc;
			switch (progOpts.geometryShader.type)
			{
			case PbrMaterialSpecification::ProgramOptions::Shader::Type::Default:
				geometryShaderSrc = "";
				break;

			case PbrMaterialSpecification::ProgramOptions::Shader::Type::File:
				geometryShaderSrc = utils::FileSystem::readTextFile(progOpts.geometryShader.data);
				break;

			case PbrMaterialSpecification::ProgramOptions::Shader::Type::Resource:
				geometryShaderSrc = dynamic_cast<String*>(getResourceManager()->getResource(progOpts.geometryShader.data).get())->getData();
				break;

			default:
				THROW_MPP("Unknown PbrMaterialStream::ProgramOptions::Shader::Type", __LINE__, __FILE__, __func__);
			}

			string fragmentShaderSrc;
			switch (progOpts.fragmentShader.type)
			{
			case PbrMaterialSpecification::ProgramOptions::Shader::Type::Default:
				fragmentShaderSrc = "";
				break;

			case PbrMaterialSpecification::ProgramOptions::Shader::Type::File:
				fragmentShaderSrc = utils::FileSystem::readTextFile(progOpts.fragmentShader.data);
				break;

			case PbrMaterialSpecification::ProgramOptions::Shader::Type::Resource:
				fragmentShaderSrc = dynamic_cast<String*>(getResourceManager()->getResource(progOpts.fragmentShader.data).get())->getData();
				break;

			default:
				THROW_MPP("Unknown PbrMaterialStream::ProgramOptions::Shader::Type", __LINE__, __FILE__, __func__);
			}

			// An omitted program selects the engine-owned metallic-roughness shader.
			if (progOpts.vertexShader.type == PbrMaterialSpecification::ProgramOptions::Shader::Type::Default) vertexShaderSrc = BuiltInPbrVertexShader;
			if (progOpts.fragmentShader.type == PbrMaterialSpecification::ProgramOptions::Shader::Type::Default) fragmentShaderSrc = BuiltInPbrFragmentShader;

			// Get or create program, either with built-in PBR shaders or loaded strings in ProgOpts.
			if (progOpts.is2d)
			{
				mProgram = resourceMgr->getDefault2dProgram(vertexShaderSrc, fragmentShaderSrc, progOpts.spec, programFlags, false);
			}
			else
			{
				mProgram = resourceMgr->getDefault3dProgram(vertexShaderSrc, fragmentShaderSrc, progOpts.spec, programFlags, false);
			}
		}

		acquireDependentResource(mProgram);
		mProgram->load();

		// Set uniforms
		mUniforms = mStr->getUniforms();
		mPbrSurface = mStr->getPbrSurface();
		// A PbrMaterial is explicit; legacy PBR_ENABLED inference belongs only to
		// the later compatibility converter, never to BasicMaterial.
		mPbrSurface.enabled = true;
		auto requireRange = [&](char const* field, float value, float minimum, float maximum)
		{
			if (value < minimum || value > maximum)
				THROW_MPP("PbrMaterial '" + getName() + "' has " + field + " outside its supported range.", __LINE__, __FILE__, __func__);
		};
		requireRange("baseColourFactor alpha", mPbrSurface.baseColourFactor.a, 0.0f, 1.0f);
		requireRange("metallicFactor", mPbrSurface.metallicFactor, 0.0f, 1.0f);
		requireRange("roughnessFactor", mPbrSurface.roughnessFactor, 0.0f, 1.0f);
		requireRange("normalScale", mPbrSurface.normalScale, 0.0f, FLT_MAX);
		requireRange("occlusionStrength", mPbrSurface.occlusionStrength, 0.0f, 1.0f);
		requireRange("alphaCutoff", mPbrSurface.alphaCutoff, 0.0f, 1.0f);
		if (mPbrSurface.baseColourFactor.r < 0.0f || mPbrSurface.baseColourFactor.g < 0.0f || mPbrSurface.baseColourFactor.b < 0.0f ||
			mPbrSurface.emissiveFactor.r < 0.0f || mPbrSurface.emissiveFactor.g < 0.0f || mPbrSurface.emissiveFactor.b < 0.0f)
			THROW_MPP("PbrMaterial '" + getName() + "' has negative colour or emissive factors.", __LINE__, __FILE__, __func__);

		// The built-in and every custom PBR program share this stable material
		// contract. Optional maps use neutral textures, not optional interfaces.
		Program* program = static_cast<Program*>(mProgram.get());
		vector<string> const requiredUniforms = {
			"PBR_BASE_COLOUR_FACTOR", "PBR_METALLIC_FACTOR", "PBR_ROUGHNESS_FACTOR",
			"PBR_EMISSIVE_FACTOR", "PBR_NORMAL_SCALE", "PBR_OCCLUSION_STRENGTH",
			"PBR_ALPHA_MODE", "PBR_ALPHA_CUTOFF", "PBR_DOUBLE_SIDED"
		};
		for (auto const& uniform : requiredUniforms)
			if (program->getUniformId(uniform) < 0)
				THROW_MPP("PbrMaterial '" + getName() + "' program is missing required uniform '" + uniform + "'.", __LINE__, __FILE__, __func__);
		vector<string> const requiredSamplers = {
			"PBR_BASE_COLOUR_MAP", "PBR_METALLIC_ROUGHNESS_MAP", "PBR_NORMAL_MAP",
			"PBR_OCCLUSION_MAP", "PBR_EMISSIVE_MAP", "PBR_IRRADIANCE_MAP",
			"PBR_PREFILTERED_SPECULAR_MAP", "PBR_BRDF_LUT"
		};
		for (auto const& sampler : requiredSamplers)
		{
			bool found = false;
			for (int index = 0; index < program->getNumSamplers(); ++index)
				if (program->getSamplerName(index) == sampler) { found = true; break; }
			if (!found)
				THROW_MPP("PbrMaterial '" + getName() + "' program is missing required sampler '" + sampler + "'.", __LINE__, __FILE__, __func__);
		}
		string fragmentOutputDiagnostic;
		if (!program->validateFragmentOutputLocations(1, fragmentOutputDiagnostic))
			THROW_MPP("PbrMaterial '" + getName() + "' program must write fragment location 0: " + fragmentOutputDiagnostic, __LINE__, __FILE__, __func__);
		// MPP model files retain backwards-compatible material streams. PBR
		// metadata is mirrored into PBR_* uniforms by FilePbrMaterialStream, so
		// recover the material state needed by the renderer after deserialization.
		auto const& serializedUniforms = mUniforms.getUniformData();
		if (!mPbrSurface.enabled && serializedUniforms.find("PBR_ENABLED") != serializedUniforms.end())
		{
			mPbrSurface.enabled = true;
		}
		auto alphaModeIt = serializedUniforms.find("PBR_ALPHA_MODE");
		if (alphaModeIt != serializedUniforms.end() && alphaModeIt->second.size >= sizeof(int32_t))
		{
			int32_t alphaMode;
			memcpy(&alphaMode, alphaModeIt->second.data, sizeof(alphaMode));
			if (alphaMode >= (int32_t)PbrMaterialSpecification::PbrAlphaMode::Opaque &&
				alphaMode <= (int32_t)PbrMaterialSpecification::PbrAlphaMode::Blend)
			{
				mPbrSurface.alphaMode = (PbrMaterialSpecification::PbrAlphaMode)alphaMode;
			}
		}
		if (mPbrSurface.enabled)
		{
			mUniforms.setUniform("PBR_BASE_COLOUR_FACTOR", mPbrSurface.baseColourFactor);
			mUniforms.setUniform("PBR_METALLIC_FACTOR", mPbrSurface.metallicFactor);
			mUniforms.setUniform("PBR_ROUGHNESS_FACTOR", mPbrSurface.roughnessFactor);
			mUniforms.setUniform("PBR_EMISSIVE_FACTOR", mPbrSurface.emissiveFactor);
			mUniforms.setUniform("PBR_NORMAL_SCALE", mPbrSurface.normalScale);
			mUniforms.setUniform("PBR_OCCLUSION_STRENGTH", mPbrSurface.occlusionStrength);
			mUniforms.setUniform("PBR_ALPHA_MODE", (int32_t)mPbrSurface.alphaMode);
			mUniforms.setUniform("PBR_ALPHA_CUTOFF", mPbrSurface.alphaCutoff);
			mUniforms.setUniform("PBR_DOUBLE_SIDED", (int32_t)(mPbrSurface.doubleSided ? 1 : 0));
		}

		// Set textures
		auto const& materialTextures = mStr->getTextures();

		// A serialized legacy stream may predate PbrSurface itself. The standard
		// PBR sampler contract is also sufficient to select its neutral maps.
		for (int i = 0; i < program->getNumSamplers(); ++i)
		{
			if (program->getSamplerName(i).rfind("PBR_", 0) == 0)
			{
				mPbrSurface.enabled = true;
				break;
			}
		}

		// Go through each texture, get the binding location.
		for (int i = 0; i < program->getNumSamplers(); ++i)
		{
			string const& samplerName = program->getSamplerName(i);

			auto it = find_if(materialTextures.begin(), materialTextures.end(),
				[samplerName] (PbrMaterialSpecification::TextureOptions const& textureOptions)
			{
				return textureOptions.sampler == samplerName;
			});

			string textureName;
			if (it == materialTextures.end())
			{
				if (samplerName == "SHADOW_MAP")
				{
					// Shadow domains replace this binding during an opted-in scene flush.
					// The normal no-texture fallback keeps non-shadow pipelines valid.
					textureName = "__mpp_tex_none__";
				}
				else if (mPbrSurface.enabled)
				{
					if (samplerName == "PBR_BASE_COLOUR_MAP" || samplerName == "PBR_OCCLUSION_MAP")
					{
						textureName = "__mpp_tex_pbr_white__";
					}
					else if (samplerName == "PBR_METALLIC_ROUGHNESS_MAP")
					{
						textureName = "__mpp_tex_pbr_metallic_roughness__";
					}
					else if (samplerName == "PBR_NORMAL_MAP")
					{
						textureName = "__mpp_tex_pbr_normal__";
					}
					else if (samplerName == "PBR_EMISSIVE_MAP")
					{
						textureName = "__mpp_tex_pbr_black__";
					}
					else if (samplerName == "PBR_IRRADIANCE_MAP" || samplerName == "PBR_PREFILTERED_SPECULAR_MAP")
					{
						textureName = "__mpp_tex_pbr_ibl_cube__";
					}
					else if (samplerName == "PBR_BRDF_LUT")
					{
						textureName = "__mpp_tex_pbr_brdf_lut__";
					}
				}

				if (textureName.empty())
				{
					string errMsg = STR_FORMAT("Sampler '{}' declared in program '{}' is not bound by material '{}'.",
						samplerName, program->getName(), getName());
					THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
				}
			}
			else
			{
				auto const& textureOptions = *it;
				textureName = textureOptions.isChild
					? getName() + "/" + textureOptions.existingResource
					: textureOptions.existingResource;
			}

			// Add as acquired resource
			auto texRes = resourceMgr->getResource(textureName);
			acquireDependentResource(texRes);
			mTextures.push_back(texRes);
		}
	}

	/*
	 * Destroy material.
	 *
	 */
	void PbrMaterial::destroyImpl()
	{
	}

	/*
	 * Load material.
	 *
	 */
	void PbrMaterial::loadImpl()
	{
		mProgram->load();

		for (auto texture: mTextures)
		{
			texture->load();
		}
	}

	/*
	 * Unload material.
	 *
	 */
	void PbrMaterial::unloadImpl()
	{
	}

	/*
	 * Get the program this material uses.
	 *
	 */
	ResourcePtr PbrMaterial::getProgram()
	{
		return mProgram;
	}

		PbrMaterialSpecification::PbrSurface const& PbrMaterial::getSurface() const
	{
		return mPbrSurface;
	}

	/*
	 * Get number of textures.
	 *
	 */
	int PbrMaterial::getNumTextures() const
	{
		return (int)mTextures.size();
	}

	void PbrMaterial::setTexture(int i, ResourcePtr texture)
	{
		assert(i >= 0 && i < getNumTextures());
		mTextures[i] = texture;
	}

	/*
	 * Get specified texture.
	 *
	 */
	ResourcePtr PbrMaterial::getTexture(int i) const
	{
		return mTextures[i];
	}
	/*
	 * Set uniforms.
	 *
	 */
	void PbrMaterial::setUniforms()
	{
		mUniforms.bindUniforms(mProgram);
	}
}