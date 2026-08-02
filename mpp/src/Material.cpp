#include "utils/FileSystem.h"

#include "mpp/Material.h"
#include "mpp/MaterialStream.h"
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
	Material::Material(string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream)
		: Resource(name, "Material", renderSystem, resourceMgr, resourceStream)
	{
	}

	/*
	 * Destructor
	 *
	 */
	Material::~Material()
	{
		destroy();
	}

	/*
	 * Create material.
	 *
	 */
	void Material::createImpl()
	{
		MaterialStream* mStr = dynamic_cast<MaterialStream*>(getResourceStream().get());
		if (!mStr)
		{
			THROW_MPP("Could not cast to type 'MaterialStream'", __LINE__, __FILE__, __func__);
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
			case MaterialSpecification::ProgramOptions::Shader::Type::Default:
				vertexShaderSrc = "";
				break;

			case MaterialSpecification::ProgramOptions::Shader::Type::File:
				vertexShaderSrc = utils::FileSystem::readTextFile(progOpts.vertexShader.data);
				break;

			case MaterialSpecification::ProgramOptions::Shader::Type::Resource:
				vertexShaderSrc = dynamic_cast<String*>(getResourceManager()->getResource(progOpts.vertexShader.data).get())->getData();
				break;

			default:
				THROW_MPP("Unknown MaterialStream::ProgramOptions::Shader::Type", __LINE__, __FILE__, __func__);
			}

			string geometryShaderSrc;
			switch (progOpts.geometryShader.type)
			{
			case MaterialSpecification::ProgramOptions::Shader::Type::Default:
				geometryShaderSrc = "";
				break;

			case MaterialSpecification::ProgramOptions::Shader::Type::File:
				geometryShaderSrc = utils::FileSystem::readTextFile(progOpts.geometryShader.data);
				break;

			case MaterialSpecification::ProgramOptions::Shader::Type::Resource:
				geometryShaderSrc = dynamic_cast<String*>(getResourceManager()->getResource(progOpts.geometryShader.data).get())->getData();
				break;

			default:
				THROW_MPP("Unknown MaterialStream::ProgramOptions::Shader::Type", __LINE__, __FILE__, __func__);
			}

			string fragmentShaderSrc;
			switch (progOpts.fragmentShader.type)
			{
			case MaterialSpecification::ProgramOptions::Shader::Type::Default:
				fragmentShaderSrc = "";
				break;

			case MaterialSpecification::ProgramOptions::Shader::Type::File:
				fragmentShaderSrc = utils::FileSystem::readTextFile(progOpts.fragmentShader.data);
				break;

			case MaterialSpecification::ProgramOptions::Shader::Type::Resource:
				fragmentShaderSrc = dynamic_cast<String*>(getResourceManager()->getResource(progOpts.fragmentShader.data).get())->getData();
				break;

			default:
				THROW_MPP("Unknown MaterialStream::ProgramOptions::Shader::Type", __LINE__, __FILE__, __func__);
			}

			// Get or create program, either with default shaders or loaded strings in ProgOpts.
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
		// MPP model files retain backwards-compatible material streams. PBR
		// metadata is mirrored into PBR_* uniforms by FileMaterialStream, so
		// recover the fallback-texture contract after deserializing such a model.
		if (!mPbrSurface.enabled && mUniforms.getUniformData().find(MPP_PROGRAM_MARKUP_UNIFORM(string("PBR_ENABLED"))) != mUniforms.getUniformData().end())
		{
			mPbrSurface.enabled = true;
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
		Program* program = (Program*)(mProgram.get());
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
				[samplerName] (MaterialSpecification::TextureOptions const& textureOptions) 
			{
				return textureOptions.sampler == samplerName;
			});
			
			string textureName;
			if (it == materialTextures.end())
			{
				if (mPbrSurface.enabled)
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
	void Material::destroyImpl()
	{
	}

	/*
	 * Load material.
	 *
	 */
	void Material::loadImpl()
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
	void Material::unloadImpl()
	{
	}

	/*
	 * Get the program this material uses.
	 *
	 */
	ResourcePtr Material::getProgram()
	{
		return mProgram;
	}

	bool Material::isPbr() const
	{
		return mPbrSurface.enabled;
	}

	MaterialSpecification::PbrSurface const& Material::getPbrSurface() const
	{
		return mPbrSurface;
	}
	
	/*
	 * Get number of textures.
	 *
	 */
	int Material::getNumTextures() const
	{
		return (int)mTextures.size();
	}

	void Material::setTexture(int i, ResourcePtr texture)
	{
		assert(i >= 0 && i < getNumTextures());
		mTextures[i] = texture;
	}

	/*
	 * Get specified texture.
	 *
	 */
	ResourcePtr Material::getTexture(int i) const
	{
		return mTextures[i];
	}
	/*
	 * Set uniforms.
	 *
	 */
	void Material::setUniforms()
	{
		mUniforms.bindUniforms(mProgram);
	}
}