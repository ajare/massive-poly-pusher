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
			switch (mStr->getTextures().size())
			{
			case 0:
				break;
			case 4:
				programFlags |= MPP_PROGRAM_TAGS_TEXTURE4;
			case 3:
				programFlags |= MPP_PROGRAM_TAGS_TEXTURE3;
			case 2:
				programFlags |= MPP_PROGRAM_TAGS_TEXTURE2;
			case 1:
				programFlags |= MPP_PROGRAM_TAGS_TEXTURE1;
				break;

			default:
				THROW_MPP("Cannot use more than 4 textures in a material.", __LINE__, __FILE__, __func__);
			}

			// Load in shaders if required
			string vertexShaderSrc;
			switch (progOpts.vertexShader.type)
			{
			case MaterialStream::ProgramOptions::Shader::Type::String:
				vertexShaderSrc = progOpts.vertexShader.data;
				break;

			case MaterialStream::ProgramOptions::Shader::Type::File:
				//load from file
				break;

			case MaterialStream::ProgramOptions::Shader::Type::Resource:
				//existing string resource
				break;

			default:
				THROW_MPP("Unknown MaterialStream::ProgramOptions::Shader::Type", __LINE__, __FILE__, __func__);
			}

			string fragmentShaderSrc;
			switch (progOpts.fragmentShader.type)
			{
			case MaterialStream::ProgramOptions::Shader::Type::String:
				fragmentShaderSrc = progOpts.fragmentShader.data;
				break;

			case MaterialStream::ProgramOptions::Shader::Type::File:
				//load from file
				break;

			case MaterialStream::ProgramOptions::Shader::Type::Resource:
				//existing string resource
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

		mProgram->load();

		// Set uniforms
		mUniforms = mStr->getUniforms();
		
		// Set textures
		Program* program = (Program*)(mProgram.get());
		auto materialTextures = mStr->getTextures();

		// Go through each texture, get the binding location.
		for (int i = 0; i < program->getNumSamplers(); ++i)
		{
			string const& samplerName = program->getSamplerName(i);

			auto it = materialTextures.find(samplerName);
			if (it == materialTextures.end())
			{
				string errMsg = utils::StringUtils::format("Sampler '{}' declared in program '{}' is not bound by material '{}'.",
					samplerName, program->getName(), getName());
				THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
			}

			string textureName;
			if (it->second.second)
			{
				// Texture is a child resource, so we need to mark it up
				textureName = getName() + "/" + it->second.first;
			}
			else
			{
				textureName = it->second.first;
			}

			mTextures.push_back(resourceMgr->getResource(textureName));
		}
	}

	/*
	 * Destroy material.
	 *
	 */
	void Material::destroyImpl()
	{
		mProgram.reset();
		mTextures.clear();
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