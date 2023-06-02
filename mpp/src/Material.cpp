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

		mProgram->acquire();
		mProgram->load();

		// Set uniforms
		mUniforms = mStr->getUniforms();
		
		// Set textures
		Program* program = (Program*)(mProgram.get());
		auto const& materialTextures = mStr->getTextures();

		// Go through each texture, get the binding location.
		for (int i = 0; i < program->getNumSamplers(); ++i)
		{
			string const& samplerName = program->getSamplerName(i);
			
			auto it = find_if(materialTextures.begin(), materialTextures.end(), 
				[samplerName] (MaterialSpecification::TextureOptions const& textureOptions) 
			{
				return textureOptions.sampler == samplerName;
			});
			
			if (it == materialTextures.end())
			{
				string errMsg = utils::StringUtils::format("Sampler '{}' declared in program '{}' is not bound by material '{}'.",
					samplerName, program->getName(), getName());
				THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
			}

			auto const& textureOptions = *it;

			string textureName = textureOptions.isChild
				? getName() + "/" + textureOptions.existingResource
				: textureOptions.existingResource;

			mTextures.push_back(resourceMgr->acquireResource(textureName));
		}
	}

	/*
	 * Destroy material.
	 *
	 */
	void Material::destroyImpl()
	{
		mProgram->release();
		
		for (auto texture : mTextures)
		{
			texture->release();
		}
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
		// Todo: reference counting on the resource such that it will unload
		// when not referenced?
	}

	/*
	 * Get the program this material uses.
	 *
	 */
	ResourcePtr Material::getProgram()
	{
		return mProgram;
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