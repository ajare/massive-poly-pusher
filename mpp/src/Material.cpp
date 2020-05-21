#include "mpp/Material.h"
#include "mpp/MaterialStream.h"
#include "mpp/RenderSystem.h"
#include "mpp/ResourceManager.h"
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
			THROW_MPP("Could not cast to type 'MaterialStream'", __LINE__, __FILE__, __FUNCTION__);
		}

		auto resourceMgr = getResourceManager();
		
		// Create program and build information about it.
		mProgram = resourceMgr->getResource(mStr->getProgram());
		mProgram->load();

		Program* program = (Program*)(mProgram.get());

		Uniform<float> floatUniform;
		auto const& uniforms = mStr->getFloatUniforms();
		for (auto uniform: uniforms)
		{
			floatUniform.valueCount = uniform.second.valueCount;
			switch (uniform.second.valueCount)
			{
			case 4:
				floatUniform.values[3] = uniform.second.values[3];

			case 3:
				floatUniform.values[2] = uniform.second.values[2];

			case 2:
				floatUniform.values[1] = uniform.second.values[1];

			case 1:
				floatUniform.values[0] = uniform.second.values[0];
				break;
			}

			mFloatUniforms.push_back(make_pair(program->getUniformId(uniform.first), floatUniform));
		}
		
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
				THROW_MPP(errMsg, __LINE__, __FILE__, __FUNCTION__);
			}

			mTextures.push_back(resourceMgr->getResource(it->second));
		}
	}

	/*
	 * Destroy material.
	 *
	 */
	void Material::destroyImpl()
	{
		mProgram.reset();
		mFloatUniforms.clear();
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
		// Material uniforms
		for (auto uniform: mFloatUniforms)
		{
			auto const& uniformData = uniform.second;
			switch (uniformData.valueCount)
			{
			case 1:
				glUniform1f(uniform.first, uniformData.values[0]);
				break;

			case 2:
				glUniform2fv(uniform.first, 1, uniformData.values);
				break;

			case 3:
				glUniform3fv(uniform.first, 1, uniformData.values);
				break;

			case 4:
				glUniform4fv(uniform.first, 1, uniformData.values);
				break;
			}
		}
	}
}